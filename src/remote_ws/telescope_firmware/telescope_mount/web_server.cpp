#include "web_server.h"
#include "web_page_enhanced.h"
#include "mount_control.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiServer.h>

static WebServer* s_server = nullptr;
static WiFiServer* s_tcpServer = nullptr;
static WiFiClient s_tcpClient;

static void handleRoot() {
    s_server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    s_server->sendHeader("Pragma", "no-cache");
    s_server->sendHeader("Expires", "0");
    s_server->send_P(200, "text/html", WEB_PAGE_ENHANCED_HTML);
}

static void handleApiStatus() {
    String json;
    get_status_json(json);
    s_server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    s_server->sendHeader("Pragma", "no-cache");
    s_server->sendHeader("Expires", "0");
    s_server->send(200, "application/json", json);
}

static void handleApiVersion() {
    String version = "jarspace-r2-2026-04-04;compiled=";
    version += __DATE__;
    version += " ";
    version += __TIME__;
    s_server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    s_server->sendHeader("Pragma", "no-cache");
    s_server->sendHeader("Expires", "0");
    s_server->send(200, "text/plain", version);
}

static void handleApiCmd() {
    if (s_server->method() != HTTP_POST) {
        s_server->send(405, "text/plain", "Method Not Allowed");
        return;
    }
    String body = s_server->arg("plain");
    if (body.length() == 0) {
        s_server->send(400, "text/plain", "No body");
        return;
    }
    StaticJsonDocument<JSON_BUF_SIZE> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        s_server->send(400, "text/plain", "Invalid JSON");
        return;
    }
    process_command(doc);
    s_server->send(200, "application/json", "{\"ok\":true}");
}

void web_server_init(void) {
    auto start_ap_fallback = []() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
        IPAddress ap_ip = WiFi.softAPIP();
        Serial.print("[WiFi] AP started: ");
        Serial.print(WIFI_AP_SSID);
        Serial.print(" pass: ");
        Serial.println(WIFI_AP_PASS);
        Serial.print("[WiFi] AP IP: ");
        Serial.println(ap_ip);
    };

    Serial.print("[WiFi] Connecting to STA network: ");
    Serial.println(WIFI_STA_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    unsigned long start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < WIFI_STA_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress sta_ip = WiFi.localIP();
        Serial.print("[WiFi] Connected to ");
        Serial.println(WIFI_STA_SSID);
        Serial.print("[WiFi] STA IP: ");
        Serial.println(sta_ip);
    } else {
        Serial.print("[WiFi] STA connect failed (status=");
        Serial.print(WiFi.status());
        Serial.println("). Falling back to AP mode.");
        WiFi.disconnect(true, true);
        delay(100);
        start_ap_fallback();
    }

    // Start HTTP server on port 80
    s_server = new WebServer(80);
    s_server->on("/", handleRoot);
    s_server->on("/api/status", HTTP_GET, handleApiStatus);
    s_server->on("/api/version", HTTP_GET, handleApiVersion);
    s_server->on("/api/cmd", HTTP_POST, handleApiCmd);
    s_server->begin();
    Serial.println("[Web] HTTP Server started on port 80");

    // Start TCP server on port 10001 for ROS2 driver
    s_tcpServer = new WiFiServer(10001);
    s_tcpServer->begin();
    Serial.println("[TCP] Server started on port 10001 for ROS2 driver");
}

void tcp_send_feedback(const char* json_str) {
    if (s_tcpClient && s_tcpClient.connected()) {
        s_tcpClient.println(json_str);
    }
}

void web_server_handle_client(void) {
    if (s_server) s_server->handleClient();
    
    // Handle TCP client connections for ROS2 driver
    if (s_tcpServer) {
        // Check for new client connections
        if (!s_tcpClient || !s_tcpClient.connected()) {
            s_tcpClient = s_tcpServer->available();
            if (s_tcpClient && s_tcpClient.connected()) {
                Serial.println("[TCP] ROS2 driver connected");
            }
        }
        
        // Handle data from connected client
        if (s_tcpClient && s_tcpClient.connected()) {
            while (s_tcpClient.available()) {
                String line = s_tcpClient.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) {
                    StaticJsonDocument<JSON_BUF_SIZE> doc;
                    DeserializationError err = deserializeJson(doc, line);
                    if (!err) {
                        process_command(doc);
                    }
                }
            }
        }
    }
}
