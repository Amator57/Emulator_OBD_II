#include "shared.h"

extern int frame_delay_ms;
extern int error_injection_rate;

// Forward declarations
void sendCurrentData(ECU &ecu, byte pid, bool use29bit);
void sendFreezeFrameData(ECU &ecu, byte pid, bool use29bit);
void sendDTCs(ECU &ecu, bool use29bit);
void sendPendingDTCs(ECU &ecu, bool use29bit);
void clearDTCs(ECU &ecu, bool use29bit);
void sendMode06Data(ECU &ecu, byte pid, bool use29bit);
void sendSupportedPids_09(ECU &ecu, byte pid, bool use29bit);
void sendVIN(ECU &ecu, byte pid, bool use29bit);
void sendCalId(ECU &ecu, byte pid, bool use29bit);
void sendCvn(ECU &ecu, byte pid, bool use29bit);
void sendPermanentDTCs(ECU &ecu, bool use29bit);
void sendNegativeResponse(byte service, byte nrc, bool use29bit);

void handleOBDRequest(uint32_t id, const uint8_t* data, uint16_t len) {
    // Error Injection
    if (error_injection_rate > 0) {
        if ((rand() % 100) < error_injection_rate) {
            Serial.println("Simulated Error: Dropped RX Frame");
            return;
        }
    }

    // Silent Mode
    if (fault_silent_mode) return;

    // Determine mode from ID (simplified, assuming ID passed is correct)
    bool is29Bit = (id > 0x7FF);
    bool is11Bit = !is29Bit;

    // Filter based on Mode
    // (Filtering already done mostly by main loop, but good to keep)

    if (len < 2) return;
    byte service = data[0]; // ISO-TP payload starts at 0
    byte pid = data[1];
    
    // Determine target ECU
    int targetEcuIndex = -1;
    bool isBroadcast = false;

    if (is11Bit) {
        if (id == 0x7DF) isBroadcast = true;
        else if (id >= 0x7E0 && id <= 0x7E7) {
            targetEcuIndex = id - 0x7E0;
        }
    } else {
        if (id == 0x18DB33F1) isBroadcast = true;
        else if ((id & 0xFFFF00FF) == 0x18DA00F1) {
            byte ta = (id >> 8) & 0xFF;
            if (ta == 0x10) targetEcuIndex = 0;
            else if (ta == 0x18) targetEcuIndex = 1;
        }
    }

    if (!isBroadcast && targetEcuIndex == -1) return;

    // Check for UDS Services (SID >= 0x10) excluding OBD-II modes
    if (service >= 0x10 && service != 0x01 && service != 0x02 && service != 0x03 && service != 0x04 && service != 0x06 && service != 0x07 && service != 0x09 && service != 0x0A) {
         if (targetEcuIndex != -1) handleUDSRequest(ecus[targetEcuIndex], id, data, len);
         return;
    }

    Serial.printf("Received OBD Request: Service 0x%02X, PID 0x%02X\n", service, pid);

    for (int i = 0; i < NUM_ECUS; i++) {
        if (!ecus[i].enabled) continue;
        if (isBroadcast || targetEcuIndex == i || fault_multiple_responses) {
            switch(service) {
                case 0x01: sendCurrentData(ecus[i], pid, is29Bit); break;
                case 0x02: sendFreezeFrameData(ecus[i], pid, is29Bit); break;
                case 0x03: sendDTCs(ecus[i], is29Bit); break;
                case 0x04: clearDTCs(ecus[i], is29Bit); break;
                case 0x05: 
                    // Mode 05 is not supported in CAN OBD-II (replaced by Mode 06)
                    sendNegativeResponse(service, 0x11, is29Bit); // Service Not Supported
                    break;
                case 0x06: sendMode06Data(ecus[i], pid, is29Bit); break;
                case 0x07: sendPendingDTCs(ecus[i], is29Bit); break;
                case 0x09: 
                    if (pid == 0x00) sendSupportedPids_09(ecus[i], pid, is29Bit);
                    else if (pid == 0x02) sendVIN(ecus[i], pid, is29Bit);
                    else if (pid == 0x04) sendCalId(ecus[i], pid, is29Bit);
                    else if (pid == 0x06) sendCvn(ecus[i], pid, is29Bit);
                    break;
                case 0x0A: sendPermanentDTCs(ecus[i], is29Bit); break;
                default:
                    // Optional: Send NRC for unknown service if addressed physically
                    if (!isBroadcast) sendNegativeResponse(service, 0x11, is29Bit);
                    break;
            }
        }
    }
}

