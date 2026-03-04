#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/twai.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/semphr.h>

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
String wifi_ssid = "";
String wifi_password = "";
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
bool simulation_running = true; // Прапорець для контролю початку обміну (true за замовчуванням)
volatile bool need_display_update = false; // Ініціалізація прапорця
volatile bool need_can_reinit = false;     // Прапорець для безпечного перезапуску CAN
volatile bool need_websocket_update = false; // Прапорець для оновлення клієнтів
volatile bool need_clear_dtcs = false;       // Прапорець для очищення помилок
volatile bool need_drive_cycle = false;      // Прапорець для циклу водіння
volatile bool need_load_config = false;      // Прапорець для завантаження конфігу
volatile bool need_save_config = false;      // Прапорець для збереження конфігу
String pending_config_json;                  // Буфер для JSON конфігурації
SemaphoreHandle_t configMutex = NULL;      // М'ютекс для захисту даних

int current_display_page = 0; // 0=General, 1=PIDs, 2=Mode06, 3=TCM, 4=Faults

// --- Function Prototypes (from other modules) ---
bool initCAN(int bitrate);
void pollCANStatus();
void setupEcus();
void setupWebServer();
void clearDTCs(ECU &ecu, bool use29bit, bool sendResponse);
bool addDTC(ECU &ecu, const char* new_dtc);
void completeDrivingCycle(ECU &ecu, bool use29bit);
void parseJsonConfig(String &json_buffer);
void saveConfig();
void loadConfig();
void logCAN(const twai_message_t& frame, bool rx);
void notifyClients();
void handleOBDRequest(uint32_t id, const uint8_t* data, uint16_t len);
void isotp_init();
void isotp_on_frame(const twai_message_t& frame);
void isotp_poll();
bool isotp_get_message(uint32_t* id, uint8_t* data, uint16_t* len);
void saveWifi(String ssid, String pass);
extern String getJsonState(); // Оголошуємо зовнішню функцію для генерації JSON



