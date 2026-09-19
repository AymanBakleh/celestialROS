#include "mount_control.h"
#include "config.h"
#include "servo_control.h"
#include "ds1302.h"
#include "web_server.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

// RTC instance - using working Ds1302 class from examples
static Ds1302 rtc(RTC_CE_PIN, RTC_SCLK_PIN, RTC_IO_PIN);

// ============ GLOBALS ============
int16_t last_ra_pos_speed = 0;
int16_t last_dec_pos_speed = 0;
unsigned long last_speed_update = 0;
// last commanded or measured servo speed (library units)
float ra_speed = 0.0f;
float dec_speed = 0.0f;

int32_t ra_zero_steps = 0;
int32_t dec_zero_steps = 0;

bool torque_enabled = false;
bool ros_started = false;

bool velocityMode = false;
float vel_ra = 0.0f;
float vel_dec = 0.0f;
float vel_ra_acc = 0.0f;
unsigned long lastVelTime = 0;

int velocity_ra_spd = 0;
int velocity_dec_spd = 0;
int velocity_ra_acc = 0;
int velocity_dec_acc = 0;



float target_ra_rad = 0.0f;
float target_dec_rad = 0.0f;

int32_t current_ra_steps = 0;
int32_t current_dec_steps = 0;
int16_t last_ra_pos = 0;
int16_t last_dec_pos = 0;

float ra_ratio = DEFAULT_RA_RATIO;
float dec_ratio = DEFAULT_DEC_RATIO;
float steps_per_rad_ra;
float steps_per_rad_dec;

// NOTE: ra_spd/dec_spd are **raw speed units** sent directly to the library.
// They default to a safe moderate value defined by GOTO_SPEED.
int ra_spd = 2000;  // Default 2000, adjustable via web GUI (range 0-4000)
int dec_spd = 2000;  // Use same default as RA
int ra_acc = DEFAULT_ACC, dec_acc = DEFAULT_ACC;

unsigned long lastFeedbackTime = 0;

int32_t last_sent_ra_steps = 0;
int32_t last_sent_dec_steps = 0;

static unsigned long last_motor_cmd_time = 0;

// servo mode: 0, motor(wheel) mode: 3
static int ra_servo_mode = -1;
static int dec_servo_mode = -1;

// Absolute targets in steps (multi-turn, relative to last zero)
static int32_t target_ra_steps = 0;
static int32_t target_dec_steps = 0;

// Desired angles (as last received from Stellarium / web)
static float desired_ra_deg = 0.0f;
static float desired_dec_deg = 0.0f;
static float last_target_ha_deg = 0.0f;

// ============ SETTLING STATE (prevents oscillation) ============
static bool ra_settled = false;
static bool dec_settled = false;
static unsigned long last_ra_cmd_time = 0;
static unsigned long last_dec_cmd_time = 0;

struct PidState {
    float i = 0.0f;
    float prev_err = 0.0f;
    unsigned long last_ms = 0;
};
static PidState s_pid_ra;
static PidState s_pid_dec;

// Meridian flip state variables
static bool meridian_flip_scheduled = false;
static bool meridian_flip_in_progress = false;
static unsigned long meridian_flip_time = 0;
static bool meridian_flipped = false;  // Tracks if we're currently on the flipped side
// Configurable HA threshold for meridian flip (degrees, default ±90° = 6h)
static float meridian_flip_ha_threshold = 90.0f;

// Runtime motor direction variables (can override compile-time defines)
static bool ra_inverted_runtime = RA_INVERTED;
static bool dec_inverted_runtime = DEC_INVERTED;

static bool effective_dec_inverted(void) {
    // On flipped pier side, DEC motor sense relative to sky reverses.
    return dec_inverted_runtime ^ meridian_flipped;
}

// Location/time for meridian flip
float site_latitude_deg = 33.50917f;   // Damascus 33°30'33" N
float site_longitude_deg = 36.31167f;  // Damascus 36°18'42" E
int local_time_hours = 0;
int local_time_minutes = 0;
int local_time_seconds = 0;

// Time management modes
enum TimeMode {
    TIME_MODE_DEFAULT = 0,
    TIME_MODE_WEB = 1,
    TIME_MODE_RTC = 2
};
static TimeMode current_time_mode = TIME_MODE_DEFAULT;
static bool rtc_available = false;

// Tracking mode variables
static int tracking_mode = DEFAULT_TRACKING_MODE;
static float tracking_rate = 0.0f;  // degrees per second
static unsigned long last_tracking_update = 0;
static bool auto_tracking_enabled = false;

// Date used by LST/JD calculations; update via RTC sync or explicit date set.
static int lst_year = 2026, lst_month = 3, lst_day = 25;

static int pid_speed_from_error(PidState& pid, int32_t err_steps, unsigned long now_ms) {
    float e = (float)abs(err_steps);
    float dt = 0.0f;
    if (pid.last_ms != 0 && now_ms > pid.last_ms) dt = (now_ms - pid.last_ms) / 1000.0f;
    pid.last_ms = now_ms;

    if (dt > 0.0f) {
        pid.i += e * dt;
        if (pid.i > PID_I_LIMIT) pid.i = PID_I_LIMIT;
        if (pid.i < -PID_I_LIMIT) pid.i = -PID_I_LIMIT;
    }

    float d = 0.0f;
    if (dt > 0.0f) d = (e - pid.prev_err) / dt;
    pid.prev_err = e;

    float out = PID_KP * e + PID_KI * pid.i + PID_KD * d;

    // slow down near target
    if (e < 400.0f) {
        out = max(out * (e / 400.0f), (float)SERVO_MIN_SPEED);
    }

    int spd = (int)out;
    spd = constrain(spd, 0, SERVO_MAX_SPEED);
    if (spd > 0 && spd < SERVO_MIN_SPEED) spd = SERVO_MIN_SPEED;
    return spd;
}

// ============ CONVERSIONS ============
// Normalize RA angle to [0, 360) range for circular wrapping
float normalize_ra_deg(float ra_deg) {
    while (ra_deg >= 360.0f) ra_deg -= 360.0f;
    while (ra_deg < 0.0f) ra_deg += 360.0f;
    return ra_deg;
}

static float normalize_signed_deg(float angle_deg) {
    while (angle_deg > 180.0f) angle_deg -= 360.0f;
    while (angle_deg <= -180.0f) angle_deg += 360.0f;
    return angle_deg;
}

float normalize_ha_deg(float ha_deg) {
    return normalize_ra_deg(ha_deg);
}

static float angular_distance_deg(float from_deg, float to_deg) {
    return normalize_signed_deg(to_deg - from_deg);
}

static void format_ha_hms(float ha_deg_0_360, char* out, size_t out_size) {
    float ha_hours = ha_deg_0_360 / 15.0f;
    int h = (int)ha_hours;
    int m = (int)((ha_hours - h) * 60.0f);
    int s = (int)((((ha_hours - h) * 60.0f) - m) * 60.0f);
    snprintf(out, out_size, "%02d:%02d:%02d", h, m, s);
}

static void format_signed_ha_hms(float ha_deg_signed, char* out, size_t out_size) {
    float ha_hours = fabsf(ha_deg_signed) / 15.0f;
    int h = (int)ha_hours;
    int m = (int)((ha_hours - h) * 60.0f);
    int s = (int)((((ha_hours - h) * 60.0f) - m) * 60.0f);
    char sign = (ha_deg_signed < 0.0f) ? '-' : '+';
    snprintf(out, out_size, "%c%02d:%02d:%02d", sign, h, m, s);
}

static bool needs_flip_for_ra_limits(float ra_deg) {
    float ra_norm = normalize_ra_deg(ra_deg);
    return (ra_norm > 90.0f && ra_norm < 180.0f) || (ra_norm >= 180.0f && ra_norm <= 270.0f);
}
// Normalize DEC angle to [-90, +90] range with hard limits
// When DEC goes beyond limits, clamp to limit value
float normalize_dec_deg(float dec_deg) {
    if (dec_deg > DEC_MAX_LIMIT) {
        dec_deg = DEC_MAX_LIMIT;
    } else if (dec_deg < DEC_MIN_LIMIT) {
        dec_deg = DEC_MIN_LIMIT;
    }
    return dec_deg;
}

static float sky_to_motor_dec_deg(float sky_dec_deg) {
    // Keep DEC coordinate continuous across meridian flips.
    // Flip handling is done by motor direction (effective_dec_inverted),
    // so DEC targets should not be mirrored around +90°.
    return normalize_dec_deg(sky_dec_deg);
}

static float motor_to_sky_dec_deg(float motor_dec_deg) {
    return normalize_dec_deg(motor_dec_deg);
}

static float sky_to_motor_ra_deg(float sky_ra_deg) {
    float ra = normalize_ra_deg(sky_ra_deg);
    if (meridian_flipped) {
        ra = normalize_ra_deg(ra + 180.0f);
    }
    return ra;
}

static float motor_to_sky_ra_deg(float motor_ra_deg) {
    float ra = normalize_ra_deg(motor_ra_deg);
    if (meridian_flipped) {
        ra = normalize_ra_deg(ra - 180.0f);
    }
    return ra;
}

// Calculate DEC target accounting for meridian flip state
// In normal mode: direct conversion from degrees to steps
// In flipped mode: calculate position from opposite side of meridian
float calculate_dec_target_deg_with_flip(float target_dec_deg) {
    return sky_to_motor_dec_deg(target_dec_deg);
}