void sendNegativeResponse(byte service, byte nrc, bool use29bit) {
    uint8_t data[3] = {0x7F, service, nrc};
    isotp_send(use29bit ? 0x18DAF110 : 0x7E8, data, 3, use29bit);
}

void sendCurrentData(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[8];
    data[0] = 0x41;
    data[1] = pid;
    int len = 2;

    bool supported = false;
    if (pid >= 0x01 && pid <= 0x20) supported = (ecu.supported_pids_01_20 >> (32 - pid)) & 1;
    else if (pid >= 0x21 && pid <= 0x40) supported = (ecu.supported_pids_21_40 >> (32 - (pid - 0x20))) & 1;
    else if (pid >= 0x41 && pid <= 0x60) supported = (ecu.supported_pids_41_60 >> (32 - (pid - 0x40))) & 1;
    else if (pid >= 0x61 && pid <= 0x80) supported = (ecu.supported_pids_61_80 >> (32 - (pid - 0x60))) & 1;
    if (pid == 0x00 || pid == 0x20 || pid == 0x40 || pid == 0x60 || pid == 0xA4) supported = true;

    if (!supported) return;

    switch(pid) {
        case 0x00: {
            data[2] = (ecu.supported_pids_01_20 >> 24) & 0xFF;
            data[3] = (ecu.supported_pids_01_20 >> 16) & 0xFF;
            data[4] = (ecu.supported_pids_01_20 >> 8) & 0xFF;
            data[5] = ecu.supported_pids_01_20 & 0xFF;
            len += 4;
            break;
        }
        case 0x01: {
            byte mil_dtc_count = ecu.num_dtcs & 0x7F;
            if (ecu.num_dtcs > 0) mil_dtc_count |= 0x80;
            data[2] = mil_dtc_count;
            data[3] = 0; data[4] = 0; data[5] = 0;
            len += 4;
            break;
        }
        case 0x05: { // Coolant Temp
            data[2] = ecu.engine_temp + 40;
            len += 1;
            break;
        }
        case 0x0A: { // Fuel Pressure
            data[2] = (byte)constrain(ecu.fuel_pressure / 3, 0, 255);
            len += 1;
            break;
        }
        case 0x0C: { // RPM
            int val = ecu.engine_rpm * 4;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x0D: { // Speed
            data[2] = ecu.vehicle_speed;
            len += 1;
            break;
        }
        case 0x0E: { // Timing Advance
            int val = (int)((ecu.timing_advance * 2) + 128);
            data[2] = (byte)constrain(val, 0, 255);
            len += 1;
            break;
        }
        case 0x10: { // MAF
            int val = ecu.maf_rate * 100;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x20: {
            data[2] = (ecu.supported_pids_21_40 >> 24) & 0xFF;
            data[3] = (ecu.supported_pids_21_40 >> 16) & 0xFF;
            data[4] = (ecu.supported_pids_21_40 >> 8) & 0xFF;
            data[5] = ecu.supported_pids_21_40 & 0xFF;
            len += 4;
            break;
        }
        case 0x2F: { // Fuel Level
            data[2] = (byte)((ecu.fuel_level * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x31: { // Dist MIL
            data[2] = highByte(ecu.distance_with_mil);
            data[3] = lowByte(ecu.distance_with_mil);
            len += 2;
            break;
        }
        case 0x40: {
            data[2] = (ecu.supported_pids_41_60 >> 24) & 0xFF;
            data[3] = (ecu.supported_pids_41_60 >> 16) & 0xFF;
            data[4] = (ecu.supported_pids_41_60 >> 8) & 0xFF;
            data[5] = ecu.supported_pids_41_60 & 0xFF;
            len += 4;
            break;
        }
        case 0x5E: { // Fuel Rate
            int val = (int)(ecu.fuel_rate * 20);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x60: {
            data[2] = (ecu.supported_pids_61_80 >> 24) & 0xFF;
            data[3] = (ecu.supported_pids_61_80 >> 16) & 0xFF;
            data[4] = (ecu.supported_pids_61_80 >> 8) & 0xFF;
            data[5] = ecu.supported_pids_61_80 & 0xFF;
            len += 4;
            break;
        }
        case 0xA4: { // Custom Gear
            data[2] = ecu.current_gear;
            len += 1;
            break;
        }
    }
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, len, use29bit);
}

void sendFreezeFrameData(ECU &ecu, byte pid, bool use29bit) {
    if (!ecu.freezeFrameSet) return;
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[8];
    data[0] = 0x42;
    data[1] = pid;
    int len = 2;

    switch(pid) {
        case 0x02: {
            uint16_t code = atoi(&ecu.dtcs[0][1]);
            if (ecu.dtcs[0][0] == 'C') code |= 0x4000;
            else if (ecu.dtcs[0][0] == 'B') code |= 0x8000;
            else if (ecu.dtcs[0][0] == 'U') code |= 0xC000;
            data[2] = highByte(code); data[3] = lowByte(code);
            len += 2;
            isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, len, use29bit);
            break;
        }
        case 0x0C: {
            int val = ecu.freezeFrame.rpm * 4;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, len, use29bit);
            break;
        }
    }
}

void sendDTCs(ECU &ecu, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    byte dtc_bytes[10];
    int byte_count = 0;
    for(int i=0; i<ecu.num_dtcs && i < 5; i++) {
        uint16_t code = atoi(&ecu.dtcs[i][1]);
        if (ecu.dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[20];
    data[0] = 0x43;
    data[1] = ecu.num_dtcs;
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}

void sendPendingDTCs(ECU &ecu, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    // For simulation, we assume Pending DTCs are the same as Current DTCs
    byte dtc_bytes[10];
    int byte_count = 0;
    for(int i=0; i<ecu.num_dtcs && i < 5; i++) {
        uint16_t code = atoi(&ecu.dtcs[i][1]);
        if (ecu.dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[20];
    data[0] = 0x47;
    data[1] = ecu.num_dtcs;
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}

void clearDTCs(ECU &ecu, bool use29bit) {
    ecu.num_dtcs = 0;
    for(int i=0; i<5; i++) ecu.dtcs[i][0] = '\0';
    ecu.freezeFrameSet = false;
    ecu.distance_with_mil = 0;

    uint8_t data[1] = {0x44};
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 1, use29bit);
    
    updateDisplay();
    notifyClients();
}

void sendMode06Data(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[64];
    data[0] = 0x46; data[1] = pid;

    int byte_count = 0;
    byte data_bytes[30];

    for (int i = 0; i < MAX_MODE06_TESTS; i++) {
        if (ecu.mode06_tests[i].enabled) {
            data_bytes[byte_count++] = ecu.mode06_tests[i].testId;
            data_bytes[byte_count++] = highByte(ecu.mode06_tests[i].value);
            data_bytes[byte_count++] = lowByte(ecu.mode06_tests[i].value);
            data_bytes[byte_count++] = highByte(ecu.mode06_tests[i].min_limit);
            data_bytes[byte_count++] = lowByte(ecu.mode06_tests[i].min_limit);
            data_bytes[byte_count++] = highByte(ecu.mode06_tests[i].max_limit);
            data_bytes[byte_count++] = lowByte(ecu.mode06_tests[i].max_limit);
        }
    }

    if (byte_count == 0) return;

    memcpy(&data[2], data_bytes, byte_count);
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}

void sendSupportedPids_09(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[6];
    data[0] = 0x49; data[1] = pid;
    data[2] = (ecu.supported_pids_09 >> 24) & 0xFF;
    data[3] = (ecu.supported_pids_09 >> 16) & 0xFF;
    data[4] = (ecu.supported_pids_09 >> 8) & 0xFF;
    data[5] = ecu.supported_pids_09 & 0xFF;
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 6, use29bit);
}

void sendVIN(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[20];
    data[0] = 0x49; data[1] = pid;
    memcpy(&data[2], ecu.vin, 17);
    // ISO-TP stack handles segmentation automatically
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 19, use29bit);
}

void sendCalId(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[20];
    data[0] = 0x49; data[1] = pid;
    int len = strlen(ecu.cal_id);
    memcpy(&data[2], ecu.cal_id, len);
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + len, use29bit);
}

void sendCvn(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[8];
    data[0] = 0x49; data[1] = pid;
    long cvn_val = strtol(ecu.cvn, NULL, 16);
    data[2] = (cvn_val >> 24) & 0xFF;
    data[3] = (cvn_val >> 16) & 0xFF;
    data[4] = (cvn_val >> 8) & 0xFF;
    data[5] = cvn_val & 0xFF;
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 6, use29bit);
}

void sendPermanentDTCs(ECU &ecu, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    byte dtc_bytes[10];
    int byte_count = 0;
    for(int i=0; i<ecu.num_permanent_dtcs && i < 5; i++) {
        uint16_t code = atoi(&ecu.permanent_dtcs[i][1]);
        if (ecu.permanent_dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.permanent_dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.permanent_dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[20];
    data[0] = 0x4A;
    data[1] = ecu.num_permanent_dtcs;
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}