void setup() {
  Serial.begin(115200);
  Serial.println("OBD-II Emulator-A Starting...");
  configMutex = xSemaphoreCreateMutex(); // Створюємо м'ютекс

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

  // --- Налаштування CAN ---
  // Ініціалізуємо CAN перед Wi-Fi, щоб емулятор міг відповідати сканеру, поки йде підключення до мережі
  if (initCAN(canBitrate)) Serial.println("TWAI (CAN) bus initialized.");
  else Serial.println("Failed to init TWAI driver");
  isotp_init();

  // --- Налаштування Wi-Fi (STA або AP) ---
  bool connected = false;
  if (wifi_ssid.length() > 0) {
      Serial.printf("Connecting to WiFi: %s\n", wifi_ssid.c_str());
      tft.println("Connecting WiFi...");
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false); // Вимикаємо режим сну Wi-Fi, щоб уникнути проблем з CAN
      WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
      
      unsigned long connect_start = millis();
      Serial.print("Connecting");
      while (WiFi.status() != WL_CONNECTED && millis() - connect_start < 10000) { // 10 секунд таймаут
          Serial.print(".");
          // Обслуговуємо CAN шину, поки чекаємо на підключення до Wi-Fi, щоб уникнути тайм-ауту сканера
          twai_message_t rx_frame;
          if (twai_receive(&rx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
              isotp_on_frame(rx_frame);
          }
          isotp_poll(); // Обробка черги ISO-TP
          uint32_t rxId = 0;
          uint8_t rxData[ISOTP_MAX_DATA_LEN] = {0};
          uint16_t rxLen = 0;
          if (isotp_get_message(&rxId, rxData, &rxLen)) {
              handleOBDRequest(rxId, rxData, rxLen);
          }
      }

      if (WiFi.status() == WL_CONNECTED) {
          connected = true;
          Serial.println("\nWiFi Connected!");
          Serial.print("IP: "); Serial.println(WiFi.localIP());
          tft.println("Mode: STA");
          tft.print("IP: "); 
          tft.println(WiFi.localIP());
      } else {
          Serial.println("\nConnection failed. Falling back to AP mode.");
      }
  }

  if (!connected) {
      Serial.print("Creating Access Point...");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_password);
      IPAddress ip = WiFi.softAPIP();
      Serial.print(" AP IP: "); Serial.println(ip);
      tft.println("Mode: AP");
      tft.print("AP IP: "); tft.println(ip);
  }

  setupWebServer();

  Serial.println("Web server started.");
  delay(1000); // Зменшено затримку, щоб прискорити старт
  // updateDisplay(); // Оновлення дисплея тепер не потрібне, все робиться вище
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
              // updateDisplay();
              last_switch = millis();
          }
      }
      // Continue simulation logic below...
  } else {
      // Normal Mode

  // --- Безпечний перезапуск CAN з основного потоку ---
  if (need_can_reinit) {
      if (initCAN(canBitrate)) {
          Serial.println("TWAI (CAN) re-initialized via Web Request.");
          isotp_init(); // Обов'язково скидаємо ISO-TP після перезапуску драйвера
      }
      need_can_reinit = false;
  }

  twai_message_t rx_frame;
  if (twai_receive(&rx_frame, pdMS_TO_TICKS(10)) == ESP_OK) {
      logCAN(rx_frame, true); // Log RX
      isotp_on_frame(rx_frame);
  }

  pollCANStatus(); // Автоматичне відновлення шини при помилках (має працювати завжди)

  isotp_poll(); // Обробка черги ISO-TP (без delay)

  uint32_t rxId = 0;
  uint8_t rxData[ISOTP_MAX_DATA_LEN] = {0};
  uint16_t rxLen = 0;
  String jsonToSend = ""; // Буфер для JSON, який буде відправлено
  static unsigned long last_ws_update = 0; // Обмежувач частоти оновлення WebSocket

  // Захищена секція: обробка даних, які можуть змінюватися веб-сервером
  if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      if (isotp_get_message(&rxId, rxData, &rxLen)) {
          handleOBDRequest(rxId, rxData, rxLen);
      }

      // --- Відкладені дії з веб-інтерфейсу (виконуються в безпечному контексті) ---
      if (need_load_config) {
          parseJsonConfig(pending_config_json);
          pending_config_json = "";
          need_load_config = false;
      }
      if (need_save_config) {
          saveConfig();
          need_save_config = false;
      }
      if (need_clear_dtcs) {
          clearDTCs(ecus[0], false, false);
          need_clear_dtcs = false;
      }
      if (need_drive_cycle) {
          completeDrivingCycle(ecus[0], false);
          need_drive_cycle = false;
      }
      // Обмежуємо частоту відправки JSON (не частіше 150 мс), щоб не перевантажити мережу
      if (need_websocket_update && (millis() - last_ws_update > 150)) {
          jsonToSend = getJsonState(); 
          need_websocket_update = false;
          last_ws_update = millis();
      }

      // Безпечне оновлення дисплея з основного потоку
      if (need_display_update && isoTpLink.txState == ISOTP_IDLE && isoTpLink.rxState == ISOTP_IDLE) {
          // updateDisplay(); // Оновлення дисплея відключено
          need_display_update = false;
      }

      // --- Simulation Logic (affects ECM - ecus[0]) ---
      if (dynamic_rpm_enabled) {
          unsigned long now = millis();
          ecus[0].engine_rpm = 2500 + 1500 * sin(2 * PI * now / 5000.0);
          ecus[1].engine_rpm = ecus[0].engine_rpm;
          
          ecus[0].maf_rate = 5.0 + (ecus[0].engine_rpm / 100.0);
          ecus[0].fuel_rate = 0.5 + (ecus[0].engine_rpm / 1000.0);
          
          ecus[0].throttle_pos = (ecus[0].engine_rpm - 800.0) / 60.0; 
          if(ecus[0].throttle_pos < 0) ecus[0].throttle_pos = 0;
          if(ecus[0].throttle_pos > 100) ecus[0].throttle_pos = 100;

          ecus[0].engine_load = 15.0 + (ecus[0].throttle_pos * 0.8);
          ecus[0].map_pressure = 30 + (int)(ecus[0].engine_load * 0.7);
          
          ecus[0].o2_voltage = 0.5 + 0.4 * sin(now / 200.0);
          
          if (!lean_mixture_simulation_enabled) ecus[0].fuel_pressure = 350 + (ecus[0].engine_rpm / 50);

          // TCM Simulation
          float gear_ratios[] = {0, 0.008, 0.014, 0.022, 0.030, 0.042, 0.055}; 
          if (ecus[0].engine_rpm > 3500 && ecus[1].current_gear < 6) ecus[1].current_gear++;
          else if (ecus[0].engine_rpm < 1200 && ecus[1].current_gear > 1) ecus[1].current_gear--;
          
          ecus[0].vehicle_speed = ecus[0].engine_rpm * gear_ratios[ecus[1].current_gear];

          if (ecus[0].num_dtcs > 0) {
              static unsigned long last_dist_update = 0;
              if (now - last_dist_update > 5000) {
                  ecus[0].distance_with_mil++;
                  last_dist_update = now;
              }
          }

          if (misfire_simulation_enabled && ecus[0].engine_rpm > 3500) {
              if (addDTC(ecus[0], "P0300")) {
                  notifyClients();
              }
          }

          if (lean_mixture_simulation_enabled) {
              ecus[0].fuel_pressure = 150 + (rand() % 30);
              if (ecus[0].fuel_pressure < 200 && ecus[0].engine_rpm > 2000) {
                  if (addDTC(ecus[0], "P0171")) { notifyClients(); }
              }
          }

          static unsigned long last_notify = 0;
          if (now - last_notify > 500) {
              last_notify = now;
              need_websocket_update = true; // Просто встановлюємо прапорець
          }
      }
      
      xSemaphoreGive(configMutex);
  }

  // Відправка даних клієнтам відбувається ПОЗА межами м'ютекса, щоб уникнути Deadlock
  if (jsonToSend.length() > 0) {
      ws.textAll(jsonToSend);
  }

  ws.cleanupClients();
  }
}

