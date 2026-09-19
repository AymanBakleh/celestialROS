#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

const float SITE_LONGITUDE = 36.31167f;

// Test the old simplified formula that might be used in GUI
double calculate_lst_old_simplified(int year, int month, int day, 
                                   int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC manually (like the old method)
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    // Use simplified Julian Day (without time fraction)
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    double jd = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    double d = jd - 2451545.0;
    
    double ut = utc_hours + local_minutes / 60.0 + local_seconds / 3600.0;
    
    // Old simplified formula
    double lst = 100.46 + 0.985647 * d + SITE_LONGITUDE + 15.0 * ut;
    
    lst = fmod(lst, 360.0);
    if (lst < 0) lst += 360.0;
    
    return lst;
}

// Test various base offsets to see if we can match the GUI
void test_different_offsets() {
    std::cout << "=== Testing Different Base Offsets ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int local_hours = 16, local_minutes = 44, local_seconds = 0;
    
    // Convert to UTC
    int utc_hours = local_hours - 3;
    if (utc_hours < 0) utc_hours += 24;
    
    // Calculate d using old method
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    double jd = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    double d = jd - 2451545.0;
    
    double ut = utc_hours + local_minutes / 60.0 + local_seconds / 3600.0;
    
    std::cout << "d: " << d << ", UT: " << ut << ", Longitude: " << SITE_LONGITUDE << std::endl;
    std::cout << std::endl;
    
    // Test different base offsets
    double offsets[] = {100.46, 74.48, 119.48, 50.0, 150.0, 200.0};
    
    for (double offset : offsets) {
        double lst = offset + 0.985647 * d + SITE_LONGITUDE + 15.0 * ut;
        lst = fmod(lst, 360.0);
        if (lst < 0) lst += 360.0;
        
        std::cout << "Offset " << offset << ": " << std::fixed << std::setprecision(4) << lst << "°";
        std::cout << " (diff from GUI: " << std::fabs(lst - 82.79) << "°)" << std::endl;
        
        if (std::fabs(lst - 82.79) < 0.1) {
            std::cout << "  ✅ MATCH FOUND!" << std::endl;
        }
    }
    
    // Calculate what offset would give exactly 82.79°
    double needed_offset = 82.79 - 0.985647 * d - SITE_LONGITUDE - 15.0 * ut;
    std::cout << std::endl;
    std::cout << "Needed offset for exact match: " << needed_offset << std::endl;
}

int main() {
    std::cout << "=== Investigating GUI Calculation Method ===" << std::endl;
    
    // Test old simplified formula
    double lst_old = calculate_lst_old_simplified(2026, 3, 25, 16, 44, 0);
    std::cout << "Old simplified formula: " << std::fixed << std::setprecision(4) << lst_old << "°" << std::endl;
    std::cout << "GUI shows: 82.79°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_old - 82.79) << "°" << std::endl;
    std::cout << std::endl;
    
    test_different_offsets();
    
    return 0;
}
