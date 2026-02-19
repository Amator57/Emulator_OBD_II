#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/twai.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "shared.h"
// #include "web_page.h" // Moved to web_server.cpp

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

const int CYCLES_THRESHOLD = 3;

// --- Emulator Modes ---
EmulatorMode emulatorMode = MODE_OBD_11BIT;
int canBitrate = 500000;

// ############## Налаштування Wi-Fi та веб-сервера ##############
const char* ap_ssid = "OBD-II-Emulator-A";
const char* ap_password = "123456789";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Preferences preferences;
// --- Global simulation state (controlled by web UI) ---
bool dynamic_rpm_enabled = false;
bool misfire_simulation_enabled = false;
bool lean_mixture_simulation_enabled = false;
int frame_delay_ms = 0; // Frame delay injection
int error_injection_rate = 0; // 0-100% probability of dropping a frame
bool fault_incorrect_sequence = false;
bool fault_silent_mode = false;
bool fault_multiple_responses = false;
bool fault_stmin_overflow = false;
bool fault_wrong_flow_control = false;
bool fault_partial_vin = false;

// --- Function Prototypes (from other modules) ---
bool initCAN(int bitrate);
void setupEcus();
void setupWebServer();
void clearDTCs(ECU &ecu, bool use29bit);
bool addDTC(ECU &ecu, const char* new_dtc);
void completeDrivingCycle(ECU &ecu, bool use29bit);
void parseJsonConfig(String &json_buffer);
void saveConfig();
void loadConfig();


void setup() {
  Serial.begin(115200);
  Serial.println("OBD-II Emulator-A Starting...");

  setupEcus();
  
  loadConfig(); // Завантажуємо збережену конфігурацію при старті

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
  if (initCAN(canBitrate)) Serial.println("TWAI (CAN) bus initialized.");
  else Serial.println("Failed to init TWAI driver");
  isotp_init();

  setupWebServer();

  Serial.println("Web server started.");
  delay(1000); // Затримка, щоб побачити стартові повідомлення
  updateDisplay(); // Перше оновлення екрану з початковими даними
}