// Calculate DEC steps accounting for meridian flip with motor direction compensation
int32_t calculate_dec_target_steps_with_flip(float target_dec_deg) {
    float adjusted_dec_deg = calculate_dec_target_deg_with_flip(target_dec_deg);
    int32_t target_steps = (int32_t)lroundf(adjusted_dec_deg * TICKS_PER_AXIS_DEG);
    
    Serial.print("CALC_DEC: target_deg=");
    Serial.print(target_dec_deg, 3);
    Serial.print(", meridian_flipped=");
    Serial.print(meridian_flipped);
    Serial.print(", adjusted_deg=");
    Serial.print(adjusted_dec_deg, 3);
    Serial.print(", steps=");
    Serial.println(target_steps);
    
    return target_steps;
}

static void select_best_pier_side_target(float input_ra_deg,
                                         float input_dec_deg,
                                         float current_ra_deg,
                                         float current_dec_deg,
                                         float lst_deg,
                                         float* out_ra_deg,
                                         float* out_dec_deg,
                                         bool* out_flip_applied) {
    // Candidate A: normal side
    float ra_a = normalize_ra_deg(input_ra_deg);
    float dec_a = normalize_dec_deg(input_dec_deg);

    // Candidate B: flipped side (RA+180) while keeping the same sky DEC.
    // DEC sign must stay astronomical (-90..+90); pier-side handles motor sense.
    float ra_b = normalize_ra_deg(ra_a + 180.0f);
    float dec_b = dec_a;

    // Evaluate movement cost from current pose.
    float ra_move_a = fabsf(ra_angular_distance(current_ra_deg, ra_a));
    float ra_move_b = fabsf(ra_angular_distance(current_ra_deg, ra_b));
    float dec_move_a = fabsf(dec_a - current_dec_deg);
    float dec_move_b = fabsf(dec_b - current_dec_deg);

    // Prefer smaller RA motion, but include DEC in the decision.
    float cost_a = ra_move_a + 0.35f * dec_move_a;
    float cost_b = ra_move_b + 0.35f * dec_move_b;

    // Keep current pier side unless there is a clear reason to switch.
    // This avoids unintended DEC direction flips for nearby same-side GOTOs.
    bool choose_flip = meridian_flipped;

    // If target RA is in the region that requires flipped side, force it.
    bool force_flip_by_ra_limits = needs_flip_for_ra_limits(ra_a);

    // Only allow automatic side switching by cost when target HA is clearly
    // beyond the configured meridian threshold.
    float target_ha_signed = normalize_signed_deg(lst_deg - ra_a);
    bool target_far_from_meridian = fabsf(target_ha_signed) > meridian_flip_ha_threshold;

    if (force_flip_by_ra_limits) {
        choose_flip = true;
    } else if (target_far_from_meridian) {
        // Use larger hysteresis to avoid side chatter around boundary cases.
        if ((cost_b + 5.0f) < cost_a) {
            choose_flip = true;
        } else if ((cost_a + 5.0f) < cost_b) {
            choose_flip = false;
        }
    }

    if (choose_flip) {
        *out_ra_deg = ra_b;
        *out_dec_deg = dec_b;
        *out_flip_applied = true;
    } else {
        *out_ra_deg = ra_a;
        *out_dec_deg = dec_a;
        *out_flip_applied = false;
    }
}


void set_local_time(int h, int m, int s) {
    local_time_hours = constrain(h, 0, 23);
    local_time_minutes = constrain(m, 0, 59);
    local_time_seconds = constrain(s, 0, 59);
}

