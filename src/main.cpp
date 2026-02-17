#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/twai.h>

#include "web_page.h"

// --- TFT Display ---
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ############## Налаштування пінів TFT ST7735 ##############
// Якщо у вас інші піни, змініть їх тут
//#define TFT_CS     5
//#define TFT_DC     2
//#define TFT_RST    4
// Для апаратної реалізації SPI, піни MOSI та SCLK зазвичай визначені платою
// Для ESP32 це GPIO 23 (MOSI) та GPIO 18 (SCLK)
// #define TFT_SCLK 18
// #define TFT_MOSI 23
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_CS     10
#define TFT_DC      9
#define TFT_RST     8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ############## Налаштування CAN ##############
const int CAN_TX_PIN = 20;
const int CAN_RX_PIN = 21;
char vin[18] = "VIN_NOT_SET";
char cal_id[17] = "EMULATOR_CAL_ID";
char cvn[9] = "A1B2C3D4";
char dtcs[5][6] = {"", "", "", "", ""};
int num_dtcs = 0;
char permanent_dtcs[5][6] = {"", "", "", "", ""};
int num_permanent_dtcs = 0;
int engine_rpm = 1500;
int engine_temp = 90;
int vehicle_speed = 60; // km/h
float maf_rate = 10.0; // g/s
bool dynamic_rpm_enabled = false;
bool misfire_simulation_enabled = false;
bool lean_mixture_simulation_enabled = false;
float timing_advance = 5.0; // degrees
float fuel_rate = 1.5; // L/h
int fuel_pressure = 350; // kPa (Normal ~300-400)
float fuel_level = 75.0; // %
int distance_with_mil = 0; // km
float battery_voltage = 14.2; // V
int error_free_cycles = 0;
const int CYCLES_THRESHOLD = 3; // Кількість циклів для очищення Permanent DTC

// --- ISO-TP State Machine ---
enum IsoTpState {
    ISOTP_IDLE,
    ISOTP_SEND_VIN_CF1,
    ISOTP_SEND_VIN_CF2,
    ISOTP_SEND_CALID_CF1,
    ISOTP_SEND_CALID_CF2
};
IsoTpState isoTpState = ISOTP_IDLE;
unsigned long isoTpNextTime = 0;
const int ISOTP_DELAY_MS = 5; // STmin simulation

// ############## Налаштування Wi-Fi та веб-сервера ##############
const char* ap_ssid = "OBD-II-Emulator-A";
const char* ap_password = "123456789";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ############## Прототипи функцій ##############
void handleOBDRequest(const twai_message_t &frame);
void sendVIN(byte pid);
void sendCalId(byte pid);
void sendCvn(byte pid);
void sendSupportedPids_09(byte pid);
void processIsoTp();
void sendDTCs();
void sendPermanentDTCs();
void clearDTCs();
void sendCurrentData(byte pid);
void updateDisplay();
void notifyClients();
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
bool addDTC(const char* new_dtc);
void completeDrivingCycle();


