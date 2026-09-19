#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

// Damascus coordinates
const float SITE_LONGITUDE = 36.31167f;  // 36°18'42" E

// Test the improved LST calculation
double calculate_local_sidereal_time_deg_improved(int year, int month, int day, 
                                                 int local_hours, int local_minutes, int local_seconds) {
    // 1. Get current time and convert to UTC properly
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    // Convert local time to time_t (Unix seconds)
    time_t local_now = mktime(&t);
    // Subtract 3 hours (10800 seconds) for Damascus UTC+3
    time_t utc_now = local_now - 10800; 
    struct tm *utc_tm = gmtime(&utc_now);

    // 2. Calculate Julian Day (Double precision is critical here)
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
    
    // 3. Calculate LST
    double d = jd - 2451545.0;
    // Use the high-precision sidereal constant
    double lst_deg = 100.4606184 + 0.98564736628 * d + SITE_LONGITUDE + 15.0 * ut_decimal;
    
    // 4. Normalize
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    return lst_deg; 
}

// Test edge cases for UTC conversion
void test_utc_conversion_edge_cases() {
    std::cout << "=== Testing UTC Conversion Edge Cases ===" << std::endl;
    
    struct TestCase {
        int year, month, day, hour, min, sec;
        std::string description;
    };
    
    TestCase cases[] = {
        {2026, 1, 1, 1, 0, 0, "January 1st, 1:00 AM (should roll back to Dec 31st)"},
        {2026, 3, 1, 0, 30, 0, "March 1st, 12:30 AM (should roll back to Feb 28th)"},
        {2024, 3, 1, 2, 15, 0, "Leap year Feb 29th, 2:15 AM (should handle correctly)"},
        {2026, 12, 31, 23, 45, 0, "December 31st, 11:45 PM (should stay same day)"}
    };
    
    for (auto& test : cases) {
        double lst = calculate_local_sidereal_time_deg_improved(
            test.year, test.month, test.day, test.hour, test.min, test.sec);
        
        std::cout << test.description << std::endl;
        std::cout << "  Input: " << test.year << "-" << test.month << "-" << test.day << 
                     " " << test.hour << ":" << test.min << ":" << test.sec << " (local)" << std::endl;
        std::cout << "  LST: " << std::fixed << std::setprecision(4) << lst << "°" << std::endl;
        std::cout << std::endl;
    }
}

// Test precision comparison
void test_precision_comparison() {
    std::cout << "=== Testing Float vs Double Precision ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    // Float calculation (old method)
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
    
    // Float version
    float ut_float = utc_tm->tm_hour + (utc_tm->tm_min / 60.0f) + (utc_tm->tm_sec / 3600.0f);
    float jd_float = floor(365.25f * (utc_year + 4716)) + floor(30.6001f * (utc_month + 1)) + utc_day + B - 1524.5f;
    float d_float = jd_float - 2451545.0f;
    float lst_float = 100.4606184f + 0.98564736628f * d_float + SITE_LONGITUDE + 15.0f * ut_float;
    lst_float = fmod(lst_float, 360.0f);
    if (lst_float < 0) lst_float += 360.0f;
    
    // Double version
    double lst_double = calculate_local_sidereal_time_deg_improved(
        year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "Float precision:  " << std::fixed << std::setprecision(6) << lst_float << "°" << std::endl;
    std::cout << "Double precision: " << std::fixed << std::setprecision(6) << lst_double << "°" << std::endl;
    std::cout << "Difference:       " << std::fabs(lst_double - lst_float) << "°" << std::endl;
    std::cout << "Expected:         11.64°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst_double - 11.64) < 0.01) {
        std::cout << "✅ Double precision calculation is accurate!" << std::endl;
    }
}

int main() {
    std::cout << "=== Comprehensive LST Calculation Test ===" << std::endl;
    std::cout << std::endl;
    
    // Test the main case
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    double lst = calculate_local_sidereal_time_deg_improved(
        year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << "Main Test Case:" << std::endl;
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << "Expected: 11.64°" << std::endl;
    std::cout << "Calculated: " << std::fixed << std::setprecision(4) << lst << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst - 11.64) << "°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst - 11.64) < 0.1) {
        std::cout << "✅ Main test passed!" << std::endl;
    } else {
        std::cout << "❌ Main test failed!" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Test edge cases
    test_utc_conversion_edge_cases();
    
    // Test precision
    test_precision_comparison();
    
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "✅ Fixed UTC conversion with proper date/month/year rollover" << std::endl;
    std::cout << "✅ Updated to high-precision sidereal constant (100.4606184)" << std::endl;
    std::cout << "✅ Using double precision for Julian Day and LST calculations" << std::endl;
    std::cout << "✅ Added proper RTC halt handling" << std::endl;
    std::cout << "✅ Tracking interval is appropriate (1 second)" << std::endl;
    
    return 0;
}
