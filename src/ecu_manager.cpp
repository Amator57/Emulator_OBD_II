#include "shared.h"

const int NUM_ECUS = 4;
ECU ecus[NUM_ECUS];

void setupEcus() {
    // --- Setup ECM (Engine Control Module) ---
    ecus[0].name = "ECM";
    ecus[0].enabled = true;
    ecus[0].canId = 0x7E8;
    ecus[0].canId29 = 0x18DAF110;
    memcpy(ecus[0].vin, "123EMULATOR001ECM", 17); // Виправлено: прибрано 'I' (VIN -> 001)
    strncpy(ecus[0].cal_id, "ECM_CAL_ID_V1", 16);
    strncpy(ecus[0].cvn, "ECE1E2E3", 8);
    
    // Додаємо тестові DTC коди за замовчуванням
    ecus[0].num_dtcs = 2;
    strcpy(ecus[0].dtcs[0], "P0101"); // MAF Sensor Performance
    strcpy(ecus[0].dtcs[1], "P0300"); // Random Misfire

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
    ecus[0].short_term_fuel_trim_b2 = 0.0;
    ecus[0].long_term_fuel_trim_b2 = 2.5;
    ecus[0].o2_voltage = 0.45;
    ecus[0].o2_trim = 0.0;
    ecus[0].timing_advance = 5.0;
    ecus[0].fuel_rate = 1.5;
    ecus[0].fuel_pressure = 350;
    ecus[0].fuel_level = 75.0;
    ecus[0].distance_with_mil = 0;
    ecus[0].battery_voltage = 14.2;
    
    // Init New PIDs
    ecus[0].fuel_system_status = 0x0200; // Closed Loop (Sys 1), Sys 2 unused
    ecus[0].distance_mil_on = 0;
    ecus[0].evap_purge = 0.0;
    ecus[0].warm_ups = 10;
    ecus[0].baro_pressure = 100; // kPa
    ecus[0].abs_load = 20.0;
    ecus[0].command_equiv_ratio = 1.0;
    ecus[0].relative_throttle = 10.0;
    ecus[0].ambient_temp = 20;
    ecus[0].oil_temp = 85;
    
    // New PIDs from this request
    ecus[0].fuel_rail_pressure_relative = 300; // kPa
    ecus[0].fuel_rail_pressure_gauge = 4000; // kPa
    ecus[0].commanded_throttle_actuator = 15.0; // %

    ecus[0].o2_lambda_b1s1 = 1.0; ecus[0].o2_current_b1s1 = 0.0;
    ecus[0].o2_lambda_b1s2 = 1.0; ecus[0].o2_current_b1s2 = 0.0;
    ecus[0].o2_lambda_b2s1 = 1.0; ecus[0].o2_current_b2s1 = 0.0;
    ecus[0].o2_lambda_b2s2 = 1.0; ecus[0].o2_current_b2s2 = 0.0;

    ecus[0].catalyst_temp_b1s1 = 400.0;
    ecus[0].catalyst_temp_b2s1 = 400.0;
    ecus[0].catalyst_temp_b1s2 = 350.0;
    ecus[0].catalyst_temp_b2s2 = 350.0;

    ecus[0].commanded_egr = 0.0;
    ecus[0].egr_error = 0.0;
    ecus[0].evap_vapor_pressure = 0;
    ecus[0].abs_evap_pressure = 100.0;

    ecus[0].rel_accel_pedal_pos = 0.0;
    ecus[0].accel_pedal_pos_d = 15.0;
    ecus[0].accel_pedal_pos_e = 15.0;

    ecus[0].obd_standard = 1; // 1 = OBD-II as defined by the CARB
    ecus[0].o2_sensors_present = 0x03; // Bank 1 Sensor 1 & 2 present
    ecus[0].time_run_mil_on = 0;
    ecus[0].time_since_dtc_cleared = 0;

    ecus[0].freezeFrameSet = false;
    ecus[0].error_free_cycles = 0;

    ecus[0].supported_pids_01_20 = (1UL << (32 - 0x01)) | (1UL << (32 - 0x03)) | (1UL << (32 - 0x04)) | (1UL << (32 - 0x05)) | (1UL << (32 - 0x06)) | 
                                   (1UL << (32 - 0x07)) | (1UL << (32 - 0x08)) | (1UL << (32 - 0x09)) | (1UL << (32 - 0x0A)) | (1UL << (32 - 0x0B)) | (1UL << (32 - 0x0C)) | 
                                   (1UL << (32 - 0x0D)) | (1UL << (32 - 0x0E)) | (1UL << (32 - 0x0F)) | (1UL << (32 - 0x10)) | 
                                   (1UL << (32 - 0x11)) | (1UL << (32 - 0x14)) | (1UL << (32 - 0x1C)) | (1UL << (32 - 0x1D)) | (1UL << (32 - 0x1F)) | (1UL << (32 - 0x20));
    ecus[0].supported_pids_21_40 = (1UL << (32 - (0x21 - 0x20))) | (1UL << (32 - (0x22 - 0x20))) | (1UL << (32 - (0x23 - 0x20))) | (1UL << (32 - (0x2C - 0x20))) | (1UL << (32 - (0x2D - 0x20))) | (1UL << (32 - (0x2E - 0x20))) | (1UL << (32 - (0x2F - 0x20))) | (1UL << (32 - (0x30 - 0x20))) | (1UL << (32 - (0x31 - 0x20))) | (1UL << (32 - (0x32 - 0x20))) | (1UL << (32 - (0x33 - 0x20))) | 
                                   (1UL << (32 - (0x34 - 0x20))) | (1UL << (32 - (0x35 - 0x20))) | (1UL << (32 - (0x36 - 0x20))) | (1UL << (32 - (0x37 - 0x20))) | 
                                   (1UL << (32 - (0x3C - 0x20))) | (1UL << (32 - (0x3D - 0x20))) | (1UL << (32 - (0x3E - 0x20))) | (1UL << (32 - (0x3F - 0x20))) | 
                                   (1UL << (32 - (0x40 - 0x20)));
    ecus[0].supported_pids_41_60 = (1UL << (32 - (0x42 - 0x40))) | (1UL << (32 - (0x43 - 0x40))) | (1UL << (32 - (0x44 - 0x40))) | (1UL << (32 - (0x45 - 0x40))) | (1UL << (32 - (0x46 - 0x40))) | (1UL << (32 - (0x49 - 0x40))) | (1UL << (32 - (0x4A - 0x40))) | (1UL << (32 - (0x4C - 0x40))) | (1UL << (32 - (0x4D - 0x40))) | (1UL << (32 - (0x4E - 0x40))) | (1UL << (32 - (0x53 - 0x40))) | (1UL << (32 - (0x5A - 0x40))) | (1UL << (32 - (0x5C - 0x40))) | (1UL << (32 - (0x5E - 0x40))) | (1UL << (32 - (0x60 - 0x40)));
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
    strncpy(ecus[1].vin, "123EMULATOR001TCM", 17);
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

    // --- Setup ABS (Anti-lock Braking System) ---
    ecus[2].name = "ABS";
    ecus[2].enabled = true;
    ecus[2].canId = 0x7EA;      // Request 0x7E2 -> Response 0x7EA
    ecus[2].canId29 = 0x18DAF128; // Source Address 0x28
    strncpy(ecus[2].vin, "123EMULATOR001ABS", 17);
    strncpy(ecus[2].cal_id, "ABS_CAL_ID_V1", 16);
    strncpy(ecus[2].cvn, "ABSE1E2E", 8);
    ecus[2].num_dtcs = 0;
    ecus[2].num_permanent_dtcs = 0;
    ecus[2].vehicle_speed = 60;
    
    ecus[2].supported_pids_01_20 = (1UL << (32 - 0x0D)); // Speed
    ecus[2].supported_pids_09 = (1UL << (32 - 0x02));   // VIN
    ecus[2].uds_session = 0x01;

    // --- Setup SRS (Airbag / Supplemental Restraint System) ---
    ecus[3].name = "SRS";
    ecus[3].enabled = true;
    ecus[3].canId = 0x7EB;      // Request 0x7E3 -> Response 0x7EB
    ecus[3].canId29 = 0x18DAF158; // Source Address 0x58
    strncpy(ecus[3].vin, "123EMULATOR001SRS", 17);
    strncpy(ecus[3].cal_id, "SRS_CAL_ID_V1", 16);
    strncpy(ecus[3].cvn, "SRSE1E2E", 8);
    ecus[3].num_dtcs = 0;
    ecus[3].num_permanent_dtcs = 0;

    ecus[3].supported_pids_01_20 = 0; // Basic PIDs usually not supported via Mode 01 for SRS
    ecus[3].supported_pids_09 = (1UL << (32 - 0x02)); // VIN
    ecus[3].uds_session = 0x01;
}