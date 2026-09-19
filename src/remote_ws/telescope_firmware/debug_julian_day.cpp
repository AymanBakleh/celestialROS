#include <iostream>
#include <iomanip>
#include <cmath>

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

int main() {
    // Test for March 24, 2026 13:28:09 UTC (16:28:09 Damascus time)
    float jd = calculate_julian_day(2026, 3, 24, 13, 28, 9);
    std::cout << "Julian Day for 2026-03-24 13:28:09 UTC: " << std::fixed << std::setprecision(6) << jd << std::endl;
    
    // Also test for 2024-01-01 12:00:00 UTC (should be ~2460311.0)
    float jd_ref = calculate_julian_day(2024, 1, 1, 12, 0, 0);
    std::cout << "Julian Day for 2024-01-01 12:00:00 UTC: " << jd_ref << std::endl;
    
    return 0;
}
