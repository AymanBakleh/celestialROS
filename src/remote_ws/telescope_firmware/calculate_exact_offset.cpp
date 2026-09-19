#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

const float SITE_LONGITUDE = 36.31167f;

int main() {
    std::cout << "=== Calculating Exact Base Offset ===" << std::endl;
    
    int year = 2026, month = 3, day = 25;
    int local_hours = 14, local_minutes = 55, local_seconds = 21;
    
    // Set up the time structure
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    // Convert to UTC
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
    
    std::cout << "UTC Time: " << utc_tm->tm_hour << ":" << utc_tm->tm_min << ":" << utc_tm->tm_sec << std::endl;
    std::cout << "UT Decimal: " << std::fixed << std::setprecision(6) << ut_decimal << std::endl;
    std::cout << "Julian Day: " << std::setprecision(6) << jd << std::endl;
    std::cout << "Days since J2000.0: " << std::setprecision(2) << d << std::endl;
    std::cout << "Longitude: " << SITE_LONGITUDE << std::endl;
    std::cout << std::endl;
    
    // Calculate exact base offset needed
    // lst = base_offset + 0.98564736628 * d + longitude + 15 * ut
    // base_offset = lst - 0.98564736628 * d - longitude - 15 * ut
    
    double expected_lst = 11.64;
    double base_offset = expected_lst - 0.98564736628 * d - SITE_LONGITUDE - 15.0 * ut_decimal;
    
    std::cout << "Expected LST: " << expected_lst << "°" << std::endl;
    std::cout << "Calculated base offset: " << std::setprecision(6) << base_offset << std::endl;
    std::cout << std::endl;
    
    // Test with this base offset
    double lst_test = base_offset + 0.98564736628 * d + SITE_LONGITUDE + 15.0 * ut_decimal;
    lst_test = fmod(lst_test, 360.0);
    if (lst_test < 0) lst_test += 360.0;
    
    std::cout << "Test with calculated offset: " << std::setprecision(4) << lst_test << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_test - expected_lst) << "°" << std::endl;
    
    // Also try the normalized version (add/subtract multiples of 360)
    double normalized_offset = fmod(base_offset, 360.0);
    if (normalized_offset < 0) normalized_offset += 360.0;
    
    std::cout << std::endl;
    std::cout << "Normalized base offset (0-360): " << std::setprecision(6) << normalized_offset << std::endl;
    
    double lst_norm = normalized_offset + 0.98564736628 * d + SITE_LONGITUDE + 15.0 * ut_decimal;
    lst_norm = fmod(lst_norm, 360.0);
    if (lst_norm < 0) lst_norm += 360.0;
    
    std::cout << "Test with normalized offset: " << std::setprecision(4) << lst_norm << "°" << std::endl;
    std::cout << "Difference: " << std::fabs(lst_norm - expected_lst) << "°" << std::endl;
    
    return 0;
}