void updateDisplay() {
  // Функція відключена згідно з вимогою. 
  // Вся ініціалізація дисплея відбувається в setup(), подальші оновлення не потрібні.
  return; 
  /*
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);

  tft.setTextColor(ST7735_CYAN);
  if (current_display_page == 0) tft.print("DASHBOARD");
  else if (current_display_page == 1) tft.print("PIDS VIEW");
  else if (current_display_page == 2) tft.print("MODE 06");
  else if (current_display_page == 3) tft.print("TCM INFO");
  else if (current_display_page == 4) tft.print("SYSTEM/FAULTS");
  
  tft.print(" ");
  if (emulatorMode == MODE_OBD_11BIT) tft.println("11b");
  else if (emulatorMode == MODE_UDS_29BIT) tft.println("29b");
  else tft.println("AUTO");
  tft.println("---------------------");
  
  char buf1[20], buf2[20];

  if (current_display_page == 0) {
      // General / Dashboard
      tft.setTextColor(ST7735_WHITE);
      tft.print("VIN: "); tft.println(ecus[0].vin);
      
      snprintf(buf1, sizeof(buf1), "RPM: %d", ecus[0].engine_rpm);
      snprintf(buf2, sizeof(buf2), "Spd: %d", ecus[0].vehicle_speed);
      tft.printf("%-10s%s\n", buf1, buf2);

      snprintf(buf1, sizeof(buf1), "Tmp: %dC", ecus[0].engine_temp);
      snprintf(buf2, sizeof(buf2), "MAF: %.1f", ecus[0].maf_rate);
      tft.printf("%-10s%s\n", buf1, buf2);

      snprintf(buf1, sizeof(buf1), "Ful: %.1f%%", ecus[0].fuel_level);
      snprintf(buf2, sizeof(buf2), "Vlt: %.1f", ecus[0].battery_voltage);
      tft.printf("%-10s%s\n", buf1, buf2);
      
      tft.println("");
      if (ecus[0].num_dtcs > 0) {
          tft.setTextColor(ST7735_RED);
          tft.print("DTCs: ");
          for(int i=0; i<ecus[0].num_dtcs; i++) {
              tft.print(ecus[0].dtcs[i]); tft.print(" ");
          }
          tft.println();
      } else {
          tft.setTextColor(ST7735_GREEN);
          tft.println("No DTCs");
      }
  } 
  else if (current_display_page == 1) {
      // PIDs
      tft.setTextColor(ST7735_WHITE);
      tft.printf("RPM :%-5d   Spd :%d\n", ecus[0].engine_rpm, ecus[0].vehicle_speed);
      tft.printf("Load:%-5.1f%%  Tmp:%dC\n", ecus[0].engine_load, ecus[0].engine_temp);
      tft.printf("MAP :%-3d     IAT :%dC\n", ecus[0].map_pressure, ecus[0].intake_temp);
      tft.printf("MAF :%-5.1f   TPS :%.1f%%\n", ecus[0].maf_rate, ecus[0].throttle_pos);
      tft.printf("ST  :%-5.1f%%  LT  :%.1f%%\n", ecus[0].short_term_fuel_trim, ecus[0].long_term_fuel_trim);
      tft.printf("O2  :%-4.2fV   Tim :%.1f\n", ecus[0].o2_voltage, ecus[0].timing_advance);
      tft.printf("\n");
      tft.printf("Volt:%-4.1fV   Press:%-3dkPa\n",ecus[0].battery_voltage, ecus[0].fuel_pressure);
  }
  else if (current_display_page == 2) {
      // Mode 06
      tft.setTextColor(ST7735_YELLOW);
      tft.println("Mode 06 Tests:");
      tft.setTextColor(ST7735_WHITE);
      for(int i=0; i<2; i++) { // Show first 2 enabled tests
           if(ecus[0].mode06_tests[i].enabled) {
               tft.printf("ID:%02X Val:%d\n", ecus[0].mode06_tests[i].testId, ecus[0].mode06_tests[i].value);
               tft.printf("Min:%d Max:%d\n", ecus[0].mode06_tests[i].min_limit, ecus[0].mode06_tests[i].max_limit);
               tft.println("-");
           }
      }
  }
  else if (current_display_page == 3) {
      // TCM
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(2);
      tft.print("GEAR: "); 
      if(ecus[1].current_gear == 0) tft.println("N");
      else if(ecus[1].current_gear == 255) tft.println("R");
      else tft.println(ecus[1].current_gear);
      tft.setTextSize(1);
  }
  else if (current_display_page == 4) {
      // Faults
      tft.setTextColor(ST7735_RED);
      if(fault_silent_mode) tft.println("[!] Silent Mode");
      if(fault_incorrect_sequence) tft.println("[!] Bad Seq Num");
      if(error_injection_rate > 0) tft.printf("[!] Err Rate: %d%%\n", error_injection_rate);
      
      tft.setTextColor(ST7735_WHITE);
      tft.print("IP: "); 
      tft.println(WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP());
  }
  else if (current_display_page == 5) { // Mode 02 Freeze Frame
      tft.setTextColor(ST7735_YELLOW);
      tft.println("Freeze Frame (Mode 02)");
      tft.println("---------------------");
      tft.setTextColor(ST7735_WHITE);
      if(ecus[0].freezeFrameSet) {
           tft.printf("DTC: %s\n", ecus[0].dtcs[0]);
           tft.printf("RPM: %d\n", ecus[0].freezeFrame.rpm);
           tft.printf("Spd: %d km/h\n", ecus[0].freezeFrame.speed);
           tft.printf("Tmp: %d C\n", ecus[0].freezeFrame.temp);
           tft.printf("MAF: %.1f\n", ecus[0].freezeFrame.maf);
           tft.printf("Prs: %d kPa\n", ecus[0].freezeFrame.fuel_pressure);
      } else {
           tft.println("No Freeze Frame Data");
      }
  }
  else if (current_display_page == 6) { // Mode 03/07/0A DTCs
      tft.setTextColor(ST7735_RED);
      tft.println("DTCs (Mode 03/07/0A)");
      tft.println("---------------------");
      tft.setTextColor(ST7735_WHITE);
      if(ecus[0].num_dtcs > 0) {
          for(int i=0; i<ecus[0].num_dtcs; i++) {
              tft.printf("%d. %s\n", i+1, ecus[0].dtcs[i]);
          }
      } else {
          tft.println("No DTCs");
      }
  }
  else if (current_display_page == 7) { // Mode 09 Vehicle Info
      tft.setTextColor(ST7735_CYAN);
      tft.println("Vehicle Info (Mode 09)");
      tft.println("---------------------");
      tft.setTextColor(ST7735_WHITE);
      tft.println("VIN:"); tft.println(ecus[0].vin);
      tft.println("CAL ID:"); tft.println(ecus[0].cal_id);
      tft.println("CVN:"); tft.println(ecus[0].cvn);
  }
  else if (current_display_page == 8) { // Mode 05
      tft.setTextColor(ST7735_WHITE);
      tft.println("Mode 05");
      tft.println("---------------------");
      tft.println("Not supported in CAN.");
  }
  else if (current_display_page == 9) { // ABS
      tft.setTextColor(ST7735_MAGENTA);
      tft.println("ABS Module (0x7EA)");
      tft.println("---------------------");
      tft.setTextColor(ST7735_WHITE);
      tft.printf("Speed: %d km/h\n", ecus[2].vehicle_speed);
      tft.println("VIN:"); tft.println(ecus[2].vin);
      tft.printf("CalID: %s\n", ecus[2].cal_id);
  }
  else if (current_display_page == 10) { // SRS
      tft.setTextColor(ST7735_MAGENTA);
      tft.println("SRS Module (0x7EB)");
      tft.println("---------------------");
      tft.setTextColor(ST7735_WHITE);
      tft.println("VIN:"); tft.println(ecus[3].vin);
      tft.printf("CalID: %s\n", ecus[3].cal_id);
  }
  */
}

