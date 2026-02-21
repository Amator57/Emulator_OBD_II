#include "shared.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web_page.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern bool dynamic_rpm_enabled, misfire_simulation_enabled, lean_mixture_simulation_enabled, fault_incorrect_sequence, fault_silent_mode, fault_multiple_responses, fault_stmin_overflow, fault_wrong_flow_control, fault_partial_vin, can_logging_enabled;
bool can_logging_enabled = false;
extern int current_display_page;
extern int frame_delay_ms;
extern int error_injection_rate;

// Forward declarations for functions used in web handlers
void clearDTCs(ECU &ecu, bool use29bit);
void completeDrivingCycle(ECU &ecu, bool use29bit);
bool addDTC(ECU &ecu, const char* new_dtc);
bool initCAN(int bitrate);
void saveConfig();
void parseJsonConfig(String &json_buffer);
String getJsonConfig();

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
    DynamicJsonDocument doc(2048);
    doc["vin"] = ecus[0].vin;
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
    doc["o2"] = ecus[0].o2_voltage;
    doc["voltage"] = ecus[0].battery_voltage;
    doc["mode"] = (int)emulatorMode;
    doc["bitrate"] = canBitrate;
    doc["fault_seq"] = fault_incorrect_sequence;
    doc["fault_silent"] = fault_silent_mode;
    doc["fault_multi"] = fault_multiple_responses;
    doc["fault_stmin"] = fault_stmin_overflow;
    doc["fault_wrong_fc"] = fault_wrong_flow_control;
    doc["fault_partial_vin"] = fault_partial_vin;
    doc["ecu0_en"] = ecus[0].enabled;
    doc["ecu1_en"] = ecus[1].enabled;
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
    for (int i = 0; i < ecus[0].num_dtcs; i++) {
        dtcs_array.add(ecus[0].dtcs[i]);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void notifyClients() {
    ws.textAll(getJsonState());
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    client->text(getJsonState());
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("rpm")) ecus[0].engine_rpm = request->getParam("rpm")->value().toInt();
    if(request->hasParam("speed")) ecus[0].vehicle_speed = request->getParam("speed")->value().toInt();
    if(request->hasParam("temp")) ecus[0].engine_temp = request->getParam("temp")->value().toInt();
    if(request->hasParam("maf")) ecus[0].maf_rate = request->getParam("maf")->value().toFloat();
    if(request->hasParam("timing")) ecus[0].timing_advance = request->getParam("timing")->value().toFloat();
    if(request->hasParam("load")) ecus[0].engine_load = request->getParam("load")->value().toFloat();
    if(request->hasParam("map")) ecus[0].map_pressure = request->getParam("map")->value().toInt();
    if(request->hasParam("tps")) ecus[0].throttle_pos = request->getParam("tps")->value().toFloat();
    if(request->hasParam("iat")) ecus[0].intake_temp = request->getParam("iat")->value().toInt();
    if(request->hasParam("stft")) ecus[0].short_term_fuel_trim = request->getParam("stft")->value().toFloat();
    if(request->hasParam("ltft")) ecus[0].long_term_fuel_trim = request->getParam("ltft")->value().toFloat();
    if(request->hasParam("o2")) ecus[0].o2_voltage = request->getParam("o2")->value().toFloat();
    if(request->hasParam("fuel_pressure")) ecus[0].fuel_pressure = request->getParam("fuel_pressure")->value().toInt();
    if(request->hasParam("fuel_rate")) ecus[0].fuel_rate = request->getParam("fuel_rate")->value().toFloat();
    if(request->hasParam("fuel")) ecus[0].fuel_level = request->getParam("fuel")->value().toFloat();
    if(request->hasParam("dist_mil")) ecus[0].distance_with_mil = request->getParam("dist_mil")->value().toInt();
    if(request->hasParam("voltage")) ecus[0].battery_voltage = request->getParam("voltage")->value().toFloat();
    
    if(request->hasParam("vin")) strncpy(ecus[0].vin, request->getParam("vin")->value().c_str(), 17);
    if(request->hasParam("cal_id")) strncpy(ecus[0].cal_id, request->getParam("cal_id")->value().c_str(), 16);
    if(request->hasParam("cvn")) strncpy(ecus[0].cvn, request->getParam("cvn")->value().c_str(), 8);
    
    if(request->hasParam("tcm_gear")) ecus[1].current_gear = request->getParam("tcm_gear")->value().toInt();

    if(request->hasParam("dynamic_rpm")) dynamic_rpm_enabled = (request->getParam("dynamic_rpm")->value() == "true");
   if(request->hasParam("misfire_sim")) misfire_simulation_enabled = (request->getParam("misfire_sim")->value() == "true");
    if(request->hasParam("lean_mixture_sim")) lean_mixture_simulation_enabled = (request->getParam("lean_mixture_sim")->value() == "true");

    if(request->hasParam("dtc_list")) {
        String dtcs = request->getParam("dtc_list")->value();
        ecus[0].num_dtcs = 0; // Очищаємо список перед оновленням
        int start = 0;
        while(start < dtcs.length()) {
            int comma = dtcs.indexOf(',', start);
            String token = (comma == -1) ? dtcs.substring(start) : dtcs.substring(start, comma);
            token.trim();
            if(token.length() > 0) addDTC(ecus[0], token.c_str());
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
            initCAN(canBitrate);
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
    if(request->hasParam("can_log")) can_logging_enabled = (request->getParam("can_log")->value() == "true");
    
    if(request->hasParam("page")) {
        current_display_page = request->getParam("page")->value().toInt();
    }

    updateDisplay();
    notifyClients();
    request->send(200, "text/plain", "Updated");
  });

  server.on("/clear_dtc", HTTP_GET, [] (AsyncWebServerRequest *request) {
    clearDTCs(ecus[0], false);
    request->send(200, "text/plain", "Cleared");
  });

  server.on("/cycle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    completeDrivingCycle(ecus[0], false);
    request->send(200, "text/plain", "Cycle Completed");
  });

  server.on("/save_nvs", HTTP_GET, [] (AsyncWebServerRequest *request) {
    saveConfig();
    request->send(200, "text/plain", "Saved to NVS");
  });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
      String jsonConfig = getJsonConfig();
      request->send(200, "application/json", jsonConfig);
  });

  server.on("/load_config", HTTP_POST, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "Loaded");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String json_buffer;
      if(index == 0) json_buffer = "";
      for(size_t i=0; i<len; i++) json_buffer += (char)data[i];
      if(index + len == total) parseJsonConfig(json_buffer);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}