void advance_local_time(void) {
    // Only increment the local clock - RTC is read only at startup or when explicitly synced
    // RTC sync happens via rtc_update_local_time() when commanded
    
    // Increment the local clock
    local_time_seconds++;
    if (local_time_seconds >= 60) {
        local_time_seconds = 0;
        local_time_minutes++;
    }
    if (local_time_minutes >= 60) {
        local_time_minutes = 0;
        local_time_hours++;
    }
    if (local_time_hours >= 24) {
        local_time_hours = 0;

        // Keep the LST date in sync when local time crosses midnight.
        static const int days_in_month_tbl[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        bool leap = ((lst_year % 4 == 0 && lst_year % 100 != 0) || (lst_year % 400 == 0));
        int dim = days_in_month_tbl[lst_month - 1];
        if (lst_month == 2 && leap) dim = 29;
        lst_day++;
        if (lst_day > dim) {
            lst_day = 1;
            lst_month++;
            if (lst_month > 12) {
                lst_month = 1;
                lst_year++;
            }
        }
    }
}

// Enhanced coordinate calculation functions
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

float calculate_gmst_deg(float jd) {
    // Calculate centuries since J2000.0
    float T = (jd - 2451545.0f) / 36525.0f;
    
    // GMST at 0h UT in seconds
    float gmst_0h = 24110.54841f + 8640184.812866f * T + 0.093104f * T * T - 0.0000062f * T * T * T;
    
    // Get the UT time from the Julian Day
    float jd_int = floor(jd);
    float jd_frac = jd - jd_int;
    float ut_hours = jd_frac * 24.0f;
    
    // Add time since 0h UT to GMST
    // Earth rotates approximately 360.985607 degrees per sidereal day
    // So the rate is about 1.00273790935 times the solar rate
    float gmst_deg = (gmst_0h / 240.0f) + (ut_hours * 15.04106864f);
    
    // Normalize to 0-360 degrees
    gmst_deg = fmod(gmst_deg, 360.0f);
    if (gmst_deg < 0) gmst_deg += 360.0f;
    
    return gmst_deg;
}

void set_date_for_lst(int year, int month, int day) {
    lst_year = year;
    lst_month = month;
    lst_day = day;
}

// Use double for JD and LST to prevent precision loss over years
double calculate_local_sidereal_time_deg(void) {
    // 1. Get current time and convert to UTC properly
    // This assumes local_time_hours etc. are updated by your RTC/Advance logic
    struct tm t = {0};
    t.tm_year = lst_year - 1900;
    t.tm_mon = lst_month - 1;
    t.tm_mday = lst_day;
    t.tm_hour = local_time_hours;
    t.tm_min = local_time_minutes;
    t.tm_sec = local_time_seconds;

    // Convert local time to time_t (Unix seconds)
    time_t local_now = mktime(&t);
    // Subtract 3 hours (10800 seconds) for Damascus UTC+3
    time_t utc_now = local_now - 10800; 
    struct tm *utc_tm = gmtime(&utc_now);

    // 2. Calculate Julian Day (Double precision is critical here)
    int year = utc_tm->tm_year + 1900;
    int month = utc_tm->tm_mon + 1;
    int day = utc_tm->tm_mday;
    
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int A = year / 100;
    int B = 2 - A + A / 4;
    
    double ut_decimal = utc_tm->tm_hour + (utc_tm->tm_min / 60.0) + (utc_tm->tm_sec / 3600.0);
    double jd = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    jd += ut_decimal / 24.0; // Add time fraction to Julian Day
    
    // 3. Calculate LST using proper method - d should be at 0h UTC, UT handles time rotation
    // Calculate d for 0h UTC of that day (remove fractional part)
    double jd_0h = floor(jd - 0.5) + 0.5; // Round down to previous noon
    double d0 = jd_0h - 2451545.0;
    
    // Calculate GMST at 0h UTC
    double gmst_0h = 100.4606 + 0.98564736628 * d0;
    
    // Add longitude and rotation since 0h UTC
    // 15.041068 is the sidereal rotation rate per solar hour
    double lst_deg = gmst_0h + site_longitude_deg + (15.041068 * ut_decimal);
    
    // 4. Normalize
    lst_deg = fmod(lst_deg, 360.0);
    if (lst_deg < 0) lst_deg += 360.0;
    
    return (float)lst_deg; 
}

float calculate_hour_angle_deg(float ra_deg) {
    float lst_deg = calculate_local_sidereal_time_deg();
    float ha_deg = lst_deg - normalize_ra_deg(ra_deg);
    return normalize_ha_deg(ha_deg);
}

void set_time_mode(int mode) {
    current_time_mode = (TimeMode)mode;
}

int get_time_mode(void) {
    return (int)current_time_mode;
}

void set_rtc_available(bool available) {
    rtc_available = available;
}

bool is_rtc_available(void) {
    return rtc_available;
}

// RTC management functions
void rtc_init(void) {
    rtc.init();
    
    // Check if RTC is halted
    bool initially_halted = rtc.isHalted();
    Serial.printf("RTC DEBUG: Initially halted: %s\n", initially_halted ? "YES" : "NO");
    
    if (!initially_halted) {
        rtc_available = true;
        Serial.println("RTC: DS1302 initialized and running");
        // Read RTC time only once at startup
        rtc_update_local_time();
        // Set time mode to RTC by default if available
        if (current_time_mode == TIME_MODE_DEFAULT) {
            current_time_mode = TIME_MODE_RTC;
        }
    } else {
        Serial.println("RTC: DS1302 is halted - attempting to un-halt and set time");
        
        // Try to set a default time (setDateTime automatically clears halt bit)
        Ds1302::DateTime default_dt = {
            .year = 26,                    // 2026
            .month = Ds1302::MONTH_MAR,    // March
            .day = 25,                     // 25th
            .hour = 12,                    // 12 PM
            .minute = 0,                   // 0 minutes
            .second = 0,                   // 0 seconds
            .dow = Ds1302::DOW_WED        // Wednesday
        };
        rtc.setDateTime(&default_dt);
        
        // Check if un-halt worked
        delay(100); // Give RTC time to process
        if (!rtc.isHalted()) {
            rtc_available = true;
            Serial.println("RTC: Successfully un-halted and initialized");
            // Set time mode to RTC by default if available
            if (current_time_mode == TIME_MODE_DEFAULT) {
                current_time_mode = TIME_MODE_RTC;
            }
        } else {
            rtc_available = false;
            Serial.println("RTC: Failed to un-halt - using default time");
            set_local_time(12, 0, 0);
        }
    }
}

void rtc_get_time(int& year, int& month, int& day, int& hour, int& minute, int& second) {
    if (!rtc_available) {
        year = 2026;
        month = 3;
        day = 25;
        hour = 12;
        minute = 0;
        second = 0;
        return;
    }
    
    Ds1302::DateTime dt;
    rtc.getDateTime(&dt);
    
    year = 2000 + dt.year;
    month = dt.month;
    day = dt.day;
    hour = dt.hour;
    minute = dt.minute;
    second = dt.second;
}

void rtc_set_time(int year, int month, int day, int hour, int minute, int second) {
    if (!rtc_available) {
        Serial.println("RTC DEBUG: Cannot set time - RTC not available");
        return;
    }
    
    // Calculate day of week using Zeller's congruence (0=Sunday, 1=Monday, ...)
    int y = year;
    int m = month;
    if (m < 3) {
        m += 12;
        y--;
    }
    int k = y % 100;
    int j = y / 100;
    int dow_zeller = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    
    // Convert Zeller (0=Sunday) to DS1302 format (1=Monday, 7=Sunday)
    uint8_t dow_ds1302;
    if (dow_zeller == 0) {
        dow_ds1302 = Ds1302::DOW_SUN;  // Sunday
    } else {
        dow_ds1302 = dow_zeller;       // Monday=1, Tuesday=2, ..., Saturday=6
    }
    
    Ds1302::DateTime dt = {
        .year = year - 2000,
        .month = month,
        .day = day,
        .hour = hour,
        .minute = minute,
        .second = second,
        .dow = dow_ds1302
    };
    
    rtc.setDateTime(&dt);
    Serial.printf("RTC: Time set to %04d-%02d-%02d %02d:%02d:%02d (DOW:%d)\n", 
                  year, month, day, hour, minute, second, dow_ds1302);
}

// Add RTC test function for debugging
// Add function to parse time string in YYMMDDWhhmmss format
uint8_t parseDigits(char* str, uint8_t count) {
    uint8_t val = 0;
    while(count-- > 0) val = (val * 10) + (*str++ - '0');
    return val;
}

void rtc_set_time_string(const char* time_str) {
    if (!rtc_available) {
        Serial.println("RTC DEBUG: Cannot set time - RTC not available");
        return;
    }
    
    if (strlen(time_str) != 13) {
        Serial.printf("RTC ERROR: Invalid time format. Expected YYMMDDWhhmmss (13 chars), got %d chars\n", strlen(time_str));
        return;
    }
    
    // Parse YYMMDDWhhmmss format
    uint8_t yy = parseDigits((char*)time_str, 2);
    uint8_t mm = parseDigits((char*)time_str + 2, 2);
    uint8_t dd = parseDigits((char*)time_str + 4, 2);
    uint8_t dow = parseDigits((char*)time_str + 6, 1);
    uint8_t hh = parseDigits((char*)time_str + 7, 2);
    uint8_t min = parseDigits((char*)time_str + 9, 2);
    uint8_t ss = parseDigits((char*)time_str + 11, 2);
    
    // Convert to full year and month constant
    int year = 2000 + yy;
    int month = mm;  // Already 1-12 format
    
    Serial.printf("RTC DEBUG: Parsed %02d%02d%02d%d%02d%02d%02d -> %04d-%02d-%02d %02d:%02d:%02d (DOW:%d)\n",
                  yy, mm, dd, dow, hh, min, ss, year, month, dd, hh, min, ss, dow);
    
    // Set the RTC time
    rtc_set_time(year, month, dd, hh, min, ss);
}

void rtc_test_hardware(void) {
    Serial.println("=== RTC Hardware Test ===");
    
    if (!rtc_available) {
        Serial.println("RTC: Not available for test");
        return;
    }
    
    // Test 1: Check if halted
    bool halted = rtc.isHalted();
    Serial.printf("RTC Halted: %s\n", halted ? "YES" : "NO");
    
    // Test 2: Try to read current time
    Ds1302::DateTime test_dt;
    rtc.getDateTime(&test_dt);
    Serial.printf("RTC Raw Read: year=%d, month=%d, day=%d, hour=%d, minute=%d, second=%d, dow=%d\n",
                  test_dt.year, test_dt.month, test_dt.day, test_dt.hour, test_dt.minute, test_dt.second, test_dt.dow);
    
    // Test 3: Write a known test time
    Serial.println("RTC: Writing test time 2026-03-25 14:30:00");
    Ds1302::DateTime test_time = {
        .year = 26,        // 2026
        .month = Ds1302::MONTH_MAR,  // March
        .day = 25,         // 25th
        .hour = 14,        // 2 PM
        .minute = 30,      // 30 minutes
        .second = 0,       // 0 seconds
        .dow = Ds1302::DOW_WED  // Wednesday
    };
    rtc.setDateTime(&test_time);
    
    // Test 4: Read it back immediately
    Ds1302::DateTime verify_dt;
    rtc.getDateTime(&verify_dt);
    Serial.printf("RTC Verify Read: year=%d, month=%d, day=%d, hour=%d, minute=%d, second=%d, dow=%d\n",
                  verify_dt.year, verify_dt.month, verify_dt.day, verify_dt.hour, verify_dt.minute, verify_dt.second, verify_dt.dow);
    
    // Test 5: Check if write was successful
    if (verify_dt.year == 26 && verify_dt.month == 3 && verify_dt.day == 25 && 
        verify_dt.hour == 14 && verify_dt.minute == 30) {
        Serial.println("RTC: ✓ Write/Read test PASSED");
    } else {
        Serial.println("RTC: ✗ Write/Read test FAILED");
    }
    
    Serial.println("=== End RTC Test ===");
}

void rtc_update_local_time(void) {
    if (!rtc_available) {
        // If RTC not available, set to default time
        set_local_time(12, 0, 0);
        Serial.printf("RTC: Not available, using default time 12:00:00\n");
        return;
    }
    
    int year, month, day, hour, minute, second;
    rtc_get_time(year, month, day, hour, minute, second);
    
    // Use RTC time directly as local time (no UTC conversion)
    set_local_time(hour, minute, second);
    set_date_for_lst(year, month, day);
    
    Serial.printf("RTC: Read %04d-%02d-%02d %02d:%02d:%02d, set local time to %02d:%02d:%02d\n", 
                  year, month, day, hour, minute, second, hour, minute, second);
}

// Check if meridian flip is needed based on current RA position
void check_meridian_flip(float current_ra_deg) {
    if (!MERIDIAN_FLIP_ENABLED) return;

    // Calculate current HA (in degrees)
    float lst_deg = calculate_local_sidereal_time_deg();
    float current_ha_deg = normalize_signed_deg(lst_deg - normalize_ra_deg(current_ra_deg));

    // Flip if |HA| exceeds threshold (default ±90°)
    bool flip_needed = (fabs(current_ha_deg) > meridian_flip_ha_threshold);

    if (!flip_needed && !meridian_flip_in_progress) {
        meridian_flip_scheduled = false;
    }

    // Only allow flip if not already scheduled/in progress and not already flipped
    if (flip_needed && !meridian_flip_scheduled && !meridian_flip_in_progress && !meridian_flipped) {
        meridian_flip_scheduled = true;
        meridian_flip_time = millis(); // Flip immediately
        Serial.print("MERIDIAN FLIP: Scheduled (HA=");
        Serial.print(current_ha_deg, 3);
        Serial.println("° exceeds threshold)");
    }

    if (meridian_flip_scheduled && millis() >= meridian_flip_time && !meridian_flip_in_progress) {
        perform_meridian_flip();
    }
}

float get_current_ra_deg(void) {
    float motor_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;
    return motor_to_sky_ra_deg(motor_ra_deg);
}

float get_current_dec_deg(void) {
    return motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);
}

