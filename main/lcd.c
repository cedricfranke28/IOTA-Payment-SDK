// Copyright 2022 Cedric Franke
#include <stdio.h>
#include <stdlib.h>

#include <../components/qrcodegen/qrcodegen/c/qrcodegen.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "sdkconfig.h"

static const char *TAG = "LCD";

#if CONFIG_ENABLE_LCD
#define LCD_ENABLED 1
static char lcd_text[32] = {};
#else
#define LCD_ENABLED 0
#endif

void lcd_init() {
#if LCD_ENABLED
  ESP_LOGI(TAG, "Initializing LCD-Panel");
  st7735_init();
#else
  ESP_LOGE(TAG, "LCD-Panel is not enabled or detected!");
#endif
}

void lcd_fill_screen(uint16_t color) {
#if LCD_ENABLED
  ESP_LOGI(TAG, "LCD fill screen");
  st7735_fill_screen(color);
#endif
}

void lcd_print(int16_t x, int16_t y, int16_t color, const char *fmt, ...) {
#if LCD_ENABLED
  va_list arg_list;
  va_start(arg_list, fmt);
  vsprintf(lcd_text, fmt, arg_list);
  va_end(arg_list);
  st7735_draw_string(x, y, lcd_text, color, COLOR_WHITE, 1);
#endif
}

void lcd_display_qr() {
#if LCD_ENABLED
  int element_size = 2;
  int qr_version = 10;
  // qr code (x,y) offset
  int offset_x = 5, offset_y = 35;
  size_t qr_buff_len = qrcodegen_BUFFER_LEN_FOR_VERSION(qr_version);
  uint8_t qr0[qr_buff_len];
  uint8_t tempBuffer[qr_buff_len];

  bool ok = qrcodegen_encodeText("atoi1qrshql50hse7t02da5g7w63w4fp2f7pv44zeakz4qalep3ypq3q574444a3", tempBuffer, qr0,
                                 qrcodegen_Ecc_MEDIUM, qr_version, qr_version, qrcodegen_Mask_AUTO, true);

  if (ok) {
    int size = qrcodegen_getSize(qr0);
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        if (qrcodegen_getModule(qr0, x, y)) {
          st7735_rect(x * element_size + offset_x, y * element_size + offset_y, element_size, element_size,
                      COLOR_BLACK);
        } else {
          st7735_rect(x * element_size + offset_x, y * element_size + offset_y, element_size, element_size,
                      COLOR_WHITE);
        }
      }
    }
  }
#endif
}