// Допоміжна функція для додавання DTC, якщо він ще не існує
bool addDTC(ECU &ecu, const char* new_dtc) {
    bool added_to_current = false;
    bool added_to_permanent = false;

    // Додаємо до поточних DTC, якщо є місце і код ще не існує
    if (ecu.num_dtcs < 8) {
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
    if (ecu.num_permanent_dtcs < 8) {
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
    bool dtc_added_this_cycle = false;

    // --- Part 1: Simulate high RPM/load conditions to trigger faults ---
    if (misfire_simulation_enabled || lean_mixture_simulation_enabled) {
        int original_rpm = ecu.engine_rpm;
        int original_fuel_pressure = ecu.fuel_pressure;
        
        Serial.println("  - Simulating high load phase to check for faults...");
        ecu.engine_rpm = 4000; // Temporarily set high RPM to trigger simulations

        if (misfire_simulation_enabled) {
            if (addDTC(ecu, "P0300")) {
                Serial.println("    -> Misfire simulation triggered P0300.");
                dtc_added_this_cycle = true;
            }
        }

        if (lean_mixture_simulation_enabled) {
            ecu.fuel_pressure = 150 + (rand() % 30); // Simulate low pressure
            if (addDTC(ecu, "P0171")) {
                Serial.println("    -> Lean mixture simulation triggered P0171.");
                dtc_added_this_cycle = true;
            }
        }

        // Restore original values
        ecu.engine_rpm = original_rpm;
        ecu.fuel_pressure = original_fuel_pressure;
        Serial.println("  - High load phase finished.");
    }

    // --- Part 2: Original logic for clearing permanent DTCs ---
    // This part runs if no DTCs were present *before* this cycle and none were added *during* this cycle.
    if (ecu.num_dtcs == 0) {
        ecu.error_free_cycles++;
        Serial.printf("  No current DTCs. Error-free cycles: %d/%d\n", ecu.error_free_cycles, CYCLES_THRESHOLD);
        
        if (ecu.error_free_cycles >= CYCLES_THRESHOLD) {
            if (ecu.num_permanent_dtcs > 0) {
                ecu.num_permanent_dtcs = 0;
                for(int i=0; i<8; i++) ecu.permanent_dtcs[i][0] = '\0';
                Serial.println("  Threshold reached! Permanent DTCs cleared.");
            }
        }
    } else {
        // If DTCs were present before this cycle, reset the counter.
        if (!dtc_added_this_cycle) {
            ecu.error_free_cycles = 0;
            Serial.println("  Current DTCs were present. Error-free cycles counter reset to 0.");
        }
    }
    notifyClients();
}

String getJsonConfig() {
    DynamicJsonDocument doc(8192);

    // Only saving config for the main ECU (ecus[0]) for now
    doc["vin"] = ecus[0].vin;
    doc["cal_id"] = ecus[0].cal_id;
    doc["cvn"] = ecus[0].cvn;
    doc["engine_temp"] = ecus[0].engine_temp;
    doc["engine_rpm"] = ecus[0].engine_rpm;
    doc["vehicle_speed"] = ecus[0].vehicle_speed;
    doc["maf_rate"] = ecus[0].maf_rate;
    doc["timing_advance"] = ecus[0].timing_advance;
    doc["engine_load"] = ecus[0].engine_load;
    doc["map_pressure"] = ecus[0].map_pressure;
    doc["throttle_pos"] = ecus[0].throttle_pos;
    doc["intake_temp"] = ecus[0].intake_temp;
    doc["fuel_trim_s"] = ecus[0].short_term_fuel_trim;
    doc["fuel_trim_l"] = ecus[0].long_term_fuel_trim;
    doc["fuel_trim_s2"] = ecus[0].short_term_fuel_trim_b2;
    doc["fuel_trim_l2"] = ecus[0].long_term_fuel_trim_b2;
    doc["o2_volts"] = ecus[0].o2_voltage;
    doc["obd_std"] = ecus[0].obd_standard;
    doc["o2_sens"] = ecus[0].o2_sensors_present;

    doc["fuel_rate"] = ecus[0].fuel_rate;
    doc["fuel_pressure"] = ecus[0].fuel_pressure;
    doc["fuel_level"] = ecus[0].fuel_level;
    doc["dist_mil"] = ecus[0].distance_with_mil;
    doc["voltage"] = ecus[0].battery_voltage;
    doc["evap"] = ecus[0].evap_purge;
    doc["egr_cmd"] = ecus[0].commanded_egr;
    doc["egr_err"] = ecus[0].egr_error;
    doc["evap_vp"] = ecus[0].evap_vapor_pressure;
    doc["evap_abs"] = ecus[0].abs_evap_pressure;
    doc["tcm_gear"] = ecus[1].current_gear;
    doc["abs_speed"] = ecus[2].vehicle_speed;
    doc["abs_vin"] = ecus[2].vin;
    doc["srs_vin"] = ecus[3].vin;
    
    doc["fuel_rail_pres_rel"] = ecus[0].fuel_rail_pressure_relative;
    doc["fuel_rail_pres_gauge"] = ecus[0].fuel_rail_pressure_gauge;
    doc["cmd_throttle"] = ecus[0].commanded_throttle_actuator;
    doc["rel_app"] = ecus[0].rel_accel_pedal_pos;
    doc["app_d"] = ecus[0].accel_pedal_pos_d;
    doc["app_e"] = ecus[0].accel_pedal_pos_e;
    doc["time_mil"] = ecus[0].time_run_mil_on;
    doc["time_clear"] = ecus[0].time_since_dtc_cleared;
    doc["amb_temp"] = ecus[0].ambient_temp;
    doc["oil_temp"] = ecus[0].oil_temp;

    doc["cat_b1s1"] = ecus[0].catalyst_temp_b1s1;
    doc["cat_b2s1"] = ecus[0].catalyst_temp_b2s1;
    doc["cat_b1s2"] = ecus[0].catalyst_temp_b1s2;
    doc["cat_b2s2"] = ecus[0].catalyst_temp_b2s2;

    doc["wb_b1s1_l"] = ecus[0].o2_lambda_b1s1; doc["wb_b1s1_c"] = ecus[0].o2_current_b1s1;
    doc["wb_b1s2_l"] = ecus[0].o2_lambda_b1s2; doc["wb_b1s2_c"] = ecus[0].o2_current_b1s2;
    doc["wb_b2s1_l"] = ecus[0].o2_lambda_b2s1; doc["wb_b2s1_c"] = ecus[0].o2_current_b2s1;
    doc["wb_b2s2_l"] = ecus[0].o2_lambda_b2s2; doc["wb_b2s2_c"] = ecus[0].o2_current_b2s2;
    doc["dynamic_rpm_enabled"] = dynamic_rpm_enabled;
    doc["misfire_simulation_enabled"] = misfire_simulation_enabled;
    doc["lean_mixture_simulation_enabled"] = lean_mixture_simulation_enabled;

    doc["mode"] = (int)emulatorMode;
    doc["bitrate"] = canBitrate;
    doc["frame_delay"] = frame_delay_ms;
    doc["error_rate"] = error_injection_rate;

    doc["ff_set"] = ecus[0].freezeFrameSet;
    doc["ff_rpm"] = ecus[0].freezeFrame.rpm;
    doc["ff_speed"] = ecus[0].freezeFrame.speed;
    doc["ff_temp"] = ecus[0].freezeFrame.temp;
    doc["ff_maf"] = ecus[0].freezeFrame.maf;
    doc["ff_pres"] = ecus[0].freezeFrame.fuel_pressure;

    JsonArray dtcs_array = doc.createNestedArray("dtcs");
    for (int i = 0; i < ecus[0].num_dtcs; i++) {
        dtcs_array.add(ecus[0].dtcs[i]);
    }
    for (int i = 0; i < ecus[2].num_dtcs; i++) {
        dtcs_array.add(ecus[2].dtcs[i]);
    }
    for (int i = 0; i < ecus[3].num_dtcs; i++) {
        dtcs_array.add(ecus[3].dtcs[i]);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void parseJsonConfig(String &json_buffer) {
    DynamicJsonDocument doc(8192);
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
    ecus[0].engine_load = doc["engine_load"] | 35.0;
    ecus[0].map_pressure = doc["map_pressure"] | 40;
    ecus[0].throttle_pos = doc["throttle_pos"] | 15.0;
    ecus[0].intake_temp = doc["intake_temp"] | 30;
    ecus[0].short_term_fuel_trim = doc["fuel_trim_s"] | 0.0;
    ecus[0].long_term_fuel_trim = doc["fuel_trim_l"] | 2.5;
    ecus[0].short_term_fuel_trim_b2 = doc["fuel_trim_s2"] | 0.0;
    ecus[0].long_term_fuel_trim_b2 = doc["fuel_trim_l2"] | 2.5;
    ecus[0].o2_voltage = doc["o2_volts"] | 0.45;
    ecus[0].obd_standard = doc["obd_std"] | 1;
    ecus[0].o2_sensors_present = doc["o2_sens"] | 0x03;

    ecus[0].fuel_rate = doc["fuel_rate"] | 1.5;
    ecus[0].fuel_pressure = doc["fuel_pressure"] | 350;
    ecus[0].fuel_level = doc["fuel_level"] | 75.0;
    ecus[0].distance_with_mil = doc["dist_mil"] | 0;
    ecus[0].battery_voltage = doc["voltage"] | 14.2;
    ecus[0].evap_purge = doc["evap"] | 0.0;
    ecus[0].commanded_egr = doc["egr_cmd"] | 0.0;
    ecus[0].egr_error = doc["egr_err"] | 0.0;
    ecus[0].evap_vapor_pressure = doc["evap_vp"] | 0;
    ecus[0].abs_evap_pressure = doc["evap_abs"] | 100.0;

    // TCM
    ecus[1].current_gear = doc["tcm_gear"] | 1;

    // ABS & SRS
    ecus[2].vehicle_speed = doc["abs_speed"] | 60;
    strncpy(ecus[2].vin, doc["abs_vin"] | "123EMULATOR001ABS", 17);
    strncpy(ecus[3].vin, doc["srs_vin"] | "123EMULATOR001SRS", 17);
    
    ecus[0].fuel_rail_pressure_relative = doc["fuel_rail_pres_rel"] | 300;
    ecus[0].fuel_rail_pressure_gauge = doc["fuel_rail_pres_gauge"] | 4000;
    ecus[0].commanded_throttle_actuator = doc["cmd_throttle"] | 15.0;
    ecus[0].rel_accel_pedal_pos = doc["rel_app"] | 0.0;
    ecus[0].accel_pedal_pos_d = doc["app_d"] | 15.0;
    ecus[0].accel_pedal_pos_e = doc["app_e"] | 15.0;
    ecus[0].time_run_mil_on = doc["time_mil"] | 0;
    ecus[0].time_since_dtc_cleared = doc["time_clear"] | 0;
    ecus[0].ambient_temp = doc["amb_temp"] | 20;
    ecus[0].oil_temp = doc["oil_temp"] | 85;

    ecus[0].catalyst_temp_b1s1 = doc["cat_b1s1"] | 400.0;
    ecus[0].catalyst_temp_b2s1 = doc["cat_b2s1"] | 400.0;
    ecus[0].catalyst_temp_b1s2 = doc["cat_b1s2"] | 350.0;
    ecus[0].catalyst_temp_b2s2 = doc["cat_b2s2"] | 350.0;

    ecus[0].o2_lambda_b1s1 = doc["wb_b1s1_l"] | 1.0; ecus[0].o2_current_b1s1 = doc["wb_b1s1_c"] | 0.0;
    ecus[0].o2_lambda_b1s2 = doc["wb_b1s2_l"] | 1.0; ecus[0].o2_current_b1s2 = doc["wb_b1s2_c"] | 0.0;
    ecus[0].o2_lambda_b2s1 = doc["wb_b2s1_l"] | 1.0; ecus[0].o2_current_b2s1 = doc["wb_b2s1_c"] | 0.0;
    ecus[0].o2_lambda_b2s2 = doc["wb_b2s2_l"] | 1.0; ecus[0].o2_current_b2s2 = doc["wb_b2s2_c"] | 0.0;

    dynamic_rpm_enabled = doc["dynamic_rpm_enabled"] | false;
    misfire_simulation_enabled = doc["misfire_simulation_enabled"] | false;
    lean_mixture_simulation_enabled = doc["lean_mixture_simulation_enabled"] | false;

    emulatorMode = (EmulatorMode)(doc["mode"] | (int)MODE_OBD_11BIT);
    canBitrate = doc["bitrate"] | 500000;
    frame_delay_ms = doc["frame_delay"] | 0;
    error_injection_rate = doc["error_rate"] | 0;

    // Freeze Frame Data
    ecus[0].freezeFrameSet = doc["ff_set"] | ecus[0].freezeFrameSet;
    ecus[0].freezeFrame.rpm = doc["ff_rpm"] | ecus[0].freezeFrame.rpm;
    ecus[0].freezeFrame.speed = doc["ff_speed"] | ecus[0].freezeFrame.speed;
    ecus[0].freezeFrame.temp = doc["ff_temp"] | ecus[0].freezeFrame.temp;
    ecus[0].freezeFrame.maf = doc["ff_maf"] | ecus[0].freezeFrame.maf;
    ecus[0].freezeFrame.fuel_pressure = doc["ff_pres"] | ecus[0].freezeFrame.fuel_pressure;

    // Clear existing DTCs before loading new ones
    for(int i=0; i<4; i++) {
        clearDTCs(ecus[i], false, false);
    }

    if (doc.containsKey("dtcs")) {
        JsonArray dtcs_array = doc["dtcs"].as<JsonArray>();
        for(JsonVariant v : dtcs_array) {
            const char* dtc_str = v.as<const char*>();
            if (strlen(dtc_str) > 0) {
                char prefix = toupper(dtc_str[0]);
                if (prefix == 'C') addDTC(ecus[2], dtc_str);
                else if (prefix == 'B') addDTC(ecus[3], dtc_str);
                else addDTC(ecus[0], dtc_str);
            }
        }
    }

    simulation_running = true; // Запускаємо обмін зі сканером після отримання конфігу
    Serial.println("Configuration loaded from JSON.");
    notifyClients();
}

void saveWifi(String ssid, String pass) {
    preferences.begin("obd-config", false);
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);
    preferences.putBool("configSaved", true); // Також встановлюємо прапорець, що конфігурація існує
    preferences.end();
    Serial.println("WiFi settings saved to NVS.");
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
    preferences.putFloat("load", ecus[0].engine_load);
    preferences.putInt("map", ecus[0].map_pressure);
    preferences.putFloat("tps", ecus[0].throttle_pos);
    preferences.putInt("iat", ecus[0].intake_temp);
    preferences.putFloat("stft", ecus[0].short_term_fuel_trim);
    preferences.putFloat("ltft", ecus[0].long_term_fuel_trim);
    preferences.putFloat("stft2", ecus[0].short_term_fuel_trim_b2);
    preferences.putFloat("ltft2", ecus[0].long_term_fuel_trim_b2);

    preferences.putInt("obd_std", ecus[0].obd_standard);
    preferences.putInt("o2_sens", ecus[0].o2_sensors_present);
    preferences.putFloat("fuelRate", ecus[0].fuel_rate);
    preferences.putInt("fuelPres", ecus[0].fuel_pressure);
    preferences.putFloat("fuelLvl", ecus[0].fuel_level);
    preferences.putInt("distMil", ecus[0].distance_with_mil);
    preferences.putFloat("voltage", ecus[0].battery_voltage);
    preferences.putFloat("evap", ecus[0].evap_purge);
    preferences.putFloat("egr_cmd", ecus[0].commanded_egr);
    preferences.putFloat("egr_err", ecus[0].egr_error);
    preferences.putInt("evap_vp", ecus[0].evap_vapor_pressure);
    preferences.putFloat("evap_abs", ecus[0].abs_evap_pressure);
    
    preferences.putInt("frp_rel", ecus[0].fuel_rail_pressure_relative);
    preferences.putInt("frp_gauge", ecus[0].fuel_rail_pressure_gauge);
    preferences.putFloat("cmd_throt", ecus[0].commanded_throttle_actuator);
    preferences.putFloat("rel_app", ecus[0].rel_accel_pedal_pos);
    preferences.putFloat("app_d", ecus[0].accel_pedal_pos_d);
    preferences.putFloat("app_e", ecus[0].accel_pedal_pos_e);
    preferences.putInt("time_mil", ecus[0].time_run_mil_on);
    preferences.putInt("time_clear", ecus[0].time_since_dtc_cleared);
    preferences.putInt("amb_temp", ecus[0].ambient_temp);
    preferences.putInt("oil_temp", ecus[0].oil_temp);

    preferences.putFloat("cat_b1s1", ecus[0].catalyst_temp_b1s1);
    preferences.putFloat("cat_b2s1", ecus[0].catalyst_temp_b2s1);
    preferences.putFloat("cat_b1s2", ecus[0].catalyst_temp_b1s2);
    preferences.putFloat("cat_b2s2", ecus[0].catalyst_temp_b2s2);

    preferences.putFloat("wb_b1s1_l", ecus[0].o2_lambda_b1s1); preferences.putFloat("wb_b1s1_c", ecus[0].o2_current_b1s1);
    preferences.putFloat("wb_b1s2_l", ecus[0].o2_lambda_b1s2); preferences.putFloat("wb_b1s2_c", ecus[0].o2_current_b1s2);
    preferences.putFloat("wb_b2s1_l", ecus[0].o2_lambda_b2s1); preferences.putFloat("wb_b2s1_c", ecus[0].o2_current_b2s1);
    preferences.putFloat("wb_b2s2_l", ecus[0].o2_lambda_b2s2); preferences.putFloat("wb_b2s2_c", ecus[0].o2_current_b2s2);

    // --- Freeze Frame ---
    preferences.putBool("ff_set", ecus[0].freezeFrameSet);
    preferences.putInt("ff_rpm", ecus[0].freezeFrame.rpm);
    preferences.putInt("ff_speed", ecus[0].freezeFrame.speed);
    preferences.putInt("ff_temp", ecus[0].freezeFrame.temp);
    preferences.putFloat("ff_maf", ecus[0].freezeFrame.maf);
    preferences.putInt("ff_pres", ecus[0].freezeFrame.fuel_pressure);

    // Save DTCs as a string
    String dtcString = "";
    for (int i = 0; i < ecus[0].num_dtcs; i++) {
        if (dtcString.length() > 0) dtcString += ",";
        dtcString += ecus[0].dtcs[i];
    }
    for (int i = 0; i < ecus[2].num_dtcs; i++) {
        if (dtcString.length() > 0) dtcString += ",";
        dtcString += ecus[2].dtcs[i];
    }
    for (int i = 0; i < ecus[3].num_dtcs; i++) {
        if (dtcString.length() > 0) dtcString += ",";
        dtcString += ecus[3].dtcs[i];
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
    
    // --- WiFi ---
    // Завантажуємо налаштування Wi-Fi в першу чергу, незалежно від іншого конфігу.
    wifi_ssid = preferences.getString("wifi_ssid", "");
    wifi_password = preferences.getString("wifi_pass", "");

    if (!preferences.getBool("configSaved", false)) {
        Serial.println("No full configuration found in NVS. Using defaults for other settings.");
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
    ecus[0].engine_load = preferences.getFloat("load", 35.0);
    ecus[0].map_pressure = preferences.getInt("map", 40);
    ecus[0].throttle_pos = preferences.getFloat("tps", 15.0);
    ecus[0].intake_temp = preferences.getInt("iat", 30);
    ecus[0].short_term_fuel_trim = preferences.getFloat("stft", 0.0);
    ecus[0].long_term_fuel_trim = preferences.getFloat("ltft", 2.5);
    ecus[0].short_term_fuel_trim_b2 = preferences.getFloat("stft2", 0.0);
    ecus[0].long_term_fuel_trim_b2 = preferences.getFloat("ltft2", 2.5);

    ecus[0].obd_standard = preferences.getInt("obd_std", 1);
    ecus[0].o2_sensors_present = preferences.getInt("o2_sens", 0x03);
    ecus[0].fuel_rate = preferences.getFloat("fuelRate", 0.8);
    ecus[0].fuel_pressure = preferences.getInt("fuelPres", 350);
    ecus[0].fuel_level = preferences.getFloat("fuelLvl", 50.0);
    ecus[0].distance_with_mil = preferences.getInt("distMil", 0);
    ecus[0].battery_voltage = preferences.getFloat("voltage", 12.5);
    ecus[0].evap_purge = preferences.getFloat("evap", 0.0);
    ecus[0].commanded_egr = preferences.getFloat("egr_cmd", 0.0);
    ecus[0].egr_error = preferences.getFloat("egr_err", 0.0);
    ecus[0].evap_vapor_pressure = preferences.getInt("evap_vp", 0);
    ecus[0].abs_evap_pressure = preferences.getFloat("evap_abs", 100.0);
    
    ecus[0].fuel_rail_pressure_relative = preferences.getInt("frp_rel", 300);
    ecus[0].fuel_rail_pressure_gauge = preferences.getInt("frp_gauge", 4000);
    ecus[0].commanded_throttle_actuator = preferences.getFloat("cmd_throt", 15.0);
    ecus[0].rel_accel_pedal_pos = preferences.getFloat("rel_app", 0.0);
    ecus[0].accel_pedal_pos_d = preferences.getFloat("app_d", 15.0);
    ecus[0].accel_pedal_pos_e = preferences.getFloat("app_e", 15.0);
    ecus[0].time_run_mil_on = preferences.getInt("time_mil", 0);
    ecus[0].time_since_dtc_cleared = preferences.getInt("time_clear", 0);
    ecus[0].ambient_temp = preferences.getInt("amb_temp", 20);
    ecus[0].oil_temp = preferences.getInt("oil_temp", 85);

    ecus[0].catalyst_temp_b1s1 = preferences.getFloat("cat_b1s1", 400.0);
    ecus[0].catalyst_temp_b2s1 = preferences.getFloat("cat_b2s1", 400.0);
    ecus[0].catalyst_temp_b1s2 = preferences.getFloat("cat_b1s2", 350.0);
    ecus[0].catalyst_temp_b2s2 = preferences.getFloat("cat_b2s2", 350.0);

    ecus[0].o2_lambda_b1s1 = preferences.getFloat("wb_b1s1_l", 1.0); ecus[0].o2_current_b1s1 = preferences.getFloat("wb_b1s1_c", 0.0);
    ecus[0].o2_lambda_b1s2 = preferences.getFloat("wb_b1s2_l", 1.0); ecus[0].o2_current_b1s2 = preferences.getFloat("wb_b1s2_c", 0.0);
    ecus[0].o2_lambda_b2s1 = preferences.getFloat("wb_b2s1_l", 1.0); ecus[0].o2_current_b2s1 = preferences.getFloat("wb_b2s1_c", 0.0);
    ecus[0].o2_lambda_b2s2 = preferences.getFloat("wb_b2s2_l", 1.0); ecus[0].o2_current_b2s2 = preferences.getFloat("wb_b2s2_c", 0.0);

    // --- Freeze Frame ---
    ecus[0].freezeFrameSet = preferences.getBool("ff_set", false);
    ecus[0].freezeFrame.rpm = preferences.getInt("ff_rpm", 0);
    ecus[0].freezeFrame.speed = preferences.getInt("ff_speed", 0);
    ecus[0].freezeFrame.temp = preferences.getInt("ff_temp", 0);
    ecus[0].freezeFrame.maf = preferences.getFloat("ff_maf", 0.0);
    ecus[0].freezeFrame.fuel_pressure = preferences.getInt("ff_pres", 0);

    // Load and parse DTCs
    String dtcString = preferences.getString("dtcs", "");
    for(int i=0; i<4; i++) {
        ecus[i].num_dtcs = 0; // Reset before loading
    }
    ecus[0].freezeFrameSet = false;
    if (dtcString.length() > 0) {
        int start = 0;
        while(start < dtcString.length()){
            int comma = dtcString.indexOf(',', start);
            String token;
            if(comma == -1) {
                token = dtcString.substring(start);
            } else {
                token = dtcString.substring(start, comma);
            }
            token.trim();
            if(token.length() == 5){
                char prefix = toupper(token[0]);
                if (prefix == 'C') addDTC(ecus[2], token.c_str());
                else if (prefix == 'B') addDTC(ecus[3], token.c_str());
                else addDTC(ecus[0], token.c_str());
            }
            if(comma == -1) break;
            start = comma + 1;
        }
    }
    // Sync permanent DTCs on load
    for(int j=0; j<4; j++) {
        ecus[j].num_permanent_dtcs = ecus[j].num_dtcs;
        for(int i=0; i<ecus[j].num_dtcs; i++) {
            strcpy(ecus[j].permanent_dtcs[i], ecus[j].dtcs[i]);
        }
    }

    // --- TCM Data (ecus[1]) ---
    ecus[1].current_gear = preferences.getInt("gear", 0);

    preferences.end();
}