// Perform the actual meridian flip
void perform_meridian_flip(void) {
    Serial.println("MERIDIAN FLIP: Starting flip procedure");
    meridian_flip_in_progress = true;
    meridian_flip_scheduled = false;
    // Stop tracking temporarily
    velocityMode = false;

    // Calculate new RA position (add 12 hours = 180 degrees)
    float current_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;
    float new_ra_deg = normalize_ra_deg(current_ra_deg + 180.0f);

    // Keep same sky DEC across side change and remap to new motor frame.
    float current_dec_deg = motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);

    meridian_flipped = true;  // Mark that we're now on the flipped side

    // Keep same sky DEC through meridian flip; motor-side inversion handles geometry.
    float new_dec_deg = normalize_dec_deg(current_dec_deg);

    // Set new targets
    target_ra_steps = (int32_t)lroundf(new_ra_deg * TICKS_PER_AXIS_DEG);
    target_dec_steps = calculate_dec_target_steps_with_flip(new_dec_deg);

    // Update target radians
    target_ra_rad = steps_to_rad_ra(target_ra_steps);
    target_dec_rad = steps_to_rad_dec(target_dec_steps);

    Serial.print("MERIDIAN FLIP: RA ");
    Serial.print(current_ra_deg, 6);
    Serial.print("° -> ");
    Serial.print(new_ra_deg, 6);
    Serial.print("°, DEC ");
    Serial.print(current_dec_deg, 6);
    Serial.print("° -> ");
    Serial.print(new_dec_deg, 6);
    Serial.println("° (flipped state active)");
}

// Complete the meridian flip procedure once positioning is done
void complete_meridian_flip(void) {
    if (!meridian_flip_in_progress) return;
    
    // Check if we've reached the target positions
    int32_t ra_err = abs(target_ra_steps - current_ra_steps);
    int32_t dec_err = abs(target_dec_steps - current_dec_steps);
    
    if (ra_err < DEADBAND_STEPS && dec_err < DEADBAND_STEPS) {
        Serial.println("MERIDIAN FLIP: Flip complete, resuming normal operation");
        meridian_flip_in_progress = false;
        // Keep meridian_flipped = true until user manually flips back
    }
}

// Reset flip state for next tracking session (called when flipping back or at user request)
void reset_meridian_flip_state(void) {
    if (meridian_flipped) {
        Serial.println("MERIDIAN FLIP: Resetting flip state to normal");
        float sky_dec = motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);
        meridian_flipped = false;
        current_dec_steps = (int32_t)lroundf(sky_to_motor_dec_deg(sky_dec) * TICKS_PER_AXIS_DEG);
        target_dec_steps = (int32_t)lroundf(sky_to_motor_dec_deg(sky_dec) * TICKS_PER_AXIS_DEG);
        meridian_flip_scheduled = false;
        meridian_flip_in_progress = false;
    }
}

// ============ TRACKING MODES ============
// Calculate tracking rate based on selected mode
void calculate_tracking_rate(void) {
    switch (tracking_mode) {
        case TRACKING_SIDEREAL:
            // Sidereal rate: 360 degrees per sidereal day
            tracking_rate = 360.0f / SIDEREAL_DAY;
            break;
        case TRACKING_SOLAR:
            // Solar rate: 360 degrees per solar day
            tracking_rate = 360.0f / SOLAR_DAY;
            break;
        case TRACKING_LUNAR:
            // Lunar rate: 360 degrees per lunar day
            tracking_rate = 360.0f / LUNAR_DAY;
            break;
        default:
            tracking_rate = 360.0f / SIDEREAL_DAY;  // Default to sidereal
            break;
    }
}

// Set tracking mode
void set_tracking_mode(int mode) {
    if (mode >= TRACKING_LUNAR && mode <= TRACKING_SIDEREAL) {
        tracking_mode = mode;
        calculate_tracking_rate();
        auto_tracking_enabled = true;
        
        Serial.print("TRACKING: Mode set to ");
        switch (mode) {
            case TRACKING_SIDEREAL:
                Serial.println("Sidereal");
                break;
            case TRACKING_SOLAR:
                Serial.println("Solar");
                break;
            case TRACKING_LUNAR:
                Serial.println("Lunar");
                break;
        }
        
        Serial.print("TRACKING: Rate = ");
        Serial.print(tracking_rate * 3600.0f, 6);  // Convert to arcsec/sec
        Serial.println(" arcsec/sec");
    }
}

// Update tracking based on current mode
void update_tracking(unsigned long now) {
    // now is in milliseconds, so compare against ms interval and convert by 1000.
    const unsigned long tracking_interval_ms = TRACKING_INTERVAL_US / 1000UL;
    if (now - last_tracking_update >= tracking_interval_ms) {
        float dt = (now - last_tracking_update) / 1000.0f;  // Convert to seconds
        float ra_movement_deg = tracking_rate * dt;
        
        // Convert to ticks
        int32_t ra_movement_ticks = (int32_t)lroundf(ra_movement_deg * TICKS_PER_AXIS_DEG);
        
        if (ra_movement_ticks > 0) {
            // Apply motor inversion
            int direction = ra_inverted_runtime ? -1 : 1;
            
            // Calculate speed based on tracking rate - use proper scaling
            // Sidereal rate is ~0.004 deg/sec, we need ~50-100 speed units for smooth tracking
            int speed_units = (int)(tracking_rate * 12000.0f);  // Increased scale factor
            if (speed_units < SERVO_MIN_SPEED) speed_units = SERVO_MIN_SPEED;
            if (speed_units > 500) speed_units = 500;  // Cap tracking speed lower than goto
            
            // Move small steps for smooth tracking
            int32_t step_size = min(ra_movement_ticks, (int32_t)P_MAX_STEP);
            if (step_size < 1) step_size = 1;  // Ensure at least 1 step
            write_servo_wheel(ID_RA, direction, speed_units, (int)step_size);
            // Position tracking: when motor is inverted, direction -1 means position +1
            int actual_movement = ra_inverted_runtime ? -direction : direction;
            current_ra_steps += actual_movement * step_size;
            
            // Update speed feedback
            ra_speed = (float)speed_units;
            
            Serial.print("TRACKING: dt=");
            Serial.print(dt, 3);
            Serial.print("s, movement=");
            Serial.print(ra_movement_deg, 6);
            Serial.print("°, steps=");
            Serial.println(step_size);
        }
        
        last_tracking_update = now;
    }
}

// Calculate shortest angular distance for RA (handles circular wrapping)
float ra_angular_distance(float current_ra, float target_ra) {
    // Normalize both angles to [0, 360) range
    current_ra = normalize_ra_deg(current_ra);
    target_ra = normalize_ra_deg(target_ra);
    
    float diff = target_ra - current_ra;
    
    // Find shortest path around the circle
    if (diff > 180.0f) {
        diff -= 360.0f;
    } else if (diff < -180.0f) {
        diff += 360.0f;
    }
    
    return diff;
}

int32_t rad_to_steps_ra(float rad) {
    // Convert radians to degrees, then to ticks using TICKS_PER_AXIS_DEG
    return (int32_t)(rad * RAD_TO_DEG * TICKS_PER_AXIS_DEG);
}

int32_t rad_to_steps_dec(float rad) {
    // Convert radians to degrees, then to ticks using TICKS_PER_AXIS_DEG  
    return (int32_t)(rad * RAD_TO_DEG * TICKS_PER_AXIS_DEG);
}

float steps_to_rad_ra(int32_t steps) {
    // Convert ticks to degrees, then to radians
    return ((float)steps / TICKS_PER_AXIS_DEG * DEG_TO_RAD);
}

float steps_to_rad_dec(int32_t steps) {
    // Convert ticks to degrees, then to radians
    return ((float)steps / TICKS_PER_AXIS_DEG * DEG_TO_RAD);
}

float steps_to_deg_ra(int32_t steps) {
    return steps_to_rad_ra(steps) * RAD_TO_DEG;
}

float steps_to_deg_dec(int32_t steps) {
    return steps_to_rad_dec(steps) * RAD_TO_DEG;
}

static void on_torque_change(bool en) { torque_enabled = en; }

// ============ INIT ============
void mount_init(void) {
    // Initialize RTC first
    rtc_init();

    // Keep tracking rate in sync with selected mode at startup.
    calculate_tracking_rate();
    
    apply_params();
    servo_set_torque_callback(on_torque_change);
    setTorque(false);
    scan_servo_bus();
    // DO NOT change servo mode here; the unit should already be configured by the user.
    // Also: do not read encoder position feedback; this firmware tracks position purely
    // from commanded motion (software counters).
    ra_servo_mode = read_servo_mode(ID_RA);
    dec_servo_mode = read_servo_mode(ID_DEC);
    Serial.print("[SERVO] RA mode="); Serial.print(ra_servo_mode);
    Serial.print(" DEC mode="); Serial.println(dec_servo_mode);
}

void mount_zero_encoders_at_startup(void) {
    // Set to polar alignment position: RA=0°, DEC=90°
    ra_zero_steps = 0;
    dec_zero_steps = -(int32_t)lroundf(90.0f * TICKS_PER_AXIS_DEG);
    last_ra_pos = 0;
    last_dec_pos = 0;
    current_ra_steps = 0;
    current_dec_steps = (int32_t)lroundf(90.0f * TICKS_PER_AXIS_DEG);
    last_ra_pos_speed = 0;
    last_dec_pos_speed = 0;

    // reset timers so any UI doesn't show bogus spikes
    last_speed_update = millis();
    last_sent_ra_steps = current_ra_steps;
    last_sent_dec_steps = current_dec_steps;
    target_ra_rad = 0.0f;
    target_dec_rad = 90.0f * DEG_TO_RAD;
    target_ra_steps = 0;
    target_dec_steps = (int32_t)lroundf(90.0f * TICKS_PER_AXIS_DEG);
    
    Serial.println("STARTUP: Set to polar alignment RA=0°, DEC=90°");
}

// ============ COMMAND PROCESSING ============

// Optional mount offset in degrees (set to 0.0f for standard behavior)

