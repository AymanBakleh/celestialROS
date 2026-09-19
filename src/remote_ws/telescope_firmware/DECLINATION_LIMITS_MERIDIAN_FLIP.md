# Declination Limits and Meridian Flip Implementation

## Overview
This implementation adds declination limits (-90° to +90°) with wraparound behavior and automatic meridian flip functionality to the telescope mount controller.

## Features Added

### 1. Declination Limits
- **Range**: -90° to +90° (celestial equator coordinate system)
- **Wraparound Behavior**: 
  - DEC 91° → 89° (wraps at +90° limit)
  - DEC -91° → -89° (wraps at -90° limit)
- **Applied to**: All DEC commands (T1 goto, T12 step, T13 sync)

### 2. Meridian Flip
- **Trigger**: 2 minutes before meridian crossing (configurable)
- **Action**: 
  - RA: Add 180° (12 hours)
  - DEC: Invert direction
- **Status Tracking**: Scheduled/In progress states
- **Integration**: Automatic during tracking

## Configuration Constants

### Motor Direction (config.h)
```c
#define RA_INVERTED  0  // RA motor normal direction
#define DEC_INVERTED 1  // DEC motor inverted
```

### Declination Limits
```c
#define DEC_MIN_LIMIT  -90.0f    // Minimum declination
#define DEC_MAX_LIMIT  90.0f     // Maximum declination
```

### Meridian Flip
```c
#define MIN_TO_MERIDIAN_FLIP  2   // Minutes before meridian
#define MERIDIAN_FLIP_ENABLED 1   // Enable/disable flip
```

## Functions Added

### Conversion Functions
- `normalize_dec_deg()` - Declination limit normalization
- `normalize_ra_deg()` - RA circular normalization (existing)

### Meridian Flip Functions
- `check_meridian_flip()` - Monitor RA position
- `perform_meridian_flip()` - Execute the flip

## Test Results

### Declination Limits
✅ 45° → 45° (normal)  
✅ 91° → 89° (wraparound)  
✅ -91° → -89° (wraparound)  
✅ 180° → 0° (extreme wraparound)

### Motor Inversion
✅ RA: 0° → 90° = Positive direction (not inverted)  
✅ DEC: 0° → 45° = Negative direction (inverted)

### Meridian Flip Logic
✅ Activates at RA = 180° with 2-minute warning  
✅ Coordinate transformation: RA+180°, DEC inverted

## Usage

### Testing
```bash
# Test declination limits and meridian flip
g++ -o test_dec_limits test_dec_limits.cpp && ./test_dec_limits

# Test motor inversion
g++ -o test_motor_inversion test_motor_inversion.cpp && ./test_motor_inversion
```

### Configuration Changes
1. **Motor Direction**: Edit `RA_INVERTED`/`DEC_INVERTED` in config.h
2. **Flip Timing**: Edit `MIN_TO_MERIDIAN_FLIP` for warning time
3. **Enable/Disable**: Edit `MERIDIAN_FLIP_ENABLED`

## Integration Notes

### Command Processing
- **T1 (goto)**: Normalizes both RA and DEC
- **T12 (step)**: Applies DEC limits to stepped movement
- **T13 (sync)**: Normalizes sync coordinates

### Status Feedback
- Added meridian flip status to JSON feedback
- `meridian_flip_scheduled`: Boolean flag
- `meridian_flip_in_progress`: Boolean flag

### Motor Control
- Inversion handled in conversion functions
- Maintains tracking through meridian flip
- Preserves torque control during flip

## Compatibility
- Based on rDUINOScope meridian flip logic
- Adapted for servo-based control system
- Maintains existing command protocol
- Compatible with Stellarium interface
