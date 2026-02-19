#include "shared.h"

extern int frame_delay_ms;

// --- Helper Prototypes ---
void sendUDSResponse(ECU &ecu, byte service, byte* data, int len, bool use29bit);
void sendUDSNegativeResponse(ECU &ecu, byte service, byte nrc, bool use29bit);
void udsDiagnosticSessionControl(ECU &ecu, byte subfunction, bool use29bit);
void udsEcuReset(ECU &ecu, byte subfunction, bool use29bit);
void udsSecurityAccess(ECU &ecu, byte subfunction, byte* data, int len, bool use29bit);
void udsReadDataByIdentifier(ECU &ecu, uint16_t did, bool use29bit);
void udsClearDiagnosticInformation(ECU &ecu, bool use29bit);
void udsReadDTCInformation(ECU &ecu, byte subfunction, byte* data, int len, bool use29bit);

void handleUDSRequest(ECU &ecu, uint32_t id, const uint8_t* data, uint16_t len) {
    if (!ecu.enabled) return;

    byte service = data[0];
    byte subfunction = (len > 1) ? data[1] : 0;
    bool use29bit = (id > 0x7FF);
    
    Serial.printf("ECU %s: Received UDS Request Service 0x%02X\n", ecu.name, service);

    switch(service) {
        case 0x10: // Diagnostic Session Control
            udsDiagnosticSessionControl(ecu, subfunction, use29bit);
            break;
        case 0x11: // ECU Reset
            udsEcuReset(ecu, subfunction, use29bit);
            break;
        case 0x14: // Clear Diagnostic Information
            udsClearDiagnosticInformation(ecu, use29bit);
            break;
        case 0x19: // Read DTC Information
            udsReadDTCInformation(ecu, subfunction, (byte*)&data[2], len - 2, use29bit);
            break;
        case 0x22: // Read Data By Identifier
            {
                if (len < 3) {
                    sendUDSNegativeResponse(ecu, service, 0x13, use29bit); // Incorrect Message Length
                    return;
                }
                uint16_t did = (data[1] << 8) | data[2];
                udsReadDataByIdentifier(ecu, did, use29bit);
            }
            break;
        case 0x27: // Security Access
            udsSecurityAccess(ecu, subfunction, (byte*)&data[2], len - 2, use29bit);
            break;
        case 0x3E: // Tester Present
            // 0x00: Response Required, 0x80: No Response Required
            if ((subfunction & 0x80) == 0) {
                byte resp[] = { 0x00 };
                sendUDSResponse(ecu, service, resp, 1, use29bit);
            }
            break;
        default:
            sendUDSNegativeResponse(ecu, service, 0x11, use29bit); // Service Not Supported
            break;
    }
}

void udsDiagnosticSessionControl(ECU &ecu, byte subfunction, bool use29bit) {
    // 0x01: Default, 0x02: Programming, 0x03: Extended
    if (subfunction == 0x01 || subfunction == 0x02 || subfunction == 0x03) {
        ecu.uds_session = subfunction;
        if (subfunction == 0x01) ecu.uds_security_unlocked = false;
        
        // Response: [Session, P2_high, P2_low, P2*_high, P2*_low]
        byte resp[] = { subfunction, 0x00, 0x32, 0x13, 0x88 };
        sendUDSResponse(ecu, 0x10, resp, 5, use29bit);
    } else {
        sendUDSNegativeResponse(ecu, 0x10, 0x12, use29bit); // Sub-function not supported
    }
}

void udsEcuReset(ECU &ecu, byte subfunction, bool use29bit) {
    if (subfunction == 0x01) { // Hard Reset
        byte resp[] = { 0x01 };
        sendUDSResponse(ecu, 0x11, resp, 1, use29bit);
        Serial.println("ECU Reset triggered (Simulation)");
        ecu.uds_session = 0x01;
        ecu.uds_security_unlocked = false;
    } else {
        sendUDSNegativeResponse(ecu, 0x11, 0x12, use29bit);
    }
}

