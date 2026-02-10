#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
// #include <ESP32CAN.h>
// #include <CAN_config.h>
#include "web_page.h"

// --- TFT Display ---
// #include <Adafruit_GFX.h>
// #include <Adafruit_ST7735.h>
// #include <SPI.h>

// ############## Налаштування пінів TFT ST7735 ##############
// Якщо у вас інші піни, змініть їх тут
#define TFT_CS     5
#define TFT_DC     2
#define TFT_RST    4 
// Для апаратної реалізації SPI, піни MOSI та SCLK зазвичай визначені платою
// Для ESP32 це GPIO 23 (MOSI) та GPIO 18 (SCLK)
// #define TFT_SCLK 18
// #define TFT_MOSI 23

// Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ############## Налаштування CAN ##############
// CAN_device_t CAN_cfg;
const int CAN_TX_PIN = 20;
const int CAN_RX_PIN = 21;
// CAN_frame_t CAN_cfg_frame;
char vin[18] = "VIN_NOT_SET";
char dtcs[5][6] = {"", "", "", "", ""};
int num_dtcs = 0;
int engine_rpm = 1500;
int engine_temp = 90;

// ############## Налаштування Wi-Fi та веб-сервера ##############
const char* ap_ssid = "OBD-II-Emulator";
const char* ap_password = "12345678";
AsyncWebServer server(80);

// ############## Прототипи функцій ##############
// void handleOBDRequest(const CAN_frame_t &frame);
// void sendVIN(byte pid);
// void sendDTCs();
// void sendCurrentData(byte pid);
void updateDisplay();


void setup() {
  Serial.begin(115200);
  Serial.println("OBD-II Emulator Starting...");

  // --- Ініціалізація TFT ---
  // tft.initR(INITR_BLACKTAB); // або INITR_GREENTAB, INITR_REDTAB
  // tft.fillScreen(ST7735_BLACK);
  // tft.setRotation(1); // Горизонтальна орієнтація
  // tft.setCursor(0, 0);
  // tft.setTextColor(ST7735_WHITE);
  // tft.setTextSize(1);
  // tft.println("OBD-II Emulator");
  // tft.println("Starting...");

  // --- Налаштування Wi-Fi (точка доступу) ---
  Serial.print("Creating Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print(" AP IP: ");
  Serial.println(IP);
  // tft.print("IP: ");
  // tft.println(IP);

  // --- Налаштування CAN ---
  // CAN_cfg.speed = CAN_SPEED_500KBPS;
  // CAN_cfg.tx_pin_id = (gpio_num_t)CAN_TX_PIN;
  // CAN_cfg.rx_pin_id = (gpio_num_t)CAN_RX_PIN;
  // CAN_cfg.rx_queue = xQueueCreate(10, sizeof(CAN_frame_t));
  // ESP32Can.CANInit();
  Serial.println("CAN bus initialized (disabled).");

  // --- Налаштування веб-сервера ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("vin")) strncpy(vin, request->getParam("vin")->value().c_str(), 17);

    // Скидаємо старі DTC
    for(int i=0; i<5; i++) dtcs[i][0] = '\0';
    num_dtcs = 0;

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
          strncpy(dtcs[num_dtcs], token.c_str(), 5);
          dtcs[num_dtcs][5] = '\0';
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

    if(request->hasParam("temp")) engine_temp = request->getParam("temp")->value().toInt();
    if(request->hasParam("rpm")) engine_rpm = request->getParam("rpm")->value().toInt();
    
    Serial.println("========== Emulator Data Updated ==========");
    Serial.println("VIN: " + String(vin));
    for(int i=0; i<num_dtcs; i++){
      Serial.println("DTC "+ String(i+1) +": " + String(dtcs[i]));
    }
    Serial.println("Engine Temp: " + String(engine_temp));
    Serial.println("Engine RPM: " + String(engine_rpm));
    Serial.println("==========================================");
    
    updateDisplay(); // Оновлюємо екран

    request->send(200, "text/plain", "Emulator data updated successfully!");
  });

  server.begin();
  Serial.println("Web server started.");
  delay(1000); // Затримка, щоб побачити стартові повідомлення
  updateDisplay(); // Перше оновлення екрану з початковими даними
}

void loop() {
  // CAN_frame_t rx_frame;
  // if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {
  //   if (rx_frame.MsgID == 0x7DF) { // OBD_CAN_ID_REQUEST
  //       handleOBDRequest(rx_frame);
  //   }
  // }
  delay(100);
}

