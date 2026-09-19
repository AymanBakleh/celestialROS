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

// Test with different coefficients
float calculate_lst_with_coeff(int year, int month, int day, 
                              int local_hours, int local_minutes, int local_seconds,
                              float coeff) {
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
    
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // LST = 100.46 + coeff * d + longitude + 15 * UT
    float lst_deg = 100.46f + coeff * d + SITE_LONGITUDE + 15.0f * ut;
    
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0) lst_deg += 360.0f;
    
    return lst_deg;
}

int main() {
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "=== Testing Different LST Coefficients ===" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    // Test different coefficients
    float coeffs[] = {0.985647f, 0.985600f, 0.985607f, 0.985647f - 0.001f, 0.985647f + 0.001f};
    
    for (float coeff : coeffs) {
        float lst = calculate_lst_with_coeff(year, month, day, local_hours, local_minutes, local_seconds, coeff);
        float diff = std::fabs(lst - 11.64f);
        
        std::cout << "Coeff " << std::fixed << std::setprecision(6) << coeff << ": " << 
                     std::setprecision(4) << lst << "° (diff: " << diff << "°)" << std::endl;
        
        if (diff < 1.0f) {
            std::cout << "  ✅ This coefficient works!" << std::endl;
        }
    }
    
    // Calculate the exact coefficient needed
    float current_lst = calculate_lst_with_coeff(year, month, day, local_hours, local_minutes, local_seconds, 0.985647f);
    
    // Calculate Julian Day and d value
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    float jd = calculate_julian_day(year, month, day, utc_hours, local_minutes, local_seconds);
    float d = jd - 2451545.0f;
    float ut = (float)utc_hours + (float)local_minutes / 60.0f + (float)local_seconds / 3600.0f;
    
    // Solve for coefficient: lst = 100.46 + coeff * d + longitude + 15 * ut
    // coeff = (lst - 100.46 - longitude - 15 * ut) / d
    float needed_coeff = (11.64f - 100.46f - SITE_LONGITUDE - 15.0f * ut) / d;
    
    std::cout << std::endl;
    std::cout << "Current coefficient: 0.985647" << std::endl;
    std::cout << "Needed coefficient: " << std::fixed << std::setprecision(6) << needed_coeff << std::endl;
    std::cout << "Difference: " << std::fabs(needed_coeff - 0.985647f) << std::endl;
    
    return 0;
}
