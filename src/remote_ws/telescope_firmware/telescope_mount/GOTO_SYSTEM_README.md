# Telescope Goto System with Enhanced Time Management

## Overview

This enhanced telescope mount system provides accurate goto functionality with comprehensive time management options and precise coordinate calculations based on astronomical algorithms.

## Features

### Time Management
- **Default Time**: Uses 12:00:00 as default time
- **Web Time**: Synchronizes with web browser time when connected
- **RTC Time**: Uses DS1302 real-time clock for accurate timekeeping

### Coordinate Calculations
- **Julian Day Calculation**: Precise conversion from calendar date/time to Julian Day
- **GMST Calculation**: Greenwich Mean Sidereal Time computation
- **LST Calculation**: Local Sidereal Time adjusted for observer longitude
- **Hour Angle**: Accurate HA = LST - RA calculation for goto positioning

### Web Interface
- **Enhanced GUI**: Modern responsive interface with real-time updates
- **WebSocket Support**: Live status updates without page refresh
- **Time Controls**: Easy switching between time sources
- **Goto Interface**: Direct RA/DEC coordinate input
- **Status Display**: Comprehensive system status including coordinates

## Hardware Requirements

### DS1302 RTC Connection
```
RTC_CE_PIN  -> GPIO 25
RTC_IO_PIN  -> GPIO 26  
RTC_SCLK_PIN -> GPIO 27
```

### Power Requirements
- 3.3V or 5V power for RTC module
- Backup battery (CR2032) for RTC timekeeping

## Coordinate Calculation Algorithm

### 1. Time Management
```cpp
// Damascus is UTC+3
local_time = utc_time + 3 hours
```

### 2. Julian Day Calculation
```cpp
if (month <= 2) {
    year -= 1;
    month += 12;
}
A = year / 100;
B = 2 - A + A / 4;
day_fraction = hour/24 + minute/1440 + second/86400;
JD = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + day_fraction + B - 1524.5
```

### 3. GMST Calculation
```cpp
T = (JD - 2451545.0) / 36525.0
GMST_0h = 24110.54841 + 8640184.812866*T + 0.093104*T² - 0.0000062*T³
GMST_deg = (GMST_0h / 240) mod 360
```

### 4. LST Calculation
```cpp
UT_hours = utc_hours + utc_minutes/60 + utc_seconds/3600
GMST_additional = UT_hours * 15.04106864
total_GMST = (GMST_deg + GMST_additional) mod 360
LST_deg = (total_GMST + longitude) mod 360
```

### 5. Hour Angle Calculation
```cpp
HA_deg = LST_deg - RA_deg
// Normalize to -180 to +180 degrees
while (HA_deg > 180) HA_deg -= 360
while (HA_deg < -180) HA_deg += 360
```

### 6. Motor Position Conversion
```cpp
HA_steps = HA_deg * TICKS_PER_AXIS_DEG
DEC_steps = DEC_deg * TICKS_PER_AXIS_DEG
```

## Command Protocol

### Time Management Commands (T=18)
```json
{
  "T": 18,
  "time_mode": 0,           // 0=Default, 1=Web, 2=RTC
  "rtc_available": true,    // RTC status
  "rtc_set_time": true,     // Set RTC time
  "year": 2024,
  "month": 1,
  "day": 1,
  "hour": 12,
  "minute": 0,
  "second": 0,
  "rtc_get_time": true      // Read from RTC
}
```

### Goto Command (T=19)
```json
{
  "T": 19,
  "ra_hours": 5.5,         // Right Ascension in hours
  "dec_degrees": 0.0       // Declination in degrees
}
```

### Location/Time Command (T=16)
```json
{
  "T": 16,
  "site_latitude_deg": 33.50917,
  "site_longitude_deg": 36.31167,
  "local_time_hours": 12,
  "local_time_minutes": 0,
  "local_time_seconds": 0
}
```

## Web Interface Usage

### Accessing the Interface
1. Connect to WiFi: "Jarspace" (password: "12345678")
2. Open browser to: http://192.168.4.1
3. Enhanced interface loads automatically

### Time Management
1. **Default Time**: Click "Use Default Time (12:00:00)"
2. **Web Time**: Click "Use Web Time" then "Sync Now"
3. **RTC Time**: Click "Use RTC Time" then "Read from RTC"

### Setting RTC Time
1. Enter date and time in the RTC section
2. Click "Set RTC Time"
3. System will confirm successful setting

### Goto Operation
1. Enter target coordinates:
   - RA in hours (0-24)
   - DEC in degrees (-90 to +90)
2. Click "Goto"
3. System calculates hour angle and moves telescope
4. Status shows calculated values and motor steps

### Location Settings
1. Enter observer latitude and longitude
2. Click "Set Location"
3. Used for LST calculations and meridian flip

## Status Information

The system provides comprehensive status including:
- Current time mode and RTC status
- Local time and Local Sidereal Time
- Julian Day for precise calculations
- Current RA/DEC positions in degrees
- Target positions and motor steps
- Tracking mode and rates
- Meridian flip status

## Testing and Validation

### Coordinate Test Program
Compile and run the test program to validate calculations:
```bash
cd /home/ayman/Jarspace/telescope_firmware
g++ -o test_coordinate_calculations test_coordinate_calculations.cpp
./test_coordinate_calculations
```

### Expected Results
- Julian Day accuracy: ±0.1 days
- LST calculation: Proper longitude adjustment
- Hour Angle: Correct HA = LST - RA computation
- Motor steps: Accurate degree-to-step conversion

## Integration Notes

### RTC Initialization
The system automatically initializes the DS1302 on startup:
```cpp
void mount_init(void) {
    rtc_init();  // Initialize RTC first
    // ... other initialization
}
```

### Time Mode Switching
The system automatically handles time source switching:
- Default mode: Uses internal clock starting at 12:00:00
- Web mode: Syncs with browser time
- RTC mode: Reads from DS1302 every second

### Coordinate Updates
All coordinate calculations use the current time mode:
- Julian Day uses current date/time
- LST calculation includes proper longitude correction
- Hour angle computed in real-time for accurate goto

## Troubleshooting

### RTC Issues
- Check connections to GPIO 25, 26, 27
- Verify backup battery is installed
- Use "Read from RTC" to test communication

### Coordinate Accuracy
- Verify location coordinates (Damascus: 33.50917°, 36.31167°)
- Check time synchronization
- Validate Julian Day calculation with test program

### Web Interface Issues
- Ensure WiFi connection to "Jarspace"
- Check browser console for WebSocket errors
- Verify IP address: 192.168.4.1

## Future Enhancements

- GPS time synchronization
- NTP network time support
- Extended object database
- Advanced pointing models
- Multiple site location storage
