#include <iostream>
#include <iomanip>
#include <cmath>

// Damascus coordinates (same as in mount_control.cpp)
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Exact copy of the function from mount_control.cpp
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

// Exact copy of the LST calculation from mount_control.cpp
float calculate_local_sidereal_time_deg_current(int year, int month, int day, 
                                               int local_hours, int local_minutes, int local_seconds) {
    // Using the standard LST formula: LST = 100.46 + 0.985647 * d + long + 15 * UT
    
    // Get current date from LST date variables
    int lst_year = year;
    int lst_month = month;
    int lst_day = day;
    
    // Convert local time to UTC (Damascus is UTC+3)
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) {
        utc_hours += 24;
        lst_day--;
        if (lst_day < 1) {
            lst_day = 28; // Simplified for March
            lst_month--;
            if (lst_month < 1) {
                lst_month = 12;
                lst_year--;
            }
        }
    }
    
    // Calculate Julian Day Number (simplified version)
    if (lst_month <= 2) {
        lst_year -= 1;
        lst_month += 12;
    }
    
    int A = lst_year / 100;
    int B = 2 - A + A / 4;
    
    float jd = floor(365.25f * (lst_year + 4716)) + 
               floor(30.6001f * (lst_month + 1)) + 
               lst_day + B - 1524.5f;
    
    // Days since J2000.0
    float d = jd - 2451545.0f;
    
    // UT in decimal hours
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // Calculate LST using standard formula
    // LST = 100.46 + 0.985647 * d + longitude + 15 * UT
    float lst_deg = 100.46f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    
    // Normalize to 0-360 degrees
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

// Alternative calculation using proper GMST method
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
    float gmst_deg = (gmst_0h / 240.0f) + (ut_hours * 15.04106864f);
    
    // Normalize to 0-360 degrees
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    return gmst_deg;
}

float calculate_lst_deg_proper(float jd) {
    float gmst_deg = calculate_gmst_deg(jd);
    float lst_deg = gmst_deg + SITE_LONGITUDE;
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    return lst_deg;
}

int main() {
    // Test for current time: March 25, 2026, 14:55:21 Damascus time
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== LST Calculation Test ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Longitude: " << SITE_LONGITUDE << "°E" << std::endl;
    std::cout << std::endl;
    
    // Convert to UTC for proper calculation
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    std::cout << "UTC Time: " << utc_hours << ":" << local_minutes << ":" << local_seconds << std::endl;
    std::cout << std::endl;
    
    // Method 1: Current implementation (simplified formula)
    float lst_current = calculate_local_sidereal_time_deg_current(
        year, month, day, local_hours, local_minutes, local_seconds);
    
    // Method 2: Proper GMST calculation
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float lst_proper = calculate_lst_deg_proper(jd);
    
    std::cout << "=== Results ===" << std::endl;
    std::cout << "Julian Day: " << std::fixed << std::setprecision(6) << jd << std::endl;
    std::cout << std::endl;
    
    std::cout << "Current Implementation (simplified formula):" << std::endl;
    std::cout << "LST: " << std::fixed << std::setprecision(4) << lst_current << "°" << std::endl;
    std::cout << "LST in hours: " << std::fixed << std::setprecision(4) << (lst_current / 15.0f) << "h" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Proper GMST Method:" << std::endl;
    std::cout << "LST: " << std::fixed << std::setprecision(4) << lst_proper << "°" << std::endl;
    std::cout << "LST in hours: " << std::fixed << std::setprecision(4) << (lst_proper / 15.0f) << "h" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Differences ===" << std::endl;
    std::cout << "Current method diff: " << std::fabs(lst_current - 11.64f) << "°" << std::endl;
    std::cout << "Proper method diff: " << std::fabs(lst_proper - 11.64f) << "°" << std::endl;
    std::cout << "Difference between methods: " << std::fabs(lst_current - lst_proper) << "°" << std::endl;
    
    return 0;
}