// Helper: Calculate HA (standard, with optional mount offset)
float calculate_ha_deg(float lst_deg, float ra_deg) {
    float ha = lst_deg - normalize_ra_deg(ra_deg);
    return normalize_signed_deg(ha);
}

bool process_command(JsonDocument& cmdDoc) {
    int T = cmdDoc["T"] | 0;

    // Calculate LST once per command
    float lst_deg = calculate_local_sidereal_time_deg();

    // T69: Return full mount state for web GUI
    if (T == 69) {
        StaticJsonDocument<512> doc;
        float ra_deg = get_current_ra_deg();
        float dec_deg = get_current_dec_deg();
        float ha_signed_deg = calculate_ha_deg(lst_deg, ra_deg);
        float ha_deg = normalize_ha_deg(ha_signed_deg);
        doc["T"] = 69;
        doc["ra_deg"] = ra_deg;
        doc["dec_deg"] = dec_deg;
        doc["lst_deg"] = lst_deg;
        doc["ha_deg"] = ha_deg;
        doc["ha_hours"] = ha_deg / 15.0f;
        doc["meridian_flip_scheduled"] = meridian_flip_scheduled;
        doc["meridian_flip_in_progress"] = meridian_flip_in_progress;
        doc["meridian_flipped"] = meridian_flipped;
        doc["torque_enabled"] = torque_enabled;
        doc["tracking_mode"] = tracking_mode;
        doc["local_time"] = String(local_time_hours) + ":" + String(local_time_minutes) + ":" + String(local_time_seconds);
        String jsonStr;
        serializeJson(doc, jsonStr);
        Serial.println(jsonStr);
        return true;
    }

    if (T == 1) {
        // T1: desired absolute RA/DEC position in degrees (from Stellarium)
        // Use received RA to calculate HA, then move to the position corresponding to that HA
        setTorque(true);
        velocityMode = false; // Stop follow mode

        float ra_deg = 0.0f;
        float dec_deg = 0.0f;
        if (cmdDoc.containsKey("ra_deg")) ra_deg = (float)cmdDoc["ra_deg"];
        if (cmdDoc.containsKey("dec_deg")) dec_deg = (float)cmdDoc["dec_deg"];
        desired_ra_deg = normalize_ra_deg(ra_deg);
        desired_dec_deg = normalize_dec_deg(dec_deg);

        bool flip_applied = false;
        float current_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;
        float current_dec_deg = motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);
        select_best_pier_side_target(desired_ra_deg, desired_dec_deg,
                         current_ra_deg, current_dec_deg, lst_deg,
                         &ra_deg, &dec_deg, &flip_applied);
        meridian_flipped = flip_applied;

        float ha_deg = calculate_ha_deg(lst_deg, ra_deg);
        last_target_ha_deg = normalize_ha_deg(ha_deg);

        // Move to the position corresponding to the received HA
        float target_ra_deg = lst_deg - ha_deg;
        target_ra_deg = normalize_ra_deg(target_ra_deg);
        current_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;
        float ra_distance_deg = ra_angular_distance(current_ra_deg, target_ra_deg);
        target_ra_steps = current_ra_steps + (int32_t)lroundf(ra_distance_deg * TICKS_PER_AXIS_DEG);
        target_dec_steps = calculate_dec_target_steps_with_flip(dec_deg);

        Serial.print("STELLARIUM: RA=");
        Serial.print(ra_deg, 6);
        Serial.print("° (LST=");
        Serial.print(lst_deg, 6);
        Serial.print("°, HA=");
        Serial.print(ha_deg, 6);
        Serial.print("°, target RA=");
        Serial.print(target_ra_deg, 6);
        Serial.print("°, current=");
        Serial.print(current_ra_deg, 6);
        Serial.print("°, distance=");
        Serial.print(ra_distance_deg, 6);
        Serial.print("°) -> ");
        Serial.print(target_ra_steps);
        Serial.print(" ticks, DEC=");
        Serial.print(dec_deg, 6);
        Serial.print("° -> ");
        Serial.println(target_dec_steps);

        // Set goto speed for T1 command
        ra_spd = constrain(ra_spd, 0, SERVO_MAX_SPEED);
        dec_spd = constrain(dec_spd, 0, SERVO_MAX_SPEED);
        Serial.println(" ticks");

        // keep legacy vars in sync for status/compat
        target_ra_rad = steps_to_rad_ra(target_ra_steps);
        target_dec_rad = steps_to_rad_dec(target_dec_steps);
        return true;
    }

    // T2: request feedback immediately
    if (T == 2) {
        send_feedback();
        return true;
    }

    if (T == 11) {
        if (cmdDoc.containsKey("torque")) {
            setTorque((int)cmdDoc["torque"] != 0);
        }
        return true;
    }

    // T12: Step RA/DEC by specified angle
    if (T == 12) {
        setTorque(true);
        velocityMode = false; // Stop follow mode
        
        float ra_step_deg = 0.0f;
        float dec_step_deg = 0.0f;
        
        if (cmdDoc.containsKey("ra_step_deg")) ra_step_deg = (float)cmdDoc["ra_step_deg"];
        if (cmdDoc.containsKey("dec_step_deg")) dec_step_deg = (float)cmdDoc["dec_step_deg"];
        
        // Calculate new DEC position and normalize to limits
        float current_dec_deg = motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);
        float new_dec_deg = normalize_dec_deg(current_dec_deg + dec_step_deg);
        dec_step_deg = new_dec_deg - current_dec_deg;
        
        // Convert to ticks and add to current position
        target_ra_steps = current_ra_steps + (int32_t)lroundf(ra_step_deg * TICKS_PER_AXIS_DEG);
        
        // Calculate new target DEC with flip compensation applied to the final position
        target_dec_steps = calculate_dec_target_steps_with_flip(new_dec_deg);
        
        Serial.print("STEP: RA=");
        Serial.print(ra_step_deg, 6);
        Serial.print("°, DEC=");
        Serial.print(dec_step_deg, 6);
        Serial.println("°");
        
        target_ra_rad = steps_to_rad_ra(target_ra_steps);
        target_dec_rad = steps_to_rad_dec(target_dec_steps);
        return true;
    }

    // T13: Synchronize RA/DEC angles without moving
    if (T == 13) {
        velocityMode = false; // Stop follow mode
        
        float ra_sync_deg = 0.0f;
        float dec_sync_deg = 0.0f;
        
        if (cmdDoc.containsKey("ra_sync_deg")) ra_sync_deg = (float)cmdDoc["ra_sync_deg"];
        if (cmdDoc.containsKey("dec_sync_deg")) dec_sync_deg = (float)cmdDoc["dec_sync_deg"];
        
        // Normalize RA
        ra_sync_deg = normalize_ra_deg(ra_sync_deg);
        
        // Normalize DEC to handle limits and wraparound behavior
        dec_sync_deg = normalize_dec_deg(dec_sync_deg);
        
        // Update zero offsets to make current position match desired angles
        int32_t desired_ra_steps = (int32_t)lroundf(sky_to_motor_ra_deg(ra_sync_deg) * TICKS_PER_AXIS_DEG);
        int32_t desired_dec_steps = calculate_dec_target_steps_with_flip(dec_sync_deg);
        
        ra_zero_steps = current_ra_steps - desired_ra_steps;
        dec_zero_steps = current_dec_steps - desired_dec_steps;
        
        // Update current position to reflect new zero
        current_ra_steps = desired_ra_steps;
        current_dec_steps = desired_dec_steps;
        target_ra_steps = desired_ra_steps;
        target_dec_steps = desired_dec_steps;
        desired_ra_deg = ra_sync_deg;
        desired_dec_deg = dec_sync_deg;
        
        Serial.print("SYNC: RA=");
        Serial.print(ra_sync_deg, 6);
        Serial.print("°, DEC=");
        Serial.print(dec_sync_deg, 6);
        Serial.println("°");
        
        target_ra_rad = steps_to_rad_ra(target_ra_steps);
        target_dec_rad = steps_to_rad_dec(target_dec_steps);
        return true;
    }

    // T16: Set local time and site location for meridian flip
    if (T == 16) {
        if (cmdDoc.containsKey("site_latitude_deg")) {
            site_latitude_deg = (float)cmdDoc["site_latitude_deg"];
        }
        if (cmdDoc.containsKey("site_longitude_deg")) {
            site_longitude_deg = (float)cmdDoc["site_longitude_deg"];
        }
        if (cmdDoc.containsKey("local_time")) {
            const char* t = cmdDoc["local_time"];
            int h = 0, m = 0, s = 0;
            if (sscanf(t, "%d:%d:%d", &h, &m, &s) == 3) {
                set_local_time(h,m,s);
            }
        }
        if (cmdDoc.containsKey("local_time_hours") && cmdDoc.containsKey("local_time_minutes") && cmdDoc.containsKey("local_time_seconds")) {
            set_local_time((int)cmdDoc["local_time_hours"], (int)cmdDoc["local_time_minutes"], (int)cmdDoc["local_time_seconds"]);
        }
        if (cmdDoc.containsKey("local_time_hours") && cmdDoc.containsKey("local_time_minutes") && !cmdDoc.containsKey("local_time_seconds")) {
            set_local_time((int)cmdDoc["local_time_hours"], (int)cmdDoc["local_time_minutes"], local_time_seconds);
        }
        return true;
    }

    // T18: Time management commands
    if (T == 18) {
        if (cmdDoc.containsKey("time_mode")) {
            int mode = (int)cmdDoc["time_mode"];
            set_time_mode(mode);
        }
        if (cmdDoc.containsKey("rtc_available")) {
            bool available = (bool)cmdDoc["rtc_available"];
            set_rtc_available(available);
        }
        if (cmdDoc.containsKey("rtc_set_time")) {
            if (cmdDoc.containsKey("year") && cmdDoc.containsKey("month") && 
                cmdDoc.containsKey("day") && cmdDoc.containsKey("hour") && 
                cmdDoc.containsKey("minute") && cmdDoc.containsKey("second")) {
                int year = (int)cmdDoc["year"];
                int month = (int)cmdDoc["month"];
                int day = (int)cmdDoc["day"];
                int hour = (int)cmdDoc["hour"];
                int minute = (int)cmdDoc["minute"];
                int second = (int)cmdDoc["second"];
                rtc_set_time(year, month, day, hour, minute, second);
                // Update LST date when time is set
                set_date_for_lst(year, month, day);
            }
        }
        if (cmdDoc.containsKey("rtc_get_time")) {
            rtc_update_local_time();
        }
        // Add command to sync time from RTC without resetting
        if (cmdDoc.containsKey("sync_time")) {
            if (cmdDoc.containsKey("from_rtc") && (bool)cmdDoc["from_rtc"]) {
                rtc_update_local_time();
                Serial.println("TIME: Synced from RTC");
            }
            // Add direct time setting for sync
            if (cmdDoc.containsKey("hour") && cmdDoc.containsKey("minute") && cmdDoc.containsKey("second")) {
                int hour = (int)cmdDoc["hour"];
                int minute = (int)cmdDoc["minute"];
                int second = (int)cmdDoc["second"];
                set_local_time(hour, minute, second);
                Serial.printf("TIME: Set to %02d:%02d:%02d\n", hour, minute, second);
            }
        }
        // Add command to set date for LST without RTC
        if (cmdDoc.containsKey("set_date")) {
            if (cmdDoc.containsKey("year") && cmdDoc.containsKey("month") && cmdDoc.containsKey("day")) {
                int year = (int)cmdDoc["year"];
                int month = (int)cmdDoc["month"];
                int day = (int)cmdDoc["day"];
                set_date_for_lst(year, month, day);
                Serial.printf("LST date set to %04d-%02d-%02d\n", year, month, day);
            }
        }
        // Add RTC hardware test command
        if (cmdDoc.containsKey("rtc_test")) {
            rtc_test_hardware();
        }
        // Add command to set RTC time in YYMMDDWhhmmss format
        if (cmdDoc.containsKey("rtc_set_string")) {
            if (cmdDoc.containsKey("time_string")) {
                const char* time_str = cmdDoc["time_string"];
                rtc_set_time_string(time_str);
            }
        }
        // Add command to force RTC available (bypass halted check)
        if (cmdDoc.containsKey("rtc_force_available")) {
            rtc_available = true;
            Serial.println("RTC: Force set to available for testing");
        }
        return true;
    }

    // T19: Goto to target coordinates using RA/DEC
    if (T == 19) {
        float ra_deg = 0.0f;
        float dec_deg = 0.0f;

        // Support both input formats: ra_hours (hours) or ra_deg (degrees)
        if (cmdDoc.containsKey("ra_hours")) {
            // Convert RA from hours to degrees
            float ra_hours = (float)cmdDoc["ra_hours"];
            ra_deg = ra_hours * 15.0f;  // 15 degrees per hour
        } else if (cmdDoc.containsKey("ra_deg")) {
            // Use RA directly in degrees
            ra_deg = (float)cmdDoc["ra_deg"];
        }

        if (cmdDoc.containsKey("dec_degrees")) {
            dec_deg = (float)cmdDoc["dec_degrees"];
        } else if (cmdDoc.containsKey("dec_deg")) {
            dec_deg = (float)cmdDoc["dec_deg"];
        }

        desired_ra_deg = normalize_ra_deg(ra_deg);
        desired_dec_deg = normalize_dec_deg(dec_deg);

        bool flip_applied = false;
        float current_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;
        float current_dec_deg = motor_to_sky_dec_deg((float)current_dec_steps / TICKS_PER_AXIS_DEG);
        select_best_pier_side_target(desired_ra_deg, desired_dec_deg,
                         current_ra_deg, current_dec_deg, lst_deg,
                         &ra_deg, &dec_deg, &flip_applied);
        meridian_flipped = flip_applied;

        float target_ha_deg = calculate_ha_deg(lst_deg, ra_deg);
        last_target_ha_deg = normalize_ha_deg(target_ha_deg);
        current_ra_deg = (float)current_ra_steps / TICKS_PER_AXIS_DEG;

        // Command RA directly using shortest circular RA path.
        // Previous HA-delta formulation could drive to the mirrored RA solution.
        float ra_distance_deg = ra_angular_distance(current_ra_deg, ra_deg);
        target_ra_steps = current_ra_steps + (int32_t)lroundf(ra_distance_deg * TICKS_PER_AXIS_DEG);

        // Calculate DEC target accounting for meridian flip state
        target_dec_steps = calculate_dec_target_steps_with_flip(dec_deg);

        Serial.print("GOTO: RA=");
        Serial.print(ra_deg, 6);
        Serial.print("°, HA=");
        Serial.print(target_ha_deg, 6);
        Serial.print("° (current RA=");
        Serial.print(current_ra_deg, 6);
        Serial.print("°, distance=");
        Serial.print(ra_distance_deg, 6);
        Serial.print("°) -> ");
        Serial.print(target_ra_steps);
        Serial.print(" ticks, DEC=");
        Serial.print(dec_deg, 6);
        Serial.print("° -> ");
        Serial.println(target_dec_steps);

        // Set goto speed for T19 command
        ra_spd = constrain(ra_spd, 0, SERVO_MAX_SPEED);
        dec_spd = constrain(dec_spd, 0, SERVO_MAX_SPEED);

        setTorque(true);
        velocityMode = false;

        Serial.print("GOTO: RA=");
        Serial.print(ra_deg, 6);
        Serial.print("°, DEC=");
        Serial.print(dec_deg, 6);
        Serial.println("°");

        target_ra_rad = steps_to_rad_ra(target_ra_steps);
        target_dec_rad = steps_to_rad_dec(target_dec_steps);
        return true;
    }

    // T3: Polar alignment - set RA=0, DEC=90
    if (T == 3) {
        // Set target to polar alignment position (RA=0°, DEC=90°)
        target_ra_steps = 0;
        target_dec_steps = (int32_t)lroundf(90.0f * TICKS_PER_AXIS_DEG);
        
        // Update zero offsets to make current position match polar alignment
        ra_zero_steps = current_ra_steps;
        dec_zero_steps = current_dec_steps - target_dec_steps;
        
        // Update current position to reflect new zero
        current_ra_steps = 0;
        current_dec_steps = target_dec_steps;
        
        last_ra_pos = 0;
        last_dec_pos = 0;
        last_ra_pos_speed = 0;
        last_dec_pos_speed = 0;
        last_sent_ra_steps = 0;
        last_sent_dec_steps = 0;
        desired_ra_deg = 0.0f;
        desired_dec_deg = 90.0f;
        s_pid_ra = PidState{};
        s_pid_dec = PidState{};
        
        Serial.println("POLAR ALIGNMENT: Set RA=0°, DEC=90°");
        return true;
    }

    // T4: Follow mode control - RA continuous tracking
    if (T == 4) {
        bool enable = cmdDoc["enable"] | false;
        float ra_speed = cmdDoc["ra_speed"] | 0.0f;
        
        if (enable) {
            // Enable follow mode - set target to very large positive number for continuous tracking
            velocityMode = false;
            setTorque(true);
            
            // Set a very large target to ensure continuous movement
            target_ra_steps = 2147483647; // Maximum 32-bit signed integer
            target_dec_steps = current_dec_steps; // Keep DEC unchanged
            
            // Store the tracking speed for motor control
            vel_ra = ra_speed;
            velocityMode = true; // Use velocity mode for continuous tracking
            
            Serial.print("FOLLOW MODE: ENABLED - RA speed=");
            Serial.print(ra_speed);
            Serial.println(" deg/sec");
        }
        return true;
    }

    // T14: Set tracking mode (Sidereal, Solar, Lunar)
    if (T == 14) {
        int mode = cmdDoc["mode"] | DEFAULT_TRACKING_MODE;
        set_tracking_mode(mode);
        return true;
    }

    if (T == 23) {
        if (cmdDoc.containsKey("tracking_enable")) {
            auto_tracking_enabled = ((int)cmdDoc["tracking_enable"] != 0);
            if (!auto_tracking_enabled) {
                velocityMode = false;
                vel_ra = 0.0f;
            }
            Serial.print("TRACKING: ");
            Serial.println(auto_tracking_enabled ? "ENABLED" : "DISABLED");
        }
        return true;
    }

    // T15: Get tracking mode status
    if (T == 15) {
        send_feedback();  // Will include tracking mode in feedback
        return true;
    }

    // T16: Manual tracking control (activate with custom speed)
    if (T == 16) {
        float tracking_speed = 0.0f;
        if (cmdDoc.containsKey("tracking_speed")) {
            tracking_speed = (float)cmdDoc["tracking_speed"];
        }
        
        if (tracking_speed > 0.0f) {
            // Activate manual tracking with custom speed
            vel_ra = tracking_speed;
            velocityMode = true; // Use velocity mode for continuous tracking
            
            Serial.print("MANUAL TRACKING: ENABLED - RA speed=");
            Serial.print(tracking_speed);
            Serial.println(" deg/sec");
        } else {
            // Stop manual tracking
            velocityMode = false;
            vel_ra = 0.0f;
            Serial.println("MANUAL TRACKING: DISABLED");
        }
        return true;
    }

    // T17: Set DEC speed
    if (T == 17) {
        if (cmdDoc.containsKey("dec_speed")) {
            dec_spd = (int)cmdDoc["dec_speed"];
            // Constrain to safe range
            dec_spd = constrain(dec_spd, 500, 4000);
            
            Serial.print("DEC SPEED: Set to ");
            Serial.println(dec_spd);
        }
        return true;
    }


    // T21: Motor direction control
    if (T == 21) {
        if (cmdDoc.containsKey("motor") && cmdDoc.containsKey("direction")) {
            String motor = cmdDoc["motor"].as<String>();
            String direction = cmdDoc["direction"].as<String>();
            
            if (motor == "ra") {
                if (direction == "inverted") {
                    ra_inverted_runtime = true;
                    Serial.println("RA Motor: INVERTED");
                } else {
                    ra_inverted_runtime = false;
                    Serial.println("RA Motor: NORMAL");
                }
            } else if (motor == "dec") {
                if (direction == "inverted") {
                    dec_inverted_runtime = true;
                    Serial.println("DEC Motor: INVERTED");
                } else {
                    dec_inverted_runtime = false;
                    Serial.println("DEC Motor: NORMAL");
                }
            }
        }
        return true;
    }

    // T22: Set current RA/DEC counters without moving motors (debug/sync helper)
    if (T == 22) {
        float ra_set_deg = get_current_ra_deg();
        float dec_set_deg = get_current_dec_deg();

        if (cmdDoc.containsKey("set_ra_deg")) {
            ra_set_deg = normalize_ra_deg((float)cmdDoc["set_ra_deg"]);
        }
        if (cmdDoc.containsKey("set_dec_deg")) {
            dec_set_deg = normalize_dec_deg((float)cmdDoc["set_dec_deg"]);
        }

        int32_t ra_steps = (int32_t)lroundf(sky_to_motor_ra_deg(ra_set_deg) * TICKS_PER_AXIS_DEG);
        int32_t dec_steps = calculate_dec_target_steps_with_flip(dec_set_deg);

        current_ra_steps = ra_steps;
        current_dec_steps = dec_steps;
        target_ra_steps = ra_steps;
        target_dec_steps = dec_steps;
        target_ra_rad = steps_to_rad_ra(target_ra_steps);
        target_dec_rad = steps_to_rad_dec(target_dec_steps);
        desired_ra_deg = ra_set_deg;
        desired_dec_deg = dec_set_deg;

        Serial.print("SET COUNTERS: RA=");
        Serial.print(ra_set_deg, 6);
        Serial.print("°, DEC=");
        Serial.print(dec_set_deg, 6);
        Serial.println("° (no movement)");
        return true;
    }

    return false;
}

