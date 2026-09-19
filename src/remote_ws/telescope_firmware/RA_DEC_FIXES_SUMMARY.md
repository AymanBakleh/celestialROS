# RA 24h Wrapper and DEC Speed Fixes

## Issues Fixed

### 1. RA 24h Wrapper Issue
**Problem**: RA values like 295° were going to negative values  
**Root Cause**: `ra_angular_distance` function wasn't handling circular wrapping correctly  

**Fix**: Updated `ra_angular_distance()` to:
- Normalize both current and target angles to [0, 360) range
- Calculate shortest path around the circle (handles wraparound at 0°/360°)
- Properly handle cases where 24h = 0°, 12h = 180°, etc.

### 2. DEC Speed Issue  
**Problem**: DEC motor was slow after position tracking fix  
**Root Cause**: Double inversion in position tracking logic

**Fix**: Corrected position tracking to account for motor inversion:
- When `DEC_INVERTED=1`: motor direction -1 = position +1
- When `DEC_INVERTED=0`: motor direction -1 = position -1
- Applied fix to all motor control modes (goto, follow, tracking)

## Technical Details

### RA Movement Logic
```c
// Before (problematic):
float diff = normalize_ra_deg(target_ra - current_ra);

// After (fixed):
current_ra = normalize_ra_deg(current_ra);
target_ra = normalize_ra_deg(target_ra);
float diff = target_ra - current_ra;
if (diff > 180.0f) diff -= 360.0f;
else if (diff < -180.0f) diff += 360.0f;
```

### DEC Position Tracking Logic
```c
// Before (double inversion):
direction = DEC_INVERTED ? -direction : direction;
current_dec_steps += direction * ticks_to_move;

// After (correct):
direction = DEC_INVERTED ? -direction : direction;
int actual_movement = DEC_INVERTED ? -direction : direction;
current_dec_steps += actual_movement * ticks_to_move;
```

## Test Results

### RA 24h Wrapper
✅ 0h → 0° (correct)  
✅ 12h → 180° (correct)  
✅ 295° → 295° (correct)  
✅ 360° → 0° (correct)  
✅ 400° → 40° (correct)

### DEC Speed with DEC_INVERTED=1
✅ Error 1000 → motor direction -1 (reverse)  
✅ Position moves +100 (toward target)  
✅ New error 900 (smaller, moving correctly)

## Current Configuration
- **RA_INVERTED = 0**: Normal direction
- **DEC_INVERTED = 1**: Inverted direction (your setting)

## Expected Behavior

### RA Movement
- **295°**: Should move from current position to 295° using shortest path
- **24h/0h**: Should both normalize to 0° (24h wrapper working)
- **12h**: Should convert to 180° (hour-to-degree conversion)

### DEC Movement  
- **Positive error**: Motor direction -1 (reverse due to inversion)
- **Position tracking**: Moves + toward target (correct speed)
- **Convergence**: Error decreases each iteration (motor stops at target)

## Usage Notes

### Sending RA Commands
- **Hours (0-24)**: Send as hours, firmware converts to degrees (h × 15)
- **Degrees (0-360)**: Send as degrees directly
- **24h wrapper**: 24h = 0°, works correctly now

### DEC Motor Behavior
- **DEC_INVERTED=1**: Motor turns reverse when commanded forward
- **Speed**: Normal speed restored (no more slow movement)
- **Accuracy**: Reaches target and stops correctly

## Files Modified
- `mount_control.cpp`: Fixed RA angular distance and DEC position tracking
- `test_ra_dec_fixes.cpp`: Test script to verify fixes

The telescope should now correctly handle RA 24h wrapping and DEC should move at normal speed with proper stopping behavior.
