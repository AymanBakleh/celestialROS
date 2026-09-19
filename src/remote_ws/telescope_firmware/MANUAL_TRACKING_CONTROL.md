# Manual Tracking Control and DEC Speed Fix

## New Features Added

### 1. Manual Tracking Control (T16)
**Purpose**: Activate tracking with custom speed (like follow mode but manual control)

**Commands**:
```json
// Start manual tracking at 1.0 deg/sec
{"T": 16, "tracking_speed": 1.0}

// Stop manual tracking
{"T": 16, "tracking_speed": 0.0}
```

**Behavior**:
- Activates `velocityMode` for continuous RA tracking
- Uses custom `tracking_speed` instead of preset tracking rates
- Can be started/stopped dynamically
- Serial output: `MANUAL TRACKING: ENABLED/DISABLED`

### 2. DEC Speed Control (T17)
**Purpose**: Adjust DEC motor speed for faster goto movements

**Commands**:
```json
// Set DEC speed to 3000 (fast)
{"T": 17, "dec_speed": 3000}

// Set DEC speed to 1000 (medium)
{"T": 17, "dec_speed": 1000}

// Set DEC speed to 500 (slow)
{"T": 17, "dec_speed": 500}
```

**Behavior**:
- Updates `dec_spd` variable in real-time
- Constrained to safe range (500-4000)
- Serial output: `DEC SPEED: Set to [value]`
- Affects all subsequent DEC movements

### 3. Enhanced DEC Speed Default
**Config Change**: `DEFAULT_DEC_SPD = 2000` (was 1500)
- DEC now moves ~33% faster by default
- Still adjustable via T17 command

## Configuration Constants

```c
#define DEFAULT_DEC_SPD   2000       // default DEC speed (faster than RA)
#define DEFAULT_SPD       1500       // default RA speed
```

## Enhanced Status Feedback

### New JSON Fields
```json
{
  "ra_speed": 1500,
  "dec_speed": 2000,
  "dec_spd_setting": 2000,    // NEW: Current DEC speed setting
  "tracking_mode": 1,
  "tracking_mode_name": "Sidereal"
}
```

## Usage Examples

### Web GUI Integration
```javascript
// Manual tracking control
function setManualTracking(speed) {
    const command = {
        "T": 16,
        "tracking_speed": speed
    };
    sendCommand(command);
}

// DEC speed control
function setDecSpeed(speed) {
    const command = {
        "T": 17,
        "dec_speed": speed
    };
    sendCommand(command);
}

// HTML Controls
<div>
    <h3>Manual Tracking</h3>
    <input type="range" id="tracking-speed" min="0.1" max="5.0" step="0.1" value="1.0">
    <button onclick="setManualTracking(document.getElementById('tracking-speed').value)">
        Start Tracking
    </button>
    <button onclick="setManualTracking(0)">Stop Tracking</button>
</div>

<div>
    <h3>DEC Speed</h3>
    <input type="range" id="dec-speed" min="500" max="4000" step="100" value="2000">
    <button onclick="setDecSpeed(document.getElementById('dec-speed').value)">
        Set DEC Speed
    </button>
    <span>Current: <span id="dec-speed-display">2000</span></span>
</div>
```

### Serial Monitor Examples
```
MANUAL TRACKING: ENABLED - RA speed=1.5 deg/sec
DEC SPEED: Set to 3000
MANUAL TRACKING: DISABLED
```

## Benefits

### Manual Tracking Control
✅ **Custom Speed**: Set any tracking speed (0.1-5.0 deg/sec)  
✅ **On/Off Control**: Start/stop tracking dynamically  
✅ **Follow Mode Alternative**: Manual control over tracking behavior  
✅ **Real-time Adjustment**: Change speed without stopping tracking  

### DEC Speed Fix
✅ **Faster Default**: 2000 vs 1500 (33% improvement)  
✅ **Dynamic Control**: Adjust speed during operation  
✅ **Range Control**: 500-4000 speed range for fine control  
✅ **Status Feedback**: See current DEC speed in JSON  

## Problem Solved

### Before
- DEC speed was slow (1500 default)
- No manual tracking control
- Fixed tracking modes only (no custom speed)

### After  
- DEC speed is fast (2000 default, adjustable)
- Manual tracking control with custom speeds
- Real-time speed adjustment
- Enhanced status feedback

The telescope now has full manual tracking control and fast DEC movement!
