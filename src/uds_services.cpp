#include "shared.h"
#include <Arduino.h> // Для esp_random() та інших функцій

// UDS Negative Response Codes (NRC)
#define NRC_SERVICE_NOT_SUPPORTED 0x11
#define NRC_SUBFUNCTION_NOT_SUPPORTED 0x12
#define NRC_INCORRECT_MESSAGE_LENGTH 0x13
#define NRC_CONDITIONS_NOT_CORRECT 0x22
#define NRC_REQUEST_OUT_OF_RANGE 0x31
#define NRC_SECURITY_ACCESS_DENIED 0x33
#define NRC_INVALID_KEY 0x35
#define NRC_EXCEEDED_NUMBER_OF_ATTEMPTS 0x36
#define NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37

// Forward declarations for local UDS handlers
void sendUdsNegativeResponse(ECU &ecu, uint8_t service, uint8_t nrc, bool use29bit);
void handleDiagnosticSessionControl(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit);
void handleReadDataByIdentifier(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit);
void handleSecurityAccess(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit);
void handleTesterPresent(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit);
void handleEcuReset(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit);

/**
 * @brief Main handler for UDS requests, called from handleOBDRequest.
 * It dispatches requests to specific handlers based on the Service ID (SID).
 */
void handleUDSRequest(ECU &ecu, uint32_t id, const uint8_t* data, uint16_t len) {
    if (len < 1) return;

    uint8_t service = data[0];
    bool is29Bit = (id > 0x7FF);

    Serial.printf("Received UDS Request for ECU '%s': Service 0x%02X\n", ecu.name, service);

    switch (service) {
        case 0x10: // Diagnostic Session Control
            handleDiagnosticSessionControl(ecu, data, len, is29Bit);
            break;
        case 0x11: // ECU Reset
            handleEcuReset(ecu, data, len, is29Bit);
            break;
        case 0x22: // Read Data By Identifier
            handleReadDataByIdentifier(ecu, data, len, is29Bit);
            break;
        case 0x27: // Security Access
            handleSecurityAccess(ecu, data, len, is29Bit);
            break;
        case 0x3E: // Tester Present
            handleTesterPresent(ecu, data, len, is29Bit);
            break;
        default:
            sendUdsNegativeResponse(ecu, service, NRC_SERVICE_NOT_SUPPORTED, is29Bit);
            break;
    }
    // After handling a UDS command, update clients
    notifyClients();
    updateDisplay();
}

void sendUdsNegativeResponse(ECU &ecu, uint8_t service, uint8_t nrc, bool use29bit) {
    uint8_t response[3] = {0x7F, service, nrc};
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 3, use29bit);
}

void handleDiagnosticSessionControl(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit) {
    if (len != 2) {
        sendUdsNegativeResponse(ecu, 0x10, NRC_INCORRECT_MESSAGE_LENGTH, use29bit);
        return;
    }
    uint8_t subfunction = data[1];
    if (subfunction == 0x01) { // Default Session
        ecu.uds_session = 0x01;
        ecu.uds_security_unlocked = false; // Security is reset on session change
        uint8_t response[6] = {0x50, 0x01, 0x00, 0x32, 0x01, 0xF4}; // Positive response with P2 timers
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 6, use29bit);
    } else if (subfunction == 0x03) { // Extended Diagnostic Session
        ecu.uds_session = 0x03;
        ecu.uds_security_unlocked = false; // Security is reset on session change
        uint8_t response[6] = {0x50, 0x03, 0x00, 0x32, 0x01, 0xF4}; // Positive response with P2 timers
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 6, use29bit);
    } else {
        sendUdsNegativeResponse(ecu, 0x10, NRC_SUBFUNCTION_NOT_SUPPORTED, use29bit);
    }
}

void handleEcuReset(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit) {
    if (len != 2) {
        sendUdsNegativeResponse(ecu, 0x11, NRC_INCORRECT_MESSAGE_LENGTH, use29bit);
        return;
    }
    uint8_t subfunction = data[1];
    if (subfunction == 0x01) { // Hard reset
        uint8_t response[2] = {0x51, 0x01};
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 2, use29bit);
        // In a real ECU, this would trigger a reboot. Here we just log it and reset state.
        Serial.printf("ECU '%s' received Hard Reset command. Simulating state reset.\n", ecu.name);
        ecu.uds_session = 0x01; // Reset to default session
        ecu.uds_security_unlocked = false;
    } else {
        sendUdsNegativeResponse(ecu, 0x11, NRC_SUBFUNCTION_NOT_SUPPORTED, use29bit);
    }
}