void setup() {
  Serial.begin(115200);
  Serial.println("OBD-II Emulator-A Starting...");

  // Явна ініціалізація SPI, щоб гарантувати використання вибраних пінів (SCLK, MISO, MOSI, SS)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  // --- Ініціалізація TFT ---
  tft.initR(INITR_BLACKTAB); // або INITR_GREENTAB, INITR_REDTAB
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1); // Горизонтальна орієнтація
  tft.setCursor(0, 0);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.println("OBD-II Emulator-A");
  tft.println("Starting...");

  // --- Налаштування Wi-Fi (точка доступу) ---
  Serial.print("Creating Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print(" AP IP: ");
  Serial.println(IP);
  tft.print("IP: ");
  tft.println(IP);

  // --- Налаштування CAN ---
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  // Accept all messages, we will filter by ID in the code
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Install and start TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
      Serial.println("Failed to install TWAI driver");
      return;
  }
  if (twai_start() != ESP_OK) {
      Serial.println("Failed to start TWAI driver");
      return;
  }
  Serial.println("TWAI (CAN) bus initialized.");

  // --- Налаштування веб-сервера ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("vin")) strncpy(vin, request->getParam("vin")->value().c_str(), 17);
    if(request->hasParam("cal_id")) strncpy(cal_id, request->getParam("cal_id")->value().c_str(), 16);
    if(request->hasParam("cvn")) strncpy(cvn, request->getParam("cvn")->value().c_str(), 8);

    // Скидаємо старі DTC
    num_dtcs = 0;
    num_permanent_dtcs = 0;
    for(int i=0; i<5; i++) {
        dtcs[i][0] = '\0';
        permanent_dtcs[i][0] = '\0';
    }

    // Якщо прийшов параметр dtc_list (кома-розділений список), використаємо його (переважно)
    if(request->hasParam("dtc_list")){
      String list = request->getParam("dtc_list")->value();
      int start = 0;
      while(num_dtcs < 5){
        int comma = list.indexOf(',', start);
        String token;
        if(comma == -1){
          token = list.substring(start);
        } else {
          token = list.substring(start, comma);
        }
        token.trim();
        if(token.length() == 5){
          // При оновленні з веб-форми, заповнюємо обидва списки однаково
          strncpy(dtcs[num_dtcs], token.c_str(), 5);
          dtcs[num_dtcs][5] = '\0';
          strncpy(permanent_dtcs[num_dtcs], token.c_str(), 5);
          permanent_dtcs[num_dtcs][5] = '\0';
          num_dtcs++;
        }
        if(comma == -1) break;
        start = comma + 1;
      }
    } else {
      // Збираємо нові DTC з частин (зворотна сумісність зі старою формою)
      for (int i=1; i<=5; i++){
        String dtc_sys_param = "dtc" + String(i) + "_sys";
        String dtc_type_param = "dtc" + String(i) + "_type";
        String dtc_code_param = "dtc" + String(i) + "_code";

        if(request->hasParam(dtc_code_param) && request->getParam(dtc_code_param)->value().length() > 0) {
          String dtc_full = request->getParam(dtc_sys_param)->value() +
                            request->getParam(dtc_type_param)->value() +
                            request->getParam(dtc_code_param)->value();

          if(dtc_full.length() >= 4){
            // Normalize: pad numeric part to 3 digits if necessary
            // Expecting total length 5 (e.g., P0123)
            if(dtc_full.length() < 5){
              // if code part shorter, pad with leading zeros
              char buf[8];
              strncpy(buf, dtc_full.c_str(), sizeof(buf)-1);
              buf[sizeof(buf)-1] = '\0';
              // ensure we only copy 5 chars into dtcs
              strncpy(dtcs[num_dtcs], buf, 5);
              dtcs[num_dtcs][5] = '\0';
            } else {
              strncpy(dtcs[num_dtcs], dtc_full.c_str(), 5);
              dtcs[num_dtcs][5] = '\0';
            }
            num_dtcs++;
          }
        }
      }
    }
    num_permanent_dtcs = num_dtcs; // Синхронізуємо лічильники

    if(request->hasParam("temp")) engine_temp = request->getParam("temp")->value().toInt();
    if(request->hasParam("rpm")) engine_rpm = request->getParam("rpm")->value().toInt();
    if(request->hasParam("speed")) vehicle_speed = request->getParam("speed")->value().toInt();
    if(request->hasParam("maf")) maf_rate = request->getParam("maf")->value().toFloat();
    if(request->hasParam("timing")) timing_advance = request->getParam("timing")->value().toFloat();
    if(request->hasParam("fuel_rate")) fuel_rate = request->getParam("fuel_rate")->value().toFloat();
    if(request->hasParam("fuel_pressure")) fuel_pressure = request->getParam("fuel_pressure")->value().toInt();
    
    if(request->hasParam("fuel")) fuel_level = request->getParam("fuel")->value().toFloat();
    if(request->hasParam("dist_mil")) distance_with_mil = request->getParam("dist_mil")->value().toInt();
    if(request->hasParam("voltage")) battery_voltage = request->getParam("voltage")->value().toFloat();
    
    if(request->hasParam("dynamic_rpm")) {
        String val = request->getParam("dynamic_rpm")->value();
        dynamic_rpm_enabled = (val == "true" || val == "1" || val == "on");
    }

    if(request->hasParam("misfire_sim")) {
        String val = request->getParam("misfire_sim")->value();
        misfire_simulation_enabled = (val == "true" || val == "1" || val == "on");
    }

    if(request->hasParam("lean_mixture_sim")) {
        String val = request->getParam("lean_mixture_sim")->value();
        lean_mixture_simulation_enabled = (val == "true" || val == "1" || val == "on");
    }
    
    Serial.println("========== Emulator Data Updated ==========");
    Serial.println("VIN: " + String(vin));
    Serial.println("CAL ID: " + String(cal_id));
    Serial.println("CVN: " + String(cvn));
    for(int i=0; i<num_dtcs; i++){
      Serial.println("DTC "+ String(i+1) +": " + String(dtcs[i]));
    }
    Serial.println("Engine Temp: " + String(engine_temp));
    Serial.println("Engine RPM: " + String(engine_rpm));
    Serial.println("Vehicle Speed: " + String(vehicle_speed) + " km/h");
    Serial.println("MAF Rate: " + String(maf_rate) + " g/s");
    Serial.println("Timing Adv: " + String(timing_advance) + " deg");
    Serial.println("Fuel Rate: " + String(fuel_rate) + " L/h");
    Serial.println("Fuel Press: " + String(fuel_pressure) + " kPa");
    Serial.println("Dynamic RPM: " + String(dynamic_rpm_enabled ? "ON" : "OFF"));
    Serial.println("Misfire Sim: " + String(misfire_simulation_enabled ? "ON" : "OFF"));
    Serial.println("Lean Sim: " + String(lean_mixture_simulation_enabled ? "ON" : "OFF"));
    Serial.println("Fuel Level: " + String(fuel_level) + " %");
    Serial.println("Distance with MIL: " + String(distance_with_mil) + " km");
    Serial.println("Battery Voltage: " + String(battery_voltage) + " V");
    Serial.println("==========================================");
    
    updateDisplay(); // Оновлюємо екран
    notifyClients(); // Повідомляємо веб-клієнтів про зміни

    request->send(200, "text/plain", "Emulator data updated successfully!");
  });

  server.on("/clear_dtc", HTTP_GET, [] (AsyncWebServerRequest *request) {
    clearDTCs(); // Ця функція вже надсилає CAN-відповідь та оновлює дисплей
    request->send(200, "text/plain", "All DTCs cleared successfully!");
  });

  server.on("/cycle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    completeDrivingCycle();
    request->send(200, "text/plain", "Driving cycle simulated.");
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("Web server started.");
  delay(1000); // Затримка, щоб побачити стартові повідомлення
  updateDisplay(); // Перше оновлення екрану з початковими даними
}

