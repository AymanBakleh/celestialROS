#include <iostream>
#include <iomanip>
#include <cmath>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Simple LST calculation using known formulas
float calculate_simple_lst_deg(int year, int month, int day, int hour, int minute, int second) {
    // Convert to UTC (Damascus is UTC+3)
    int utc_hour = hour - 3;
    if (utc_hour < 0) {
        utc_hour += 24;
        day--;
    }
    
    // Simple Julian Day calculation
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    float day_fraction = (float)utc_hour / 24.0f + (float)minute / 1440.0f + (float)second / 86400.0f;
    
    float jd = floor(365.25f * (year + 4716)) + 
               floor(30.6001f * (month + 1)) + 
               day + day_fraction + B - 1524.5f;
    
    // Calculate days since J2000.0
    float days_since_j2000 = jd - 2451545.0f;
    
    // Simple GMST calculation: GMST = 100.46 + 0.985647 * days_since_j2000
    float gmst_deg = 100.46f + 0.985647f * days_since_j2000;
    
    // Normalize to 0-360
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    // Add longitude to get LST
    float lst_deg = gmst_deg + SITE_LONGITUDE;
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

int main() {
    // Test for March 24, 2026 16:46:33 Damascus time
    // UTC time: 13:46:33
    
    std::cout << "=== LST Verification ===" << std::endl;
    std::cout << "Date: 2026-03-24" << std::endl;
    std::cout << "Local Time: 16:46:33 (Damascus)" << std::endl;
    std::cout << "UTC Time: 13:46:33" << std::endl;
    std::cout << "Longitude: 36.31167°E" << std::endl;
    std::cout << std::endl;
    
    float lst_deg = calculate_simple_lst_deg(2026, 3, 24, 16, 46, 33);
    
    std::cout << "Calculated LST: " << std::fixed << std::setprecision(4) << lst_deg << "°" << std::endl;
    std::cout << "LST in hours: " << lst_deg / 15.0f << std::endl;
    std::cout << "Expected LST: 60.33° (from user)" << std::endl;
    std::cout << "Expected LST in hours: 4.022" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_deg - 60.33f) << "°" << std::endl;
    
    // Test if 60.33° could be in hours format
    float expected_hours = 60.33f;
    float expected_deg = expected_hours * 15.0f;
    std::cout << "60.33 hours = " << expected_deg << "°" << std::endl;
    std::cout << "Expected deg mod 360: " << fmod(expected_deg, 360.0f) << "°" << std::endl;
    
    return 0;
}
