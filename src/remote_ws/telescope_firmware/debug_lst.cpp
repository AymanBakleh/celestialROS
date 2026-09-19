#include <iostream>
#include <iomanip>
#include <cmath>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

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
    
    // Get the UT time from the Julian Day
    float jd_int = floor(jd);
    float jd_frac = jd - jd_int;
    float ut_hours = jd_frac * 24.0f;
    
    // Add time since 0h UT to GMST
    // Earth rotates approximately 360.985607 degrees per sidereal day
    // So the rate is about 1.00273790935 times the solar rate
    float gmst_deg = (gmst_0h / 240.0f) + (ut_hours * 15.04106864f);
    
    // Normalize to 0-360 degrees
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    return gmst_deg;
}

float calculate_lst_deg(float jd) {
    float gmst_deg = calculate_gmst_deg(jd);
    float lst_deg = gmst_deg + SITE_LONGITUDE;
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    return lst_deg;
}

int main() {
    // Test for March 24, 2026 13:28:09 UTC (16:28:09 Damascus time)
    float jd = calculate_julian_day(2026, 3, 24, 13, 28, 9);
    std::cout << "Julian Day: " << std::fixed << std::setprecision(6) << jd << std::endl;
    
    float gmst_deg = calculate_gmst_deg(jd);
    std::cout << "GMST: " << std::fixed << std::setprecision(4) << gmst_deg << "°" << std::endl;
    
    float lst_deg = calculate_lst_deg(jd);
    std::cout << "LST: " << std::fixed << std::setprecision(4) << lst_deg << "°" << std::endl;
    
    // Convert expected LST from hours to degrees
    float expected_lst_hours = 60.33f;
    float expected_lst_deg = expected_lst_hours * 15.0f;
    expected_lst_deg = fmod(expected_lst_deg, 360.0f);
    
    std::cout << "Expected LST: " << expected_lst_hours << " hours = " << expected_lst_deg << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_deg - expected_lst_deg) << "°" << std::endl;
    
    // Also show LST in hours for comparison
    float lst_hours = lst_deg / 15.0f;
    std::cout << "LST in hours: " << lst_hours << std::endl;
    
    return 0;
}
