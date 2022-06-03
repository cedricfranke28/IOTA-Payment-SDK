// Copyright 2021 IOTA Stiftung
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sensor.h"
#include "../components/DHT22/DHT22/DHT22.h"


#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
#include "driver/temp_sensor.h"
#define ENABLE_TEMP 1
#else
#define ENABLE_TEMP 0
#endif

//DHT22 Config
#if CONFIG_DHT22_TEMP
  #define ENABLE_DHT22_TEMP 1
#else
  #define ENABLE_DHT22_TEMP 0
#endif

static const char *TAG = "TempSensor";

static const char *TAGDHT = "DHT22Temp";

const int dhtpin = 4;

void init_tempsensor() {
#if ENABLE_TEMP
  // Initialize touch pad peripheral, it will start a timer to run a filter
  ESP_LOGI(TAG, "Initializing Temperature sensor");
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor_get_config(&temp_sensor);
  ESP_LOGI(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
  temp_sensor.dac_offset = TSENS_DAC_DEFAULT;  // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
  temp_sensor_set_config(temp_sensor);
#else
  ESP_LOGE(TAG, "Temperature sensor is not supported on this hardware");
#endif
}

float get_temp() {
#if ENABLE_TEMP
  float temp = 0.0;
  temp_sensor_start();
  vTaskDelay(1000 / portTICK_RATE_MS);
  temp_sensor_read_celsius(&temp);
  temp_sensor_stop();
  return temp;
#else
  ESP_LOGE(TAG, "Temperature sensor is not supported on this hardware");
  return 0.0;
#endif
}

void init_dht_tempsensor() {
  #if ENABLE_DHT22_TEMP
    ESP_LOGI(TAGDHT, "Initializing DHT22 Temperature Sensor");
    void setDHTgpio(dhtpin);
  #else
    ESP_LOGE(TAGDHT, "DHT22 Temperature is not enabled on this hardware");
  #endif
}

float get_dht_temp() {
  #if ENABLE_DHT22_TEMP
    int ret = readDHT();

    errorHandler(ret);

    vTaskDelay(3000 / portTICK_RATE_MS);

    return getTemperature();
  #else
    ESP_LOGE(TAGDHT, "DHT22 Temperature cant get value");
  #endif
}