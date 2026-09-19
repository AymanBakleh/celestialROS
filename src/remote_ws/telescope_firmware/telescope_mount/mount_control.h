#ifndef MOUNT_CONTROL_H
#define MOUNT_CONTROL_H

#include "config.h"
#include "servo_control.h"
#include <ArduinoJson.h>

// ============ Mount state (read by OLED, web, loop) ============
extern int16_t last_ra_pos_speed;
extern int16_t last_dec_pos_speed;
extern unsigned long last_speed_update;
extern float ra_speed;
extern float dec_speed;

extern int32_t ra_zero_steps;
extern int32_t dec_zero_steps;

extern bool torque_enabled;
extern bool ros_started;

extern bool velocityMode;
extern float vel_ra;
extern float vel_dec;
extern float vel_ra_acc;

// Follow mode variables (commented out)
// extern bool follow_mode;
// extern float follow_ra_speed;

// Hardware constants are now defined as macros in config.h
extern unsigned long lastVelTime;

extern int velocity_ra_spd;
extern int velocity_dec_spd;
extern int velocity_ra_acc;
extern int velocity_dec_acc;

extern float target_ra_rad;
extern float target_dec_rad;

extern int32_t current_ra_steps;
extern int32_t current_dec_steps;
extern int16_t last_ra_pos;
extern int16_t last_dec_pos;

extern float ra_ratio;
extern float dec_ratio;
extern float steps_per_rad_ra;
extern float steps_per_rad_dec;

extern int ra_spd;
extern int dec_spd;
extern int ra_acc;
extern int dec_acc;

extern unsigned long lastFeedbackTime;

extern int32_t last_sent_ra_steps;
extern int32_t last_sent_dec_steps;

// ============ API ============
void mount_init(void);
void mount_zero_encoders_at_startup(void);

bool process_command(JsonDocument& cmdDoc);

void mount_update_feedback(void);
void mount_update_motors(unsigned long now);
bool mount_is_moving(void);

void send_feedback(void);
void get_status_json(String& out);

void setTorque(bool enable);
void readDEFAStatus(void);
void setDEFAOff(void);
void setDEFAOn(void);
void setDEFACustom(int torque_value);

void apply_params(void);

int32_t rad_to_steps_ra(float rad);
int32_t rad_to_steps_dec(float rad);
float steps_to_rad_ra(int32_t steps);
float steps_to_rad_dec(int32_t steps);

float steps_to_deg_ra(int32_t steps);
float steps_to_deg_dec(int32_t steps);

float normalize_ra_deg(float ra_deg);
float normalize_dec_deg(float dec_deg);
float ra_angular_distance(float current_ra, float target_ra);
float get_current_ra_deg(void);
float get_current_dec_deg(void);

void check_meridian_flip(float current_ra_deg);
void perform_meridian_flip(void);

// Location/time for meridian flip
extern float site_latitude_deg;
extern float site_longitude_deg;
extern int local_time_hours;
extern int local_time_minutes;
extern int local_time_seconds;

void set_local_time(int h, int m, int s);
void advance_local_time(void);
double calculate_local_sidereal_time_deg(void);
void set_date_for_lst(int year, int month, int day);

// Enhanced coordinate calculation functions
float calculate_julian_day(int year, int month, int day, int hour, int minute, int second);
float calculate_gmst_deg(float jd);
float calculate_hour_angle_deg(float ra_deg);

// Time management functions
void set_time_mode(int mode);
int get_time_mode(void);
void set_rtc_available(bool available);
bool is_rtc_available(void);

// RTC management functions
void rtc_init(void);
void rtc_get_time(int& year, int& month, int& day, int& hour, int& minute, int& second);
void rtc_set_time(int year, int month, int day, int hour, int minute, int second);
void rtc_set_time_string(const char* time_str);
void rtc_update_local_time(void);
void rtc_test_hardware(void);

// Tracking mode functions
void calculate_tracking_rate(void);
void set_tracking_mode(int mode);
void update_tracking(unsigned long now);

#endif // MOUNT_CONTROL_H
