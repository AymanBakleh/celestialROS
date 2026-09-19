#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

const float SITE_LONGITUDE = 36.31167f;

// Test different Julian Day calculation methods
void test_julian_day_methods() {
    std::cout << "=== Testing Julian Day Calculation Methods ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int hour = 13, minute = 44, second = 0; // UTC time
    
    // Method 1: Current implementation
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    double ut_decimal = hour + minute / 60.0 + second / 3600.0;
    double jd1 = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    
    std::cout << "Method 1 (current): " << jd1 << std::endl;
    
    // Method 2: Standard astronomical formula
    year = 2026; month = 3; day = 25; // Reset
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    A = year / 100;
    B = 2 - A + A / 4;
    
    double jd2 = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    jd2 += ut_decimal / 24.0; // Add time fraction
    
    std::cout << "Method 2 (with time fraction): " << jd2 << std::endl;
    
    // Method 3: Alternative formula
    year = 2026; month = 3; day = 25; // Reset
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    A = year / 100;
    B = 2 - A + A / 4;
    
    double jd3 = 365.25 * (year + 4716) + 30.6001 * (month + 1) + day + B - 1524.5;
    jd3 = floor(jd3) + (ut_decimal / 24.0);
    
    std::cout << "Method 3 (alternative): " << jd3 << std::endl;
    
    std::cout << "Differences:" << std::endl;
    std::cout << "Method 2 - Method 1: " << jd2 - jd1 << std::endl;
    std::cout << "Method 3 - Method 1: " << jd3 - jd1 << std::endl;
}

// Test what happens if we use the GUI's apparent calculation
double reverse_engineer_gui_value() {
    std::cout << "\n=== Reverse Engineering GUI Value ===" << std::endl;
    
    // GUI shows 82.79° for March 25, 2026, 16:44 Damascus time
    // UTC would be 13:44
    double ut_decimal = 13.733333;
    double expected_lst = 82.79;
    
    // Let's see what d would need to be to get this result
    // LST = 100.4606184 + 0.98564736628 * d0 + longitude + 15.041068 * UT
    // d0 = (LST - 100.4606184 - longitude - 15.041068 * UT) / 0.98564736628
    
    double d0_needed = (expected_lst - 100.4606184 - SITE_LONGITUDE - 15.041068 * ut_decimal) / 0.98564736628;
    double jd_0h_needed = d0_needed + 2451545.0;
    
    std::cout << "To get GUI value of " << expected_lst << "°:" << std::endl;
    std::cout << "d0 needed: " << d0_needed << std::endl;
    std::cout << "JD_0h needed: " << jd_0h_needed << std::endl;
    
    // What would the full JD be?
    double jd_full_needed = jd_0h_needed + ut_decimal / 24.0;
    std::cout << "Full JD needed: " << jd_full_needed << std::endl;
    
    return jd_full_needed;
}

int main() {
    test_julian_day_methods();
    reverse_engineer_gui_value();
    
    return 0;
}
