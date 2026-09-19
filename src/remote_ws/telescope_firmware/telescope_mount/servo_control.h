#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "config.h"
#include <Arduino.h>

// ST3215 / SCServo: official header is SCServo.h, class SMS_STS
#include <SCServo.h>

// Low-level servo bus control. No mount state (radians, zero, etc.) — just position read/write and torque/DEFA.

extern SMS_STS st;

void init_servo(void);

// read servo mode; 0=servo(position) mode, 3=motor(wheel) mode, -1 unknown
int read_servo_mode(uint8_t id);

void clamp_delta(int32_t current_steps, int32_t target_steps, int32_t* out_delta, int16_t* out_pos);

// motor(wheel) mode drive: send exact number of ticks
void write_servo_wheel(uint8_t id, int direction, int spd, int ticks_num);

// goto mode drive: use fixed GOTO_SPEED from config
void write_servo_goto(uint8_t id, int direction, int ticks_num);

void servo_enable_torque_ra(bool on);
void servo_enable_torque_dec(bool on);

void set_defa_off(void);
void set_defa_on(void);
void set_defa_custom(int torque_value);
void read_defa_status(void);

void servo_set_torque_callback(void (*callback)(bool));

void scan_servo_bus(void);



#endif // SERVO_CONTROL_H
