//Klasse lcd zum betreiben des LCD-Panels
#pragma once 

#include <../components/ST7735/ST7735.h>

void lcd_init();
void lcd_fill_screen(uint16_t color);
void lcd_print(int16_t x, int16_t y, int16_t color, const char *fmt, ...);
void lcd_display_qr();