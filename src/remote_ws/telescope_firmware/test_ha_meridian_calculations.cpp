#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>

// Constants
const float PI = 3.14159265359f;
const float DEG_TO_RAD = PI / 180.0f;
const float RAD_TO_DEG = 180.0f / PI;

// Site location (Damascus, Syria)
const float SITE_LATITUDE = 33.50917f;
const float SITE_LONGITUDE = 36.31167f;

// Meridian flip threshold in degrees (typically ±60 to ±90)
const float MERIDIAN_FLIP_THRESHOLD = 60.0f;

// Structure to hold test scenario
struct TestScenario {
    std::string name;
    int year, month, day, hour, minute, second;
    float target_ra_hours;  // RA in hours (0-24)
    float target_dec_deg;   // DEC in degrees (-90 to +90)
    float expected_ha_deg;  // Expected hour angle
    bool expect_meridian_flip;
};

// ============ CALCULATION FUNCTIONS ============

// Calculate Julian Day from date/time
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

// Calculate GMST (Greenwich Mean Sidereal Time) in degrees
float calculate_gmst_deg(float jd) {
    float T = (jd - 2451545.0f) / 36525.0f;
    
    // GMST at 0h UT in seconds
    float gmst_0h = 24110.54841f + 8640184.812866f * T + 0.093104f * T * T - 0.0000062f * T * T * T;
    
    // Get the UT time from the Julian Day
    float jd_int = floor(jd);
    float jd_frac = jd - jd_int;
    
    // Seconds since J2000.0 for this specific time
    float ut_seconds = jd_frac * 86400.0f;
    
    // Earth rotation angle
    float rotation_angle = 67310.54841f + (876600.0f * 3600.0f + 8640184.812866f) * T + 0.093104f * T * T;
    
    // GMST in seconds
    float gmst_seconds = gmst_0h + rotation_angle * (ut_seconds / 86400.0f);
    
    // Convert to degrees (240 arcseconds = 1 degree, 86400 seconds = 360 degrees)
    float gmst_deg = (gmst_seconds / 86400.0f) * 360.0f;
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0.0f) gmst_deg += 360.0f;
    
    return gmst_deg;
}

// Calculate Local Sidereal Time in degrees
float calculate_lst_deg(int year, int month, int day, int hour, int minute, int second, float longitude) {
    float jd = calculate_julian_day(year, month, day, hour, minute, second);
    float gmst_deg = calculate_gmst_deg(jd);
    
    // LST = GMST + Longitude
    float lst_deg = gmst_deg + longitude;
    lst_deg = fmod(lst_deg, 360.0f);
    if (lst_deg < 0.0f) lst_deg += 360.0f;
    
    return lst_deg;
}

// Normalize angle to ±180 range
float normalize_angle_180(float angle_deg) {
    while (angle_deg > 180.0f) angle_deg -= 360.0f;
    while (angle_deg < -180.0f) angle_deg += 360.0f;
    return angle_deg;
}

// Normalize angle to 0-360 range
float normalize_angle_360(float angle_deg) {
    angle_deg = fmod(angle_deg, 360.0f);
    if (angle_deg < 0.0f) angle_deg += 360.0f;
    return angle_deg;
}

// Calculate Hour Angle from RA and LST (all in degrees)
float calculate_hour_angle(float lst_deg, float ra_deg) {
    float ha_deg = lst_deg - ra_deg;
    return normalize_angle_180(ha_deg);
}

// Convert RA from hours to degrees
float ra_hours_to_degrees(float ra_hours) {
    return ra_hours * 15.0f;  // 1 hour = 15 degrees
}

// Convert RA from degrees to hours
float ra_degrees_to_hours(float ra_deg) {
    return ra_deg / 15.0f;
}

