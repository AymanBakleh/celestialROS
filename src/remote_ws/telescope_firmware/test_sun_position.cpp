#include <iostream>
#include <iomanip>
#include <cmath>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

float calculate_local_sidereal_time_deg(int local_hour, int local_minute, int local_second) {
    // Using the standard LST formula: LST = 100.46 + 0.985647 * d + long + 15 * UT
    
    // Get current date (using March 24, 2026 as reference)
    int year = 2026, month = 3, day = 24;
    
    // Convert local time to UTC (Damascus is UTC+3)
    int utc_hours = local_hour - 3;
    if (utc_hours < 0) {
        utc_hours += 24;
        day--;
        if (day < 1) {
            day = 28; // Simplified for March
            month--;
            if (month < 1) {
                month = 12;
                year--;
            }
        }
    }
    
    // Calculate Julian Day Number (simplified version)
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    float jd = floor(365.25f * (year + 4716)) + 
               floor(30.6001f * (month + 1)) + 
               day + B - 1524.5f;
    
    // Days since J2000.0
    float d = jd - 2451545.0f;
    
    // UT in decimal hours
    float ut = (float)utc_hours + (float)local_minute / 60.0f + (float)local_second / 3600.0f;
    
    // Calculate LST using standard formula
    // LST = 100.46 + 0.985647 * d + longitude + 15 * UT
    float lst_deg = 100.46f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    
    // Normalize to 0-360 degrees
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

float calculate_hour_angle_deg(float ra_deg, float lst_deg) {
    float ha_deg = lst_deg - ra_deg;
    
    // Normalize to -180 to +180 degrees
    while (ha_deg > 180.0f) ha_deg -= 360.0f;
    while (ha_deg < -180.0f) ha_deg += 360.0f;
    
    return ha_deg;
}

int main() {
    std::cout << "=== Sun Position Test ===" << std::endl;
    std::cout << "Local Time: 16:46:33 (Damascus)" << std::endl;
    std::cout << "Sun RA: 0h17m = 4.25°" << std::endl;
    std::cout << "Sun DEC: 1.3°" << std::endl;
    std::cout << std::endl;
    
    // Calculate LST for user's time
    float lst_deg = calculate_local_sidereal_time_deg(16, 46, 33);
    std::cout << "Local Sidereal Time: " << std::fixed << std::setprecision(4) << lst_deg << "°" << std::endl;
    std::cout << "Expected LST: ~60.33°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_deg - 60.33f) << "°" << std::endl;
    std::cout << std::endl;
    
    // Calculate hour angle for the sun
    float sun_ra_deg = 4.25f;  // 0h17m in degrees
    float sun_ha_deg = calculate_hour_angle_deg(sun_ra_deg, lst_deg);
    
    std::cout << "Sun Hour Angle: " << sun_ha_deg << "°" << std::endl;
    std::cout << "Sun Hour Angle in hours: " << sun_ha_deg / 15.0f << std::endl;
    std::cout << std::endl;
    
    // Interpret the hour angle
    if (sun_ha_deg > 0) {
        std::cout << "Sun is " << sun_ha_deg << "° east of meridian (rising)" << std::endl;
    } else if (sun_ha_deg < 0) {
        std::cout << "Sun is " << -sun_ha_deg << "° west of meridian (setting)" << std::endl;
    } else {
        std::cout << "Sun is on the meridian" << std::endl;
    }
    
    return 0;
}
