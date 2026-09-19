#include <iostream>
#include <iomanip>
#include <cmath>

const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

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

// US Naval Observatory formula
float calculate_lst_usno(int year, int month, int day, 
                        int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    
    // Calculate GMST using USNO formula
    float T = (jd - 2451545.0f) / 36525.0f;
    float gmst = 280.46061837f + 360.98564736629f * (jd - 2451545.0f) + 
                0.000387933f * T * T - T * T * T / 38710000.0f;
    
    gmst = fmod(gmst, 360.0f);
    if (gmst < 0) gmst += 360.0f;
    
    // Add longitude to get LST
    float lst = gmst + SITE_LONGITUDE;
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

// Alternative formula from practical astronomy
float calculate_lst_practical(int year, int month, int day, 
                             int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    
    // UT in decimal hours
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // Practical astronomy formula
    float lst = 100.46f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

// Try without longitude (just to see)
float calculate_lst_no_long(int year, int month, int day, 
                           int local_hours, int local_minutes, int local_seconds) {
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    float lst = 100.46f + 0.985647f * d + 15.0f * ut;  // No longitude
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Testing Different LST Formulas ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Longitude: " << SITE_LONGITUDE << "°E" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    float lst_usno = calculate_lst_usno(year, month, day, local_hours, local_minutes, local_seconds);
    float lst_practical = calculate_lst_practical(year, month, day, local_hours, local_minutes, local_seconds);
    float lst_no_long = calculate_lst_no_long(year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "US Naval Observatory: " << std::fixed << std::setprecision(4) << lst_usno << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst_usno - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Practical Astronomy: " << std::fixed << std::setprecision(4) << lst_practical << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst_practical - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Without longitude: " << std::fixed << std::setprecision(4) << lst_no_long << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst_no_long - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    // Check if any are close
    if (std::fabs(lst_usno - 11.64f) < 1.0f) {
        std::cout << "✅ USNO formula is close!" << std::endl;
    }
    if (std::fabs(lst_practical - 11.64f) < 1.0f) {
        std::cout << "✅ Practical formula is close!" << std::endl;
    }
    if (std::fabs(lst_no_long - 11.64f) < 1.0f) {
        std::cout << "✅ No longitude formula is close!" << std::endl;
    }
    
    return 0;
}
