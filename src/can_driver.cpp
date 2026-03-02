#include "shared.h"
#include <driver/twai.h>
#include <Arduino.h>

 const int CAN_TX_PIN = 4;
 const int CAN_RX_PIN = 5;

bool initCAN(int bitrate) {
    static bool driver_installed = false;
    if (driver_installed) {
        twai_stop();
        twai_driver_uninstall();
        driver_installed = false;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    g_config.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED; // Вмикаємо сповіщення про статус шини
    twai_timing_config_t t_config;
    
    if (bitrate == 250000) {
        twai_timing_config_t temp = TWAI_TIMING_CONFIG_250KBITS();
        t_config = temp;
    } else {
        twai_timing_config_t temp = TWAI_TIMING_CONFIG_500KBITS();
        t_config = temp;
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        driver_installed = true;
        return twai_start() == ESP_OK;
    }
    return false;
}

// Функція для моніторингу та відновлення шини CAN
void pollCANStatus() {
    uint32_t alerts;
    if (twai_read_alerts(&alerts, 0) == ESP_OK) {
        if (alerts & TWAI_ALERT_BUS_OFF) {
            Serial.println("TWAI: Bus Off state detected. Initiating recovery...");
            twai_initiate_recovery(); // Починаємо відновлення
        }
        if (alerts & TWAI_ALERT_BUS_RECOVERED) {
            Serial.println("TWAI: Bus Recovered. Restarting driver...");
            twai_start(); // Перезапускаємо драйвер після відновлення
        }
    }
}