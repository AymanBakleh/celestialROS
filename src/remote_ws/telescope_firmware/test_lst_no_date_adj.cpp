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

// Test without date adjustment for UTC conversion
float calculate_lst_no_date_adj(int year, int month, int day, 
                                int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC without adjusting date
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    // Use original date with UTC time
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = -26.0 + 0.985647 * d + longitude + 15 * UT
    float lst_deg = -26.0f + 0.985647f * d + SITE_LONGITUDE + 15.0f * ut;
    
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Testing LST Without Date Adjustment ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Longitude: " << SITE_LONGITUDE << "°E" << std::endl;
    std::cout << std::endl;
    
    float lst_no_adj = calculate_lst_no_date_adj(year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << "No date adjustment: " << std::fixed << std::setprecision(4) << lst_no_adj << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_no_adj - 11.64f) << "°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst_no_adj - 11.64f) < 1.0f) {
        std::cout << "✅ This works! The issue was date adjustment." << std::endl;
    } else {
        std::cout << "❌ Still not working." << std::endl;
    }
    
    return 0;
}
