#include "shared.h"
#include <driver/twai.h>

const int CAN_TX_PIN = 20;
const int CAN_RX_PIN = 21;

bool initCAN(int bitrate) {
    static bool driver_installed = false;
    if (driver_installed) {
        twai_stop();
        twai_driver_uninstall();
        driver_installed = false;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config;
    
    if (bitrate == 250000) t_config = TWAI_TIMING_CONFIG_250KBITS();
    else t_config = TWAI_TIMING_CONFIG_500KBITS();

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        driver_installed = true;
        return twai_start() == ESP_OK;
    }
    return false;
}