void loop() {
  twai_message_t rx_frame;
  // Перевіряємо наявність вхідних CAN-повідомлень з невеликим таймаутом.
  // Основна робота керується подіями від CAN або веб-сервера.
  if (twai_receive(&rx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
    // Відповідаємо тільки на загальні OBD-II запити (ID 0x7DF)
    if (rx_frame.identifier == 0x7DF && !rx_frame.rtr) { // OBD_CAN_ID_REQUEST
        handleOBDRequest(rx_frame);
    }
  }

  processIsoTp(); // Обробка черги ISO-TP (без delay)

  // Емуляція динамічної зміни RPM (синусоїда)
  if (dynamic_rpm_enabled) {
      unsigned long now = millis();
      // Синусоїда: Центр 2500, Амплітуда 1500 (від 1000 до 4000), Період ~5 секунд
      engine_rpm = 2500 + 1500 * sin(2 * PI * now / 5000.0);
      
      // Динамічна зміна інших параметрів для демонстрації графіків
      maf_rate = 5.0 + (engine_rpm / 100.0); // Приблизна залежність
      fuel_rate = 0.5 + (engine_rpm / 1000.0); // Приблизна залежність
      if (!lean_mixture_simulation_enabled) fuel_pressure = 350 + (engine_rpm / 50); // Нормальна робота

      // Симуляція пробігу з помилкою
      if (num_dtcs > 0) {
          static unsigned long last_dist_update = 0;
          if (now - last_dist_update > 5000) { // Додаємо 1 км кожні 5с для демонстрації
              distance_with_mil++;
              last_dist_update = now;
          }
      }

      if (misfire_simulation_enabled && engine_rpm > 3500) {
          // Якщо додали новий DTC, одразу оновлюємо інтерфейси
          if (addDTC("P0300")) {
              notifyClients();
              updateDisplay();
          }
      }

      // Емуляція бідної суміші (P0171) при низькому тиску пального
      if (lean_mixture_simulation_enabled) {
          fuel_pressure = 150 + (rand() % 30); // Імітуємо падіння тиску до ~165 kPa
          // Якщо тиск низький (< 200 kPa) і є навантаження (RPM > 2000)
          if (fuel_pressure < 200 && engine_rpm > 2000) {
              if (addDTC("P0171")) {
                  notifyClients();
                  updateDisplay();
              }
          }
      }

      static unsigned long last_dynamic_notify = 0;
      // Оновлюємо веб-інтерфейс та дисплей не частіше ніж раз на 500 мс, щоб не перевантажувати
      if (now - last_dynamic_notify > 500) {
          last_dynamic_notify = now;
          notifyClients();
          updateDisplay();
      }
  }
  ws.cleanupClients();
}

void updateDisplay() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.println("OBD-II EMULATOR STATUS");
  
  tft.setTextColor(ST7735_WHITE);
  tft.print("VIN: ");
  tft.println(vin);
  tft.println(""); // Spacer

  char buf1[15], buf2[15];

  // Line 1: RPM & Speed
  snprintf(buf1, sizeof(buf1), "RPM: %d", engine_rpm);
  snprintf(buf2, sizeof(buf2), "Speed: %d", vehicle_speed);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 2: Temp & MAF
  snprintf(buf1, sizeof(buf1), "Temp: %dC", engine_temp);
  snprintf(buf2, sizeof(buf2), "MAF: %.1f", maf_rate);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 3: Fuel & Voltage
  snprintf(buf1, sizeof(buf1), "Fuel: %.0f%%", fuel_level);
  snprintf(buf2, sizeof(buf2), "Volt: %.1f", battery_voltage);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 4: Dist MIL & Cycles
  snprintf(buf1, sizeof(buf1), "MIL km: %d", distance_with_mil);
  snprintf(buf2, sizeof(buf2), "Cyc: %d/%d", error_free_cycles, CYCLES_THRESHOLD);
  tft.printf("%-14s%s\n", buf1, buf2);

  tft.println(""); // Spacer

  tft.println("DTCs:");
  if (num_dtcs > 0) {
    tft.setTextColor(ST7735_RED);
    String dtc_line = "";
    for(int i=0; i<num_dtcs; i++) {
        dtc_line += String(dtcs[i]) + " ";
    }
    tft.println(dtc_line);
  } else {
    tft.setTextColor(ST7735_GREEN);
    tft.println("  None");
  }
}

