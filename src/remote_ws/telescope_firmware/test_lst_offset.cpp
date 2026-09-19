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

// Calculate LST with adjustable base offset
float calculate_lst_with_offset(int year, int month, int day, 
                                int local_hours, int local_minutes, int local_seconds,
                                float base_offset) {
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = base_offset + 0.985647 * d + longitude + 15 * UT
    float lst = base_offset + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    lst = fmod(lst, 360.0f);
    if (lst < 0) lst += 360.0f;
    
    return lst;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Finding Correct Base Offset ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    // Calculate current values
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    std::cout << "Julian Day: " << std::fixed << std::setprecision(6) << jd << std::endl;
    std::cout << "Days since J2000.0: " << std::setprecision(2) << d << std::endl;
    std::cout << "UT hours: " << std::setprecision(4) << ut << std::endl;
    std::cout << std::endl;
    
    // Calculate the exact base offset needed
    // lst = base_offset + 0.985647 * d + longitude + 15 * ut
    // base_offset = lst - 0.985647 * d - longitude - 15 * ut
    float needed_offset = 11.64f - 0.985647f * d - SITE_LONGITUDE - 15.0f * ut;
    
    std::cout << "Current base offset: 100.46" << std::endl;
    std::cout << "Needed base offset: " << std::fixed << std::setprecision(4) << needed_offset << std::endl;
    std::cout << std::endl;
    
    // Test with the needed offset
    float lst_needed = calculate_lst_with_offset(year, month, day, local_hours, local_minutes, local_seconds, needed_offset);
    std::cout << "With needed offset: " << std::setprecision(4) << lst_needed << "°" << std::endl;
    std::cout << "Difference from expected: " << std::fabs(lst_needed - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    // Test with some reasonable offsets around the needed value
    float offsets[] = {needed_offset, 100.46f, 0.0f, -26.0f, 74.0f};
    
    for (float offset : offsets) {
        float lst = calculate_lst_with_offset(year, month, day, local_hours, local_minutes, local_seconds, offset);
        float diff = std::fabs(lst - 11.64f);
        
        std::cout << "Offset " << std::fixed << std::setprecision(2) << offset << ": " << 
                     std::setprecision(4) << lst << "° (diff: " << diff << "°)" << std::endl;
        
        if (diff < 0.1f) {
            std::cout << "  ✅ Perfect match!" << std::endl;
        } else if (diff < 1.0f) {
            std::cout << "  ✅ Close enough!" << std::endl;
        }
    }
    
    return 0;
}