void updateDisplay() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setCursor(0, 0);
  // tft.setTextColor(ST7735_WHITE);
  // tft.setTextSize(1);
  
  // tft.setTextColor(ST7735_YELLOW);
  // tft.println("OBD-II EMULATOR STATUS");
  
  // tft.setTextColor(ST7735_WHITE);
  // tft.print("VIN: ");
  // tft.println(vin);
  
  // tft.print("RPM: ");
  // tft.print(engine_rpm);
  // tft.println(" rpm");

  // tft.print("Temp: ");
  // tft.print(engine_temp);
  // tft.println(" C");

  // tft.println("DTCs:");
  // tft.setTextColor(ST7735_RED);
  // if (num_dtcs > 0) {
  //   for(int i=0; i<num_dtcs; i++) {
  //       tft.print("  ");
  //       tft.println(dtcs[i]);
  //   }
  // } else {
  //   tft.setTextColor(ST7735_GREEN);
  //   tft.println("  None");
  // }

  // Serial output instead of TFT
  Serial.println("\n========== Current Emulator Status ==========");
  Serial.print("VIN: ");
  Serial.println(vin);
  Serial.print("RPM: ");
  Serial.print(engine_rpm);
  Serial.println(" rpm");
  Serial.print("Temp: ");
  Serial.print(engine_temp);
  Serial.println(" C");
  Serial.println("DTCs:");
  if (num_dtcs > 0) {
    for(int i=0; i<num_dtcs; i++) {
        Serial.print("  ");
        Serial.println(dtcs[i]);
    }
  } else {
    Serial.println("  None");
  }
  Serial.println("==========================================\n");
}

/*
void handleOBDRequest(const CAN_frame_t &frame) {
    byte service = frame.data.u8[1];
    byte pid = frame.data.u8[2];

    Serial.printf("Received OBD Request: Service 0x%02X, PID 0x%02X\n", service, pid);

    switch(service) {
        case 0x01: sendCurrentData(pid); break;
        case 0x03: sendDTCs(); break;
        case 0x09: if (pid == 0x02) sendVIN(pid); break;
    }
}

void sendCurrentData(byte pid) {
    CAN_frame_t tx_frame;
    tx_frame.MsgID = 0x7E8; // OBD_CAN_ID_RESPONSE;
    tx_frame.FIR.B.RTR = CAN_no_RTR;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.data.u8[1] = 0x40 + 0x01;
    tx_frame.data.u8[2] = pid;
    tx_frame.dlc = 8;

    switch(pid) {
        case 0x0C: { // Engine RPM
            int rpm_value = engine_rpm * 4;
            tx_frame.data.u8[0] = 4;
            tx_frame.data.u8[3] = highByte(rpm_value);
            tx_frame.data.u8[4] = lowByte(rpm_value);
            ESP32Can.CANWriteFrame(&tx_frame);
            Serial.println("Sent RPM data");
            break;
        }
        case 0x05: { // Engine Coolant Temperature
            int temp_value = engine_temp + 40;
            tx_frame.data.u8[0] = 3;
            tx_frame.data.u8[3] = temp_value;
            ESP32Can.CANWriteFrame(&tx_frame);
            Serial.println("Sent Engine Temp data");
            break;
        }
    }
}

void sendDTCs() {
    CAN_frame_t tx_frame;
    tx_frame.MsgID = 0x7E8; // OBD_CAN_ID_RESPONSE;
    tx_frame.FIR.B.RTR = CAN_no_RTR;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.dlc = 8;

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

    tx_frame.data.u8[0] = 1 + byte_count;
    tx_frame.data.u8[1] = 0x43; // Response to service 03
    tx_frame.data.u8[2] = num_dtcs;
    memcpy(&tx_frame.data.u8[3], dtc_bytes, byte_count);
    
    // ESP32Can.CANWriteFrame(&tx_frame);
    Serial.println("Sent DTCs");
}

void sendVIN(byte pid) {
    // Simplified VIN response (sends first few chars)
    CAN_frame_t tx_frame;
    tx_frame.MsgID = 0x7E8; // OBD_CAN_ID_RESPONSE;
    // tx_frame.FIR.B.RTR = CAN_no_RTR;
    // tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.dlc = 8;
    
    // tx_frame.data.u8[0] = 0x10; // ISO-TP: First Frame
    // tx_frame.data.u8[1] = 19;   // Total bytes: 2 (header) + 17 (VIN)
    // tx_frame.data.u8[2] = 0x49; // Response for service 09
    // tx_frame.data.u8[3] = pid;
    // tx_frame.data.u8[4] = vin[0];
    // tx_frame.data.u8[5] = vin[1];
    // tx_frame.data.u8[6] = vin[2];
    // tx_frame.data.u8[7] = vin[3];
    // ESP32Can.CANWriteFrame(&tx_frame);

    // In a full implementation, consecutive frames would follow here.
    Serial.println("Sent VIN (simplified)");
}
*/