String getJsonState() {
    String json = "{";
    json += "\"vin\":\"" + String(vin) + "\",";
    json += "\"cal_id\":\"" + String(cal_id) + "\",";
    json += "\"cvn\":\"" + String(cvn) + "\",";
    json += "\"rpm\":" + String(engine_rpm) + ",";
    json += "\"temp\":" + String(engine_temp) + ",";
    json += "\"speed\":" + String(vehicle_speed) + ",";
    json += "\"maf\":" + String(maf_rate, 2) + ",";
    json += "\"timing\":" + String(timing_advance, 1) + ",";
    json += "\"fuel_rate\":" + String(fuel_rate, 2) + ",";
    json += "\"fuel_pressure\":" + String(fuel_pressure) + ",";
    json += "\"fuel\":" + String(fuel_level, 1) + ",";
    json += "\"dist_mil\":" + String(distance_with_mil) + ",";
    json += "\"voltage\":" + String(battery_voltage, 1) + ",";
    json += "\"cycles\":" + String(error_free_cycles) + ",";
    json += "\"dynamic_rpm\":" + String(dynamic_rpm_enabled ? "true" : "false") + ",";
    json += "\"misfire_sim\":" + String(misfire_simulation_enabled ? "true" : "false") + ",";
    json += "\"lean_mixture_sim\":" + String(lean_mixture_simulation_enabled ? "true" : "false") + ",";
    json += "\"dtcs\":[";
    if (num_dtcs > 0) {
        for(int i=0; i<num_dtcs; i++) {
            json += "\"" + String(dtcs[i]) + "\"";
            if (i < num_dtcs - 1) {
                json += ",";
            }
        }
    }
    json += "],";
    json += "\"permanent_dtcs\":[";
    if (num_permanent_dtcs > 0) {
        for(int i=0; i<num_permanent_dtcs; i++) {
            json += "\"" + String(permanent_dtcs[i]) + "\"";
            if (i < num_permanent_dtcs - 1) {
                json += ",";
            }
        }
    }
    json += "]}";
    return json;
}

void notifyClients() {
    ws.textAll(getJsonState());
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Відправляємо поточний стан новому клієнту
    client->text(getJsonState());
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("WebSocket client #%u error(%u): %s\n", client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("WebSocket client #%u pong: %s\n", client->id(), (len)?(char*)data:"");
  }
}

