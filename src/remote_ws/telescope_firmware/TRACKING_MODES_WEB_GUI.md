# Tracking Modes Implementation for Web GUI

## Overview
Added three tracking modes to telescope firmware based on rDUINOScope implementation:
- **Sidereal Tracking** (mode 1): Tracks stars at 15.0411 arcsec/sec
- **Solar Tracking** (mode 2): Tracks sun at 15.0000 arcsec/sec  
- **Lunar Tracking** (mode 0): Tracks moon at 14.4921 arcsec/sec

## Commands for Web GUI

### Set Tracking Mode
```json
{"T":14,"mode":1}  // Sidereal
{"T":14,"mode":2}  // Solar
{"T":14,"mode":0}  // Lunar
```

### Get Tracking Status
```json
{"T":15}  // Returns full status including tracking mode
```

### Response Format (T15)
```json
{
  "T": 2,
  "ra_ticks": 12345,
  "dec_ticks": 67890,
  "ra_deg": 45.123,
  "ra_deg_normalized": 45.123,
  "dec_deg": 30.456,
  "moving": true,
  "target_ra_ticks": 13000,
  "target_dec_ticks": 68000,
  "ra_error": 655,
  "dec_error": 110,
  "ra_speed": 1500,
  "dec_speed": 0,
  "meridian_flip_scheduled": false,
  "meridian_flip_in_progress": false,
  "tracking_mode": 1,
  "tracking_mode_name": "Sidereal",
  "tracking_rate": 0.00417807
}
```

## Web GUI Implementation

### HTML Controls
```html
<div class="tracking-controls">
  <h3>Tracking Mode</h3>
  <select id="tracking-mode">
    <option value="1">Sidereal (Stars)</option>
    <option value="2">Solar (Sun)</option>
    <option value="0">Lunar (Moon)</option>
  </select>
  <button onclick="setTrackingMode()">Set Mode</button>
</div>

<div class="tracking-status">
  <h4>Current: <span id="current-mode">Sidereal</span></h4>
  <p>Rate: <span id="tracking-rate">15.0411</span> arcsec/sec</p>
</div>
```

### JavaScript Functions
```javascript
function setTrackingMode() {
  const mode = parseInt(document.getElementById('tracking-mode').value);
  const command = {"T": 14, "mode": mode};
  
  fetch('/api/command', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(command)
  })
  .then(response => response.json())
  .then(data => {
    updateTrackingStatus(data);
  });
}

function updateTrackingStatus(status) {
  document.getElementById('current-mode').textContent = status.tracking_mode_name;
  document.getElementById('tracking-rate').textContent = 
    (status.tracking_rate * 3600).toFixed(4); // Convert to arcsec/sec
}
```

## Tracking Rate Details

| Mode | Day Length (seconds) | Rate (deg/sec) | Rate (arcsec/sec) | Rate (deg/day) |
|-------|-------------------|----------------|-------------------|----------------|
| Sidereal | 86,164.0905 | 0.00417807 | 15.0411 | 360.986 |
| Solar | 86,400.0000 | 0.00416667 | 15.0000 | 360.000 |
| Lunar | 89,428.2000 | 0.00402558 | 14.4921 | 347.810 |

### Key Differences
- **Solar vs Sidereal**: Solar is 0.27% faster (0.0411 arcsec/sec difference)
- **Lunar vs Sidereal**: Lunar is 3.65% slower (0.5490 arcsec/sec difference)

## Usage Examples

### Astrophotography
- **Sidereal**: For star tracking and deep sky objects
- **Solar**: For solar imaging and eclipse tracking

### Lunar Observation  
- **Lunar**: For lunar photography and surface features

### Visual Observation
- **Sidereal**: Default mode for general star watching
- **Solar**: Only for solar observation with proper filters

## Implementation Notes

1. **Automatic Updates**: Tracking updates every 1 second when active
2. **Motor Integration**: Respects RA_INVERTED setting from config.h
3. **Smooth Tracking**: Uses small step increments for accurate following
4. **Status Feedback**: Real-time tracking mode and rate in status updates
5. **Compatibility**: Works with existing meridian flip and declination limits

## Testing

Use the test script to verify implementation:
```bash
cd /home/ayman/Jarspace/telescope_firmware
g++ -o test_tracking_modes test_tracking_modes.cpp
./test_tracking_modes
```

This will display all tracking rates, command examples, and mode comparisons.
