#include "shared.h"

extern int frame_delay_ms;
extern int error_injection_rate;
extern EmulatorMode emulatorMode;

// Forward declarations
void sendCurrentData(ECU &ecu, byte pid, bool use29bit);
void sendFreezeFrameData(ECU &ecu, byte pid, bool use29bit);
void sendDTCs(ECU &ecu, bool use29bit);
void sendPendingDTCs(ECU &ecu, bool use29bit);
void clearDTCs(ECU &ecu, bool use29bit, bool sendResponse);
void sendMode06Data(ECU &ecu, byte pid, bool use29bit);
void sendSupportedPids_09(ECU &ecu, byte pid, bool use29bit);
void sendVIN(ECU &ecu, byte pid, bool use29bit);
void sendCalId(ECU &ecu, byte pid, bool use29bit);
void sendCvn(ECU &ecu, byte pid, bool use29bit);
void sendPermanentDTCs(ECU &ecu, bool use29bit);
void sendNegativeResponse(ECU &ecu, byte service, byte nrc, bool use29bit);
void handleUDSRequest(ECU &ecu, uint32_t id, const uint8_t* data, uint16_t len);
bool isotp_send(uint32_t id, const uint8_t* data, uint16_t len, bool extended);

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

    // Enforce Emulator Mode
    if (emulatorMode == MODE_OBD_11BIT && is29Bit) return;
    if (emulatorMode == MODE_UDS_29BIT && is11Bit) return;

    // Filter based on Mode
    // (Filtering already done mostly by main loop, but good to keep)

    if (len < 1) return; // Дозволяємо запити довжиною 1 байт (наприклад, Mode 03)
    byte service = data[0]; // ISO-TP payload starts at 0
    byte pid = (len >= 2) ? data[1] : 0; // PID зчитуємо тільки якщо він є
    
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
            else if (ta == 0x28) targetEcuIndex = 2; // ABS
            else if (ta == 0x58) targetEcuIndex = 3; // SRS
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
                case 0x04: clearDTCs(ecus[i], is29Bit, true); break; // Відповідаємо, бо це запит сканера
                case 0x05: 
                    // Mode 05 is not supported in CAN OBD-II (replaced by Mode 06)
                    sendNegativeResponse(ecus[i], service, 0x11, is29Bit); // Service Not Supported
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
                    if (!isBroadcast) sendNegativeResponse(ecus[i], service, 0x11, is29Bit);
                    break;
            }
        }
    }
}