// Check if meridian flip is needed
bool should_perform_meridian_flip(float current_ha_deg, float target_ha_deg) {
    // Normalize for comparison
    float curr = normalize_angle_180(current_ha_deg);
    float targ = normalize_angle_180(target_ha_deg);
    
    // Check if we're crossing the meridian (HA = 0)
    // This happens when HA changes from negative to positive (approaching to past)
    bool crosses_meridian = (curr < 0.0f && targ > 0.0f) ||
                            (curr > 0.0f && targ < 0.0f);
    
    // Also check distance to meridian
    float dist_to_meridian = targ;
    bool near_meridian = (fabs(dist_to_meridian) < MERIDIAN_FLIP_THRESHOLD);
    
    return crosses_meridian && near_meridian;
}

// Calculate DEC position after meridian flip
float calculate_dec_after_flip(float current_dec_deg) {
    // After meridian flip, we're on the opposite side
    // The new DEC is the negative of the current
    return -current_dec_deg;
}

// ============ TEST SCENARIOS ============

void print_header() {
    std::cout << "\n" << std::string(100, '=') << "\n";
    std::cout << "HOUR ANGLE & MERIDIAN FLIP CALCULATION TEST SUITE\n";
    std::cout << std::string(100, '=') << "\n\n";
}

void print_scenario_header(const TestScenario& scenario, int index) {
    std::cout << "\n" << std::string(100, '-') << "\n";
    std::cout << "SCENARIO " << (index + 1) << ": " << scenario.name << "\n";
    std::cout << std::string(100, '-') << "\n";
}

void print_calculation_result(const TestScenario& scenario, 
                             float jd, float gmst, float lst, 
                             float ra_deg, float current_ha, float target_ha,
                             bool flip_needed) {
    std::cout << std::fixed << std::setprecision(6);
    
    std::cout << "\n[INPUT DATA]\n";
    std::cout << "  Date/Time: " << scenario.year << "-" << std::setfill('0')
              << std::setw(2) << scenario.month << "-" << std::setw(2) << scenario.day << " "
              << std::setw(2) << scenario.hour << ":" << std::setw(2) << scenario.minute << ":"
              << std::setw(2) << scenario.second << " UTC\n";
    std::cout << "  Location: Lat=" << SITE_LATITUDE << "°, Lon=" << SITE_LONGITUDE << "°\n";
    std::cout << "  Target RA: " << scenario.target_ra_hours << "h = " << ra_deg << "°\n";
    std::cout << "  Target DEC: " << scenario.target_dec_deg << "°\n";
    
    std::cout << "\n[CALCULATED VALUES]\n";
    std::cout << "  Julian Day: " << jd << "\n";
    std::cout << "  GMST: " << gmst << "°\n";
    std::cout << "  LST: " << lst << "° (" << ra_degrees_to_hours(lst) << "h)\n";
    std::cout << "  RA (target): " << ra_deg << "° (" << scenario.target_ra_hours << "h)\n";
    std::cout << "  Hour Angle (current): " << current_ha << "°\n";
    std::cout << "  Hour Angle (target): " << target_ha << "°\n";
    
    std::cout << "\n[ANALYSIS]\n";
    std::cout << "  HA Status: ";
    if (current_ha < 0.0f) {
        std::cout << "APPROACHING MERIDIAN (East side, |HA|=" << fabs(current_ha) << "°)\n";
    } else if (current_ha > 0.0f) {
        std::cout << "PAST MERIDIAN (West side, HA=" << current_ha << "°)\n";
    } else {
        std::cout << "ON MERIDIAN (HA=0°)\n";
    }
    
    std::cout << "  Distance to Meridian: " << fabs(current_ha) << "°\n";
    std::cout << "  Meridian Flip Threshold: " << MERIDIAN_FLIP_THRESHOLD << "°\n";
    
    std::cout << "\n[MERIDIAN FLIP DECISION]\n";
    if (flip_needed) {
        std::cout << "  ✓ MERIDIAN FLIP REQUIRED\n";
        std::cout << "    - Reason: Approaching meridian crossing\n";
        float new_dec = calculate_dec_after_flip(scenario.target_dec_deg);
        std::cout << "    - New DEC: " << scenario.target_dec_deg << "° -> " << new_dec << "°\n";
        std::cout << "    - Motor Direction: INVERTED for DEC axis\n";
    } else {
        std::cout << "  ✗ No meridian flip needed\n";
        std::cout << "    - Distance to meridian: " << fabs(current_ha) << "° (threshold: " << MERIDIAN_FLIP_THRESHOLD << "°)\n";
    }
    
    std::cout << "\n[VERIFICATION]\n";
    std::cout << "  Expected HA: " << scenario.expected_ha_deg << "°\n";
    std::cout << "  Calculated HA: " << current_ha << "°\n";
    float error = fabs(scenario.expected_ha_deg - current_ha);
    if (error < 0.5f) {
        std::cout << "  ✓ PASS - Error: " << error << "° (acceptable < 0.5°)\n";
    } else {
        std::cout << "  ✗ FAIL - Error: " << error << "° (unacceptable > 0.5°)\n";
    }
}

