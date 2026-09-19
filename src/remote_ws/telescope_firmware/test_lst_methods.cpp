#include <iostream>
#include <iomanip>
#include <cmath>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Test different LST calculation methods
float calculate_julian_day(int year, int month, int day, int hour, int minute, int second) {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    float day_fraction = (float)hour / 24.0f + (float)minute / 1440.0f + (float)second / 86400.0f;
    
    float jd = floor(365.25f * (year + 4716)) + 
               floor(30.6001f * (month + 1)) + 
               day + day_fraction + B - 1524.5f;
    return jd;
}

// Method 1: Current implementation (simplified formula)
float calculate_lst_simplified(int year, int month, int day, 
                              int local_hours, int local_minutes, int local_seconds) {
    // Convert local time to UTC (Damascus is UTC+3)
    int utc_hours = local_hours - 3;
    int utc_day = day;
    int utc_month = month;
    int utc_year = year;
    
    if (utc_hours < 0) {
        utc_hours += 24;
        utc_day--;
        if (utc_day < 1) {
            utc_day = 28; // Simplified for March
            utc_month--;
            if (utc_month < 1) {
                utc_month = 12;
                utc_year--;
            }
        }
    }
    
    // Calculate Julian Day
    if (utc_month <= 2) {
        utc_year -= 1;
        utc_month += 12;
    }
    
    int A = utc_year / 100;
    int B = 2 - A + A / 4;
    
    float jd = floor(365.25f * (utc_year + 4716)) + 
               floor(30.6001f * (utc_month + 1)) + 
               utc_day + B - 1524.5f;
    
    // Days since J2000.0
    float d = jd - 2451545.0f;
    
    // UT in decimal hours
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = 100.46 + 0.985647 * d + longitude + 15 * UT
    float lst_deg = 100.46f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    
    // Normalize to 0-360 degrees
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

// Method 2: Proper GMST calculation
float calculate_gmst_deg(float jd) {
    float T = (jd - 2451545.0f) / 36525.0f;
    float gmst_0h = 24110.54841f + 8640184.812866f * T + 0.093104f * T * T - 0.0000062f * T * T * T;
    
    float jd_int = floor(jd);
    float jd_frac = jd - jd_int;
    float ut_hours = jd_frac * 24.0f;
    
    float gmst_deg = (gmst_0h / 240.0f) + (ut_hours * 15.04106864f);
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    return gmst_deg;
}

float calculate_lst_proper(int year, int month, int day, 
                           int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC
    int utc_hours = local_hours - 3;
    int utc_day = day;
    int utc_month = month;
    int utc_year = year;
    
    if (utc_hours < 0) {
        utc_hours += 24;
        utc_day--;
        if (utc_day < 1) {
            utc_day = 28;
            utc_month--;
            if (utc_month < 1) {
                utc_month = 12;
                utc_year--;
            }
        }
    }
    
    float jd = calculate_julian_day(utc_year, utc_month, utc_day, utc_hours, local_minutes, local_seconds);
    float gmst_deg = calculate_gmst_deg(jd);
    float lst_deg = gmst_deg + SITE_LONGITUDE;
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

// Method 3: Alternative simplified formula
float calculate_lst_alt(int year, int month, int day, 
                        int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC first
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    // Calculate JD for UTC time
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    
    // Days since J2000.0
    float d = jd - 2451545.0f;
    
    // UT in decimal hours
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // Alternative formula: LST = 100.46 + 0.985647 * d + longitude + 15 * UT
    float lst_deg = 100.46f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

int main() {
    // Test for current time: March 25, 2026, 14:55:21 Damascus time
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== LST Calculation Comparison ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Longitude: " << SITE_LONGITUDE << "°E" << std::endl;
    std::cout << std::endl;
    
    // Convert to UTC
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    std::cout << "UTC Time: " << utc_hours << ":" << local_minutes << ":" << local_seconds << std::endl;
    std::cout << std::endl;
    
    // Calculate using all methods
    float lst_simplified = calculate_lst_simplified(year, month, day, local_hours, local_minutes, local_seconds);
    float lst_proper = calculate_lst_proper(year, month, day, local_hours, local_minutes, local_seconds);
    float lst_alt = calculate_lst_alt(year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "=== Results ===" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Method 1 (Current simplified): " << std::fixed << std::setprecision(4) << lst_simplified << "°" << std::endl;
    std::cout << "Difference from expected: " << std::fabs(lst_simplified - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Method 2 (Proper GMST): " << std::fixed << std::setprecision(4) << lst_proper << "°" << std::endl;
    std::cout << "Difference from expected: " << std::fabs(lst_proper - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Method 3 (Alternative simplified): " << std::fixed << std::setprecision(4) << lst_alt << "°" << std::endl;
    std::cout << "Difference from expected: " << std::fabs(lst_alt - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    // Check if any method is close to expected
    bool found_match = false;
    if (std::fabs(lst_simplified - 11.64f) < 1.0f) {
        std::cout << "✓ Method 1 is close to expected!" << std::endl;
        found_match = true;
    }
    if (std::fabs(lst_proper - 11.64f) < 1.0f) {
        std::cout << "✓ Method 2 is close to expected!" << std::endl;
        found_match = true;
    }
    if (std::fabs(lst_alt - 11.64f) < 1.0f) {
        std::cout << "✓ Method 3 is close to expected!" << std::endl;
        found_match = true;
    }
    
    if (!found_match) {
        std::cout << "❌ None of the methods are close to expected 11.64°" << std::endl;
        std::cout << "Expected value might be incorrect or using different reference." << std::endl;
    }
    
    return 0;
}
