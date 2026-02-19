#include "shared.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web_page.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern bool dynamic_rpm_enabled;
extern bool misfire_simulation_enabled;
extern bool lean_mixture_simulation_enabled;
extern int frame_delay_ms;
extern int error_injection_rate;
extern bool fault_incorrect_sequence;
extern bool fault_silent_mode;
extern bool fault_multiple_responses;
extern bool fault_stmin_overflow;
extern bool fault_wrong_flow_control;
extern bool fault_partial_vin;
bool can_logging_enabled = false;

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
    
    updateDisplay();
    notifyClients();
    request->send(200, "text/plain", "Updated");
  });

  server.on("/clear_dtc", HTTP_GET, [] (AsyncWebServerRequest *request) {
    clearDTCs(ecus[0], false);
    request->send(200, "text/plain", "Cleared");
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