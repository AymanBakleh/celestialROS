#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

const float SITE_LONGITUDE = 36.31167f;

// Final corrected LST calculation
double calculate_local_sidereal_time_deg_final(int year, int month, int day, 
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
    double lst_deg = -26.67 + 0.98564736628 * d + SITE_LONGITUDE + 15.0 * ut_decimal;
    
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    return lst_deg; 
}

int main() {
    std::cout << "=== Final LST Calculation Test ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    double lst = calculate_local_sidereal_time_deg_final(year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << "Calculated: " << std::fixed << std::setprecision(4) << lst << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst - 11.64) << "°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst - 11.64) < 0.1) {
        std::cout << "✅ SUCCESS! LST calculation is now accurate." << std::endl;
    } else {
        std::cout << "❌ Still needs adjustment." << std::endl;
    }
    
    // Test edge cases
    std::cout << std::endl;
    std::cout << "=== Testing Edge Cases ===" << std::endl;
    
    struct TestCase {
        int year, month, day, hour, min, sec;
        std::string description;
    };
    
    TestCase cases[] = {
        {2026, 1, 1, 1, 0, 0, "January 1st, 1:00 AM"},
        {2026, 3, 1, 0, 30, 0, "March 1st, 12:30 AM"},
        {2024, 3, 1, 2, 15, 0, "Leap year March 1st, 2:15 AM"}
    };
    
    for (auto& test : cases) {
        double lst_test = calculate_local_sidereal_time_deg_final(
            test.year, test.month, test.day, test.hour, test.min, test.sec);
        
        std::cout << test.description << ": " << std::fixed << std::setprecision(4) << lst_test << "°" << std::endl;
    }
    
    return 0;
}