void loop() {
  // --- Auto-detect Logic ---
  if (emulatorMode == MODE_AUTODETECT) {
      static unsigned long last_switch = 0;
      static int detect_stage = 0; // 0=500k, 1=250k
      
      // Simple logic: switch bitrate every 2 seconds if no messages received
      // In a real scenario, we would listen for valid frames.
      // Since we are an emulator (server), we usually wait for requests.
      // If we receive ANY valid frame, we lock.
      
      twai_message_t rx_frame;
      if (twai_receive(&rx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
          // Received a frame! Lock this bitrate?
          // For now, just process it.
          isotp_on_frame(rx_frame);
      } else {
          if (millis() - last_switch > 2000) {
              detect_stage = !detect_stage;
              canBitrate = (detect_stage == 0) ? 500000 : 250000;
              initCAN(canBitrate);
              Serial.printf("Auto-detect: Switching to %d bps\n", canBitrate);
              updateDisplay();
              last_switch = millis();
          }
      }
      // Continue simulation logic below...
  } else {
      // Normal Mode
  twai_message_t rx_frame;
  if (twai_receive(&rx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
      logCAN(rx_frame, true); // Log RX
      isotp_on_frame(rx_frame);
  }

  isotp_poll(); // Обробка черги ISO-TP (без delay)

  uint32_t rxId = 0;
  uint8_t rxData[ISOTP_MAX_DATA_LEN] = {0};
  uint16_t rxLen = 0;
  if (isotp_get_message(&rxId, rxData, &rxLen)) {
      handleOBDRequest(rxId, rxData, rxLen);
  }

  // --- Simulation Logic (affects ECM - ecus[0]) ---
  // Емуляція динамічної зміни RPM (синусоїда)
  if (dynamic_rpm_enabled) {
      unsigned long now = millis();
      // Синусоїда: Центр 2500, Амплітуда 1500 (від 1000 до 4000), Період ~5 секунд
      ecus[0].engine_rpm = 2500 + 1500 * sin(2 * PI * now / 5000.0);
      // Sync other ECUs to the same RPM
      ecus[1].engine_rpm = ecus[0].engine_rpm;
      
      // Динамічна зміна інших параметрів для демонстрації графіків
      ecus[0].maf_rate = 5.0 + (ecus[0].engine_rpm / 100.0); // Приблизна залежність
      ecus[0].fuel_rate = 0.5 + (ecus[0].engine_rpm / 1000.0); // Приблизна залежність
      if (!lean_mixture_simulation_enabled) ecus[0].fuel_pressure = 350 + (ecus[0].engine_rpm / 50); // Нормальна робота

      // --- TCM Simulation (Automatic Gear Shifting) ---
      // Проста логіка АКПП залежно від швидкості
      if (ecus[0].vehicle_speed < 10) ecus[1].current_gear = 1;
      else if (ecus[0].vehicle_speed < 30) ecus[1].current_gear = 2;
      else if (ecus[0].vehicle_speed < 50) ecus[1].current_gear = 3;
      else if (ecus[0].vehicle_speed < 80) ecus[1].current_gear = 4;
      else if (ecus[0].vehicle_speed < 110) ecus[1].current_gear = 5;
      else ecus[1].current_gear = 6;

      // Симуляція пробігу з помилкою
      if (ecus[0].num_dtcs > 0) {
          static unsigned long last_dist_update = 0;
          if (now - last_dist_update > 5000) { // Додаємо 1 км кожні 5с для демонстрації
              ecus[0].distance_with_mil++;
              last_dist_update = now;
          }
      }

      if (misfire_simulation_enabled && ecus[0].engine_rpm > 3500) {
          // Якщо додали новий DTC, одразу оновлюємо інтерфейси
          if (addDTC(ecus[0], "P0300")) {
              notifyClients();
              updateDisplay();
          }
      }

      // Емуляція бідної суміші (P0171) при низькому тиску пального
      if (lean_mixture_simulation_enabled) {
          ecus[0].fuel_pressure = 150 + (rand() % 30); // Імітуємо падіння тиску до ~165 kPa
          // Якщо тиск низький (< 200 kPa) і є навантаження (RPM > 2000)
          if (ecus[0].fuel_pressure < 200 && ecus[0].engine_rpm > 2000) {
              if (addDTC(ecus[0], "P0171")) {
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
}

void updateDisplay() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  
  tft.setTextColor(ST7735_CYAN);
  tft.print("MODE: ");
  if (emulatorMode == MODE_OBD_11BIT) tft.println("OBD 11b");
  else if (emulatorMode == MODE_UDS_29BIT) tft.println("UDS 29b");
  else if (emulatorMode == MODE_HYBRID) tft.println("HYBRID");
  else tft.println("AUTO");
  
  tft.print("CAN: "); tft.print(canBitrate/1000); tft.println("k");
  tft.println("---------------------");
  
  tft.setTextColor(ST7735_WHITE);
  tft.print("VIN: ");
  tft.println(ecus[0].vin);
  tft.println(""); // Spacer

  char buf1[15], buf2[15];

  // Line 1: RPM & Speed
  snprintf(buf1, sizeof(buf1), "RPM: %d", ecus[0].engine_rpm);
  snprintf(buf2, sizeof(buf2), "Speed: %d", ecus[0].vehicle_speed);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 2: Temp & MAF
  snprintf(buf1, sizeof(buf1), "Temp: %dC", ecus[0].engine_temp);
  snprintf(buf2, sizeof(buf2), "MAF: %.1f", ecus[0].maf_rate);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 3: Fuel & Voltage
  snprintf(buf1, sizeof(buf1), "Fuel: %.0f%%", ecus[0].fuel_level);
  snprintf(buf2, sizeof(buf2), "Volt: %.1f", ecus[0].battery_voltage);
  tft.printf("%-14s%s\n", buf1, buf2);

  // Line 4: Dist MIL & Cycles
  snprintf(buf1, sizeof(buf1), "MIL km: %d", ecus[0].distance_with_mil);
  snprintf(buf2, sizeof(buf2), "Cyc: %d/%d", ecus[0].error_free_cycles, CYCLES_THRESHOLD);
  tft.printf("%-14s%s\n", buf1, buf2);

  tft.println(""); // Spacer

  tft.println("DTCs:");
  if (ecus[0].num_dtcs > 0) {
    tft.setTextColor(ST7735_RED);
    String dtc_line = "";
    for(int i=0; i<ecus[0].num_dtcs; i++) {
        dtc_line += String(ecus[0].dtcs[i]) + " ";
    }
    tft.println(dtc_line);
  } else {
    tft.setTextColor(ST7735_GREEN);
    tft.println("  None");
  }
}

// Допоміжна функція для додавання DTC, якщо він ще не існує
bool addDTC(ECU &ecu, const char* new_dtc) {
    bool added_to_current = false;
    bool added_to_permanent = false;

    // Додаємо до поточних DTC, якщо є місце і код ще не існує
    if (ecu.num_dtcs < 5) {
        bool exists = false;
        for (int i = 0; i < ecu.num_dtcs; i++) {
            if (strcmp(ecu.dtcs[i], new_dtc) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            strncpy(ecu.dtcs[ecu.num_dtcs], new_dtc, 5);
            ecu.dtcs[ecu.num_dtcs][5] = '\0';
            ecu.num_dtcs++;
            added_to_current = true;
        }
    }

    // Додаємо до постійних DTC, якщо є місце і код ще не існує
    if (ecu.num_permanent_dtcs < 5) {
        bool exists = false;
        for (int i = 0; i < ecu.num_permanent_dtcs; i++) {
            if (strcmp(ecu.permanent_dtcs[i], new_dtc) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            strncpy(ecu.permanent_dtcs[ecu.num_permanent_dtcs], new_dtc, 5);
            ecu.permanent_dtcs[ecu.num_permanent_dtcs][5] = '\0';
            ecu.num_permanent_dtcs++;
            added_to_permanent = true;
        }
    }

    if (added_to_current && !ecu.freezeFrameSet) {
        ecu.freezeFrame.rpm = ecu.engine_rpm;
        ecu.freezeFrame.speed = ecu.vehicle_speed;
        ecu.freezeFrame.temp = ecu.engine_temp;
        ecu.freezeFrame.maf = ecu.maf_rate;
        ecu.freezeFrame.fuel_pressure = ecu.fuel_pressure;
        ecu.freezeFrameSet = true;
        Serial.println("Freeze Frame captured.");
    }

    if (added_to_current || added_to_permanent) {
        Serial.printf("Misfire detected! Added DTC: %s. Current: %d, Permanent: %d\n", new_dtc, ecu.num_dtcs, ecu.num_permanent_dtcs);
        return true; // Повертаємо true, якщо код було додано хоча б до одного списку
    }
    return false;
}

void completeDrivingCycle(ECU &ecu, bool use29bit) {
    Serial.println("Simulating Driving Cycle...");
    if (ecu.num_dtcs == 0) {
        ecu.error_free_cycles++;
        Serial.printf("  No current DTCs. Error-free cycles: %d/%d\n", ecu.error_free_cycles, CYCLES_THRESHOLD);
        
        if (ecu.error_free_cycles >= CYCLES_THRESHOLD) {
            if (ecu.num_permanent_dtcs > 0) {
                ecu.num_permanent_dtcs = 0;
                for(int i=0; i<5; i++) ecu.permanent_dtcs[i][0] = '\0';
                Serial.println("  Threshold reached! Permanent DTCs cleared.");
            }
            // Скидаємо лічильник після успішного очищення (або можна залишити, щоб показувати "здоров'я")
            // ecu.error_free_cycles = 0; 
        }
    } else {
        ecu.error_free_cycles = 0;
        Serial.println("  Current DTCs present. Error-free cycles counter reset to 0.");
    }
    updateDisplay();
    notifyClients();
}

String getJsonConfig() {
    DynamicJsonDocument doc(2048);

    // Only saving config for the main ECU (ecus[0]) for now
    doc["vin"] = ecus[0].vin;
    doc["cal_id"] = ecus[0].cal_id;
    doc["cvn"] = ecus[0].cvn;
    doc["engine_temp"] = ecus[0].engine_temp;
    doc["engine_rpm"] = ecus[0].engine_rpm;
    doc["vehicle_speed"] = ecus[0].vehicle_speed;
    doc["maf_rate"] = ecus[0].maf_rate;
    doc["timing_advance"] = ecus[0].timing_advance;
    doc["fuel_rate"] = ecus[0].fuel_rate;
    doc["fuel_pressure"] = ecus[0].fuel_pressure;
    doc["fuel_level"] = ecus[0].fuel_level;
    doc["dist_mil"] = ecus[0].distance_with_mil;
    doc["voltage"] = ecus[0].battery_voltage;
    doc["dynamic_rpm_enabled"] = dynamic_rpm_enabled;
    doc["misfire_simulation_enabled"] = misfire_simulation_enabled;
    doc["lean_mixture_simulation_enabled"] = lean_mixture_simulation_enabled;

    doc["mode"] = (int)emulatorMode;
    doc["bitrate"] = canBitrate;
    doc["frame_delay"] = frame_delay_ms;
    doc["error_rate"] = error_injection_rate;

    JsonArray dtcs_array = doc.createNestedArray("dtcs");
    for (int i = 0; i < ecus[0].num_dtcs; i++) {
        dtcs_array.add(ecus[0].dtcs[i]);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void parseJsonConfig(String &json_buffer) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, json_buffer);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    // Parse and update global variables for the main ECU (ecus[0])
    strncpy(ecus[0].vin, doc["vin"] | "VIN_NOT_SET", 17);
    strncpy(ecus[0].cal_id, doc["cal_id"] | "EMULATOR_CAL_ID", 16);
    strncpy(ecus[0].cvn, doc["cvn"] | "A1B2C3D4", 8);

    ecus[0].engine_temp = doc["engine_temp"] | 90;
    ecus[0].engine_rpm = doc["engine_rpm"] | 1500;
    ecus[0].vehicle_speed = doc["vehicle_speed"] | 60;
    ecus[0].maf_rate = doc["maf_rate"] | 10.0;
    ecus[0].timing_advance = doc["timing_advance"] | 5.0;
    ecus[0].fuel_rate = doc["fuel_rate"] | 1.5;
    ecus[0].fuel_pressure = doc["fuel_pressure"] | 350;
    ecus[0].fuel_level = doc["fuel_level"] | 75.0;
    ecus[0].distance_with_mil = doc["dist_mil"] | 0;
    ecus[0].battery_voltage = doc["voltage"] | 14.2;

    // TCM
    ecus[1].current_gear = doc["tcm_gear"] | 1;

    dynamic_rpm_enabled = doc["dynamic_rpm_enabled"] | false;
    misfire_simulation_enabled = doc["misfire_simulation_enabled"] | false;
    lean_mixture_simulation_enabled = doc["lean_mixture_simulation_enabled"] | false;

    emulatorMode = (EmulatorMode)(doc["mode"] | (int)MODE_OBD_11BIT);
    canBitrate = doc["bitrate"] | 500000;
    frame_delay_ms = doc["frame_delay"] | 0;
    error_injection_rate = doc["error_rate"] | 0;

    // Clear existing DTCs before loading new ones
    clearDTCs(ecus[0], false);

    if (doc.containsKey("dtcs")) {
        JsonArray dtcs_array = doc["dtcs"].as<JsonArray>();
        for(JsonVariant v : dtcs_array) {
            const char* dtc_str = v.as<const char*>();
            addDTC(ecus[0], dtc_str); // Use addDTC to correctly set freeze frame
        }
    }

    Serial.println("Configuration loaded from JSON.");
    updateDisplay();
    notifyClients();
}

void saveConfig() {
    preferences.begin("obd-config", false); // R/W mode

    // --- Global Sim State ---
    preferences.putBool("dynRpm", dynamic_rpm_enabled);
    preferences.putBool("misfireSim", misfire_simulation_enabled);
    preferences.putBool("leanSim", lean_mixture_simulation_enabled);
    
    preferences.putInt("mode", (int)emulatorMode);
    preferences.putInt("bitrate", canBitrate);
    preferences.putInt("fDelay", frame_delay_ms);
    preferences.putInt("errRate", error_injection_rate);

    // --- ECM Data (ecus[0]) ---
    preferences.putString("vin", ecus[0].vin);
    preferences.putString("calId", ecus[0].cal_id);
    preferences.putString("cvn", ecus[0].cvn);
    preferences.putInt("temp", ecus[0].engine_temp);
    preferences.putInt("rpm", ecus[0].engine_rpm);
    preferences.putInt("speed", ecus[0].vehicle_speed);
    preferences.putFloat("maf", ecus[0].maf_rate);
    preferences.putFloat("timing", ecus[0].timing_advance);
    preferences.putFloat("fuelRate", ecus[0].fuel_rate);
    preferences.putInt("fuelPres", ecus[0].fuel_pressure);
    preferences.putFloat("fuelLvl", ecus[0].fuel_level);
    preferences.putInt("distMil", ecus[0].distance_with_mil);
    preferences.putFloat("voltage", ecus[0].battery_voltage);

    // Save DTCs as a string
    String dtcString = "";
    for (int i = 0; i < ecus[0].num_dtcs; i++) {
        dtcString += ecus[0].dtcs[i];
        if (i < ecus[0].num_dtcs - 1) {
            dtcString += ",";
        }
    }
    preferences.putString("dtcs", dtcString);

    // --- TCM Data (ecus[1]) ---
    preferences.putInt("gear", ecus[1].current_gear);
    
    preferences.putBool("configSaved", true); // Magic key to indicate that config exists
    preferences.end();
    Serial.println("Configuration saved to NVS.");
}

void loadConfig() {
    preferences.begin("obd-config", true); // Read-only mode
    
    if (!preferences.getBool("configSaved", false)) {
        Serial.println("No saved configuration found in NVS. Using defaults.");
        preferences.end();
        return;
    }

    Serial.println("Loading configuration from NVS...");

    // --- Global Sim State ---
    dynamic_rpm_enabled = preferences.getBool("dynRpm", false);
    misfire_simulation_enabled = preferences.getBool("misfireSim", false);
    lean_mixture_simulation_enabled = preferences.getBool("leanSim", false);
    
    emulatorMode = (EmulatorMode)preferences.getInt("mode", (int)MODE_OBD_11BIT);
    canBitrate = preferences.getInt("bitrate", 500000);
    frame_delay_ms = preferences.getInt("fDelay", 0);
    error_injection_rate = preferences.getInt("errRate", 0);

    // --- ECM Data (ecus[0]) ---
    strncpy(ecus[0].vin, preferences.getString("vin", "NVS_VIN_ECM").c_str(), 17);
    strncpy(ecus[0].cal_id, preferences.getString("calId", "NVS_CALID_ECM").c_str(), 16);
    strncpy(ecus[0].cvn, preferences.getString("cvn", "NVSE1E2E3").c_str(), 8);
    ecus[0].engine_temp = preferences.getInt("temp", 90);
    ecus[0].engine_rpm = preferences.getInt("rpm", 800);
    ecus[0].vehicle_speed = preferences.getInt("speed", 0);
    ecus[0].maf_rate = preferences.getFloat("maf", 2.5);
    ecus[0].timing_advance = preferences.getFloat("timing", 10.0);
    ecus[0].fuel_rate = preferences.getFloat("fuelRate", 0.8);
    ecus[0].fuel_pressure = preferences.getInt("fuelPres", 350);
    ecus[0].fuel_level = preferences.getFloat("fuelLvl", 50.0);
    ecus[0].distance_with_mil = preferences.getInt("distMil", 0);
    ecus[0].battery_voltage = preferences.getFloat("voltage", 12.5);

    // Load and parse DTCs
    String dtcString = preferences.getString("dtcs", "");
    ecus[0].num_dtcs = 0; // Reset before loading
    ecus[0].freezeFrameSet = false;
    if (dtcString.length() > 0) {
        int start = 0;
        while(ecus[0].num_dtcs < 5){
            int comma = dtcString.indexOf(',', start);
            String token;
            if(comma == -1) {
                token = dtcString.substring(start);
            } else {
                token = dtcString.substring(start, comma);
            }
            token.trim();
            if(token.length() == 5){
                addDTC(ecus[0], token.c_str()); // This will also set freeze frame if it's the first DTC
            }
            if(comma == -1) break;
            start = comma + 1;
        }
    }
    // Sync permanent DTCs on load
    ecus[0].num_permanent_dtcs = ecus[0].num_dtcs;
    for(int i=0; i<ecus[0].num_dtcs; i++) {
        strcpy(ecus[0].permanent_dtcs[i], ecus[0].dtcs[i]);
    }

    // --- TCM Data (ecus[1]) ---
    ecus[1].current_gear = preferences.getInt("gear", 0);

    preferences.end();
}