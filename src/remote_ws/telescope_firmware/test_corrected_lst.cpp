#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

const float SITE_LONGITUDE = 36.31167f;

// Corrected LST calculation function
double calculate_local_sidereal_time_deg_corrected(int year, int month, int day, 
                                                   int local_hours, int local_minutes, int local_seconds) {
    // Convert to UTC
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = local_hours;
    t.tm_min = local_minutes;
    t.tm_sec = local_seconds;

    time_t local_now = mktime(&t);
    time_t utc_now = local_now - 10800; // Damascus UTC+3
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
    
    std::cout << "DEBUG - UT: " << std::fixed << std::setprecision(6) << ut_decimal;
    std::cout << ", JD: " << jd;
    std::cout << ", JD_0h: " << floor(jd - 0.5) + 0.5;
    std::cout << ", d0: " << (floor(jd - 0.5) + 0.5) - 2451545.0 << std::endl;
    
    // Calculate d for 0h UTC of that day (remove fractional part)
    double jd_0h = floor(jd - 0.5) + 0.5;
    double d0 = jd_0h - 2451545.0;
    
    // Calculate GMST at 0h UTC
    double gmst_0h = 100.4606184 + 0.98564736628 * d0;
    
    std::cout << "DEBUG - GMST at 0h UTC: " << gmst_0h;
    std::cout << ", Longitude: " << SITE_LONGITUDE;
    std::cout << ", Rotation: " << 15.041068 * ut_decimal << std::endl;
    
    // Add longitude and rotation since 0h UTC
    double lst_deg = gmst_0h + SITE_LONGITUDE + (15.041068 * ut_decimal);
    
    std::cout << "DEBUG - Before normalization: " << lst_deg << std::endl;
    
    // Normalize
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    std::cout << "DEBUG - Final LST: " << lst_deg << std::endl;
    
    return lst_deg;
}

int main() {
    std::cout << "=== Testing Corrected LST Calculation ===" << std::endl;
    
    // Test with the user's current time
    int year = 2026, month = 3, day = 25;
    int local_hours = 16, local_minutes = 44, local_seconds = 0;
    
    std::cout << "Date: " << year << "-" << month << "-" << day << std::endl;
    std::cout << "Local Time: " << local_hours << ":" << local_minutes << ":" << local_seconds << " (Damascus)" << std::endl;
    std::cout << std::endl;
    
    double lst = calculate_local_sidereal_time_deg_corrected(year, month, day, local_hours, local_minutes, local_seconds);
    
    std::cout << std::endl;
    std::cout << "=== RESULT ===" << std::endl;
    std::cout << "Calculated LST: " << std::fixed << std::setprecision(4) << lst << "°" << std::endl;
    std::cout << "Current GUI shows: 82.79°" << std::endl;
    std::cout << std::endl;
    
    if (std::fabs(lst - 82.79) < 1.0) {
        std::cout << "✅ Close to current GUI value - calculation may be correct" << std::endl;
    } else {
        std::cout << "❌ Different from GUI value - needs further investigation" << std::endl;
    }
    
    // Test with the expected value from earlier
    std::cout << std::endl;
    std::cout << "=== COMPARISON WITH EARLIER EXPECTED VALUE ===" << std::endl;
    std::cout << "Earlier expected: 49.143° (for 15:54)" << std::endl;
    std::cout << "Current calculated: " << lst << "° (for 16:44)" << std::endl;
    
    // The difference should be approximately 1 hour * 15°/hour = 15°
    double expected_diff = (16.0 + 44.0/60.0) - (15.0 + 54.0/60.0); // ~0.833 hours
    double lst_diff = std::fabs(lst - 49.143);
    
    std::cout << "Time difference: " << expected_diff << " hours" << std::endl;
    std::cout << "LST difference: " << lst_diff << "°" << std::endl;
    std::cout << "Expected LST change: " << expected_diff * 15.0 << "°" << std::endl;
    
    return 0;
}
