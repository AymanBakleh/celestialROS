/*
 * Jarspace Telescope Mount — main sketch (V1 app shell)
 *
 * This file is the entrypoint that wires together:
 * - Serial JSON commands (from Stellarium/ROS2 bridge)
 * - Web UI 
 * - Servo feedback + motor control loop
 * - OLED display
 */

#include "config.h"
#include "servo_control.h"
#include "mount_control.h"
#include "oled_display.h"
#include "web_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <WiFi.h>

static StaticJsonDocument<JSON_BUF_SIZE> s_cmdDoc;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { }

    Serial.println("\n\n========== TELESCOPE MOUNT INIT ==========");

    Wire.begin(S_SDA, S_SCL);
    init_oled();
    oled_update("TELESCOPE MOUNT", "Initializing...", nullptr, nullptr, nullptr);

    init_servo();
    mount_init();
    mount_zero_encoders_at_startup();

    // torque starts OFF, then ON (as requested)
    setTorque(false);
    delay(100);
    setTorque(true);

    web_server_init();

    char wifiLine[32];
    const char* activeWifi = (WiFi.status() == WL_CONNECTED) ? WIFI_STA_SSID : WIFI_AP_SSID;
    snprintf(wifiLine, sizeof(wifiLine), "WiFi: %s", activeWifi);
    oled_update("TELESCOPE READY", "Torque: ON", wifiLine, "Pass: 12345678", nullptr);
    Serial.println("===== INIT COMPLETE =====");
}

void loop() {
    unsigned long now = millis();

    // ----- Serial JSON commands -----
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            DeserializationError err = deserializeJson(s_cmdDoc, line);
            if (!err) {
                process_command(s_cmdDoc);
            }
            s_cmdDoc.clear();
        }
    }

    // ----- Web server -----
    web_server_handle_client();

    // ----- No encoder feedback -----
    // Position is tracked purely from commanded motion (software counters).

    // ----- Motor outputs -----
    mount_update_motors(now);

    // ----- Periodic Serial feedback (T2 stream) -----
    static unsigned long lastFb = 0;
    if (now - lastFb >= FEEDBACK_INTERVAL_MS) {
        lastFb = now;
        send_feedback();
    }

    // ----- OLED -----
    static unsigned long lastOled = 0;
    if (now - lastOled >= 200) {
        lastOled = now;
        static char l0[32], l1[32], l2[40], l3[40];
        
        // Line 1: Jarspace mount + current time (without "TIME" word)
        static unsigned long lastClockUpdate = 0;
        if (now - lastClockUpdate >= 1000) {
            lastClockUpdate = now;
            advance_local_time();
        }
        snprintf(l0, sizeof(l0), "JARSPACE %02d:%02d:%02d", local_time_hours, local_time_minutes, local_time_seconds);
        
        // Line 2: Mount status + DEC in degrees
        float ra_deg = get_current_ra_deg();
        float dec_deg = get_current_dec_deg();
        const char* status = torque_enabled ? (mount_is_moving() ? "Moving" : "Idle") : "Torque OFF";
        snprintf(l1, sizeof(l1), "%s DEC:%+06.2f", status, dec_deg);
        
        // Line 3: RA as HH:MM:SS and angle in degrees
        float ra_norm_deg = normalize_ra_deg(ra_deg);
        float ra_hours = ra_norm_deg / 15.0f;
        int ra_h = (int)ra_hours;
        int ra_m = (int)((ra_hours - ra_h) * 60.0f);
        int ra_s = (int)((((ra_hours - ra_h) * 60.0f) - ra_m) * 60.0f);
        snprintf(l2, sizeof(l2), "RA: %02d:%02d:%02d %07.3f", ra_h, ra_m, ra_s, ra_norm_deg);

        // Line 4: HA since meridian crossing in [0, 24h) and [0, 360°)
        float ha_deg = calculate_local_sidereal_time_deg() - ra_deg;
        while (ha_deg >= 360.0f) ha_deg -= 360.0f;
        while (ha_deg < 0.0f) ha_deg += 360.0f;
        float ha_hours = ha_deg / 15.0f;
        int ha_h = (int)ha_hours;
        int ha_m = (int)((ha_hours - ha_h) * 60.0f);
        int ha_s = (int)((((ha_hours - ha_h) * 60.0f) - ha_m) * 60.0f);
        snprintf(l3, sizeof(l3), "HA %02d:%02d:%02d %06.1f", ha_h, ha_m, ha_s, ha_deg);
        
        oled_update(l0, l1, l2, l3, nullptr);
    }

    delay(10);
}
