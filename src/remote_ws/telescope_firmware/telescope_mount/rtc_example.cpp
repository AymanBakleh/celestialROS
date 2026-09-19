/**
 * RTC Example for Telescope Mount
 * 
 * This example demonstrates how to use the DS1302 RTC functionality
 * integrated into the telescope mount firmware.
 * 
 * The RTC provides:
 * - Real-time clock backup when power is off
 * - Automatic time synchronization on startup
 * - Fallback to default time if RTC is not available
 * 
 * Usage examples:
 * 
 * 1. Get current RTC time:
 *    {"T":18, "rtc_get_time":true}
 * 
 * 2. Set RTC time:
 *    {"T":18, "rtc_set_time":true, "year":2026, "month":3, "day":25, "hour":14, "minute":30, "second":0}
 * 
 * 3. Check RTC availability:
 *    {"T":18, "rtc_available":true}
 * 
 * 4. Set time mode:
 *    {"T":18, "time_mode":2}  // 0=Default, 1=Web, 2=RTC
 */

#include "mount_control.h"
#include <Arduino.h>

void setup_rtc_example() {
    Serial.begin(115200);
    Serial.println("=== RTC Example for Telescope Mount ===");
    
    // RTC is automatically initialized in mount_init()
    // Check if RTC is available
    if (is_rtc_available()) {
        Serial.println("✓ RTC is available and running");
        Serial.println("✓ Time mode automatically set to RTC");
    } else {
        Serial.println("✗ RTC not available - using default time");
        Serial.println("✗ Check RTC connections (CE, IO, SCLK pins)");
    }
    
    // Show current time mode
    int mode = get_time_mode();
    const char* mode_names[] = {"Default", "Web", "RTC"};
    Serial.printf("Current time mode: %s (%d)\n", mode_names[mode], mode);
    
    Serial.println("\n=== RTC Commands ===");
    Serial.println("Get RTC time: {\"T\":18, \"rtc_get_time\":true}");
    Serial.println("Set RTC time: {\"T\":18, \"rtc_set_time\":true, \"year\":2026, \"month\":3, \"day\":25, \"hour\":14, \"minute\":30, \"second\":0}");
    Serial.println("Check RTC: {\"T\":18, \"rtc_available\":true}");
    Serial.println("Set mode: {\"T\":18, \"time_mode\":2}");
    Serial.println("\n=== Time Management ===");
    Serial.println("- RTC mode: Time automatically updated from RTC every second");
    Serial.println("- Default mode: Time increments internally (starts at 12:00:00)");
    Serial.println("- Web mode: Time set via web interface commands");
}

void loop_rtc_example() {
    // Show current time every 5 seconds
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay >= 5000) {
        lastDisplay = millis();
        
        extern int local_time_hours, local_time_minutes, local_time_seconds;
        Serial.printf("Current time: %02d:%02d:%02d (Mode: %s, RTC: %s)\n", 
                     local_time_hours, local_time_minutes, local_time_seconds,
                     get_time_mode() == 2 ? "RTC" : "Other",
                     is_rtc_available() ? "Available" : "Not Available");
    }
}

/*
RTC Hardware Connections (ESP32):
- RTC_CE_PIN  (25) -> DS1302 CE/RST pin
- RTC_IO_PIN  (26) -> DS1302 I/O pin  
- RTC_SCLK_PIN (27) -> DS1302 SCLK pin
- VCC -> 3.3V
- GND -> GND

DS1302 Features:
- Battery backup maintains time when power is off
- Real-time clock with seconds, minutes, hours, day, date, month, year
- Automatic leap year compensation
- 31 x 8 battery-backed RAM (optional use)

Time Zone Handling:
- RTC stores time in UTC
- Firmware converts to local time (UTC+3 for Damascus)
- Local time used for sidereal calculations and display

Error Handling:
- If RTC is halted on startup, firmware falls back to default time
- If RTC communication fails, system continues with internal clock
- RTC status included in feedback for monitoring
*/
