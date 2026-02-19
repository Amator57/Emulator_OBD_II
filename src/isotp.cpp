#include "shared.h"

IsoTpLink isoTpLink;

// Helper to send a raw CAN frame
static void send_can_frame(uint32_t id, bool ext, const uint8_t* data, uint8_t len) {
    twai_message_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.identifier = id;
    frame.extd = ext;
    frame.data_length_code = len;
    memcpy(frame.data, data, len);
    logCAN(frame, false); // Log TX
    twai_transmit(&frame, pdMS_TO_TICKS(10));
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
        isoTpLink.rxId = frame.identifier;
        isoTpLink.rxLen = frame.data[0] & 0x0F;
        if (isoTpLink.rxLen == 0) return; // Invalid SF
        memcpy(isoTpLink.rxBuffer, &frame.data[1], isoTpLink.rxLen);
        // SF is immediately complete
        // We don't change state to IDLE here because we want isotp_get_message to pick it up.
        // But since we don't have a "Message Ready" queue, we assume polling picks it up immediately.
        // For simplicity in this loop-based architecture, we set a flag or just let get_message read it.
        // Actually, get_message checks rxLen > 0 and state IDLE? No, SF doesn't use state machine.
        // We'll handle SF by setting a "ready" state or just filling the buffer and letting the app read it.
        // Let's assume the app calls isotp_get_message frequently.
        isoTpLink.rxState = ISOTP_IDLE; // SF is stateless
    }
    else if (pci_type == 0x01) { // First Frame (FF)
        isoTpLink.rxId = frame.identifier;
        isoTpLink.rxExpectedLen = ((frame.data[0] & 0x0F) << 8) | frame.data[1];
        if (isoTpLink.rxExpectedLen > ISOTP_MAX_DATA_LEN) return; // Overflow

        isoTpLink.rxLen = 0;
        memcpy(isoTpLink.rxBuffer, &frame.data[2], 6);
        isoTpLink.rxLen = 6;
        isoTpLink.rxNextSeq = 1;
        isoTpLink.rxState = ISOTP_RX_WAIT_CF;
        isoTpLink.rxTimer = millis();

        // Send Flow Control (CTS)
        // ID: Request ID + 8 (Standard practice for 11-bit) or specific logic.
        // For this emulator, we reply to the sender.
        // If RX ID is 0x7E0, we reply on 0x7E8.
        // If RX ID is 0x18DA10F1, we reply on 0x18DAF110.
        // We need to know "who" we are.
        // Simplified: Reply ID = RX ID + 8 (11-bit) or Swap Source/Target (29-bit)
        uint32_t replyId = 0;
        if (frame.extd) {
            // Swap Source (bits 0-7) and Target (bits 8-15)
            uint32_t sa = frame.identifier & 0xFF;
            uint32_t ta = (frame.identifier >> 8) & 0xFF;
            uint32_t prio = (frame.identifier >> 24) & 0x1F;
            replyId = (prio << 24) | (sa << 8) | ta;
        } else {
            replyId = frame.identifier + 8; // Standard OBD offset
        }

        if (fault_wrong_flow_control) {
            uint8_t fc_data[3] = {0x31, 0x00, 0x00}; // FC: WAIT
            send_can_frame(replyId, frame.extd, fc_data, 3);
        } else {
            uint8_t fc_data[3] = {0x30, 0x00, 0x00}; // FC: CTS, BS=0 (unlimited), STmin=0
            send_can_frame(replyId, frame.extd, fc_data, 3);
        }
    }
    else if (pci_type == 0x02) { // Consecutive Frame (CF)
        if (isoTpLink.rxState != ISOTP_RX_WAIT_CF) return;
        if (frame.identifier != isoTpLink.rxId) return; // Not our stream

        uint8_t seq = frame.data[0] & 0x0F;
        if (seq != isoTpLink.rxNextSeq) {
            // Wrong sequence, reset
            isoTpLink.rxState = ISOTP_IDLE;
            return;
        }

        int bytes_to_copy = min(7, isoTpLink.rxExpectedLen - isoTpLink.rxLen);
        memcpy(&isoTpLink.rxBuffer[isoTpLink.rxLen], &frame.data[1], bytes_to_copy);
        isoTpLink.rxLen += bytes_to_copy;
        isoTpLink.rxNextSeq = (isoTpLink.rxNextSeq + 1) & 0x0F;
        isoTpLink.rxTimer = millis();

        if (isoTpLink.rxLen >= isoTpLink.rxExpectedLen) {
            isoTpLink.rxState = ISOTP_IDLE; // Complete
        }
    }
    else if (pci_type == 0x03) { // Flow Control (FC)
        if (isoTpLink.txState != ISOTP_TX_WAIT_FC) return;
        
        uint8_t fs = frame.data[0] & 0x0F;
        if (fs == 0) { // CTS
            isoTpLink.txBlockSize = frame.data[1];
            isoTpLink.txStMin = frame.data[2];
            
            // Decode STmin
            if (isoTpLink.txStMin > 0x7F && (isoTpLink.txStMin < 0xF1 || isoTpLink.txStMin > 0xF9)) isoTpLink.txStMin = 127; // Max ms
            
            isoTpLink.txBsCounter = 0;
            isoTpLink.txState = ISOTP_TX_SEND_CF;
            isoTpLink.txTimer = millis();
        } else if (fs == 1) { // WAIT
            isoTpLink.txTimer = millis(); // Reset timeout
        } else { // OVFL or Reserved
            isoTpLink.txState = ISOTP_IDLE; // Abort
        }
    }
}

