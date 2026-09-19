/*
 * Test program for telescope coordinate calculations
 * Compile and run on your computer to validate the algorithms
 */

#include <iostream>
#include <cmath>
#include <iomanip>

// Damascus coordinates
const float SITE_LATITUDE = 33.50917f;   // 33°30'33" N
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Constants
const float TWO_PI = 2.0f * 3.14159265359f;
const float DEG_TO_RAD = 3.14159265359f / 180.0f;
const float RAD_TO_DEG = 180.0f / 3.14159265359f;

float calculate_julian_day(int year, int month, int day, int hour, int minute, int second) {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    // Calculate fractional day
    float day_fraction = (float)hour / 24.0f + (float)minute / 1440.0f + (float)second / 86400.0f;
    
    // Main formula from the original request
    float jd = floor(365.25f * (year + 4716)) + 
               floor(30.6001f * (month + 1)) + 
               day + day_fraction + B - 1524.5f;
    return jd;
}

float calculate_gmst_deg(float jd) {
    // Calculate centuries since J2000.0
    float T = (jd - 2451545.0f) / 36525.0f;
    
    // GMST at 0h UT in seconds
    float gmst_0h = 24110.54841f + 8640184.812866f * T + 0.093104f * T * T - 0.0000062f * T * T * T;
    
    // Convert to degrees
    float gmst_deg = gmst_0h / 240.0f; // 240 seconds = 1 degree
    
    // Normalize to 0-360 degrees
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    return gmst_deg;
}

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

float calculate_hour_angle_deg(float ra_deg, int local_hour, int local_minute, int local_second) {
    float lst_deg = calculate_local_sidereal_time_deg(local_hour, local_minute, local_second);
    float ha_deg = lst_deg - ra_deg;
    
    // Normalize to -180 to +180 degrees
    while (ha_deg > 180.0f) ha_deg -= 360.0f;
    while (ha_deg < -180.0f) ha_deg += 360.0f;
    
    return ha_deg;
}

void test_coordinate_calculations() {
    std::cout << "=== Telescope Coordinate Calculation Test ===" << std::endl;
    std::cout << "Location: Damascus (Lat: " << SITE_LATITUDE << "°, Lon: " << SITE_LONGITUDE << "°)" << std::endl;
    std::cout << std::endl;
    
    // Test case 1: Local time 12:00:00
    int local_h = 12, local_m = 0, local_s = 0;
    float lst_deg = calculate_local_sidereal_time_deg(local_h, local_m, local_s);
    
    std::cout << "Test Case 1: Local Time 12:00:00" << std::endl;
    std::cout << "Local Sidereal Time: " << std::fixed << std::setprecision(4) << lst_deg << "°" << std::endl;
    
    // Test with a known object (e.g., RA = 5.5 hours = 82.5°)
    float ra_hours = 5.5f;
    float ra_deg = ra_hours * 15.0f;
    float ha_deg = calculate_hour_angle_deg(ra_deg, local_h, local_m, local_s);
    
    std::cout << "Target RA: " << ra_hours << " hours (" << ra_deg << "°)" << std::endl;
    std::cout << "Calculated Hour Angle: " << ha_deg << "°" << std::endl;
    std::cout << std::endl;
    
    // Test case 2: Different time
    local_h = 20, local_m = 30, local_s = 0;
    lst_deg = calculate_local_sidereal_time_deg(local_h, local_m, local_s);
    ha_deg = calculate_hour_angle_deg(ra_deg, local_h, local_m, local_s);
    
    std::cout << "Test Case 2: Local Time 20:30:00" << std::endl;
    std::cout << "Local Sidereal Time: " << lst_deg << "°" << std::endl;
    std::cout << "Target RA: " << ra_hours << " hours (" << ra_deg << "°)" << std::endl;
    std::cout << "Calculated Hour Angle: " << ha_deg << "°" << std::endl;
    std::cout << std::endl;
    
    // Test case 3: User's specific time (16:46:33)
    local_h = 16, local_m = 46, local_s = 33;
    lst_deg = calculate_local_sidereal_time_deg(local_h, local_m, local_s);
    ha_deg = calculate_hour_angle_deg(ra_deg, local_h, local_m, local_s);
    
    std::cout << "Test Case 3: Local Time 16:46:33 (User's time)" << std::endl;
    std::cout << "Local Sidereal Time: " << lst_deg << "°" << std::endl;
    std::cout << "Expected LST: ~60.33°" << std::endl;
    std::cout << "Target RA: " << ra_hours << " hours (" << ra_deg << "°)" << std::endl;
    std::cout << "Calculated Hour Angle: " << ha_deg << "°" << std::endl;
    std::cout << std::endl;
    
    // Test Julian Day calculation
    float jd = calculate_julian_day(2024, 1, 1, 12, 0, 0);
    std::cout << "Julian Day for 2024-01-01 12:00:00 UTC: " << std::setprecision(6) << jd << std::endl;
    
    // Let's verify with a known reference: J2000.0 = 2451545.0 (2000-01-01 12:00 UTC)
    // From 2000-01-01 to 2024-01-01 is 24 years = 24*365.25 = 8766 days approximately
    // So expected JD should be around 2451545.0 + 8766 = 2460311.0
    float expected_jd = 2460311.0f;
    float error = fabs(jd - expected_jd);
    std::cout << "Expected JD (approximate): " << expected_jd << std::endl;
    std::cout << "Error: " << error << std::endl;
    
    if (error < 0.1f) {
        std::cout << "✓ Julian Day calculation is accurate" << std::endl;
    } else {
        std::cout << "✗ Julian Day calculation has significant error" << std::endl;
    }
}

int main() {
    test_coordinate_calculations();
    return 0;
}
