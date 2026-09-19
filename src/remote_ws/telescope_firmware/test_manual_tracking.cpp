// Test manual tracking control and DEC speed features
#include <iostream>

void test_manual_tracking_commands() {
    std::cout << "=== Manual Tracking Commands ===" << std::endl;
    
    std::cout << "Start Manual Tracking (1.0 deg/sec):" << std::endl;
    std::cout << "{\"T\": 16, \"tracking_speed\": 1.0}" << std::endl;
    
    std::cout << std::endl << "Stop Manual Tracking:" << std::endl;
    std::cout << "{\"T\": 16, \"tracking_speed\": 0.0}" << std::endl;
    
    std::cout << std::endl << "Start Slow Tracking (0.5 deg/sec):" << std::endl;
    std::cout << "{\"T\": 16, \"tracking_speed\": 0.5}" << std::endl;
    
    std::cout << std::endl << "Start Fast Tracking (3.0 deg/sec):" << std::endl;
    std::cout << "{\"T\": 16, \"tracking_speed\": 3.0}" << std::endl;
}

void test_dec_speed_commands() {
    std::cout << "\n=== DEC Speed Commands ===" << std::endl;
    
    std::cout << "Set DEC Speed to Fast (3000):" << std::endl;
    std::cout << "{\"T\": 17, \"dec_speed\": 3000}" << std::endl;
    
    std::cout << std::endl << "Set DEC Speed to Medium (2000):" << std::endl;
    std::cout << "{\"T\": 17, \"dec_speed\": 2000}" << std::endl;
    
    std::cout << std::endl << "Set DEC Speed to Slow (1000):" << std::endl;
    std::cout << "{\"T\": 17, \"dec_speed\": 1000}" << std::endl;
}

void test_status_feedback() {
    std::cout << "\n=== Enhanced Status Feedback ===" << std::endl;
    std::cout << "JSON response now includes:" << std::endl;
    std::cout << "{" << std::endl;
    std::cout << "  \"ra_speed\": 1500," << std::endl;
    std::cout << "  \"dec_speed\": 2000," << std::endl;
    std::cout << "  \"dec_spd_setting\": 2000," << std::endl;  // NEW field
    std::cout << "  \"tracking_mode\": 1," << std::endl;
    std::cout << "  \"tracking_mode_name\": \"Sidereal\"," << std::endl;
    std::cout << "  \"tracking_rate\": 0.00417807" << std::endl;
    std::cout << "}" << std::endl;
}

void test_speed_comparison() {
    std::cout << "\n=== Speed Comparison ===" << std::endl;
    
    std::cout << "Before:" << std::endl;
    std::cout << "  RA speed: 1500 (default)" << std::endl;
    std::cout << "  DEC speed: 1500 (default, slow)" << std::endl;
    
    std::cout << std::endl << "After:" << std::endl;
    std::cout << "  RA speed: 1500 (default)" << std::endl;
    std::cout << "  DEC speed: 2000 (default, 33% faster)" << std::endl;
    
    std::cout << std::endl << "Speed Range (T17): 500-4000" << std::endl;
    std::cout << "Manual Tracking Range (T16): 0.1-5.0 deg/sec" << std::endl;
}

void show_web_gui_integration() {
    std::cout << "\n=== Web GUI Integration ===" << std::endl;
    
    std::cout << "// Manual Tracking Controls" << std::endl;
    std::cout << "<div>" << std::endl;
    std::cout << "  <label>Tracking Speed (deg/sec):</label>" << std::endl;
    std::cout << "  <input type=\"range\" id=\"tracking-speed\" min=\"0.1\" max=\"5.0\" step=\"0.1\">" << std::endl;
    std::cout << "  <button onclick=\"setManualTracking(document.getElementById('tracking-speed').value)\">Start</button>" << std::endl;
    std::cout << "  <button onclick=\"setManualTracking(0)\">Stop</button>" << std::endl;
    std::cout << "</div>" << std::endl;
    
    std::cout << std::endl << "// DEC Speed Controls" << std::endl;
    std::cout << "<div>" << std::endl;
    std::cout << "  <label>DEC Speed:</label>" << std::endl;
    std::cout << "  <input type=\"range\" id=\"dec-speed\" min=\"500\" max=\"4000\" step=\"100\">" << std::endl;
    std::cout << "  <button onclick=\"setDecSpeed(document.getElementById('dec-speed').value)\">Set</button>" << std::endl;
    std::cout << "  <span>Current: <span id=\"dec-speed-display\">2000</span></span>" << std::endl;
    std::cout << "</div>" << std::endl;
    
    std::cout << std::endl << "// JavaScript Functions" << std::endl;
    std::cout << "function setManualTracking(speed) {" << std::endl;
    std::cout << "  sendCommand({\"T\": 16, \"tracking_speed\": speed});" << std::endl;
    std::cout << "}" << std::endl;
    std::cout << std::endl;
    std::cout << "function setDecSpeed(speed) {" << std::endl;
    std::cout << "  sendCommand({\"T\": 17, \"dec_speed\": speed});" << std::endl;
    std::cout << "}" << std::endl;
}

int main() {
    std::cout << "Manual Tracking Control & DEC Speed Test" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    test_manual_tracking_commands();
    test_dec_speed_commands();
    test_status_feedback();
    test_speed_comparison();
    show_web_gui_integration();
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "✅ Added T16: Manual tracking control with custom speed" << std::endl;
    std::cout << "✅ Added T17: DEC speed control (500-4000 range)" << std::endl;
    std::cout << "✅ Enhanced DEC default speed: 2000 (was 1500)" << std::endl;
    std::cout << "✅ Added dec_spd_setting to status feedback" << std::endl;
    std::cout << "✅ Web GUI can control tracking speed and DEC speed" << std::endl;
    
    return 0;
}
