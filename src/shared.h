#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <driver/twai.h>

// --- Enums ---
enum EmulatorMode {
    MODE_OBD_11BIT = 1,
    MODE_UDS_29BIT = 2,
    MODE_HYBRID = 3,
    MODE_AUTODETECT = 4
};

// --- Structs ---
struct FreezeFrameData {
    int rpm;
    int speed;
    int temp;
    float maf;
    int fuel_pressure;
};

#define MAX_MODE06_TESTS 5
struct Mode06Data {
    uint8_t testId;
    uint16_t value;
    uint16_t min_limit;
    uint16_t max_limit;
    bool enabled = false;
};

struct ECU {
    bool enabled;
    const char* name;
    uint32_t canId;
    uint32_t canId29;
    char vin[18];
    char cal_id[17];
    char cvn[9];
    char dtcs[5][6];
    int num_dtcs;
    char permanent_dtcs[5][6];
    int num_permanent_dtcs;
    
    // Parameters
    int engine_rpm;
    int engine_temp;
    int vehicle_speed;
    float maf_rate;
    float timing_advance;
    float fuel_rate;
    int fuel_pressure;
    float fuel_level;
    int distance_with_mil;
    float battery_voltage;
    int current_gear;

    // Supported PIDs
    uint32_t supported_pids_01_20;
    uint32_t supported_pids_21_40;
    uint32_t supported_pids_41_60;
    uint32_t supported_pids_61_80;
    uint32_t supported_pids_09;

    // State
    FreezeFrameData freezeFrame;
    bool freezeFrameSet;
    int error_free_cycles;
    Mode06Data mode06_tests[MAX_MODE06_TESTS];

    // UDS State
    uint8_t uds_session;
    bool uds_security_unlocked;
    uint32_t uds_seed;
};

// --- ISO-TP Definitions ---
#define ISOTP_MAX_DATA_LEN 4095
#define ISOTP_TIMEOUT_MS 1000
#define ISOTP_STMIN_MS 0

enum IsoTpProtocolState {
    ISOTP_IDLE,
    ISOTP_RX_WAIT_CF,
    ISOTP_TX_WAIT_FC,
    ISOTP_TX_SEND_CF
};

struct IsoTpLink {
    // RX State
    uint32_t rxId;
    uint8_t rxBuffer[ISOTP_MAX_DATA_LEN];
    uint16_t rxLen;
    uint16_t rxExpectedLen;
    uint8_t rxNextSeq;
    uint8_t rxBlockSize;
    uint8_t rxBsCounter;
    unsigned long rxTimer;
    IsoTpProtocolState rxState;

    // TX State
    uint32_t txId;
    bool txExtended;
    uint8_t txBuffer[ISOTP_MAX_DATA_LEN];
    uint16_t txTotalLen;
    uint16_t txBytesSent;
    uint8_t txSeq;
    uint8_t txBlockSize;
    uint8_t txBsCounter;
    uint8_t txStMin;
    unsigned long txTimer;
    IsoTpProtocolState txState;
};

// --- Global Externs ---
extern ECU ecus[];
extern const int NUM_ECUS;
extern EmulatorMode emulatorMode;
extern int canBitrate;
extern Adafruit_ST7735 tft;
extern IsoTpLink isoTpLink;
extern bool fault_incorrect_sequence;
extern bool fault_silent_mode;
extern bool fault_multiple_responses;
extern bool fault_stmin_overflow;
extern bool fault_wrong_flow_control;
extern bool fault_partial_vin;
extern bool can_logging_enabled;

// --- Common Functions ---
void updateDisplay();
void notifyClients();
void clearDTCs(ECU &ecu, bool use29bit);

// ISO-TP & Handlers
void isotp_init();
void isotp_poll();
void isotp_on_frame(const twai_message_t& frame);
bool isotp_send(uint32_t id, const uint8_t* data, uint16_t len, bool extended);
bool isotp_get_message(uint32_t* id, uint8_t* data, uint16_t* len);

void handleOBDRequest(uint32_t id, const uint8_t* data, uint16_t len);
void handleUDSRequest(ECU &ecu, uint32_t id, const uint8_t* data, uint16_t len);
void logCAN(const twai_message_t& frame, bool rx);