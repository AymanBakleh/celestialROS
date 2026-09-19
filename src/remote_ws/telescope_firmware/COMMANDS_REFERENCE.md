# Telescope Mount Commands Reference

This document describes all available JSON commands for the Jarspace Telescope Mount system.

## Command Format
All commands use JSON format:
```json
{"T": <command_number>, "parameter": value, ...}
```

## Available Commands

### T=1 - Absolute RA/DEC Position (Stellarium)
Move telescope to absolute RA/DEC coordinates.
```json
{"T": 1, "ra_deg": 45.5, "dec_deg": 30.0}
```
- **ra_deg**: Right Ascension in degrees (0-360)
- **dec_deg**: Declination in degrees (-90 to +90)
- **Usage**: Primary GOTO command from Stellarium/ROS2

### T=2 - Request Status Feedback
Immediately send current telescope status.
```json
{"T": 2}
```
- **Response**: T=2 feedback with position, speed, tracking mode, etc.

### T=3 - Polar Alignment Position
Set telescope to polar alignment (RA=0°, DEC=90°) without moving.
```json
{"T": 3}
```
- **Usage**: For polar alignment setup

### T=4 - Follow Mode Control (Deprecated)
Enable/disable continuous RA tracking.
```json
{"T": 4, "enable": true, "ra_speed": 15.0}
```
- **enable**: true/false to enable follow mode
- **ra_speed**: RA tracking speed in arcsec/sec
- **Note**: Largely replaced by T=14 tracking modes

### T=11 - Torque Control
Enable/disable motor torque.
```json
{"T": 11, "torque": 1}
```
- **torque**: 1=ON, 0=OFF
- **Usage**: Enable/disable motors

### T=12 - Step Movement
Move RA/DEC by specified angle increments.
```json
{"T": 12, "ra_degrees": 1.0, "dec_degrees": 0.5}
```
- **ra_degrees**: RA movement in degrees
- **dec_degrees**: DEC movement in degrees
- **Usage**: Manual fine adjustment

### T=13 - Sync Position
Synchronize telescope position without moving.
```json
{"T": 13, "ra_degrees": 45.5, "dec_degrees": 30.0}
```
- **ra_degrees**: RA to sync to
- **dec_degrees**: DEC to sync to
- **Usage**: Tell telescope where it's pointing

### T=14 - Set Tracking Mode
Configure astronomical tracking mode.
```json
{"T": 14, "mode": 1}
```
- **mode**: 0=Lunar, 1=Sidereal, 2=Solar
- **Usage**: Set tracking rate for celestial objects

### T=15 - Get Tracking Status
Request current tracking mode status.
```json
{"T": 15}
```
- **Response**: T=2 feedback with tracking mode info

### T=16 - Set Location & Time
Set observer location and/or tracking parameters.
```json
{"T": 16, "site_latitude_deg": 33.5, "site_longitude_deg": 36.3}
```
```json
{"T": 16, "tracking_speed": 15.041}
```
- **site_latitude_deg**: Observer latitude
- **site_longitude_deg**: Observer longitude  
- **tracking_speed**: Custom tracking rate in arcsec/sec

### T=17 - Set DEC Speed
Configure DEC motor speed.
```json
{"T": 17, "dec_speed": 2000}
```
- **dec_speed**: DEC motor speed (0-4000)
- **Usage**: Fine-tune DEC movement speed

### T=18 - Time Management
Configure time source and settings.
```json
{"T": 18, "time_mode": 2}
```
```json
{"T": 18, "rtc_get_time": true}
```
```json
{"T": 18, "rtc_set_time": "2026-03-27 10:15:00"}
```
- **time_mode**: 0=RTC, 1=Web, 2=Default
- **rtc_get_time**: Read time from RTC
- **rtc_set_time**: Set RTC time

### T=19 - GOTO Coordinates
Advanced GOTO with hour angle calculation.
```json
{"T": 19, "ra_hours": 5.5, "dec_degrees": 0.0}
```
- **ra_hours**: RA in hours (0-24)
- **dec_degrees**: DEC in degrees (-90 to +90)
- **Usage**: Primary GOTO with HA-based servo movement

### T=20 - Set GOTO Speed
Configure GOTO movement speed for both axes.
```json
{"T": 20, "goto_speed": 3000}
```
- **goto_speed**: GOTO speed (0-4000)
- **Usage**: Set GOTO movement speed

### T=21 - Motor Direction Control
Invert motor direction if needed.
```json
{"T": 21, "motor": "ra", "direction": "inverted"}
```
```json
{"T": 21, "motor": "dec", "direction": "normal"}
```
- **motor**: "ra" or "dec"
- **direction**: "normal" or "inverted"
- **Usage**: Reverse motor rotation direction

### T=605 - Initialize Connection
System initialization command (sent by driver on connect).
```json
{"T": 605, "cmd": 0}
```
- **Usage**: Driver initialization handshake

## T=2 Feedback Response

The system responds with comprehensive status information:

```json
{
  "T": 2,
  "ra_ticks": 16384,
  "dec_ticks": 8192,
  "ra_deg": 90.0,
  "ra_deg_normalized": 90.0,
  "dec_deg": 45.0,
  "hour_angle_deg": -15.0,
  "hour_angle_hours": -1.0,
  "hour_angle_str": "-01:00:00",
  "moving": false,
  "target_ra_ticks": 16384,
  "target_dec_ticks": 8192,
  "ra_error": 0,
  "dec_error": 0,
  "ra_speed": 0.0,
  "dec_speed": 0.0,
  "dec_spd_setting": 3950,
  "goto_speed": 1500,
  "meridian_flip_scheduled": false,
  "meridian_flip_in_progress": false,
  "tracking_mode": 1,
  "tracking_mode_name": "Sidereal",
  "tracking_rate": 15.041,
  "site_latitude_deg": 33.50917,
  "site_longitude_deg": 36.31167,
  "local_time": "10:15:30",
  "local_sidereal_time_deg": 105.0,
  "time_mode": 2,
  "rtc_available": true,
  "julian_day": 2460474.92743
}
```

## Usage Notes

1. **Connection**: Driver sends T=605 on connect to initialize system
2. **GOTO Logic**: T=19 uses Hour Angle for servo movement (correct astronomical approach)
3. **Tracking**: T=14 sets tracking mode, T=16 can set custom tracking rates
4. **Motor Control**: T=11 for torque, T=21 for direction inversion
5. **Feedback**: T=2 requests immediate status, sent periodically every 50ms

## Web Interface Commands

The web interface uses HTTP POST to `/api/cmd` with the same JSON format as above.

## TCP Server Commands

The TCP server on port 10001 accepts the same JSON commands as the serial interface.
