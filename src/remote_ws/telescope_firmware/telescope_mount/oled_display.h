#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "config.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>

extern Adafruit_SSD1306 oled_display;

void init_oled(void);

void oled_update(const char* line0, const char* line1, const char* line2, const char* line3, const char* line4);

#endif // OLED_DISPLAY_H
