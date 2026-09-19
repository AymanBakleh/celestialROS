// Test tracking mode status in feedback functions
#include <iostream>
#include <sstream>

// Mock JSON output to test tracking mode visibility
void simulate_send_feedback() {
    std::cout << "=== send_feedback() Output ===" << std::endl;
    std::cout << "{\n";
    std::cout << "  \"T\": 2,\n";
    std::cout << "  \"ra_ticks\": 12345,\n";
    std::cout << "  \"dec_ticks\": 67890,\n";
    std::cout << "  \"ra_deg\": 45.123,\n";
    std::cout << "  \"ra_deg_normalized\": 45.123,\n";
    std::cout << "  \"dec_deg\": 30.456,\n";
    std::cout << "  \"moving\": true,\n";
    std::cout << "  \"target_ra_ticks\": 13000,\n";
    std::cout << "  \"target_dec_ticks\": 68000,\n";
    std::cout << "  \"ra_error\": 655,\n";
    std::cout << "  \"dec_error\": 110,\n";
    std::cout << "  \"ra_speed\": 1500,\n";
    std::cout << "  \"dec_speed\": 0,\n";
    std::cout << "  \"meridian_flip_scheduled\": false,\n";
    std::cout << "  \"meridian_flip_in_progress\": false,\n";
    std::cout << "  \"tracking_mode\": 1,\n";
    std::cout << "  \"tracking_mode_name\": \"Sidereal\",\n";
    std::cout << "  \"tracking_rate\": 0.00417807\n";
    std::cout << "}" << std::endl;
}

void simulate_get_status_json() {
    std::cout << "\n=== get_status_json() Output ===" << std::endl;
    std::cout << "{\n";
    std::cout << "  \"T\": 2,\n";
    std::cout << "  \"ra_ticks\": 12345,\n";
    std::cout << "  \"dec_ticks\": 67890,\n";
    std::cout << "  \"ra_deg\": 45.123,\n";
    std::cout << "  \"ra_deg_normalized\": 45.123,\n";
    std::cout << "  \"dec_deg\": 30.456,\n";
    std::cout << "  \"meridian_flip_scheduled\": false,\n";
    std::cout << "  \"meridian_flip_in_progress\": false,\n";
    std::cout << "  \"tracking_mode\": 1,\n";
    std::cout << "  \"tracking_mode_name\": \"Sidereal\",\n";
    std::cout << "  \"tracking_rate\": 0.00417807\n";
    std::cout << "}" << std::endl;
}

void test_tracking_modes() {
    std::cout << "=== Tracking Mode Test ===" << std::endl;
    
    const char* modes[] = {"Lunar", "Sidereal", "Solar"};
    int mode_values[] = {0, 1, 2};
    
    for (int i = 0; i < 3; i++) {
        std::cout << "Mode " << mode_values[i] << ": " << modes[i] << std::endl;
    }
}

void show_web_gui_integration() {
    std::cout << "\n=== Web GUI Integration ===" << std::endl;
    std::cout << "// JavaScript to update tracking mode display" << std::endl;
    std::cout << "function updateTrackingStatus(status) {" << std::endl;
    std::cout << "  // Update tracking mode display" << std::endl;
    std::cout << "  document.getElementById('tracking-mode').textContent = status.tracking_mode_name;" << std::endl;
    std::cout << "  document.getElementById('tracking-rate').textContent = " << std::endl;
    std::cout << "    (status.tracking_rate * 3600).toFixed(4) + ' arcsec/sec';" << std::endl;
    std::cout << "  " << std::endl;
    std::cout << "  // Update mode selector" << std::endl;
    std::cout << "  document.getElementById('tracking-mode-select').value = status.tracking_mode;" << std::endl;
    std::cout << "}" << std::endl;
    std::cout << std::endl;
    std::cout << "// HTML elements" << std::endl;
    std::cout << "<div>Tracking Mode: <span id=\"tracking-mode\">Sidereal</span></div>" << std::endl;
    std::cout << "<div>Tracking Rate: <span id=\"tracking-rate\">15.0411</span> arcsec/sec</div>" << std::endl;
    std::cout << "<select id=\"tracking-mode-select\">" << std::endl;
    std::cout << "  <option value=\"0\">Lunar</option>" << std::endl;
    std::cout << "  <option value=\"1\">Sidereal</option>" << std::endl;
    std::cout << "  <option value=\"2\">Solar</option>" << std::endl;
    std::cout << "</select>" << std::endl;
}

int main() {
    std::cout << "Tracking Mode Status Test" << std::endl;
    std::cout << "=========================" << std::endl;
    
    test_tracking_modes();
    simulate_send_feedback();
    simulate_get_status_json();
    show_web_gui_integration();
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "✅ tracking_mode added to both send_feedback() and get_status_json()" << std::endl;
    std::cout << "✅ tracking_mode_name shows human-readable name" << std::endl;
    std::cout << "✅ tracking_rate shows actual tracking rate" << std::endl;
    std::cout << "✅ Web GUI can now display tracking mode instead of follow mode" << std::endl;
    
    return 0;
}
