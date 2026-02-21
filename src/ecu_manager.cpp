#include "shared.h"

const int NUM_ECUS = 2;
ECU ecus[NUM_ECUS];

void setupEcus() {
    // --- Setup ECM (Engine Control Module) ---
    ecus[0].name = "ECM";
    ecus[0].enabled = true;
    ecus[0].canId = 0x7E8;
    ecus[0].canId29 = 0x18DAF110;
    strncpy(ecus[0].vin, "123EMULATORVINECM", 17);
    strncpy(ecus[0].cal_id, "ECM_CAL_ID_V1", 16);
    strncpy(ecus[0].cvn, "ECE1E2E3", 8);
    ecus[0].num_dtcs = 0;
    ecus[0].num_permanent_dtcs = 0;
    ecus[0].engine_rpm = 1500;
    ecus[0].engine_temp = 90;
    ecus[0].vehicle_speed = 60;
    ecus[0].maf_rate = 10.0;
    ecus[0].engine_load = 35.0;
    ecus[0].map_pressure = 40;
    ecus[0].throttle_pos = 15.0;
    ecus[0].intake_temp = 30;
    ecus[0].short_term_fuel_trim = 0.0;
    ecus[0].long_term_fuel_trim = 2.5;
    ecus[0].o2_voltage = 0.45;
    ecus[0].o2_trim = 0.0;
    ecus[0].timing_advance = 5.0;
    ecus[0].fuel_rate = 1.5;
    ecus[0].fuel_pressure = 350;
    ecus[0].fuel_level = 75.0;
    ecus[0].distance_with_mil = 0;
    ecus[0].battery_voltage = 14.2;
    ecus[0].freezeFrameSet = false;
    ecus[0].error_free_cycles = 0;

    ecus[0].supported_pids_01_20 = (1UL << (32 - 0x01)) | (1UL << (32 - 0x04)) | (1UL << (32 - 0x05)) | (1UL << (32 - 0x06)) | 
                                   (1UL << (32 - 0x07)) | (1UL << (32 - 0x0A)) | (1UL << (32 - 0x0B)) | (1UL << (32 - 0x0C)) | 
                                   (1UL << (32 - 0x0D)) | (1UL << (32 - 0x0E)) | (1UL << (32 - 0x0F)) | (1UL << (32 - 0x10)) | 
                                   (1UL << (32 - 0x11)) | (1UL << (32 - 0x14)) | (1UL << (32 - 0x20));
    ecus[0].supported_pids_21_40 = (1UL << (32 - (0x2F - 0x20))) | (1UL << (32 - (0x31 - 0x20))) | (1UL << (32 - (0x40 - 0x20)));
    ecus[0].supported_pids_41_60 = (1UL << (32 - (0x42 - 0x40))) | (1UL << (32 - (0x5E - 0x40))) | (1UL << (32 - (0x60 - 0x40)));
    ecus[0].supported_pids_61_80 = 0;
    ecus[0].supported_pids_09 = (1UL << (32 - 0x02)) | (1UL << (32 - 0x04)) | (1UL << (32 - 0x06));
    
    ecus[0].uds_session = 0x01;
    ecus[0].uds_security_unlocked = false;
    ecus[0].uds_seed = 0;
    
    // --- Mode 06 Tests Init ---
    // Test 1: Catalyst Monitor
    ecus[0].mode06_tests[0].testId = 0x01;
    ecus[0].mode06_tests[0].value = 300;
    ecus[0].mode06_tests[0].min_limit = 0;
    ecus[0].mode06_tests[0].max_limit = 500;
    ecus[0].mode06_tests[0].enabled = true;
    
    // Test 2: EGR Monitor
    ecus[0].mode06_tests[1].testId = 0x02;
    ecus[0].mode06_tests[1].enabled = false; // Disabled by default

    // --- Setup TCM (Transmission Control Module) ---
    ecus[1].name = "TCM";
    ecus[1].enabled = true;
    ecus[1].canId = 0x7E9;
    ecus[1].canId29 = 0x18DAF118;
    strncpy(ecus[1].vin, "123EMULATORVINTCM", 17);
    strncpy(ecus[1].cal_id, "TCM_CAL_ID_V2", 16);
    strncpy(ecus[1].cvn, "TCE1E2E3", 8);
    ecus[1].num_dtcs = 0;
    ecus[1].num_permanent_dtcs = 0;
    ecus[1].engine_rpm = 1500;
    ecus[1].vehicle_speed = 60;
    ecus[1].current_gear = 4;
    ecus[1].freezeFrameSet = false;
    ecus[1].error_free_cycles = 0;

    ecus[1].supported_pids_01_20 = (1UL << (32 - 0x0C)) | (1UL << (32 - 0x0D));
    ecus[1].supported_pids_09 = (1UL << (32 - 0x02));
    ecus[1].uds_session = 0x01;
}