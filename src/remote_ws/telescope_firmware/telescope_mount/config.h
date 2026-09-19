#ifndef CONFIG_H
#define CONFIG_H

// -------- Serial servo bus --------
#define SERVO_SERIAL Serial1
#define SERVO_BAUD   1000000
#define S_RXD        18
#define S_TXD        19

#define ID_RA  13
#define ID_DEC 14

#define STEPS_PER_REV  4096

// Math constants
#define TWO_PI  (2.0f * 3.14159265359f)
#define RAD_TO_DEG  (180.0f / 3.14159265359f)
#define DEG_TO_RAD  (3.14159265359f / 180.0f)


// -------- Motor Direction --------
// Set to 1 to invert motor direction, 0 for normal direction
#define RA_INVERTED  1
#define DEC_INVERTED 1

// -------- Motion defaults --------
// Ratio: steps_per_rad = (4096/2π) * ratio.
// If your mount has reduction such that 2.5° of axis motion equals 4096 encoder ticks,
// then the axis has 144× reduction (360/2.5 = 144), so use ratio=144.
// NOTE: Ratio system disabled - using direct TICKS_PER_AXIS_DEG conversion instead
#define DEFAULT_RA_RATIO  1.0f
#define DEFAULT_DEC_RATIO 1.0f
#define DEFAULT_SPD       1500       // default speed units for RPM conversion
#define DEFAULT_DEC_SPD   3500       // default DEC speed (faster than RA)
#define DEFAULT_ACC       80         // ST3215: acc 0–150 (servo mode only)

// -------- SPEED UNIT CONVERSION --------
// Conversion between raw speed units and physical rpm.  These helper
// constants are retained so you can translate between RPM and the library's
// arbitrary speed units; however commands and feedback exclusively use the
// native units.
#define SPEED_TO_RPM       0.01464f   // multiply by speed units to get RPM
#define RPM_TO_SPEED       68.24f     // multiply by RPM to get speed units

#define GOTO_SPEED         3950       // default speed units when performing a GOTO
#define GOTO_ERROR_THRESHOLD_DEG 1.0f // within this many degrees considered reached

// Servo speed limits (ST3215 documentation shows 0--4000).
#define SERVO_MAX_SPEED    4000       // clamp outgoing speed units
#define SERVO_MIN_SPEED    50         // force nonzero command to overcome deadband

// -------- Stellarium angle -> ticks (wheel mode) --------
// Mount axis degrees: 2.5° axis motion == 4096 encoder ticks (1 servo revolution)
// ticks_per_deg = 4096 / 2.5 = 1638.4
#define AXIS_DEG_PER_REV_EQUIV  2.5f
#define TICKS_PER_AXIS_DEG      (4096.0f / 2.5f)


// -------- PID speed control (wheel mode) --------
// Output is servo raw speed units (0..4000).
#define PID_KP  1.0f
#define PID_KI  0.0f
#define PID_KD  0.2f
#define PID_I_LIMIT  20000.0f

// -------- Control (reduce oscillation) --------
// TUNED FOR ST3215: Increased deadband and P-zone to reduce hunting/oscillation
#define DEADBAND_STEPS 8           // ~0.005 deg (~18 arcsec) final stop band for accurate centering
#define STEP_SEND_THRESHOLD 60      // Minimum step before sending command 
#define MOTOR_CMD_INTERVAL_MS 60   // Wait longer between commands  - lets servo settle

// P-zone: when error < this, move in small steps (reduces overshoot/oscillation)
#define P_ZONE_STEPS  600          // Activate proportional control earlier 
#define P_GAIN        0.25f         // Gentler correction gain 
#define P_MAX_STEP    100            // Smaller max step per cycle 
#define SETTLING_THRESHOLD 20       // When error < this, servo is considered "settled"
#define SETTLING_TIME_MS 200        // Hold position without new commands when settled (ms)

// -------- JSON / protocol --------
// Keep this comfortably above status payload size so new fields are not dropped.
#define JSON_BUF_SIZE 2048

// -------- DEFA / torque (servo EEPROM) --------
#define ST_TORQUE_LIMIT_ADDR   39
#define ST_CURRENT_LIMIT_ADDR  34

// -------- OLED --------
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
#define S_SDA 32
#define S_SCL 33

// -------- DS1302 RTC --------
#define RTC_CE_PIN    16
#define RTC_SCLK_PIN  5
#define RTC_IO_PIN    27

// -------- Timing --------
#define FEEDBACK_INTERVAL_MS 50

// -------- WiFi AP (web interface) --------
#define WIFI_STA_SSID   "Bakleh 4G"
#define WIFI_STA_PASS   "21236888"
#define WIFI_STA_CONNECT_TIMEOUT_MS 12000

#define WIFI_AP_SSID   "Jarspace-R2"
#define WIFI_AP_PASS   "12345678"
#define WIFI_AP_CHANNEL 6
#define WIFI_AP_MAX_CONN 4

// -------- DECLINATION LIMITS --------
#define DEC_MIN_LIMIT  -90.0f    // Minimum declination in degrees
#define DEC_MAX_LIMIT  90.0f     // Maximum declination in degrees

// -------- MERIDIAN FLIP --------
#define MIN_TO_MERIDIAN_FLIP  2   // Minutes before meridian crossing to perform flip
#define MERIDIAN_FLIP_ENABLED 1   // Enable automatic meridian flip (1=enabled, 0=disabled)

// -------- TRACKING MODES --------
#define TRACKING_SIDEREAL  1   // Sidereal tracking (stars)
#define TRACKING_SOLAR    2   // Solar tracking (sun)  
#define TRACKING_LUNAR    0   // Lunar tracking (moon)
#define DEFAULT_TRACKING_MODE TRACKING_SIDEREAL  // Default tracking mode

// -------- TRACKING RATES --------
// Sidereal day: 23h 56m 4.0905s = 86164.0905 seconds
#define SIDEREAL_DAY  86164.0905f
// Solar day: 24 hours = 86400 seconds  
#define SOLAR_DAY    86400.0f
// Lunar day: 24h 50m 28.2s = 89428.2 seconds
#define LUNAR_DAY    89428.2f

// Convert to tracking intervals in microseconds
#define TRACKING_INTERVAL_US  1000000  // 1 second intervals for tracking updates

#endif // CONFIG_H
