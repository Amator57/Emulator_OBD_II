#include "shared.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web_page.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern bool dynamic_rpm_enabled, misfire_simulation_enabled, lean_mixture_simulation_enabled, fault_incorrect_sequence, fault_silent_mode, fault_multiple_responses, fault_stmin_overflow, fault_wrong_flow_control, fault_partial_vin;
bool can_logging_enabled = false;
extern int frame_delay_ms;
extern int error_injection_rate;
extern bool simulation_running; // Оголошуємо зовнішню змінну для керування станом
extern volatile bool need_can_reinit; // Оголошуємо зовнішній прапорець
extern SemaphoreHandle_t configMutex; // Оголошуємо зовнішній м'ютекс
extern volatile bool need_websocket_update;
extern volatile bool need_clear_dtcs;
extern volatile bool need_drive_cycle;
extern volatile bool need_load_config;
extern volatile bool need_save_config;
extern String pending_config_json;

// Forward declarations for functions used in web handlers
void clearDTCs(ECU &ecu, bool use29bit, bool sendResponse);
void completeDrivingCycle(ECU &ecu, bool use29bit);
bool addDTC(ECU &ecu, const char* new_dtc);
bool initCAN(int bitrate);
void saveConfig();
void parseJsonConfig(String &json_buffer);
String getJsonConfig();
extern void saveWifi(String ssid, String pass, String ip, String gw, String sn);

void logCAN(const twai_message_t& frame, bool rx) {
    if (!can_logging_enabled) return;
    
    // Simple JSON format for log
    String logMsg = "{\"log\": \"";
    logMsg += (rx ? "[RX] " : "[TX] ");
    
    char buf[128];
    snprintf(buf, sizeof(buf), "ID: %03X%s DLC: %d Data:", frame.identifier, frame.extd ? " (EXT)" : "", frame.data_length_code);
    logMsg += buf;

    for (int i = 0; i < frame.data_length_code; i++) {
        snprintf(buf, sizeof(buf), " %02X", frame.data[i]);
        logMsg += buf;
    }
    logMsg += "\"}";
    ws.textAll(logMsg);
}

