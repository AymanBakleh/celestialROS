#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Test with longitude subtraction
double calculate_lst_subtract_long(int year, int month, int day, 
                                  int local_hours, int local_minutes, int local_seconds) {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    time_t local_now = mktime(&t);
    time_t utc_now = local_now - 10800; 
    struct tm *utc_tm = gmtime(&utc_now);

    int utc_year = utc_tm->tm_year + 1900;
    int utc_month = utc_tm->tm_mon + 1;
    int utc_day = utc_tm->tm_mday;
    
    if (utc_month <= 2) {
        utc_year -= 1;
        utc_month += 12;
    }
    int A = utc_year / 100;
    int B = 2 - A + A / 4;
    
    double ut_decimal = utc_tm->tm_hour + (utc_tm->tm_min / 60.0) + (utc_tm->tm_sec / 3600.0);
    double jd = floor(365.25 * (utc_year + 4716)) + floor(30.6001 * (utc_month + 1)) + utc_day + B - 1524.5;
    
    double d = jd - 2451545.0;
    // Try subtracting longitude instead of adding
    double lst_deg = 100.4606184 + 0.98564736628 * d - SITE_LONGITUDE + 15.0 * ut_decimal;
    
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    return lst_deg; 
}

// Test with different base offset
double calculate_lst_adjusted_offset(int year, int month, int day, 
                                    int local_hours, int local_minutes, int local_seconds,
                                    double base_offset) {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    time_t local_now = mktime(&t);
    time_t utc_now = local_now - 10800; 
    struct tm *utc_tm = gmtime(&utc_now);

    int utc_year = utc_tm->tm_year + 1900;
    int utc_month = utc_tm->tm_mon + 1;
    int utc_day = utc_tm->tm_mday;
    
    if (utc_month <= 2) {
        utc_year -= 1;
        utc_month += 12;
    }
    int A = utc_year / 100;
    int B = 2 - A + A / 4;
    
    double ut_decimal = utc_tm->tm_hour + (utc_tm->tm_min / 60.0) + (utc_tm->tm_sec / 3600.0);
    double jd = floor(365.25 * (utc_year + 4716)) + floor(30.6001 * (utc_month + 1)) + utc_day + B - 1524.5;
    
    double d = jd - 2451545.0;
    double lst_deg = base_offset + 0.98564736628 * d + SITE_LONGITUDE + 15.0 * ut_decimal;
    
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    return lst_deg; 
}

int main() {
    std::cout << "=== Debugging LST Calculation ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Subtract longitude
    double lst1 = calculate_lst_subtract_long(year, month, day, local_hours, local_minutes, local_seconds);
    std::cout << "Subtract longitude: " << std::fixed << std::setprecision(4) << lst1 << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst1 - 11.64) << "°" << std::endl;
    std::cout << std::endl;
    
    // Test 2: Adjust base offset to match expected
    // Calculate what base offset would give us 11.64°
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    time_t local_now = mktime(&t);
    time_t utc_now = local_now - 10800; 
    struct tm *utc_tm = gmtime(&utc_now);

    int utc_year = utc_tm->tm_year + 1900;
    int utc_month = utc_tm->tm_mon + 1;
    int utc_day = utc_tm->tm_mday;
    
    if (utc_month <= 2) {
        utc_year -= 1;
        utc_month += 12;
    }
    int A = utc_year / 100;
    int B = 2 - A + A / 4;
    
    double ut_decimal = utc_tm->tm_hour + (utc_tm->tm_min / 60.0) + (utc_tm->tm_sec / 3600.0);
    double jd = floor(365.25 * (utc_year + 4716)) + floor(30.6001 * (utc_month + 1)) + utc_day + B - 1524.5;
    double d = jd - 2451545.0;
    
    // Solve for base_offset: 11.64 = base_offset + 0.98564736628 * d + longitude + 15 * ut
    double needed_offset = 11.64 - 0.98564736628 * d - SITE_LONGITUDE - 15.0 * ut_decimal;
    
    std::cout << "Current base offset: 100.4606184" << std::endl;
    std::cout << "Needed base offset: " << std::fixed << std::setprecision(6) << needed_offset << std::endl;
    std::cout << std::endl;
    
    // Test with needed offset
    double lst2 = calculate_lst_adjusted_offset(year, month, day, local_hours, local_minutes, local_seconds, needed_offset);
    std::cout << "With needed offset: " << std::fixed << std::setprecision(4) << lst2 << "°" << std::endl;
    std::cout << "Diff from expected: " << std::fabs(lst2 - 11.64) << "°" << std::endl;
    std::cout << std::endl;
    
    // Test 3: Check if the expected value might be using a different longitude convention
    double lst3 = calculate_lst_adjusted_offset(year, month, day, local_hours, local_minutes, local_seconds, 100.4606184);
    std::cout << "Original formula: " << std::fixed << std::setprecision(4) << lst3 << "°" << std::endl;
    std::cout << "If we subtract 360°: " << std::fixed << std::setprecision(4) << (lst3 - 360.0) << "°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst1 - 11.64) < 1.0) {
        std::cout << "✅ Subtracting longitude works!" << std::endl;
    } else if (std::fabs(lst2 - 11.64) < 0.1) {
        std::cout << "✅ Adjusted base offset works!" << std::endl;
    } else if (std::fabs((lst3 - 360.0) - 11.64) < 1.0) {
        std::cout << "✅ Expected value might be using different angle convention" << std::endl;
    } else {
        std::cout << "❌ Need further investigation" << std::endl;
    }
    
    return 0;
}
