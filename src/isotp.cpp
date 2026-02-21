#include "shared.h"
#include <driver/twai.h>

IsoTpLink isoTpLink;

// Helper to send a raw CAN frame
static void isotp_send_can(uint32_t id, const uint8_t* data, uint8_t len, bool ext) {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(twai_message_t));
    tx_frame.identifier = id;
    tx_frame.extd = ext;
    tx_frame.data_length_code = len;
    memcpy(tx_frame.data, data, len);
    
    // Відправка з таймаутом 10мс
    if (twai_transmit(&tx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
        logCAN(tx_frame, false); // Логування TX
    } else {
        Serial.println("TWAI Transmit Failed");
    }
}

void isotp_init() {
    memset(&isoTpLink, 0, sizeof(IsoTpLink));
    isoTpLink.rxState = ISOTP_IDLE;
    isoTpLink.txState = ISOTP_IDLE;
}

void isotp_on_frame(const twai_message_t& frame) {
    uint8_t pci_type = (frame.data[0] & 0xF0) >> 4;

    // --- RX Handling ---
    if (pci_type == 0x00) { // Single Frame (SF)
        if (isoTpLink.rxState != ISOTP_IDLE) {
             isoTpLink.rxState = ISOTP_IDLE; // Abort previous multi-frame reception
             isoTpLink.rxLen = 0;
        }
        
        isoTpLink.rxLen = frame.data[0] & 0x0F;
        if (isoTpLink.rxLen == 0 || isoTpLink.rxLen > 7) { // Invalid SF length
            isoTpLink.rxLen = 0;
            return;
        }
        
        isoTpLink.rxId = frame.identifier;
        memcpy(isoTpLink.rxBuffer, &frame.data[1], isoTpLink.rxLen);
        isoTpLink.rxExpectedLen = 0; // SF is a complete message
        isoTpLink.rxState = ISOTP_IDLE; // Message is ready to be read by isotp_get_message
    }
    else if (pci_type == 0x01) { // First Frame (FF)
        isoTpLink.rxExpectedLen = ((frame.data[0] & 0x0F) << 8) | frame.data[1];
        if (isoTpLink.rxExpectedLen > ISOTP_MAX_DATA_LEN) return; // Overflow, ignore packet

        isoTpLink.rxId = frame.identifier;
        isoTpLink.rxLen = 0;
        memcpy(isoTpLink.rxBuffer, &frame.data[2], 6);
        isoTpLink.rxLen = 6;
        isoTpLink.rxNextSeq = 1;
        isoTpLink.rxState = ISOTP_RX_WAIT_CF;
        isoTpLink.rxTimer = millis();

        // Determine Reply ID
        uint32_t replyId = 0;
        if (frame.extd) {
            // For 29-bit UDS, the reply ID is constructed by swapping source and target addresses
            // Example: Request 0x18DA10F1 (TA=10, SA=F1) -> Reply 0x18DAF110 (TA=F1, SA=10)
            uint32_t sa = frame.identifier & 0xFF; // Source Address
            uint32_t ta = (frame.identifier >> 8) & 0xFF; // Target Address
            // The rest of the ID (prio, etc.) is usually preserved.
            // Assuming format is 0x18DA<TA><SA>
            replyId = (frame.identifier & 0xFFFF0000) | (sa << 8) | ta;
        } else {
            // For 11-bit OBD, reply is usually Tester ID + 8
            // Example: Request 0x7E0 -> Reply 0x7E8
            replyId = frame.identifier + 8;
        }

        if (fault_wrong_flow_control) {
            uint8_t fc_data[8] = {0x31, 0x00, 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // FC: WAIT
            isotp_send_can(replyId, fc_data, 8, frame.extd);
        } else {
            uint8_t fc_data[8] = {0x30, 0x00, 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // FC: CTS, BS=0, STmin=0
            isotp_send_can(replyId, fc_data, 8, frame.extd);
        }
    }
    else if (pci_type == 0x02) { // Consecutive Frame (CF)
        if (isoTpLink.rxState != ISOTP_RX_WAIT_CF || frame.identifier != isoTpLink.rxId) {
            return; // Not for us or we are not waiting for it
        }

        uint8_t seq = frame.data[0] & 0x0F;
        if (seq != isoTpLink.rxNextSeq) {
            // Wrong sequence number, abort reception
            isoTpLink.rxState = ISOTP_IDLE;
            isoTpLink.rxLen = 0;
            return;
        }

        uint16_t bytes_to_copy = min((uint16_t)7, (uint16_t)(isoTpLink.rxExpectedLen - isoTpLink.rxLen));
        memcpy(&isoTpLink.rxBuffer[isoTpLink.rxLen], &frame.data[1], bytes_to_copy);
        isoTpLink.rxLen += bytes_to_copy;
        isoTpLink.rxNextSeq = (isoTpLink.rxNextSeq + 1) & 0x0F;
        isoTpLink.rxTimer = millis();

        if (isoTpLink.rxLen >= isoTpLink.rxExpectedLen) {
            isoTpLink.rxState = ISOTP_IDLE; // Reception complete
        }
    }
    else if (pci_type == 0x03) { // Flow Control (FC)
        if (isoTpLink.txState != ISOTP_TX_WAIT_FC) return;
        
        uint8_t fs = frame.data[0] & 0x0F; // Flow Status
        if (fs == 0) { // CTS (Continue To Send)
            isoTpLink.txBlockSize = frame.data[1];
            isoTpLink.txStMin = frame.data[2];
            
            // As per ISO 15765-2, values 0x80-0xF0 are reserved and should be interpreted as 0x7F (127ms)
            if (isoTpLink.txStMin > 0x7F && (isoTpLink.txStMin < 0xF1 || isoTpLink.txStMin > 0xF9)) {
                isoTpLink.txStMin = 127; 
            }
            
            isoTpLink.txBsCounter = 0;
            isoTpLink.txState = ISOTP_TX_SEND_CF;
            isoTpLink.txTimer = millis(); // Start timer for STmin
        } else if (fs == 1) { // WAIT
            isoTpLink.txTimer = millis(); // Reset timeout while waiting
        } else { // OVFLW (Overflow) or reserved
            isoTpLink.txState = ISOTP_IDLE; // Abort transmission
        }
    }
}

void isotp_poll() {
    unsigned long now = millis();

    // --- RX Timeout (N_Cr) ---
    if (isoTpLink.rxState == ISOTP_RX_WAIT_CF) {
        if (now - isoTpLink.rxTimer > ISOTP_TIMEOUT_MS) {
            isoTpLink.rxState = ISOTP_IDLE;
            isoTpLink.rxLen = 0; // Discard partial message
            Serial.println("ISO-TP RX Timeout");
        }
    }

    // --- TX Handling ---
    if (isoTpLink.txState == ISOTP_TX_SEND_CF) {
        unsigned long delay_needed = 0;
        
        if (!fault_stmin_overflow) {
            if (isoTpLink.txStMin <= 0x7F) { // 0-127 ms
                delay_needed = isoTpLink.txStMin;
            } else if (isoTpLink.txStMin >= 0xF1 && isoTpLink.txStMin <= 0xF9) { // 100-900 us
                delay_needed = 1; // Treat as 1ms for simplicity in a non-realtime system
            }
        }
        // if fault_stmin_overflow is true, delay_needed is 0, so we send as fast as possible

        if (now - isoTpLink.txTimer >= delay_needed) {
            // Check BlockSize before sending
            if (isoTpLink.txBlockSize > 0 && isoTpLink.txBsCounter >= isoTpLink.txBlockSize) {
                isoTpLink.txState = ISOTP_TX_WAIT_FC;
                isoTpLink.txTimer = now;
                return;
            }

            // Send Consecutive Frame (CF)
            uint8_t frame_data[8];
            uint8_t seq_to_send = isoTpLink.txSeq;
            if (fault_incorrect_sequence && (rand() % 4 == 0)) { // Inject error sometimes
                seq_to_send = (seq_to_send + 1 + (rand() % 3)) & 0x0F; 
            }
            
            frame_data[0] = 0x20 | (seq_to_send & 0x0F);
            
            uint16_t bytes_left = isoTpLink.txTotalLen - isoTpLink.txBytesSent;
            uint8_t chunk = min((uint16_t)7, bytes_left);
            memcpy(&frame_data[1], &isoTpLink.txBuffer[isoTpLink.txBytesSent], chunk);
            
            // Pad unused bytes
            for (int i = 1 + chunk; i < 8; i++) {
                frame_data[i] = 0xAA; // Common padding value
            }

            isotp_send_can(isoTpLink.txId, frame_data, 8, isoTpLink.txExtended);

            isoTpLink.txBytesSent += chunk;
            isoTpLink.txSeq = (isoTpLink.txSeq + 1) & 0x0F;
            isoTpLink.txBsCounter++;
            isoTpLink.txTimer = now;

            // Fault: Stop sending after the first few frames
            if (fault_partial_vin && isoTpLink.txBytesSent > 7) {
                isoTpLink.txState = ISOTP_IDLE; // Abort transmission silently
                return;
            }

            if (isoTpLink.txBytesSent >= isoTpLink.txTotalLen) {
                isoTpLink.txState = ISOTP_IDLE; // Transmission complete
            }
        }
    }
    else if (isoTpLink.txState == ISOTP_TX_WAIT_FC) {
        // TX Timeout (N_Bs)
        if (now - isoTpLink.txTimer > ISOTP_TIMEOUT_MS) {
            isoTpLink.txState = ISOTP_IDLE; // Timeout waiting for Flow Control
            Serial.println("ISO-TP TX Timeout (FC)");
        }
    }
}

bool isotp_send(uint32_t id, const uint8_t* data, uint16_t len, bool extended) {
    if (isoTpLink.txState != ISOTP_IDLE) return false; // Transmitter is busy

    if (len > ISOTP_MAX_DATA_LEN) return false; // Message too long
    
    isoTpLink.txId = id;
    isoTpLink.txExtended = extended;
    isoTpLink.txTotalLen = len;
    memcpy(isoTpLink.txBuffer, data, len);

    if (len <= 7) {
        // Single Frame (SF)
        uint8_t frame_data[8] = {0};
        frame_data[0] = len;
        memcpy(&frame_data[1], data, len);
        // Pad with a common value
        for (int i = 1 + len; i < 8; i++) frame_data[i] = 0xAA;
        
        isotp_send_can(id, frame_data, 8, extended);
        // No state change needed, txState is already IDLE
    } else {
        // First Frame (FF) of a multi-frame message
        uint8_t frame_data[8];
        frame_data[0] = 0x10 | ((len >> 8) & 0x0F);
        frame_data[1] = len & 0xFF;
        memcpy(&frame_data[2], data, 6);
        
        isotp_send_can(id, frame_data, 8, extended);
        
        isoTpLink.txBytesSent = 6;
        isoTpLink.txSeq = 1;
        isoTpLink.txState = ISOTP_TX_WAIT_FC;
        isoTpLink.txTimer = millis();
    }
    return true;
}

bool isotp_get_message(uint32_t* id, uint8_t* data, uint16_t* len) {
    // This function is polled from the main loop to check for a completed message.
    if (isoTpLink.rxLen > 0 && isoTpLink.rxState == ISOTP_IDLE) {
        *id = isoTpLink.rxId;
        *len = isoTpLink.rxLen;
        memcpy(data, isoTpLink.rxBuffer, isoTpLink.rxLen);
        
        // Mark the message as consumed
        isoTpLink.rxLen = 0;
        return true;
    }
    return false;
}