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

// Константи для професійної емуляції
#define MAX_DTCS_PER_ECU 32
#define MAX_DTC_LENGTH 6
#define MAX_MODE06_TESTS 8

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
    char dtcs[MAX_DTCS_PER_ECU][MAX_DTC_LENGTH];
    int num_dtcs;
    char permanent_dtcs[MAX_DTCS_PER_ECU][MAX_DTC_LENGTH];
    int num_permanent_dtcs;
    
    // Parameters
    int engine_rpm;
    int engine_temp;
    int vehicle_speed;
    float maf_rate;
    float engine_load;
    int map_pressure;
    float throttle_pos;
    int intake_temp;
    float short_term_fuel_trim;
    float long_term_fuel_trim;
    float short_term_fuel_trim_b2; // PID 0x08
    float long_term_fuel_trim_b2;  // PID 0x09
    float o2_voltage;
    float o2_trim;
    float timing_advance;
    float fuel_rate;
    int fuel_pressure;
    float fuel_level;
    int distance_with_mil;
    float battery_voltage;
    int current_gear;
    
    // New PIDs
    uint16_t fuel_system_status; // PID 0x03
    int distance_mil_on;       // PID 0x21
    float evap_purge;          // PID 0x2E
    int warm_ups;              // PID 0x30
    int baro_pressure;         // PID 0x33
    float abs_load;            // PID 0x43
    float command_equiv_ratio; // PID 0x44
    float relative_throttle;   // PID 0x45
    int ambient_temp;          // PID 0x46
    int oil_temp;              // PID 0x5C
    
    // Wideband O2 PIDs 34-37
    float o2_lambda_b1s1; float o2_current_b1s1; // PID 0x34
    float o2_lambda_b1s2; float o2_current_b1s2; // PID 0x35
    float o2_lambda_b2s1; float o2_current_b2s1; // PID 0x36
    float o2_lambda_b2s2; float o2_current_b2s2; // PID 0x37

    // Catalyst Temperatures
    float catalyst_temp_b1s1; // PID 0x3C
    float catalyst_temp_b2s1; // PID 0x3D
    float catalyst_temp_b1s2; // PID 0x3E
    float catalyst_temp_b2s2; // PID 0x3F

    // New PIDs
    float commanded_egr;       // PID 0x2C
    float egr_error;           // PID 0x2D
    int evap_vapor_pressure;   // PID 0x32 (Pa)
    float abs_evap_pressure;   // PID 0x53 (kPa)

    // New PIDs from this request
    int fuel_rail_pressure_relative; // PID 0x22
    int fuel_rail_pressure_gauge;    // PID 0x23
    float commanded_throttle_actuator; // PID 0x4C
    
    // New PIDs
    float rel_accel_pedal_pos; // PID 0x5A
    float accel_pedal_pos_d;   // PID 0x49
    float accel_pedal_pos_e;   // PID 0x4A

    // New PIDs (1C, 1D, 4D, 4E)
    uint8_t obd_standard;          // PID 0x1C
    uint8_t o2_sensors_present;    // PID 0x1D
    uint16_t time_run_mil_on;      // PID 0x4D
    uint16_t time_since_dtc_cleared; // PID 0x4E

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
extern volatile bool need_display_update; // Прапорець для синхронізації дисплея
extern volatile bool config_locked;       // Прапорець блокування конфігурації

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