// ============ MOTOR UPDATE ============
// PID-based wheel mode: speed proportional to distance for smooth control
void mount_update_motors(unsigned long now) {
    if (!torque_enabled) return;

    // Only evaluate automatic meridian flips while tracking is actively enabled.
    // This prevents unintended startup motion before any observing session begins.
    if (auto_tracking_enabled) {
        check_meridian_flip(get_current_ra_deg());
    }
    
    // Complete meridian flip if positioning is done
    complete_meridian_flip();

    // Update tracking if torque is enabled (tracking_mode 1=Sidereal, 2=Solar, 0=Lunar)
    // All modes except when explicitly disabled should track
    if (torque_enabled && !velocityMode && auto_tracking_enabled && tracking_mode >= 0 && tracking_rate > 0.0f) {
        update_tracking(now);
    }

    // Follow mode - continuous RA tracking at specified speed
    if (velocityMode) {
        // Calculate RA movement based on desired speed (deg/sec)
        float dt = (now - lastVelTime) / 1000.0f; // Convert to seconds
        if (dt > 0.01f) { // Minimum 10ms interval
            float ra_movement_deg = vel_ra * dt;
            int32_t ra_movement_ticks = (int32_t)lroundf(ra_movement_deg * TICKS_PER_AXIS_DEG);
            
            if (ra_movement_ticks > 0) {
                // Move RA in positive direction
                // Scale vel_ra (deg/sec) to servo speed units
                // For 15 deg/sec = ~2000 speed units
                int speed_units = (int)(vel_ra * 133.0f); // Scale deg/sec to servo units
                if (speed_units < SERVO_MIN_SPEED) speed_units = SERVO_MIN_SPEED;
                if (speed_units > SERVO_MAX_SPEED) speed_units = SERVO_MAX_SPEED;
                
                // Move small steps for smooth tracking
                int32_t step_size = min(ra_movement_ticks, (int32_t)P_MAX_STEP);
                if (step_size < 1) step_size = 1;
                int direction = ra_inverted_runtime ? -1 : 1;  // Apply motor inversion
                write_servo_wheel(ID_RA, direction, speed_units, (int)step_size);
                // Position tracking: when motor is inverted, direction -1 means position +1
                int actual_movement = ra_inverted_runtime ? -direction : direction;
                current_ra_steps += actual_movement * step_size;
                
                // Update speed feedback
                ra_speed = (float)speed_units;
                
                Serial.print("FOLLOW: dt=");
                Serial.print(dt, 3);
                Serial.print("s, movement=");
                Serial.print(ra_movement_deg, 6);
                Serial.print("°, steps=");
                Serial.println(step_size);
            }
            
            lastVelTime = now;
        }
        
        // DEC stays stationary in follow mode
        dec_speed = 0.0f;
        return;
    }
    
    // Normal goto mode
    int32_t ra_err = target_ra_steps - current_ra_steps;

    // Motor DEC frame can exceed sky DEC limits on flipped side.
    const int32_t dec_min_steps = (int32_t)lroundf(-180.0f * TICKS_PER_AXIS_DEG);
    const int32_t dec_max_steps = (int32_t)lroundf(270.0f * TICKS_PER_AXIS_DEG);
    target_dec_steps = constrain(target_dec_steps, dec_min_steps, dec_max_steps);
    int32_t dec_err = target_dec_steps - current_dec_steps;
    
    Serial.print("MOTOR: ra_err=");
    Serial.print(ra_err);
    Serial.print(", dec_err=");
    Serial.println(dec_err);

    // RA motor control with PID-like speed scaling
    if (abs(ra_err) > DEADBAND_STEPS) {
        int direction = ra_err > 0 ? 1 : -1;
        // Apply motor inversion
        direction = ra_inverted_runtime ? -direction : direction;
        
        // Speed scaling: faster when far, slower when close
        int32_t distance = abs(ra_err);
        float distance_deg = distance / TICKS_PER_AXIS_DEG;
        int speed_scale;
        
        if (distance_deg > 5.0f) {
            speed_scale = ra_spd * 2.0f;  // 2x speed for >5 degrees
        } else if (distance > 1000) {
            speed_scale = ra_spd;  // Full speed when far
        } else if (distance > 100) {
            speed_scale = ra_spd * 0.7;  // Medium speed
        } else if (distance > 20) {
            speed_scale = ra_spd * 0.4;  // Slow speed
        } else {
            speed_scale = ra_spd * 0.2;  // Very slow for accuracy
        }
        
        // Limit movement to prevent overshoot
        int32_t ticks_to_move = min(distance, (int32_t)max((int32_t)(distance / 10), (int32_t)1));
        ticks_to_move = min(ticks_to_move, (int32_t)P_MAX_STEP);
        
        Serial.print("MOTOR_DEBUG: RA - direction=");
        Serial.print(direction);
        Serial.print(", distance=");
        Serial.print(distance);
        Serial.print(", speed=");
        Serial.print(speed_scale);
        Serial.print(", ticks=");
        Serial.println(ticks_to_move);
        
        write_servo_goto(ID_RA, direction, (int)ticks_to_move);
        // Position tracking: when motor is inverted, direction -1 means position +1
        int actual_movement = ra_inverted_runtime ? -direction : direction;
        current_ra_steps += actual_movement * ticks_to_move;
    }

    // DEC motor control with PID-like speed scaling
    if (abs(dec_err) > DEADBAND_STEPS) {
        int direction = dec_err > 0 ? 1 : -1;
        
        // Apply DEC motor inversion with pier-side reversal compensation.
        bool dec_inv = effective_dec_inverted();
        direction = dec_inv ? -direction : direction;
        
        // Speed scaling: faster when far, slower when close
        int32_t distance = abs(dec_err);
        float distance_deg = distance / TICKS_PER_AXIS_DEG;
        int speed_scale;
        
        if (distance_deg > 5.0f) {
            speed_scale = dec_spd * 2.0f;  // 2x speed for >5 degrees
        } else if (distance > 1000) {
            speed_scale = dec_spd;  // Full speed when far
        } else if (distance > 100) {
            speed_scale = dec_spd * 0.7;  // Medium speed
        } else if (distance > 20) {
            speed_scale = dec_spd * 0.4;  // Slow speed
        } else {
            speed_scale = dec_spd * 0.2;  // Very slow for accuracy
        }
        
        // Limit movement to prevent overshoot
        int32_t ticks_to_move = min(distance, (int32_t)max((int32_t)(distance / 10), (int32_t)1));
        ticks_to_move = min(ticks_to_move, (int32_t)P_MAX_STEP);
        
        Serial.print("MOTOR_DEBUG: DEC - direction=");
        Serial.print(direction);
        Serial.print(", meridian_flipped=");
        Serial.print(meridian_flipped);
        Serial.print(", distance=");
        Serial.print(distance);
        Serial.print(", speed=");
        Serial.print(speed_scale);
        Serial.print(", ticks=");
        Serial.println(ticks_to_move);
        
        write_servo_goto(ID_DEC, direction, (int)ticks_to_move);
        // Position tracking: when motor is inverted, direction -1 means position +1
        int actual_movement = dec_inv ? -direction : direction;
        current_dec_steps += actual_movement * ticks_to_move;
        current_dec_steps = constrain(current_dec_steps, dec_min_steps, dec_max_steps);
    }

    // Update speed feedback
    ra_speed = (abs(ra_err) > DEADBAND_STEPS) ? (float)ra_spd : 0.0f;
    dec_speed = (abs(dec_err) > DEADBAND_STEPS) ? (float)dec_spd : 0.0f;
    
    // Send current speeds for OLED display
    Serial.print("SPEEDS: ra_speed=");
    Serial.print(ra_speed);
    Serial.print(", dec_speed=");
    Serial.println(dec_speed);
}