void sendNegativeResponse(ECU &ecu, byte service, byte nrc, bool use29bit) {
    uint8_t data[3] = {0x7F, service, nrc};
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 3, use29bit);
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
        case 0x03: { // Fuel System Status
            data[2] = highByte(ecu.fuel_system_status);
            data[3] = lowByte(ecu.fuel_system_status);
            len += 2;
            break;
        }
        case 0x04: { // Engine Load
            data[2] = (byte)((ecu.engine_load * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x05: { // Coolant Temp
            data[2] = ecu.engine_temp + 40;
            len += 1;
            break;
        }
        case 0x06: { // Short Term Fuel Trim
            data[2] = (byte)(((ecu.short_term_fuel_trim + 100.0) * 128.0) / 100.0);
            len += 1;
            break;
        }
        case 0x07: { // Long Term Fuel Trim
            data[2] = (byte)(((ecu.long_term_fuel_trim + 100.0) * 128.0) / 100.0);
            len += 1;
            break;
        }
        case 0x08: { // Short Term Fuel Trim Bank 2
            data[2] = (byte)(((ecu.short_term_fuel_trim_b2 + 100.0) * 128.0) / 100.0);
            len += 1;
            break;
        }
        case 0x09: { // Long Term Fuel Trim Bank 2
            data[2] = (byte)(((ecu.long_term_fuel_trim_b2 + 100.0) * 128.0) / 100.0);
            len += 1;
            break;
        }
        case 0x0B: { // MAP
            data[2] = ecu.map_pressure;
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
        case 0x0F: { // Intake Air Temp
            data[2] = ecu.intake_temp + 40;
            len += 1;
            break;
        }
        case 0x10: { // MAF
            int val = ecu.maf_rate * 100;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x11: { // Throttle Position
            data[2] = (byte)((ecu.throttle_pos * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x14: { // O2 Sensor Bank 1 Sensor 1 (Voltage + Trim)
            data[2] = (byte)(ecu.o2_voltage * 200.0); // 0-1.275V
            data[3] = (byte)(((ecu.o2_trim + 100.0) * 128.0) / 100.0);
            len += 2;
            break;
        }
        case 0x1C: { // OBD Standards
            data[2] = ecu.obd_standard;
            len += 1;
            break;
        }
        case 0x1D: { // Oxygen Sensors Present
            data[2] = ecu.o2_sensors_present;
            len += 1;
            break;
        }
        case 0x1F: { // Run Time Since Engine Start
            uint16_t seconds = millis() / 1000;
            data[2] = highByte(seconds); data[3] = lowByte(seconds);
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
        case 0x21: { // Distance Traveled with MIL On
            uint16_t val = ecu.distance_mil_on;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x22: { // Fuel Rail Pressure (relative)
            uint16_t val = (uint16_t)(ecu.fuel_rail_pressure_relative / 0.079);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x23: { // Fuel Rail Gauge Pressure
            uint16_t val = ecu.fuel_rail_pressure_gauge / 10;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x2C: { // Commanded EGR
            data[2] = (byte)((ecu.commanded_egr * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x2D: { // EGR Error
            data[2] = (byte)(((ecu.egr_error + 100.0) * 128.0) / 100.0);
            len += 1;
            break;
        }
        case 0x2E: { // Commanded Evaporative Purge
            data[2] = (byte)((ecu.evap_purge * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x2F: { // Fuel Level
            data[2] = (byte)((ecu.fuel_level * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x30: { // Warm-ups since codes cleared
            data[2] = (byte)ecu.warm_ups;
            len += 1;
            break;
        }
        case 0x31: { // Dist MIL
            data[2] = highByte(ecu.distance_with_mil);
            data[3] = lowByte(ecu.distance_with_mil);
            len += 2;
            break;
        }
        case 0x32: { // EVAP System Vapor Pressure
            int16_t val = ecu.evap_vapor_pressure * 4;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x33: { // Barometric Pressure
            data[2] = (byte)ecu.baro_pressure;
            len += 1;
            break;
        }
        case 0x34: { // O2 Wideband B1S1
            uint16_t lambda = (uint16_t)(ecu.o2_lambda_b1s1 * 32768.0);
            uint16_t current = (uint16_t)((ecu.o2_current_b1s1 + 128.0) * 256.0);
            data[2] = highByte(lambda); data[3] = lowByte(lambda);
            data[4] = highByte(current); data[5] = lowByte(current);
            len += 4;
            break;
        }
        case 0x35: { // O2 Wideband B1S2
            uint16_t lambda = (uint16_t)(ecu.o2_lambda_b1s2 * 32768.0);
            uint16_t current = (uint16_t)((ecu.o2_current_b1s2 + 128.0) * 256.0);
            data[2] = highByte(lambda); data[3] = lowByte(lambda);
            data[4] = highByte(current); data[5] = lowByte(current);
            len += 4;
            break;
        }
        case 0x36: { // O2 Wideband B2S1
            uint16_t lambda = (uint16_t)(ecu.o2_lambda_b2s1 * 32768.0);
            uint16_t current = (uint16_t)((ecu.o2_current_b2s1 + 128.0) * 256.0);
            data[2] = highByte(lambda); data[3] = lowByte(lambda);
            data[4] = highByte(current); data[5] = lowByte(current);
            len += 4;
            break;
        }
        case 0x37: { // O2 Wideband B2S2
            uint16_t lambda = (uint16_t)(ecu.o2_lambda_b2s2 * 32768.0);
            uint16_t current = (uint16_t)((ecu.o2_current_b2s2 + 128.0) * 256.0);
            data[2] = highByte(lambda); data[3] = lowByte(lambda);
            data[4] = highByte(current); data[5] = lowByte(current);
            len += 4;
            break;
        }
        case 0x3C: { // Catalyst Temp B1S1
            uint16_t val = (uint16_t)((ecu.catalyst_temp_b1s1 + 40.0) * 10.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x3D: { // Catalyst Temp B2S1
            uint16_t val = (uint16_t)((ecu.catalyst_temp_b2s1 + 40.0) * 10.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x3E: { // Catalyst Temp B1S2
            uint16_t val = (uint16_t)((ecu.catalyst_temp_b1s2 + 40.0) * 10.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x3F: { // Catalyst Temp B2S2
            uint16_t val = (uint16_t)((ecu.catalyst_temp_b2s2 + 40.0) * 10.0);
            data[2] = highByte(val); data[3] = lowByte(val);
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
        case 0x42: { // Control Module Voltage
            int val = ecu.battery_voltage * 1000;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x43: { // Absolute Load Value
            uint16_t val = (uint16_t)((ecu.abs_load * 255.0) / 100.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x44: { // Commanded Equivalence Ratio (Lambda)
            uint16_t val = (uint16_t)(ecu.command_equiv_ratio * 32768.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x45: { // Relative Throttle Position
            data[2] = (byte)((ecu.relative_throttle * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x46: { // Ambient Air Temperature
            data[2] = (byte)(ecu.ambient_temp + 40);
            len += 1;
            break;
        }
        case 0x49: { // Accelerator Pedal Position D
            data[2] = (byte)((ecu.accel_pedal_pos_d * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x4A: { // Accelerator Pedal Position E
            data[2] = (byte)((ecu.accel_pedal_pos_e * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x4C: { // Commanded Throttle Actuator
            data[2] = (byte)((ecu.commanded_throttle_actuator * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x4D: { // Time Run with MIL On
            uint16_t val = ecu.time_run_mil_on;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x4E: { // Time Since DTC Cleared
            uint16_t val = ecu.time_since_dtc_cleared;
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x53: { // Absolute EVAP System Vapor Pressure
            uint16_t val = (uint16_t)(ecu.abs_evap_pressure * 200.0);
            data[2] = highByte(val); data[3] = lowByte(val);
            len += 2;
            break;
        }
        case 0x5A: { // Relative Accelerator Pedal Position
            data[2] = (byte)((ecu.rel_accel_pedal_pos * 255.0) / 100.0);
            len += 1;
            break;
        }
        case 0x5C: { // Engine Oil Temperature
            data[2] = (byte)(ecu.oil_temp + 40);
            len += 1;
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
            // Використовуємо strtol з базою 16 для коректного парсингу HEX-частини коду DTC
            uint16_t code = strtol(&ecu.dtcs[0][1], NULL, 16);
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

    byte dtc_bytes[MAX_DTCS_PER_ECU * 2];
    int byte_count = 0;
    for(int i=0; i<ecu.num_dtcs && i < MAX_DTCS_PER_ECU; i++) {
        uint16_t code = strtol(&ecu.dtcs[i][1], NULL, 16);
        if (ecu.dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[MAX_DTCS_PER_ECU * 2 + 2];
    data[0] = 0x43;
    data[1] = ecu.num_dtcs; 
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}

void sendPendingDTCs(ECU &ecu, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    byte dtc_bytes[MAX_DTCS_PER_ECU * 2];
    int byte_count = 0;
    for(int i=0; i<ecu.num_dtcs && i < MAX_DTCS_PER_ECU; i++) {
        uint16_t code = strtol(&ecu.dtcs[i][1], NULL, 16);
        if (ecu.dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[MAX_DTCS_PER_ECU * 2 + 2];
    data[0] = 0x47;
    data[1] = ecu.num_dtcs;
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}

void clearDTCs(ECU &ecu, bool use29bit, bool sendResponse) {
    ecu.num_dtcs = 0;
    for(int i=0; i<MAX_DTCS_PER_ECU; i++) ecu.dtcs[i][0] = '\0';
    ecu.freezeFrameSet = false;
    ecu.distance_with_mil = 0;

    // Також очищуємо постійні DTC (Mode 0A) та скидаємо лічильник циклів водіння,
    // щоб поведінка відповідала очікуванням.
    ecu.num_permanent_dtcs = 0;
    for(int i=0; i<MAX_DTCS_PER_ECU; i++) ecu.permanent_dtcs[i][0] = '\0';
    ecu.error_free_cycles = 0;

    if (sendResponse) {
        uint8_t data[1] = {0x44};
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 1, use29bit);
    }
    
    updateDisplay();
    notifyClients();
}

void sendMode06Data(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[64];
    data[0] = 0x46; data[1] = pid;

    int byte_count = 0;
    byte data_bytes[64]; // Збільшено для безпеки (MAX_MODE06_TESTS * 7)

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

    uint8_t data[22];
    data[0] = 0x49; data[1] = pid;
    data[2] = 0x01; // Number of data items (1 VIN)
    memcpy(&data[3], ecu.vin, 17);
    // ISO-TP stack handles segmentation automatically
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 20, use29bit);
}

void sendCalId(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[22];
    data[0] = 0x49; data[1] = pid;
    data[2] = 0x01; // Number of data items (1 CAL ID)
    int len = strlen(ecu.cal_id);
    memcpy(&data[3], ecu.cal_id, len);
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 3 + len, use29bit);
}

void sendCvn(ECU &ecu, byte pid, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    uint8_t data[8];
    data[0] = 0x49; data[1] = pid;
    data[2] = 0x01; // Number of data items (1 CVN)
    long cvn_val = strtol(ecu.cvn, NULL, 16);
    data[3] = (cvn_val >> 24) & 0xFF;
    data[4] = (cvn_val >> 16) & 0xFF;
    data[5] = (cvn_val >> 8) & 0xFF;
    data[6] = cvn_val & 0xFF;
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 7, use29bit);
}

void sendPermanentDTCs(ECU &ecu, bool use29bit) {
    if (frame_delay_ms > 0) delay(frame_delay_ms);

    byte dtc_bytes[MAX_DTCS_PER_ECU * 2];
    int byte_count = 0;
    for(int i=0; i<ecu.num_permanent_dtcs && i < MAX_DTCS_PER_ECU; i++) {
        uint16_t code = strtol(&ecu.permanent_dtcs[i][1], NULL, 16);
        if (ecu.permanent_dtcs[i][0] == 'C') code |= 0x4000;
        else if (ecu.permanent_dtcs[i][0] == 'B') code |= 0x8000;
        else if (ecu.permanent_dtcs[i][0] == 'U') code |= 0xC000;
        dtc_bytes[byte_count++] = highByte(code);
        dtc_bytes[byte_count++] = lowByte(code);
    }

    uint8_t data[MAX_DTCS_PER_ECU * 2 + 2];
    data[0] = 0x4A;
    data[1] = ecu.num_permanent_dtcs;
    memcpy(&data[2], dtc_bytes, byte_count);
    
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, data, 2 + byte_count, use29bit);
}