// Допоміжна функція для додавання DTC, якщо він ще не існує
bool addDTC(const char* new_dtc) {
    bool added_to_current = false;
    bool added_to_permanent = false;

    // Додаємо до поточних DTC, якщо є місце і код ще не існує
    if (num_dtcs < 5) {
        bool exists = false;
        for (int i = 0; i < num_dtcs; i++) {
            if (strcmp(dtcs[i], new_dtc) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            strncpy(dtcs[num_dtcs], new_dtc, 5);
            dtcs[num_dtcs][5] = '\0';
            num_dtcs++;
            added_to_current = true;
        }
    }

    // Додаємо до постійних DTC, якщо є місце і код ще не існує
    if (num_permanent_dtcs < 5) {
        bool exists = false;
        for (int i = 0; i < num_permanent_dtcs; i++) {
            if (strcmp(permanent_dtcs[i], new_dtc) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            strncpy(permanent_dtcs[num_permanent_dtcs], new_dtc, 5);
            permanent_dtcs[num_permanent_dtcs][5] = '\0';
            num_permanent_dtcs++;
            added_to_permanent = true;
        }
    }

    if (added_to_current || added_to_permanent) {
        Serial.printf("Misfire detected! Added DTC: %s. Current: %d, Permanent: %d\n", new_dtc, num_dtcs, num_permanent_dtcs);
        return true; // Повертаємо true, якщо код було додано хоча б до одного списку
    }
    return false;
}

void completeDrivingCycle() {
    Serial.println("Simulating Driving Cycle...");
    if (num_dtcs == 0) {
        error_free_cycles++;
        Serial.printf("  No current DTCs. Error-free cycles: %d/%d\n", error_free_cycles, CYCLES_THRESHOLD);
        
        if (error_free_cycles >= CYCLES_THRESHOLD) {
            if (num_permanent_dtcs > 0) {
                num_permanent_dtcs = 0;
                for(int i=0; i<5; i++) permanent_dtcs[i][0] = '\0';
                Serial.println("  Threshold reached! Permanent DTCs cleared.");
            }
            // Скидаємо лічильник після успішного очищення (або можна залишити, щоб показувати "здоров'я")
            // error_free_cycles = 0; 
        }
    } else {
        error_free_cycles = 0;
        Serial.println("  Current DTCs present. Error-free cycles counter reset to 0.");
    }
    updateDisplay();
    notifyClients();
}

void handleOBDRequest(const twai_message_t &frame) {
    byte service = frame.data[1];
    byte pid = frame.data[2];

    Serial.printf("Received OBD Request: Service 0x%02X, PID 0x%02X\n", service, pid);

    switch(service) {
        case 0x01: sendCurrentData(pid); break;
        case 0x03: sendDTCs(); break;
        case 0x04: clearDTCs(); break;
        case 0x09: 
            if (pid == 0x00) sendSupportedPids_09(pid);
            else if (pid == 0x02) sendVIN(pid);
            else if (pid == 0x04) sendCalId(pid);
            else if (pid == 0x06) sendCvn(pid);
            break;
        case 0x0A: sendPermanentDTCs(); break;
    }
}

void sendCurrentData(byte pid) {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8; // OBD_CAN_ID_RESPONSE
    tx_frame.extd = 0;
    tx_frame.data[1] = 0x40 + 0x01; // Відповідь на сервіс 01
    tx_frame.data[2] = pid;

    switch(pid) {
        case 0x00: { // Supported PIDs [01-20]
            // Кожен біт відповідає за підтримку одного PID.
            // Біт 31 (MSB) -> PID 0x01, ..., Біт 0 (LSB) -> PID 0x20.
            // Ми підтримуємо: 0x05, 0x0C, 0x0D, 0x10
            uint32_t supported_pids = 0;
            supported_pids |= (1UL << (32 - 0x01)); // Monitor status
            supported_pids |= (1UL << (32 - 0x05)); // Temp
            supported_pids |= (1UL << (32 - 0x0A)); // Fuel Pressure
            supported_pids |= (1UL << (32 - 0x0E)); // Timing Advance
            supported_pids |= (1UL << (32 - 0x0C)); // RPM
            supported_pids |= (1UL << (32 - 0x0D)); // Speed
            supported_pids |= (1UL << (32 - 0x10)); // MAF
            supported_pids |= (1UL << (32 - 0x20)); // Announce support for PIDs 21-40

            tx_frame.data[0] = 6; // Length: 1 (service) + 1 (PID) + 4 (data)
            tx_frame.data_length_code = 1 + 6;
            tx_frame.data[3] = (supported_pids >> 24) & 0xFF; // MSB
            tx_frame.data[4] = (supported_pids >> 16) & 0xFF;
            tx_frame.data[5] = (supported_pids >> 8) & 0xFF;
            tx_frame.data[6] = supported_pids & 0xFF;        // LSB
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Supported PIDs [01-20] data");
            break;
        }
        case 0x01: { // Monitor status since DTCs cleared
            // Byte A: Bit 7 = MIL Status, Bits 0-6 = DTC Count
            byte mil_dtc_count = num_dtcs & 0x7F;
            if (num_dtcs > 0) {
                mil_dtc_count |= 0x80; // Set MIL ON
            }
            tx_frame.data[0] = 6; // Length: 1 (service) + 1 (PID) + 4 (data)
            tx_frame.data_length_code = 1 + 6;
            tx_frame.data[3] = mil_dtc_count;
            tx_frame.data[4] = 0x00; // Byte B (Tests supported/complete - simplified)
            tx_frame.data[5] = 0x00; // Byte C
            tx_frame.data[6] = 0x00; // Byte D
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Monitor Status (PID 0x01)");
            break;
        }
        case 0x0A: { // Fuel Pressure
            // Формула: A * 3 (kPa) -> A = val / 3
            int val = fuel_pressure / 3;
            tx_frame.data[0] = 3; 
            tx_frame.data_length_code = 1 + 3;
            tx_frame.data[3] = (byte)constrain(val, 0, 255);
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Fuel Pressure data");
            break;
        }
        case 0x0C: { // Engine RPM
            // Формула: (A*256+B)/4
            int rpm_value = engine_rpm * 4; 
            tx_frame.data[0] = 4; // Length: 1 (service) + 1 (PID) + 2 (data)
            tx_frame.data_length_code = 1 + 4;
            tx_frame.data[3] = highByte(rpm_value);
            tx_frame.data[4] = lowByte(rpm_value);
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent RPM data");
            break;
        }
        case 0x0E: { // Timing Advance
            // Формула: (A-128)/2 => A = (val * 2) + 128
            int val = (int)((timing_advance * 2) + 128);
            tx_frame.data[0] = 3; 
            tx_frame.data_length_code = 1 + 3;
            tx_frame.data[3] = (byte)constrain(val, 0, 255);
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Timing Advance data");
            break;
        }
        case 0x05: { // Engine Coolant Temperature
            // Формула: A-40
            int temp_value = engine_temp + 40; 
            tx_frame.data[0] = 3; // Length: 1 (service) + 1 (PID) + 1 (data)
            tx_frame.data_length_code = 1 + 3;
            tx_frame.data[3] = temp_value;
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Engine Temp data");
            break;
        }
        case 0x0D: { // Vehicle Speed
            // Формула: A
            tx_frame.data[0] = 3; // Length: 1 (service) + 1 (PID) + 1 (data)
            tx_frame.data_length_code = 1 + 3;
            tx_frame.data[3] = vehicle_speed;
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Vehicle Speed data");
            break;
        }
        case 0x10: { // MAF air flow rate
            // Формула: (A*256+B)/100
            int maf_value = maf_rate * 100;
            tx_frame.data[0] = 4; // Length: 1 (service) + 1 (PID) + 2 (data)
            tx_frame.data_length_code = 1 + 4;
            tx_frame.data[3] = highByte(maf_value);
            tx_frame.data[4] = lowByte(maf_value);
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent MAF rate data");
            break;
        }
        case 0x20: { // Supported PIDs [21-40]
            uint32_t supported_pids_21_40 = 0;
            supported_pids_21_40 |= (1UL << (32 - (0x2F - 0x20))); // Fuel Level
            supported_pids_21_40 |= (1UL << (32 - (0x31 - 0x20))); // Dist with MIL
            supported_pids_21_40 |= (1UL << (32 - (0x40 - 0x20))); // Support for 41-60

            tx_frame.data[0] = 6; // Length: 1 (service) + 1 (PID) + 4 (data)
            tx_frame.data_length_code = 1 + 6;
            tx_frame.data[3] = (supported_pids_21_40 >> 24) & 0xFF;
            tx_frame.data[4] = (supported_pids_21_40 >> 16) & 0xFF;
            tx_frame.data[5] = (supported_pids_21_40 >> 8) & 0xFF;
            tx_frame.data[6] = supported_pids_21_40 & 0xFF;
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Supported PIDs [21-40] data");
            break;
        }
        case 0x2F: { // Fuel Tank Level Input
            // Формула: 100/255 * A
            byte fuel_value = (fuel_level * 255.0) / 100.0;
            tx_frame.data[0] = 3; // Length: 1 (service) + 1 (PID) + 1 (data)
            tx_frame.data_length_code = 1 + 3;
            tx_frame.data[3] = fuel_value;
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Fuel Level data");
            break;
        }
        case 0x31: { // Distance Traveled with MIL On
            // Формула: A*256 + B
            tx_frame.data[0] = 4; // Length: 1 (service) + 1 (PID) + 2 (data)
            tx_frame.data_length_code = 1 + 4;
            tx_frame.data[3] = highByte(distance_with_mil);
            tx_frame.data[4] = lowByte(distance_with_mil);
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Distance with MIL data");
            break;
        }
        case 0x40: { // Supported PIDs [41-60]
            uint32_t supported_pids_41_60 = 0;
            supported_pids_41_60 |= (1UL << (32 - (0x5E - 0x40))); // Engine Fuel Rate
            supported_pids_41_60 |= (1UL << (32 - (0x60 - 0x40))); // Support for 61-80

            tx_frame.data[0] = 6;
            tx_frame.data_length_code = 1 + 6;
            tx_frame.data[3] = (supported_pids_41_60 >> 24) & 0xFF;
            tx_frame.data[4] = (supported_pids_41_60 >> 16) & 0xFF;
            tx_frame.data[5] = (supported_pids_41_60 >> 8) & 0xFF;
            tx_frame.data[6] = supported_pids_41_60 & 0xFF;
            twai_transmit(&tx_frame, portMAX_DELAY);
            break;
        }
        case 0x5E: { // Engine Fuel Rate
            // Формула: ((A*256)+B)/20 L/h => val = rate * 20
            int val = (int)(fuel_rate * 20);
            tx_frame.data[0] = 4;
            tx_frame.data_length_code = 1 + 4;
            tx_frame.data[3] = highByte(val);
            tx_frame.data[4] = lowByte(val);
            twai_transmit(&tx_frame, portMAX_DELAY);
            break;
        }
        case 0x60: { // Supported PIDs [61-80]
            uint32_t supported_pids_61_80 = 0;
            // No PIDs supported in this range yet.
            // supported_pids_61_80 |= (1UL << (32 - (0xXX - 0x60))); 

            tx_frame.data[0] = 6;
            tx_frame.data_length_code = 1 + 6;
            tx_frame.data[3] = (supported_pids_61_80 >> 24) & 0xFF;
            tx_frame.data[4] = (supported_pids_61_80 >> 16) & 0xFF;
            tx_frame.data[5] = (supported_pids_61_80 >> 8) & 0xFF;
            tx_frame.data[6] = supported_pids_61_80 & 0xFF;
            twai_transmit(&tx_frame, portMAX_DELAY);
            Serial.println("Sent Supported PIDs [61-80] data");
            break;
        }
    }
}

void sendSupportedPids_09(byte pid) {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8;
    tx_frame.extd = 0;
    
    // Announce support for PIDs 01-20 in service 09
    // We support 0x02 (VIN), 0x04 (CAL ID), 0x06 (CVN)
    uint32_t supported_pids = 0;
    supported_pids |= (1UL << (32 - 0x02)); // VIN
    supported_pids |= (1UL << (32 - 0x04)); // CAL ID
    supported_pids |= (1UL << (32 - 0x06)); // CVN
    
    tx_frame.data[0] = 6; // Length: 1 (service) + 1 (PID) + 4 (data)
    tx_frame.data_length_code = 1 + 6;
    tx_frame.data[1] = 0x49; // Response to service 09
    tx_frame.data[2] = pid;  // PID 0x00
    tx_frame.data[3] = (supported_pids >> 24) & 0xFF; // MSB
    tx_frame.data[4] = (supported_pids >> 16) & 0xFF;
    tx_frame.data[5] = (supported_pids >> 8) & 0xFF;
    tx_frame.data[6] = supported_pids & 0xFF;        // LSB
    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent Supported PIDs [09/01-20] data");
}

void sendCalId(byte pid) {
    // CAL ID is up to 16 bytes. Total data length = 1 (service) + 1 (PID) + 16 = 18 bytes.
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8;
    tx_frame.extd = 0;
    tx_frame.data_length_code = 8;

    // --- First Frame (FF) ---
    const int total_data_len = 1 + 1 + strlen(cal_id);
    tx_frame.data[0] = 0x10 | ((total_data_len >> 8) & 0x0F); // PCI: First Frame
    tx_frame.data[1] = total_data_len & 0xFF;                 // PCI: Length
    tx_frame.data[2] = 0x49; // Response to service 09
    tx_frame.data[3] = pid;  // PID 0x04
    
    // Copy first part of CAL ID
    memcpy(&tx_frame.data[4], cal_id, 4);
    
    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent CAL ID FF");

    // Start state machine for CFs
    isoTpState = ISOTP_SEND_CALID_CF1;
    isoTpNextTime = millis() + ISOTP_DELAY_MS;
}

void sendCvn(byte pid) {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8;
    tx_frame.extd = 0;
    tx_frame.data_length_code = 8;

    tx_frame.data[0] = 1 + 1 + 4; // Length: 1 (service) + 1 (PID) + 4 (CVN)
    tx_frame.data[1] = 0x49;      // Response to service 09
    tx_frame.data[2] = pid;       // PID 0x06

    // Convert CVN hex string to bytes
    long cvn_val = strtol(cvn, NULL, 16);
    tx_frame.data[3] = (cvn_val >> 24) & 0xFF;
    tx_frame.data[4] = (cvn_val >> 16) & 0xFF;
    tx_frame.data[5] = (cvn_val >> 8) & 0xFF;
    tx_frame.data[6] = cvn_val & 0xFF;
    tx_frame.data[7] = 0xAA; // Padding

    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent CVN data (single frame).");
}

void sendDTCs() {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8; // OBD_CAN_ID_RESPONSE;
    tx_frame.extd = 0;

    byte dtc_bytes[10]; // Max 5 DTCs * 2 bytes each
    int byte_count = 0;
    for(int i=0; i<num_dtcs && i < 5; i++) {
        char p_code_char = dtcs[i][0];
        int code_val = atoi(&dtcs[i][1]);
        byte b1 = highByte(code_val);
        byte b2 = lowByte(code_val);
        if (p_code_char == 'P') b1 |= 0x00;
        else if (p_code_char == 'C') b1 |= 0x40;
        else if (p_code_char == 'B') b1 |= 0x80;
        else if (p_code_char == 'U') b1 |= 0xC0;
        
        dtc_bytes[byte_count++] = b1;
        dtc_bytes[byte_count++] = b2;
    }

    tx_frame.data[0] = 2 + byte_count; // Length: 1 (service) + 1 (num_dtcs) + byte_count
    tx_frame.data[1] = 0x43; // Response to service 03
    tx_frame.data[2] = num_dtcs;
    memcpy(&tx_frame.data[3], dtc_bytes, byte_count);
    tx_frame.data_length_code = 3 + byte_count;
    
    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent DTCs");
}

void sendPermanentDTCs() {
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8; // OBD_CAN_ID_RESPONSE;
    tx_frame.extd = 0;

    byte dtc_bytes[10]; // Max 5 DTCs * 2 bytes each
    int byte_count = 0;
    // Використовуємо список постійних помилок
    for(int i=0; i<num_permanent_dtcs && i < 5; i++) {
        char p_code_char = permanent_dtcs[i][0];
        int code_val = atoi(&permanent_dtcs[i][1]);
        byte b1 = highByte(code_val);
        byte b2 = lowByte(code_val);
        if (p_code_char == 'P') b1 |= 0x00;
        else if (p_code_char == 'C') b1 |= 0x40;
        else if (p_code_char == 'B') b1 |= 0x80;
        else if (p_code_char == 'U') b1 |= 0xC0;
        
        dtc_bytes[byte_count++] = b1;
        dtc_bytes[byte_count++] = b2;
    }

    tx_frame.data[0] = 2 + byte_count; // Length: 1 (service) + 1 (num_dtcs) + byte_count
    tx_frame.data[1] = 0x4A; // Відповідь на сервіс 0A
    tx_frame.data[2] = num_permanent_dtcs;
    memcpy(&tx_frame.data[3], dtc_bytes, byte_count);
    tx_frame.data_length_code = 3 + byte_count;
    
    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent Permanent DTCs");
}

void clearDTCs() {
    Serial.println("Received request to clear DTCs (Service 04).");
    
    // Скидаємо коди помилок
    num_dtcs = 0;
    for(int i=0; i<5; i++) {
        dtcs[i][0] = '\0';
    }

    // Скидаємо лічильник пробігу з помилкою
    distance_with_mil = 0;

    // Надсилаємо позитивну відповідь для сервісу 04
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8; // OBD_CAN_ID_RESPONSE
    tx_frame.extd = 0;
    tx_frame.data_length_code = 2;
    tx_frame.data[0] = 0x01; // Довжина відповіді
    tx_frame.data[1] = 0x44; // Позитивна відповідь на сервіс 04
    // Заповнюємо решту нулями
    for(int i=2; i<8; i++) tx_frame.data[i] = 0x00;

    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent Service 04 positive response. DTCs cleared.");

    // Оновлюємо дисплей, щоб показати відсутність помилок
    updateDisplay();
    notifyClients();
}

void sendVIN(byte pid) {
    // Повна реалізація передачі VIN за протоколом ISO-TP (багатокадрові повідомлення)
    twai_message_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.identifier = 0x7E8; // OBD_CAN_ID_RESPONSE;
    tx_frame.extd = 0;
    tx_frame.data_length_code = 8;

    // --- First Frame (FF) ---
    // Загальна довжина даних = 1 (сервіс) + 1 (PID) + 17 (VIN) = 19 байт
    const int total_data_len = 19;
    tx_frame.data[0] = 0x10 | ((total_data_len >> 8) & 0x0F); // PCI: First Frame
    tx_frame.data[1] = total_data_len & 0xFF;                 // PCI: Довжина
    tx_frame.data[2] = 0x40 + 0x09; // Відповідь на сервіс 09
    tx_frame.data[3] = pid;         // PID 0x02
    tx_frame.data[4] = vin[0];
    tx_frame.data[5] = vin[1];
    tx_frame.data[6] = vin[2];
    tx_frame.data[7] = vin[3];
    twai_transmit(&tx_frame, portMAX_DELAY);
    Serial.println("Sent VIN FF");

    // Ініціалізуємо машину станів для відправки наступних кадрів без блокування
    isoTpState = ISOTP_SEND_VIN_CF1;
    isoTpNextTime = millis() + ISOTP_DELAY_MS;
}

void processIsoTp() {
    if (isoTpState == ISOTP_IDLE) return;

    if (millis() >= isoTpNextTime) {
        twai_message_t tx_frame;
        memset(&tx_frame, 0, sizeof(tx_frame));
        tx_frame.identifier = 0x7E8;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 8;

        switch (isoTpState) {
            case ISOTP_SEND_VIN_CF1: {
                byte sequence_number = 1;
                tx_frame.data[0] = 0x20 | sequence_number; // PCI: Consecutive Frame, SN=1
                memcpy(&tx_frame.data[1], &vin[4], 7);
                twai_transmit(&tx_frame, portMAX_DELAY);
                Serial.println("Sent VIN CF1");
                
                isoTpState = ISOTP_SEND_VIN_CF2;
                isoTpNextTime = millis() + ISOTP_DELAY_MS;
                break;
            }
            case ISOTP_SEND_VIN_CF2: {
                byte sequence_number = 2;
                tx_frame.data[0] = 0x20 | sequence_number; // PCI: Consecutive Frame, SN=2
                memcpy(&tx_frame.data[1], &vin[11], 6);
                tx_frame.data[7] = 0xAA; // Padding
                twai_transmit(&tx_frame, portMAX_DELAY);
                Serial.println("Sent VIN CF2");
                Serial.println("Sent full VIN via ISO-TP.");
                
                isoTpState = ISOTP_IDLE;
                break;
            }
            case ISOTP_SEND_CALID_CF1: {
                byte sequence_number = 1;
                tx_frame.data[0] = 0x20 | sequence_number;
                memcpy(&tx_frame.data[1], &cal_id[4], 7);
                twai_transmit(&tx_frame, portMAX_DELAY);
                Serial.println("Sent CAL ID CF1");

                isoTpState = ISOTP_SEND_CALID_CF2;
                isoTpNextTime = millis() + ISOTP_DELAY_MS;
                break;
            }
            case ISOTP_SEND_CALID_CF2: {
                byte sequence_number = 2;
                tx_frame.data[0] = 0x20 | sequence_number;
                int remaining = strlen(cal_id) - 11;
                if (remaining > 0) memcpy(&tx_frame.data[1], &cal_id[11], remaining);
                for (int i = 1 + remaining; i < 8; i++) tx_frame.data[i] = 0xAA;
                twai_transmit(&tx_frame, portMAX_DELAY);
                Serial.println("Sent CAL ID CF2");
                Serial.println("Sent full CAL ID via ISO-TP.");

                isoTpState = ISOTP_IDLE;
                break;
            }
        }
    }
}