bool mount_is_moving(void) {
    if (!torque_enabled) return false;
    int32_t ra_err = target_ra_steps - current_ra_steps;
    while (ra_err > (STEPS_PER_REV / 2)) ra_err -= STEPS_PER_REV;
    while (ra_err < -(STEPS_PER_REV / 2)) ra_err += STEPS_PER_REV;
    int32_t dec_err = target_dec_steps - current_dec_steps;
    return (abs(ra_err) > DEADBAND_STEPS || abs(dec_err) > DEADBAND_STEPS);
}

// ============ FEEDBACK / STATUS ============
void send_feedback(void) {
    // Only send RA and DEC (degrees) and T=2 to Serial
    StaticJsonDocument<128> doc;
    doc["T"] = 2;
    float ra_deg = get_current_ra_deg();
    float dec_deg = get_current_dec_deg();
    doc["ra_deg"] = ra_deg;
    doc["dec_deg"] = dec_deg;
    String jsonStr;
    serializeJson(doc, jsonStr);
    Serial.println(jsonStr);

    // WiFi ROS2 driver reads from TCP socket (port 10001), not USB serial.
    // Send full status there so all expected fields are available.
    String fullStatus;
    get_status_json(fullStatus);
    tcp_send_feedback(fullStatus.c_str());
}