void handleReadDataByIdentifier(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit) {
    if (len != 3) {
        sendUdsNegativeResponse(ecu, 0x22, NRC_INCORRECT_MESSAGE_LENGTH, use29bit);
        return;
    }
    uint16_t did = (data[1] << 8) | data[2];
    uint8_t response[32];
    response[0] = 0x62; // Positive response SID
    response[1] = data[1];
    response[2] = data[2];
    uint16_t response_len = 3;

    switch (did) {
        case 0xF190: // VIN
            memcpy(&response[response_len], ecu.vin, 17);
            response_len += 17;
            break;
        case 0x0202: // Engine RPM (example DID)
            if (ecu.uds_session != 0x03) { // Example: requires extended session
                sendUdsNegativeResponse(ecu, 0x22, NRC_CONDITIONS_NOT_CORRECT, use29bit);
                return;
            }
            {
                int val = ecu.engine_rpm * 4;
                response[response_len++] = highByte(val);
                response[response_len++] = lowByte(val);
            }
            break;
        case 0x0203: // Vehicle Speed (example DID)
            response[response_len++] = ecu.vehicle_speed;
            break;
        default:
            sendUdsNegativeResponse(ecu, 0x22, NRC_REQUEST_OUT_OF_RANGE, use29bit);
            return;
    }
    isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, response_len, use29bit);
}

void handleSecurityAccess(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit) {
    if (len != 2 && len != 6) { // 2 for requestSeed, 6 for sendKey
        sendUdsNegativeResponse(ecu, 0x27, NRC_INCORRECT_MESSAGE_LENGTH, use29bit);
        return;
    }

    uint8_t subfunction = data[1];

    if (subfunction == 0x01) { // Request Seed (Level 1)
        if (ecu.uds_session != 0x03) { // Must be in extended session
            sendUdsNegativeResponse(ecu, 0x27, NRC_CONDITIONS_NOT_CORRECT, use29bit);
            return;
        }
        // Generate a simple pseudo-random seed
        ecu.uds_seed = esp_random();
        uint8_t response[6] = { 0x67, 0x01, (uint8_t)(ecu.uds_seed >> 24), (uint8_t)(ecu.uds_seed >> 16), (uint8_t)(ecu.uds_seed >> 8), (uint8_t)(ecu.uds_seed) };
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 6, use29bit);
    } else if (subfunction == 0x02) { // Send Key (Level 1)
        if (ecu.uds_seed == 0) {
            sendUdsNegativeResponse(ecu, 0x27, NRC_REQUEST_OUT_OF_RANGE, use29bit);
            return;
        }
        uint32_t key_received = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
        uint32_t expected_key = ecu.uds_seed ^ 0xDEADBEEF; // Simple XOR algorithm

        if (key_received == expected_key) {
            ecu.uds_security_unlocked = true;
            ecu.uds_seed = 0; // Invalidate seed
            uint8_t response[2] = {0x67, 0x02};
            isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 2, use29bit);
        } else {
            ecu.uds_security_unlocked = false;
            sendUdsNegativeResponse(ecu, 0x27, NRC_INVALID_KEY, use29bit);
        }
    } else {
        sendUdsNegativeResponse(ecu, 0x27, NRC_SUBFUNCTION_NOT_SUPPORTED, use29bit);
    }
}

void handleTesterPresent(ECU &ecu, const uint8_t* data, uint16_t len, bool use29bit) {
    if (len != 2) {
        sendUdsNegativeResponse(ecu, 0x3E, NRC_INCORRECT_MESSAGE_LENGTH, use29bit);
        return;
    }
    uint8_t subfunction = data[1];
    if (subfunction == 0x00) { // Zero subfunction
        uint8_t response[2] = {0x7E, 0x00};
        isotp_send(use29bit ? ecu.canId29 : ecu.canId, response, 2, use29bit);
    } else {
        sendUdsNegativeResponse(ecu, 0x3E, NRC_SUBFUNCTION_NOT_SUPPORTED, use29bit);
    }
}