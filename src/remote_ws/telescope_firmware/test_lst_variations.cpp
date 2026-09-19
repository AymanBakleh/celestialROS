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

// Test variations of the simplified formula
float calculate_lst_variation(int year, int month, int day, 
                              int local_hours, int local_minutes, int local_seconds,
                              float longitude_mult, float base_offset) {
    // Convert to UTC
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = base_offset + 0.985647 * d + longitude_mult * longitude + 15 * UT
    float lst_deg = base_offset + 0.985647f * d + longitude_mult * SITE_LONGITUDE + 15.0f * ut;
    
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Testing LST Formula Variations ===" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << "Current calculation: ~37.6°" << std::endl;
    std::cout << std::endl;
    
    // Test different variations
    std::cout << "Testing variations:" << std::endl;
    
    // Variation 1: Negative longitude (West positive convention)
    float lst1 = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, -1.0f, 100.46f);
    std::cout << "1. Negative longitude: " << std::fixed << std::setprecision(4) << lst1 << "°" << std::endl;
    std::cout << "   Diff from expected: " << std::fabs(lst1 - 11.64f) << "°" << std::endl;
    
    // Variation 2: Different base offset
    float lst2 = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, 1.0f, 280.46f);
    std::cout << "2. Base offset 280.46: " << std::fixed << std::setprecision(4) << lst2 << "°" << std::endl;
    std::cout << "   Diff from expected: " << std::fabs(lst2 - 11.64f) << "°" << std::endl;
    
    // Variation 3: Subtract 180 degrees
    float lst3 = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, 1.0f, 100.46f);
    lst3 = fmod(lst3 - 180.0f, 360.0f);
    if (lst3 < 0) lst3 += 360.0f;
    std::cout << "3. Subtract 180°: " << std::fixed << std::setprecision(4) << lst3 << "°" << std::endl;
    std::cout << "   Diff from expected: " << std::fabs(lst3 - 11.64f) << "°" << std::endl;
    
    // Variation 4: Different coefficient
    float lst4 = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, 1.0f, 100.46f - 26.0f);
    std::cout << "4. Base offset -26°: " << std::fixed << std::setprecision(4) << lst4 << "°" << std::endl;
    std::cout << "   Diff from expected: " << std::fabs(lst4 - 11.64f) << "°" << std::endl;
    
    // Variation 5: Try to match expected by adjusting base offset
    float current_lst = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, 1.0f, 100.46f);
    float needed_offset = 100.46f - (current_lst - 11.64f);
    float lst5 = calculate_lst_variation(year, month, day, local_hours, local_minutes, local_seconds, 1.0f, needed_offset);
    std::cout << "5. Calibrated offset: " << std::fixed << std::setprecision(4) << lst5 << "°" << std::endl;
    std::cout << "   Needed offset: " << needed_offset << "°" << std::endl;
    
    // Check if expected might be in different units
    float expected_in_hours = 11.64f / 15.0f;
    std::cout << std::endl;
    std::cout << "Expected 11.64° in hours: " << expected_in_hours << "h" << std::endl;
    
    return 0;
}
