# Motor Direction Configuration

## Overview
You can now invert the direction of RA and DEC motors independently using the configuration settings in `config.h`.

## Configuration

Edit the following lines in `config.h`:

```cpp
// -------- Motor Direction --------
// Set to 1 to invert motor direction, 0 for normal direction
#define RA_INVERTED  1
#define DEC_INVERTED 0
```

## Settings

| Setting | Value | Effect |
|---------|-------|--------|
| `RA_INVERTED` | `1` | RA motor moves in opposite direction |
| `RA_INVERTED` | `0` | RA motor moves in normal direction |
| `DEC_INVERTED` | `1` | DEC motor moves in opposite direction |
| `DEC_INVERTED` | `0` | DEC motor moves in normal direction |

## How to Test

1. **Upload the firmware** with current settings
2. **Test RA direction**: Send a small RA movement command
3. **Test DEC direction**: Send a small DEC movement command
4. **Adjust if needed**: Change the inverted flags and re-upload

## Common Scenarios

### Both motors move in wrong direction
```cpp
#define RA_INVERTED  1  // Change to 0 if currently 1
#define DEC_INVERTED 1  // Change to 0 if currently 1
```

### Only RA moves in wrong direction
```cpp
#define RA_INVERTED  1  // Change to 0 if currently 1
#define DEC_INVERTED 0  // Keep as is
```

### Only DEC moves in wrong direction
```cpp
#define RA_INVERTED  0  // Keep as is
#define DEC_INVERTED 1  // Change to 0 if currently 1
```

## Current Default Settings

- **RA**: Inverted (`RA_INVERTED = 1`)
- **DEC**: Normal direction (`DEC_INVERTED = 0`)

## Notes

- After changing these settings, you must re-upload the firmware
- The inversion affects both goto commands and manual movements
- The web interface will show corrected angles regardless of inversion
- Motor speed and torque settings remain the same

## Troubleshooting

If motors still don't move correctly after changing these settings:

1. Check servo wiring (ensure correct servo IDs)
2. Verify power supply to servos
3. Test with small movements first
4. Check for mechanical binding in the mount