void get_status_json(String& out) {
    StaticJsonDocument<JSON_BUF_SIZE> doc;
    doc["T"] = 2;
    doc["firmware_rev"] = "ha-wrap-2026-04-04-r2";
    doc["ra_ticks"] = current_ra_steps;
    doc["dec_ticks"] = current_dec_steps;
    
    // Also send degrees for easier debugging
    float ra_deg = get_current_ra_deg();
    float dec_deg = get_current_dec_deg();
    
    // Send both raw and normalized RA values
    doc["ra_deg"] = ra_deg;
    doc["ra_deg_normalized"] = normalize_ra_deg(ra_deg);
    doc["dec_deg"] = dec_deg;
    doc["target_ra_steps"] = target_ra_steps;
    doc["target_dec_steps"] = target_dec_steps;
    
    // Add Hour Angle calculation
    float lst_deg = calculate_local_sidereal_time_deg();
    float ha_signed_deg = normalize_signed_deg(lst_deg - normalize_ra_deg(ra_deg));
    float ha_deg = normalize_ha_deg(ha_signed_deg);
    doc["hour_angle_deg"] = ha_deg;
    doc["hour_angle_hours"] = ha_deg / 15.0f;
    doc["hour_angle_signed_deg"] = ha_signed_deg;
    char ha_hms[16];
    format_ha_hms(ha_deg, ha_hms, sizeof(ha_hms));
    doc["hour_angle_hms"] = ha_hms;
    char ha_signed_hms[16];
    format_signed_ha_hms(ha_signed_deg, ha_signed_hms, sizeof(ha_signed_hms));
    doc["hour_angle_signed_hms"] = ha_signed_hms;
    doc["target_hour_angle_deg"] = last_target_ha_deg;
    char target_ha_hms[16];
    format_ha_hms(last_target_ha_deg, target_ha_hms, sizeof(target_ha_hms));
    doc["target_hour_angle_hms"] = target_ha_hms;
    doc["commanded_ra_deg"] = desired_ra_deg;
    doc["commanded_dec_deg"] = desired_dec_deg;
    
    // Add meridian flip status
    doc["meridian_flip_scheduled"] = meridian_flip_scheduled;
    doc["meridian_flip_in_progress"] = meridian_flip_in_progress;
    doc["meridian_flipped"] = meridian_flipped;
    
    // Add tracking mode status
    doc["tracking_mode"] = tracking_mode;
    const char* mode_name = "Unknown";
    switch (tracking_mode) {
        case TRACKING_SIDEREAL: mode_name = "Sidereal"; break;
        case TRACKING_SOLAR: mode_name = "Solar"; break;
        case TRACKING_LUNAR: mode_name = "Lunar"; break;
    }
    doc["tracking_mode_name"] = mode_name;
    doc["tracking_rate"] = tracking_rate;
    doc["tracking_enabled"] = auto_tracking_enabled;
    
    // Add location/time for meridian flip config
    doc["site_latitude_deg"] = site_latitude_deg;
    doc["site_longitude_deg"] = site_longitude_deg;
    char clockStr[16];
    snprintf(clockStr, sizeof(clockStr), "%02d:%02d:%02d", local_time_hours, local_time_minutes, local_time_seconds);
    doc["local_time"] = clockStr;
    float lst_value = calculate_local_sidereal_time_deg();
    doc["local_sidereal_time_deg"] = lst_value;
    
    // Add time management information
    doc["time_mode"] = get_time_mode();
    doc["rtc_available"] = is_rtc_available();
    
    // Add coordinate calculation info
    // Calculate Julian Day using same date as LST calculation
    int jd_year = lst_year;
    int jd_month = lst_month;
    int jd_day = lst_day;
    int jd_utc_hours = local_time_hours - 3;
    if (jd_utc_hours < 0) {
        jd_utc_hours += 24;
        jd_day--;
        if (jd_day < 1) {
            jd_day = 28;
            jd_month--;
            if (jd_month < 1) {
                jd_month = 12;
                jd_year--;
            }
        }
    }
    doc["julian_day"] = calculate_julian_day(jd_year, jd_month, jd_day, jd_utc_hours, local_time_minutes, local_time_seconds);
    
    // Add motor direction status
    doc["ra_inverted"] = ra_inverted_runtime;
    doc["dec_inverted"] = effective_dec_inverted();
    
    out = "";
    serializeJson(doc, out);
}

// ============ TORQUE / DEFA ============
void setTorque(bool enable) {
    if (torque_enabled != enable) {
        torque_enabled = enable;
        if (!enable) {
            // Stop all motion including follow mode
            velocityMode = false;
            vel_ra = 0.0f;
            target_ra_rad = steps_to_rad_ra(current_ra_steps);
            target_dec_rad = steps_to_rad_dec(current_dec_steps);
            servo_enable_torque_ra(false);
            servo_enable_torque_dec(false);
            st.EnableTorque(254, 0);
            ros_started = false;
            Serial.println("TORQUE: OFF - All motion stopped");
        } else {
            target_ra_rad = steps_to_rad_ra(current_ra_steps);
            target_dec_rad = steps_to_rad_dec(current_dec_steps);
            last_sent_ra_steps = (int32_t)((current_ra_steps % STEPS_PER_REV + STEPS_PER_REV) % STEPS_PER_REV);
            if (last_sent_ra_steps == STEPS_PER_REV) last_sent_ra_steps = 0;
            last_sent_dec_steps = (int32_t)((current_dec_steps % STEPS_PER_REV + STEPS_PER_REV) % STEPS_PER_REV);
            if (last_sent_dec_steps == STEPS_PER_REV) last_sent_dec_steps = 0;
            servo_enable_torque_ra(true);
            servo_enable_torque_dec(true);
            Serial.println("TORQUE: ON");
        }
    }
}

void readDEFAStatus(void) { read_defa_status(); }
void setDEFAOff(void) { set_defa_off(); }
void setDEFAOn(void) { set_defa_on(); }
void setDEFACustom(int torque_value) { set_defa_custom(torque_value); }

void apply_params(void) {
    // Ratio system disabled - using direct TICKS_PER_AXIS_DEG conversion
    // Keep these variables for compatibility but set to neutral values
    steps_per_rad_ra = 1.0f;
    steps_per_rad_dec = 1.0f;
}