void isotp_poll() {
    // --- RX Timeout ---
    if (isoTpLink.rxState == ISOTP_RX_WAIT_CF) {
        if (millis() - isoTpLink.rxTimer > ISOTP_TIMEOUT_MS) {
            isoTpLink.rxState = ISOTP_IDLE;
            isoTpLink.rxLen = 0; // Discard partial
        }
    }

    // --- TX Handling ---
    if (isoTpLink.txState == ISOTP_TX_SEND_CF) {
        // Check STmin
        unsigned long now = millis();
        unsigned long delay_needed = 0;
        
        if (!fault_stmin_overflow) {
            if (isoTpLink.txStMin <= 0x7F) delay_needed = isoTpLink.txStMin;
            else delay_needed = 1; // Microsecond ranges treated as 1ms for simplicity
        }

        if (now - isoTpLink.txTimer >= delay_needed) {
            // Send CF
            uint8_t frame_data[8];
            uint8_t seq_to_send = isoTpLink.txSeq;
            if (fault_incorrect_sequence && (rand() % 3 == 0)) seq_to_send = (seq_to_send + 1) & 0x0F; // Inject error
            
            frame_data[0] = 0x20 | (seq_to_send & 0x0F);
            
            int bytes_left = isoTpLink.txTotalLen - isoTpLink.txBytesSent;
            int chunk = min(7, bytes_left);
            memcpy(&frame_data[1], &isoTpLink.txBuffer[isoTpLink.txBytesSent], chunk);
            
            // Padding if needed (optional, but good for compatibility)
            if (chunk < 7) {
                for (int i = 1 + chunk; i < 8; i++) frame_data[i] = 0xAA;
            }

            send_can_frame(isoTpLink.txId, isoTpLink.txExtended, frame_data, 8); // CF is always 8 bytes usually

            isoTpLink.txBytesSent += chunk;
            isoTpLink.txSeq++;
            isoTpLink.txTimer = now;

            // Partial VIN/Multi-frame fault: Stop sending after the first CF
            if (fault_partial_vin && isoTpLink.txBytesSent > 7) {
                isoTpLink.txState = ISOTP_IDLE; // Abort transmission
                return;
            }

            if (isoTpLink.txBytesSent >= isoTpLink.txTotalLen) {
                isoTpLink.txState = ISOTP_IDLE; // Done
            } else {
                // Handle Block Size
                if (isoTpLink.txBlockSize > 0) {
                    isoTpLink.txBsCounter++;
                    if (isoTpLink.txBsCounter >= isoTpLink.txBlockSize) {
                        isoTpLink.txState = ISOTP_TX_WAIT_FC;
                        isoTpLink.txTimer = now;
                    }
                }
            }
        }
    }
    else if (isoTpLink.txState == ISOTP_TX_WAIT_FC) {
        if (millis() - isoTpLink.txTimer > ISOTP_TIMEOUT_MS) {
            isoTpLink.txState = ISOTP_IDLE; // Timeout waiting for FC
        }
    }
}

bool isotp_send(uint32_t id, const uint8_t* data, uint16_t len, bool extended) {
    if (isoTpLink.txState != ISOTP_IDLE) return false; // Busy

    isoTpLink.txId = id;
    isoTpLink.txExtended = extended;
    isoTpLink.txTotalLen = len;
    
    if (len > ISOTP_MAX_DATA_LEN) return false;
    memcpy(isoTpLink.txBuffer, data, len);

    if (len <= 7) {
        // Single Frame
        uint8_t frame_data[8];
        frame_data[0] = len;
        memcpy(&frame_data[1], data, len);
        // Padding
        for (int i = 1 + len; i < 8; i++) frame_data[i] = 0xAA;
        
        send_can_frame(id, extended, frame_data, 8);
        isoTpLink.txState = ISOTP_IDLE;
    } else {
        // First Frame
        uint8_t frame_data[8];
        frame_data[0] = 0x10 | ((len >> 8) & 0x0F);
        frame_data[1] = len & 0xFF;
        memcpy(&frame_data[2], data, 6);
        
        send_can_frame(id, extended, frame_data, 8);
        
        isoTpLink.txBytesSent = 6;
        isoTpLink.txSeq = 1;
        isoTpLink.txState = ISOTP_TX_WAIT_FC;
        isoTpLink.txTimer = millis();
    }
    return true;
}

bool isotp_get_message(uint32_t* id, uint8_t* data, uint16_t* len) {
    // Check if we have a completed message in RX buffer
    // For SF, rxLen > 0 and state is IDLE (set in on_frame)
    // For Multi-frame, state goes to IDLE after last CF
    
    if (isoTpLink.rxLen > 0 && isoTpLink.rxState == ISOTP_IDLE) {
        *id = isoTpLink.rxId;
        *len = isoTpLink.rxLen;
        memcpy(data, isoTpLink.rxBuffer, isoTpLink.rxLen);
        
        // Clear buffer/len to indicate consumed
        isoTpLink.rxLen = 0;
        return true;
    }
    return false;
}