String getJsonState() {
    DynamicJsonDocument doc(8192);
    doc["ram"] = ESP.getFreeHeap();
    doc["vin"] = ecus[0].vin;
    doc["cal_id"] = ecus[0].cal_id;
    doc["cvn"] = ecus[0].cvn;
    doc["rpm"] = ecus[0].engine_rpm;
    doc["speed"] = ecus[0].vehicle_speed;
    doc["temp"] = ecus[0].engine_temp;
    doc["maf"] = ecus[0].maf_rate;
    doc["load"] = ecus[0].engine_load;
    doc["map"] = ecus[0].map_pressure;
    doc["tps"] = ecus[0].throttle_pos;
    doc["iat"] = ecus[0].intake_temp;
    doc["stft"] = ecus[0].short_term_fuel_trim;
    doc["ltft"] = ecus[0].long_term_fuel_trim;
    doc["stft2"] = ecus[0].short_term_fuel_trim_b2;
    doc["ltft2"] = ecus[0].long_term_fuel_trim_b2;
    doc["o2"] = ecus[0].o2_voltage;
    doc["obd_std"] = ecus[0].obd_standard;
    doc["o2_sens"] = ecus[0].o2_sensors_present;
    doc["timing"] = ecus[0].timing_advance;
    doc["fuel_pressure"] = ecus[0].fuel_pressure;
    doc["fuel_rate"] = ecus[0].fuel_rate;
    doc["fuel"] = ecus[0].fuel_level;
    doc["voltage"] = ecus[0].battery_voltage;
    doc["fuel_sys"] = ecus[0].fuel_system_status;
    doc["dist_mil_on"] = ecus[0].distance_mil_on;
    doc["evap"] = ecus[0].evap_purge;
    doc["egr_cmd"] = ecus[0].commanded_egr;
    doc["egr_err"] = ecus[0].egr_error;
    doc["evap_vp"] = ecus[0].evap_vapor_pressure;
    doc["evap_abs"] = ecus[0].abs_evap_pressure;
    doc["warm_ups"] = ecus[0].warm_ups;
    doc["baro"] = ecus[0].baro_pressure;
    doc["fuel_rail_pres_rel"] = ecus[0].fuel_rail_pressure_relative;
    doc["fuel_rail_pres_gauge"] = ecus[0].fuel_rail_pressure_gauge;
    doc["abs_load"] = ecus[0].abs_load;
    doc["wb_b1s1_l"] = ecus[0].o2_lambda_b1s1; doc["wb_b1s1_c"] = ecus[0].o2_current_b1s1;
    doc["wb_b1s2_l"] = ecus[0].o2_lambda_b1s2; doc["wb_b1s2_c"] = ecus[0].o2_current_b1s2;
    doc["wb_b2s1_l"] = ecus[0].o2_lambda_b2s1; doc["wb_b2s1_c"] = ecus[0].o2_current_b2s1;
    doc["wb_b2s2_l"] = ecus[0].o2_lambda_b2s2; doc["wb_b2s2_c"] = ecus[0].o2_current_b2s2;
    doc["cat_b1s1"] = ecus[0].catalyst_temp_b1s1;
    doc["cat_b2s1"] = ecus[0].catalyst_temp_b2s1;
    doc["cat_b1s2"] = ecus[0].catalyst_temp_b1s2;
    doc["cat_b2s2"] = ecus[0].catalyst_temp_b2s2;
    doc["lambda"] = ecus[0].command_equiv_ratio;
    doc["rel_tps"] = ecus[0].relative_throttle;
    doc["cmd_throttle"] = ecus[0].commanded_throttle_actuator;
    doc["rel_app"] = ecus[0].rel_accel_pedal_pos;
    doc["app_d"] = ecus[0].accel_pedal_pos_d;
    doc["app_e"] = ecus[0].accel_pedal_pos_e;
    doc["time_mil"] = ecus[0].time_run_mil_on;
    doc["time_clear"] = ecus[0].time_since_dtc_cleared;
    doc["amb_temp"] = ecus[0].ambient_temp;
    doc["oil_temp"] = ecus[0].oil_temp;
    doc["tcm_gear"] = ecus[1].current_gear;
    doc["abs_speed"] = ecus[2].vehicle_speed;
    doc["abs_vin"] = ecus[2].vin;
    doc["srs_vin"] = ecus[3].vin;
    doc["mode"] = (int)emulatorMode;
    doc["dynamic_rpm"] = dynamic_rpm_enabled;
    doc["bitrate"] = canBitrate;
    doc["fault_seq"] = fault_incorrect_sequence;
    doc["fault_silent"] = fault_silent_mode;
    doc["fault_multi"] = fault_multiple_responses;
    doc["fault_stmin"] = fault_stmin_overflow;
    doc["fault_wrong_fc"] = fault_wrong_flow_control;
    doc["fault_partial_vin"] = fault_partial_vin;
    doc["ecu0_en"] = ecus[0].enabled;
    doc["ecu1_en"] = ecus[1].enabled;
    doc["ecu2_en"] = ecus[2].enabled;
    doc["ecu3_en"] = ecus[3].enabled;
    doc["can_log"] = can_logging_enabled;
    doc["uds_session"] = ecus[0].uds_session;
    doc["uds_security"] = ecus[0].uds_security_unlocked;
    doc["cycles"] = ecus[0].error_free_cycles;
    
    doc["freeze_frame_set"] = ecus[0].freezeFrameSet;
    if (ecus[0].freezeFrameSet) {
        doc["ff_dtc"] = ecus[0].dtcs[0];
        doc["ff_rpm"] = ecus[0].freezeFrame.rpm;
        doc["ff_speed"] = ecus[0].freezeFrame.speed;
        doc["ff_temp"] = ecus[0].freezeFrame.temp;
        doc["ff_maf"] = ecus[0].freezeFrame.maf;
        doc["ff_fuel_pressure"] = ecus[0].freezeFrame.fuel_pressure;
    }
    
    JsonArray dtcs_array = doc.createNestedArray("dtcs");
    // Збираємо унікальні коди зі всіх блоків для відображення в UI
    for (int ecu_idx = 0; ecu_idx < NUM_ECUS; ecu_idx++) {
        for (int i = 0; i < ecus[ecu_idx].num_dtcs; i++) {
            const char* code = ecus[ecu_idx].dtcs[i]; // Assuming dtcs[i] is a char array
            bool already_exists = false;
            for (size_t j = 0; j < dtcs_array.size(); j++) {
                if (strcmp(dtcs_array[j].as<const char*>(), code) == 0) {
                    already_exists = true;
                    break;
                }
            }
            if (!already_exists) dtcs_array.add(code);
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// Ця функція тепер безпечна для виклику з будь-якого місця (ISR, таймери, інші задачі)
void notifyClients() {
    need_websocket_update = true;
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    client->text(getJsonState());
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Використовуємо send_P з приведенням типу та довжиною, щоб читати прямо з Flash без копіювання в RAM
    request->send(200, "text/html", (const uint8_t*)index_html, strlen(index_html));
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // Блокуємо доступ до ecus на час оновлення
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
    
    if(request->hasParam("rpm")) ecus[0].engine_rpm = request->getParam("rpm")->value().toInt();
    if(request->hasParam("speed")) ecus[0].vehicle_speed = request->getParam("speed")->value().toInt();
    if(request->hasParam("temp")) ecus[0].engine_temp = request->getParam("temp")->value().toInt();
    if(request->hasParam("fuel_sys")) ecus[0].fuel_system_status = request->getParam("fuel_sys")->value().toInt();
    if(request->hasParam("maf")) ecus[0].maf_rate = request->getParam("maf")->value().toFloat();
    if(request->hasParam("timing")) ecus[0].timing_advance = request->getParam("timing")->value().toFloat();
    if(request->hasParam("load")) ecus[0].engine_load = request->getParam("load")->value().toFloat();
    if(request->hasParam("map")) ecus[0].map_pressure = request->getParam("map")->value().toInt();
    if(request->hasParam("tps")) ecus[0].throttle_pos = request->getParam("tps")->value().toFloat();
    if(request->hasParam("iat")) ecus[0].intake_temp = request->getParam("iat")->value().toInt();
    if(request->hasParam("stft")) ecus[0].short_term_fuel_trim = request->getParam("stft")->value().toFloat();
    if(request->hasParam("ltft")) ecus[0].long_term_fuel_trim = request->getParam("ltft")->value().toFloat();
    if(request->hasParam("stft2")) ecus[0].short_term_fuel_trim_b2 = request->getParam("stft2")->value().toFloat();
    if(request->hasParam("ltft2")) ecus[0].long_term_fuel_trim_b2 = request->getParam("ltft2")->value().toFloat();
    if(request->hasParam("o2")) ecus[0].o2_voltage = request->getParam("o2")->value().toFloat();
    if(request->hasParam("obd_std")) ecus[0].obd_standard = request->getParam("obd_std")->value().toInt();
    if(request->hasParam("o2_sens")) ecus[0].o2_sensors_present = request->getParam("o2_sens")->value().toInt();
    if(request->hasParam("fuel_pressure")) ecus[0].fuel_pressure = request->getParam("fuel_pressure")->value().toInt();
    
    // PIDs 20-3F
    if(request->hasParam("fuel")) ecus[0].fuel_level = request->getParam("fuel")->value().toFloat();
    if(request->hasParam("dist_since_clear")) ecus[0].distance_with_mil = request->getParam("dist_since_clear")->value().toInt();
    if(request->hasParam("dist_mil_on")) ecus[0].distance_mil_on = request->getParam("dist_mil_on")->value().toInt();
    if(request->hasParam("evap")) ecus[0].evap_purge = request->getParam("evap")->value().toFloat();
    if(request->hasParam("warm_ups")) ecus[0].warm_ups = request->getParam("warm_ups")->value().toInt();
    if(request->hasParam("egr_cmd")) ecus[0].commanded_egr = request->getParam("egr_cmd")->value().toFloat();
    if(request->hasParam("egr_err")) ecus[0].egr_error = request->getParam("egr_err")->value().toFloat();
    if(request->hasParam("evap_vp")) ecus[0].evap_vapor_pressure = request->getParam("evap_vp")->value().toInt();
    if(request->hasParam("evap_abs")) ecus[0].abs_evap_pressure = request->getParam("evap_abs")->value().toFloat();
    if(request->hasParam("baro")) ecus[0].baro_pressure = request->getParam("baro")->value().toInt();
    if(request->hasParam("fuel_rail_pres_rel")) ecus[0].fuel_rail_pressure_relative = request->getParam("fuel_rail_pres_rel")->value().toInt();
    if(request->hasParam("fuel_rail_pres_gauge")) ecus[0].fuel_rail_pressure_gauge = request->getParam("fuel_rail_pres_gauge")->value().toInt();
    if(request->hasParam("wb_b1s1_l")) ecus[0].o2_lambda_b1s1 = request->getParam("wb_b1s1_l")->value().toFloat();
    if(request->hasParam("wb_b1s1_c")) ecus[0].o2_current_b1s1 = request->getParam("wb_b1s1_c")->value().toFloat();
    if(request->hasParam("wb_b1s2_l")) ecus[0].o2_lambda_b1s2 = request->getParam("wb_b1s2_l")->value().toFloat();
    if(request->hasParam("wb_b1s2_c")) ecus[0].o2_current_b1s2 = request->getParam("wb_b1s2_c")->value().toFloat();
    if(request->hasParam("wb_b2s1_l")) ecus[0].o2_lambda_b2s1 = request->getParam("wb_b2s1_l")->value().toFloat();
    if(request->hasParam("wb_b2s1_c")) ecus[0].o2_current_b2s1 = request->getParam("wb_b2s1_c")->value().toFloat();
    if(request->hasParam("wb_b2s2_l")) ecus[0].o2_lambda_b2s2 = request->getParam("wb_b2s2_l")->value().toFloat();
    if(request->hasParam("wb_b2s2_c")) ecus[0].o2_current_b2s2 = request->getParam("wb_b2s2_c")->value().toFloat();
    if(request->hasParam("cat_b1s1")) ecus[0].catalyst_temp_b1s1 = request->getParam("cat_b1s1")->value().toFloat();
    if(request->hasParam("cat_b2s1")) ecus[0].catalyst_temp_b2s1 = request->getParam("cat_b2s1")->value().toFloat();
    if(request->hasParam("cat_b1s2")) ecus[0].catalyst_temp_b1s2 = request->getParam("cat_b1s2")->value().toFloat();
    if(request->hasParam("cat_b2s2")) ecus[0].catalyst_temp_b2s2 = request->getParam("cat_b2s2")->value().toFloat();

    // PIDs 40-5F
    if(request->hasParam("fuel_rate")) ecus[0].fuel_rate = request->getParam("fuel_rate")->value().toFloat();
    if(request->hasParam("voltage")) ecus[0].battery_voltage = request->getParam("voltage")->value().toFloat();
    if(request->hasParam("abs_load")) ecus[0].abs_load = request->getParam("abs_load")->value().toFloat();
    if(request->hasParam("lambda")) ecus[0].command_equiv_ratio = request->getParam("lambda")->value().toFloat();
    if(request->hasParam("rel_tps")) ecus[0].relative_throttle = request->getParam("rel_tps")->value().toFloat();
    if(request->hasParam("cmd_throttle")) ecus[0].commanded_throttle_actuator = request->getParam("cmd_throttle")->value().toFloat();
    if(request->hasParam("rel_app")) ecus[0].rel_accel_pedal_pos = request->getParam("rel_app")->value().toFloat();
    if(request->hasParam("app_d")) ecus[0].accel_pedal_pos_d = request->getParam("app_d")->value().toFloat();
    if(request->hasParam("app_e")) ecus[0].accel_pedal_pos_e = request->getParam("app_e")->value().toFloat();
    if(request->hasParam("time_mil")) ecus[0].time_run_mil_on = request->getParam("time_mil")->value().toInt();
    if(request->hasParam("time_clear")) ecus[0].time_since_dtc_cleared = request->getParam("time_clear")->value().toInt();
    if(request->hasParam("amb_temp")) ecus[0].ambient_temp = request->getParam("amb_temp")->value().toInt();
    if(request->hasParam("oil_temp")) ecus[0].oil_temp = request->getParam("oil_temp")->value().toInt();
    
    if(request->hasParam("vin")) {
        memset(ecus[0].vin, 0, sizeof(ecus[0].vin)); // Очищаємо буфер
        strncpy(ecus[0].vin, request->getParam("vin")->value().c_str(), 17);
    }
    if(request->hasParam("cal_id")) {
        memset(ecus[0].cal_id, 0, sizeof(ecus[0].cal_id));
        strncpy(ecus[0].cal_id, request->getParam("cal_id")->value().c_str(), 16);
    }
    if(request->hasParam("cvn")) {
        memset(ecus[0].cvn, 0, sizeof(ecus[0].cvn));
        strncpy(ecus[0].cvn, request->getParam("cvn")->value().c_str(), 8);
    }
    
    if(request->hasParam("tcm_gear")) ecus[1].current_gear = request->getParam("tcm_gear")->value().toInt();

    if(request->hasParam("abs_speed")) ecus[2].vehicle_speed = request->getParam("abs_speed")->value().toInt();
    if(request->hasParam("abs_vin")) {
         memset(ecus[2].vin, 0, sizeof(ecus[2].vin));
         strncpy(ecus[2].vin, request->getParam("abs_vin")->value().c_str(), 17);
    }
    if(request->hasParam("srs_vin")) {
         memset(ecus[3].vin, 0, sizeof(ecus[3].vin));
         strncpy(ecus[3].vin, request->getParam("srs_vin")->value().c_str(), 17);
    }

    // Mode 02 (Freeze Frame)
    if(request->hasParam("ff_rpm")) ecus[0].freezeFrame.rpm = request->getParam("ff_rpm")->value().toInt();
    if(request->hasParam("ff_speed")) ecus[0].freezeFrame.speed = request->getParam("ff_speed")->value().toInt();
    if(request->hasParam("ff_temp")) ecus[0].freezeFrame.temp = request->getParam("ff_temp")->value().toInt();
    if(request->hasParam("ff_maf")) ecus[0].freezeFrame.maf = request->getParam("ff_maf")->value().toFloat();
    if(request->hasParam("ff_pres")) ecus[0].freezeFrame.fuel_pressure = request->getParam("ff_pres")->value().toInt();

    // Mode 06
    if(request->hasParam("m06_t1_id")) ecus[0].mode06_tests[0].testId = strtol(request->getParam("m06_t1_id")->value().c_str(), NULL, 16);
    if(request->hasParam("m06_t1_val")) ecus[0].mode06_tests[0].value = request->getParam("m06_t1_val")->value().toInt();
    if(request->hasParam("m06_t1_min")) ecus[0].mode06_tests[0].min_limit = request->getParam("m06_t1_min")->value().toInt();
    if(request->hasParam("m06_t1_max")) ecus[0].mode06_tests[0].max_limit = request->getParam("m06_t1_max")->value().toInt();
    
    if(request->hasParam("m06_t2_id")) ecus[0].mode06_tests[1].testId = strtol(request->getParam("m06_t2_id")->value().c_str(), NULL, 16);
    if(request->hasParam("m06_t2_val")) ecus[0].mode06_tests[1].value = request->getParam("m06_t2_val")->value().toInt();
    if(request->hasParam("m06_t2_min")) ecus[0].mode06_tests[1].min_limit = request->getParam("m06_t2_min")->value().toInt();
    if(request->hasParam("m06_t2_max")) ecus[0].mode06_tests[1].max_limit = request->getParam("m06_t2_max")->value().toInt();

    if(request->hasParam("dynamic_rpm")) dynamic_rpm_enabled = (request->getParam("dynamic_rpm")->value() == "true");
    if(request->hasParam("misfire_sim")) misfire_simulation_enabled = (request->getParam("misfire_sim")->value() == "true");
    if(request->hasParam("lean_mixture_sim")) lean_mixture_simulation_enabled = (request->getParam("lean_mixture_sim")->value() == "true");

    if(request->hasParam("dtc_list")) {
        String dtcs = request->getParam("dtc_list")->value();
        
        // Очищаємо помилки у всіх блоках перед оновленням, щоб уникнути дублювання та "фантомних" кодів
        for(int i=0; i<4; i++) {
            ecus[i].num_dtcs = 0; // Reset current DTCs
            ecus[i].num_permanent_dtcs = 0;
            ecus[i].freezeFrameSet = false;
        }

        int start = 0;
        while(start < dtcs.length()) {
            int comma = dtcs.indexOf(',', start);
            String token = (comma == -1) ? dtcs.substring(start) : dtcs.substring(start, comma);
            token.trim();
            if(token.length() > 0) {
                char prefix = toupper(token[0]);
                if (prefix == 'C') addDTC(ecus[2], token.c_str());      // ABS
                else if (prefix == 'B') addDTC(ecus[3], token.c_str()); // SRS
                else addDTC(ecus[0], token.c_str());                    // ECM
            }
            if(comma == -1) break;
            start = comma + 1;
        }
    }
    
    if(request->hasParam("mode")) {
        emulatorMode = (EmulatorMode)request->getParam("mode")->value().toInt();
    }
    if(request->hasParam("bitrate")) {
        int new_bitrate = request->getParam("bitrate")->value().toInt();
        if (new_bitrate != canBitrate) {
            canBitrate = new_bitrate;
            need_can_reinit = true; // Запитуємо перезапуск CAN у main loop
        }
    }
   if(request->hasParam("frame_delay")) {
        frame_delay_ms = request->getParam("frame_delay")->value().toInt();
   }
   if(request->hasParam("error_rate")) {
        error_injection_rate = request->getParam("error_rate")->value().toInt();
    }
    
    // Fault Injection Toggles
    if(request->hasParam("fault_seq")) fault_incorrect_sequence = (request->getParam("fault_seq")->value() == "true");
    if(request->hasParam("fault_silent")) fault_silent_mode = (request->getParam("fault_silent")->value() == "true");
    if(request->hasParam("fault_multi")) fault_multiple_responses = (request->getParam("fault_multi")->value() == "true");
    if(request->hasParam("fault_stmin")) fault_stmin_overflow = (request->getParam("fault_stmin")->value() == "true");
    if(request->hasParam("fault_wrong_fc")) fault_wrong_flow_control = (request->getParam("fault_wrong_fc")->value() == "true");
    if(request->hasParam("fault_partial_vin")) fault_partial_vin = (request->getParam("fault_partial_vin")->value() == "true");

    if(request->hasParam("ecu0_en")) ecus[0].enabled = (request->getParam("ecu0_en")->value() == "true");
    if(request->hasParam("ecu1_en")) ecus[1].enabled = (request->getParam("ecu1_en")->value() == "true");
    if(request->hasParam("ecu2_en")) ecus[2].enabled = (request->getParam("ecu2_en")->value() == "true");
    if(request->hasParam("ecu3_en")) ecus[3].enabled = (request->getParam("ecu3_en")->value() == "true");
    if(request->hasParam("can_log")) can_logging_enabled = (request->getParam("can_log")->value() == "true");
    
    simulation_running = true; // Вмикаємо обмін зі сканером при натисканні Update Display
    // need_display_update = true; // Вимкнено, щоб фіксувати початковий екран
    
    need_websocket_update = true; // Запитуємо оновлення WebSocket в main loop
    
    xSemaphoreGive(configMutex);
    }
    request->send(200, "application/json", getJsonState()); // Повертаємо повний стан як JSON
  });

  server.on("/clear_dtc", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
    need_clear_dtcs = true; // Запитуємо очищення в main loop
    xSemaphoreGive(configMutex);
    }
    request->send(200, "text/plain", "Cleared");
  });

  server.on("/cycle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
    need_drive_cycle = true; // Запитуємо цикл в main loop
    xSemaphoreGive(configMutex);
    }
    request->send(200, "text/plain", "Cycle Completed");
    // need_display_update = true; // Вимкнено для фіксації початкового екрану
  });

  server.on("/save_nvs", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        need_save_config = true; // Запитуємо збереження в main loop
        xSemaphoreGive(configMutex);
    }
    request->send(200, "text/plain", "Saved to NVS");
  });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
      String jsonConfig;
      if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
      jsonConfig = getJsonConfig();
      xSemaphoreGive(configMutex);
      }
      request->send(200, "application/json", jsonConfig);
  });

  server.on("/load_config", HTTP_POST, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "Loaded");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String json_buffer;
      if(index == 0) json_buffer = "";
      for(size_t i=0; i<len; i++) json_buffer += (char)data[i];
      if(index + len == total) {
          if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
              pending_config_json = json_buffer; // Копіюємо JSON
              need_load_config = true;           // Запитуємо парсинг в main loop
              xSemaphoreGive(configMutex);
          }
      }
  });

  server.on("/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request){
      if(request->hasParam("ssid", true)) {
          String ssid = request->getParam("ssid", true)->value();
          String pass = request->getParam("pass", true)->value();
          String ip = request->getParam("ip", true)->value();
          String gw = request->getParam("gw", true)->value();
          String sn = request->getParam("sn", true)->value();
          saveWifi(ssid, pass, ip, gw, sn);
          request->send(200, "text/plain", "WiFi Saved. Restarting...");
          delay(1000);
          ESP.restart();
      } else {
          request->send(400, "text/plain", "Missing Configuration");
      }
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}