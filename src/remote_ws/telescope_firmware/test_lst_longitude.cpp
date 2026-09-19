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

// Test with longitude subtraction
float calculate_lst_subtract_long(int year, int month, int day, 
                                int local_hours, int local_minutes, int local_seconds) {
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = 100.46 + 0.985647 * d - longitude + 15 * UT
    float lst = 100.46f + 0.985647f * d - SITE_LONGITUDE + 15.0f * ut;
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

// Test with different longitude value
float calculate_lst_diff_long(int year, int month, int day, 
                            int local_hours, int local_minutes, int local_seconds,
                            float longitude) {
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    float lst = 100.46f + 0.985647f * d + longitude + 15.0f * ut;
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Testing Longitude Handling ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    // Test subtracting longitude
    float lst_subtract = calculate_lst_subtract_long(year, month, day, local_hours, local_minutes, local_seconds);
    std::cout << "Subtract longitude: " << std::fixed << std::setprecision(4) << lst_subtract << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst_subtract - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    // Test with different longitude values
    float longitudes[] = {0.0f, -36.31167f, 323.68833f, 36.31167f / 2.0f, -36.31167f / 2.0f};
    
    for (float lon : longitudes) {
        float lst = calculate_lst_diff_long(year, month, day, local_hours, local_minutes, local_seconds, lon);
        float diff = std::fabs(lst - 11.64f);
        
        std::cout << "Longitude " << std::fixed << std::setprecision(4) << lon << "°: " << 
                     std::setprecision(4) << lst << "° (diff: " << diff << "°)" << std::endl;
        
        if (diff < 1.0f) {
            std::cout << "  ✅ This longitude works!" << std::endl;
        }
    }
    
    // Calculate the exact longitude needed
    float current_lst = calculate_lst_diff_long(year, month, day, local_hours, local_minutes, local_seconds, 0.0f);
    
    // Calculate Julian Day and values
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // Solve for longitude: lst = 100.46 + 0.985647 * d + longitude + 15 * ut
    // longitude = lst - 100.46 - 0.985647 * d - 15 * ut
    float needed_longitude = 11.64f - 100.46f - 0.985647f * d - 15.0f * ut;
    
    std::cout << std::endl;
    std::cout << "Current longitude: " << SITE_LONGITUDE << "°" << std::endl;
    std::cout << "Needed longitude: " << std::fixed << std::setprecision(4) << needed_longitude << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(needed_longitude - SITE_LONGITUDE) << "°" << std::endl;
    
    return 0;
}
