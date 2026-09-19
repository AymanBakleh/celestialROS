#include "oled_display.h"
#include "config.h"
#include <Arduino.h>

Adafruit_SSD1306 oled_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void init_oled(void) {
    if (!oled_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[OLED] SSD1306 allocation failed"));
        return;
    }
    oled_display.clearDisplay();
    oled_display.setTextSize(1);
    oled_display.setTextColor(SSD1306_WHITE);
    oled_display.setCursor(0, 0);
    oled_display.display();
}

void oled_update(const char* line0, const char* line1, const char* line2, const char* line3, const char* line4) {
    if (oled_display.height() == 0) return;
    oled_display.clearDisplay();
    oled_display.setCursor(0, 0);
    if (line0) oled_display.println(line0);
    if (line1) oled_display.println(line1);
    if (line2) oled_display.println(line2);
    if (line3) oled_display.println(line3);
    if (line4) oled_display.println(line4);
    oled_display.display();
}
