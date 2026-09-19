#include "servo_control.h"
#include "config.h"
#include <Arduino.h>

SMS_STS st;

static void (*s_torque_callback)(bool) = nullptr;

void servo_set_torque_callback(void (*callback)(bool)) {
    s_torque_callback = callback;
}

void init_servo(void) {
    SERVO_SERIAL.begin(SERVO_BAUD, SERIAL_8N1, S_RXD, S_TXD);
    st.pSerial = &SERVO_SERIAL;
    delay(1000);
}

int read_servo_mode(uint8_t id) {
    // SCServo library returns mode for SMS_STS servos
    int m = st.ReadMode(id);
    if (m == 0 || m == 3) return m;
    return -1;
}

void clamp_delta(int32_t current_steps, int32_t target_steps, int32_t* out_delta, int16_t* out_pos) {
    int32_t delta = target_steps - current_steps;
    if (delta > 2048) delta = 2048;
    if (delta < -2048) delta = -2048;
    *out_delta = delta;
    int32_t next = current_steps + delta;
    *out_pos = (int16_t)(((next % STEPS_PER_REV) + STEPS_PER_REV) % STEPS_PER_REV);
    if (*out_pos == STEPS_PER_REV) *out_pos = 0;
}

// Force recompilation - remove this line after successful build
static int force_recompile = 0;

void write_servo_wheel(uint8_t id, int direction, int spd, int ticks_num) {
    // direction: -1 reverse, +1 forward, 0 stop
    int s = constrain(spd, 0, SERVO_MAX_SPEED);
    if (direction == 0 || s == 0) {
        // stop by sending speed 0 (pos doesn't matter much)
        st.WritePosEx(id, 0, 0, 0);
        return;
    }
    int pos = direction > 0 ? ticks_num : -ticks_num;
    st.WritePosEx(id, pos, s, 0);
}

void write_servo_goto(uint8_t id, int direction, int ticks_num) {
    // direction: -1 reverse, +1 forward, 0 stop
    if (direction == 0) {
        // stop by sending speed 0
        st.WritePosEx(id, 0, 0, 0);
        return;
    }
    int pos = direction > 0 ? ticks_num : -ticks_num;
    st.WritePosEx(id, pos, 3950, 0);
}

void servo_enable_torque_ra(bool on) {
    st.EnableTorque(ID_RA, on ? 1 : 0);
    delay(5);
}

void servo_enable_torque_dec(bool on) {
    st.EnableTorque(ID_DEC, on ? 1 : 0);
}

void set_defa_off(void) {
    Serial.println("[DEFA] Setting torque to MINIMUM");
    st.unLockEprom(ID_RA);
    st.unLockEprom(ID_DEC);
    delay(5);
    st.writeByte(ID_RA, ST_TORQUE_LIMIT_ADDR, 50);
    delay(5);
    st.writeByte(ID_DEC, ST_TORQUE_LIMIT_ADDR, 50);
    delay(5);
    st.writeByte(ID_RA, ST_CURRENT_LIMIT_ADDR, 50);
    delay(5);
    st.writeByte(ID_DEC, ST_CURRENT_LIMIT_ADDR, 50);
    delay(5);
    st.LockEprom(ID_RA);
    st.LockEprom(ID_DEC);
    st.EnableTorque(ID_RA, 0);
    st.EnableTorque(ID_DEC, 0);
    if (s_torque_callback) s_torque_callback(false);
    read_defa_status();
}

void set_defa_on(void) {
    Serial.println("[DEFA] Setting torque to MAXIMUM");
    st.unLockEprom(ID_RA);
    st.unLockEprom(ID_DEC);
    delay(5);
    st.writeByte(ID_RA, ST_TORQUE_LIMIT_ADDR, 1000);
    delay(5);
    st.writeByte(ID_DEC, ST_TORQUE_LIMIT_ADDR, 1000);
    delay(5);
    st.writeByte(ID_RA, ST_CURRENT_LIMIT_ADDR, 1000);
    delay(5);
    st.writeByte(ID_DEC, ST_CURRENT_LIMIT_ADDR, 1000);
    delay(5);
    st.LockEprom(ID_RA);
    st.LockEprom(ID_DEC);
    st.EnableTorque(ID_RA, 1);
    delay(5);
    st.EnableTorque(ID_DEC, 1);
    if (s_torque_callback) s_torque_callback(true);
    read_defa_status();
}

void set_defa_custom(int torque_value) {
    torque_value = constrain(torque_value, 0, 1000);
    Serial.print("[DEFA] Setting custom torque: ");
    Serial.println(torque_value);
    st.unLockEprom(ID_RA);
    st.unLockEprom(ID_DEC);
    delay(5);
    st.writeByte(ID_RA, ST_TORQUE_LIMIT_ADDR, torque_value);
    delay(5);
    st.writeByte(ID_DEC, ST_TORQUE_LIMIT_ADDR, torque_value);
    delay(5);
    st.LockEprom(ID_RA);
    st.LockEprom(ID_DEC);
    if (torque_value == 0) {
        st.EnableTorque(ID_RA, 0);
        st.EnableTorque(ID_DEC, 0);
        if (s_torque_callback) s_torque_callback(false);
    } else {
        st.EnableTorque(ID_RA, 1);
        st.EnableTorque(ID_DEC, 1);
        if (s_torque_callback) s_torque_callback(true);
    }
    read_defa_status();
}

void read_defa_status(void) {
    Serial.println("\n--- DEFA Status ---");
    st.unLockEprom(ID_RA);
    int ra_torque = st.readByte(ID_RA, ST_TORQUE_LIMIT_ADDR);
    int ra_current = st.readByte(ID_RA, ST_CURRENT_LIMIT_ADDR);
    st.LockEprom(ID_RA);
    Serial.print("RA (ID "); Serial.print(ID_RA); Serial.print("): Torque="); Serial.print(ra_torque); Serial.print(", Current="); Serial.println(ra_current);
    st.unLockEprom(ID_DEC);
    int dec_torque = st.readByte(ID_DEC, ST_TORQUE_LIMIT_ADDR);
    int dec_current = st.readByte(ID_DEC, ST_CURRENT_LIMIT_ADDR);
    st.LockEprom(ID_DEC);
    Serial.print("DEC (ID "); Serial.print(ID_DEC); Serial.print("): Torque="); Serial.print(dec_torque); Serial.print(", Current="); Serial.println(dec_current);
    Serial.println("-------------------\n");
}

void scan_servo_bus(void) {
    Serial.println("\n[SERVO_SCAN] Scanning for IDs 11-15 (ST3215)...");
    int found_count = 0;
    for (uint8_t id = 11; id <= 15; id++) {
        int ping_id = st.Ping(id);
        if (ping_id != -1) {
            Serial.print("Servo ID:"); Serial.print(id, DEC);
            Serial.println(" (ping ok)");
            found_count++;
            delay(50);
        }
    }
    Serial.print("Total servos found: ");
    Serial.println(found_count);
}