void udsSecurityAccess(ECU &ecu, byte subfunction, byte* data, int len, bool use29bit) {
    if (subfunction % 2 != 0) { // Request Seed (Odd numbers: 01, 03, etc.)
        ecu.uds_seed = millis();
        byte resp[] = { subfunction, (byte)((ecu.uds_seed >> 24) & 0xFF), (byte)((ecu.uds_seed >> 16) & 0xFF), (byte)((ecu.uds_seed >> 8) & 0xFF), (byte)(ecu.uds_seed & 0xFF) };
        sendUDSResponse(ecu, 0x27, resp, 5, use29bit);
    } else { // Send Key (Even numbers: 02, 04, etc.)
        if (len < 4) {
            sendUDSNegativeResponse(ecu, 0x27, 0x13, use29bit);
            return;
        }
        uint32_t key = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        if (key == ecu.uds_seed + 1) {
            ecu.uds_security_unlocked = true;
            byte resp[] = { subfunction };
            sendUDSResponse(ecu, 0x27, resp, 1, use29bit);
        } else {
            sendUDSNegativeResponse(ecu, 0x27, 0x35, use29bit); // Invalid Key
        }
    }
}

void udsReadDataByIdentifier(ECU &ecu, uint16_t did, bool use29bit) {
    if (did == 0x1234) { // Battery Voltage
        uint16_t val = (uint16_t)(ecu.battery_voltage * 10);
        byte resp[] = { (byte)(did >> 8), (byte)(did & 0xFF), (byte)(val >> 8), (byte)(val & 0xFF) };
        sendUDSResponse(ecu, 0x22, resp, 4, use29bit);
    } else if (did == 0xF188) { // ECU Software Number
        byte resp[] = { 0xF1, 0x88, 'V', '1', '.', '0' };
        sendUDSResponse(ecu, 0x22, resp, 6, use29bit);
    } else if (did == 0xF190) { // VIN
        // Simplified VIN response (first 17 bytes + DID)
        // In real UDS this would be multi-frame.
        byte resp[19];
        resp[0] = 0xF1; resp[1] = 0x90;
        memcpy(&resp[2], ecu.vin, 17);
        sendUDSResponse(ecu, 0x22, resp, 19, use29bit);
    } else {
        sendUDSNegativeResponse(ecu, 0x22, 0x31, use29bit); // Request Out Of Range
    }
}

void udsClearDiagnosticInformation(ECU &ecu, bool use29bit) {
    // 14 FF FF FF (Group)
    clearDTCs(ecu, false); // Call shared logic (false = don't send OBD response inside)
    
    // Send UDS Positive Response
    byte resp[] = { }; // No data bytes for 0x14 response
    sendUDSResponse(ecu, 0x14, resp, 0, use29bit);
}

void udsReadDTCInformation(ECU &ecu, byte subfunction, byte* data, int len, bool use29bit) {
    if (subfunction == 0x01) { // ReportNumberOfDTCByStatusMask
        // Resp: [AvailabilityMask, Format, CountHigh, CountLow]
        byte count = ecu.num_dtcs;
        byte resp[] = { 0x01, 0xFF, 0x00, 0x00, count };
        sendUDSResponse(ecu, 0x19, resp, 5, use29bit);
    } else if (subfunction == 0x02) { // ReportDTCByStatusMask
        // Resp: [AvailabilityMask, DTC1_H, DTC1_M, DTC1_L, Status1, ...]
        byte resp[64];
        resp[0] = 0x02;
        resp[1] = 0xFF; // Mask
        
        int offset = 2;
        for (int i = 0; i < ecu.num_dtcs && i < 5; i++) {
            // Convert "P0123" to 3 bytes
            uint16_t code_val = atoi(&ecu.dtcs[i][1]);
            char type = ecu.dtcs[i][0];
            byte high = (code_val >> 8) & 0xFF;
            byte low = code_val & 0xFF;
            
            if (type == 'C') high |= 0x40;
            else if (type == 'B') high |= 0x80;
            else if (type == 'U') high |= 0xC0;
            
            resp[offset++] = high;
            resp[offset++] = low;
            resp[offset++] = 0x00; // Failure Type Byte (FTB)
            resp[offset++] = 0x2F; // Status (Confirmed, etc.)
        }
        sendUDSResponse(ecu, 0x19, resp, offset, use29bit);
    } else {
        sendUDSNegativeResponse(ecu, 0x19, 0x12, use29bit);
    }
}

void sendUDSResponse(ECU &ecu, byte service, byte* data, int len, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t buf[ISOTP_MAX_DATA_LEN] = {0};
    buf[0] = service + 0x40;
    if (len > 0) memcpy(&buf[1], data, len);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, buf, len + 1, use29bit);
}

void sendUDSNegativeResponse(ECU &ecu, byte service, byte nrc, bool use29bit) {
    uint8_t data[3] = {0x7F, service, nrc};
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 3, use29bit);
}