// ============ MAIN TEST EXECUTION ============

int main() {
    print_header();
    
    // Define test scenarios
    std::vector<TestScenario> scenarios = {
        {
            "Dawn Observation - Object Approaching Meridian (East)",
            2026, 3, 27, 4, 30, 0,    // 04:30:00 UTC on March 27, 2026
            5.0f,                       // Vega: 5h RA
            38.783f,                    // Vega: +38.78° DEC
            -45.0f,                     // Expected: ~45° before meridian
            false                       // No flip yet
        },
        {
            "Morning Observation - Object On Meridian",
            2026, 3, 27, 6, 0, 0,      // 06:00:00 UTC
            6.0f,                       // Altair: 6h RA (approaching meridian at this location)
            8.867f,                     // Altair: +8.87° DEC
            0.0f,                       // Expected: On meridian
            false
        },
        {
            "Afternoon Observation - Object Past Meridian (West)",
            2026, 3, 27, 8, 0, 0,      // 08:00:00 UTC
            4.0f,                       // Sirius: 4h RA
            -16.716f,                   // Sirius: -16.72° DEC
            60.0f,                      // Expected: ~60° past meridian
            false
        },
        {
            "Critical Scenario - Close to Meridian Flip Threshold (East)",
            2026, 3, 27, 5, 45, 30,    // 05:45:30 UTC
            6.0f,                       // Target at 6h RA
            45.0f,                      // High declination object
            -58.0f,                     // Expected: ~58° before meridian (near flip threshold)
            true                        // SHOULD flip
        },
        {
            "Critical Scenario - Crossing Meridian to West",
            2026, 3, 27, 6, 10, 0,     // 06:10:00 UTC (10 min after meridian)
            6.0f,                       // Same target
            45.0f,
            68.0f,                      // Expected: ~68° past meridian after crossing
            false                       // Flip already done
        },
        {
            "Pole Star - Nearly on Celestial Pole",
            2026, 3, 27, 12, 0, 0,     // 12:00:00 UTC (noon)
            2.31f,                      // Polaris: 2h 31m RA
            89.264f,                    // Polaris: +89.26° DEC (nearly at pole)
            -30.0f,                     // Expected: approaching meridian
            false                       // South celestial pole, different dynamics
        },
        {
            "Southern Hemisphere Object - Negative DEC",
            2026, 3, 27, 7, 30, 0,     // 07:30:00 UTC
            21.0f,                      // Canopus: 21h RA
            -52.696f,                   // Canopus: -52.70° DEC (southern)
            45.0f,                      // Expected: past meridian
            false
        },
        {
            "Equatorial Object - Zero DEC Motion",
            2026, 3, 27, 6, 0, 0,      // 06:00:00 UTC
            18.0f,                      // Hypothetical equatorial object
            0.0f,                       // On celestial equator
            -0.0f,                      // Expected: near meridian
            false
        }
    };
    
    // Run all scenarios
    int pass_count = 0;
    int fail_count = 0;
    
    for (size_t i = 0; i < scenarios.size(); i++) {
        const TestScenario& scenario = scenarios[i];
        print_scenario_header(scenario, i);
        
        // Calculate values
        float jd = calculate_julian_day(scenario.year, scenario.month, scenario.day,
                                       scenario.hour, scenario.minute, scenario.second);
        float gmst = calculate_gmst_deg(jd);
        float lst = calculate_lst_deg(scenario.year, scenario.month, scenario.day,
                                     scenario.hour, scenario.minute, scenario.second,
                                     SITE_LONGITUDE);
        
        float ra_deg = ra_hours_to_degrees(scenario.target_ra_hours);
        float current_ha = calculate_hour_angle(lst, ra_deg);
        float target_ha = current_ha;  // For this scenario, same as current
        
        bool flip_needed = should_perform_meridian_flip(current_ha, target_ha);
        
        print_calculation_result(scenario, jd, gmst, lst, ra_deg, 
                                current_ha, target_ha, flip_needed);
        
        // Check if result matches expectation
        float error = fabs(scenario.expected_ha_deg - current_ha);
        if (error < 0.5f && flip_needed == scenario.expect_meridian_flip) {
            pass_count++;
            std::cout << "\n✓ SCENARIO PASS\n";
        } else {
            fail_count++;
            std::cout << "\n✗ SCENARIO FAIL\n";
            if (error >= 0.5f) {
                std::cout << "  - HA Error: " << error << "° exceeds 0.5°\n";
            }
            if (flip_needed != scenario.expect_meridian_flip) {
                std::cout << "  - Flip decision mismatch. Expected: " << scenario.expect_meridian_flip
                          << ", Got: " << flip_needed << "\n";
            }
        }
    }
    
    // Print summary
    std::cout << "\n" << std::string(100, '=') << "\n";
    std::cout << "TEST SUMMARY\n";
    std::cout << std::string(100, '=') << "\n";
    std::cout << "Total Scenarios: " << scenarios.size() << "\n";
    std::cout << "Passed: " << pass_count << "\n";
    std::cout << "Failed: " << fail_count << "\n";
    std::cout << "Success Rate: " << (pass_count * 100.0f / scenarios.size()) << "%\n";
    std::cout << std::string(100, '=') << "\n";
    
    // Print calculation formulas used
    std::cout << "\n[FORMULAS USED]\n";
    std::cout << "1. Julian Day (JD): floor(365.25*(year+4716)) + floor(30.6001*(month+1)) + day + fraction\n";
    std::cout << "2. GMST: Uses standard astronomical algorithms\n";
    std::cout << "3. Local Sidereal Time (LST): LST = GMST + Longitude\n";
    std::cout << "4. Hour Angle (HA): HA = LST - RA (normalized to ±180°)\n";
    std::cout << "5. Meridian Flip: HA ≈ 0° ± threshold (default: ±60°)\n";
    std::cout << "6. DEC after flip: DEC_new = -DEC_current\n";
    
    std::cout << "\n[COORDINATE SYSTEM REFERENCE]\n";
    std::cout << "HA = -180° to 0°: Object East of meridian (approaching)\n";
    std::cout << "HA = 0°: Object on meridian\n";
    std::cout << "HA = 0° to +180°: Object West of meridian (past)\n";
    std::cout << "DEC: -90° (South pole) to +90° (North pole)\n";
    
    std::cout << "\n[SITE CONFIGURATION]\n";
    std::cout << "Location: Damascus, Syria\n";
    std::cout << "Latitude: " << SITE_LATITUDE << "°\n";
    std::cout << "Longitude: " << SITE_LONGITUDE << "°\n";
    std::cout << "Meridian Flip Threshold: ±" << MERIDIAN_FLIP_THRESHOLD << "°\n";
    
    return (fail_count == 0) ? 0 : 1;
}
