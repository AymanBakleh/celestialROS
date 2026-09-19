# Tracking Mode Replacement for Follow Mode

## Overview
Replace the "Follow Mode" section with a comprehensive "Tracking Mode" interface that includes:
- **Sidereal Tracking** (stars)
- **Solar Tracking** (sun) 
- **Lunar Tracking** (moon)
- **Manual Tracking** (custom speed control)

## Features

### 1. Tracking Mode Selection
- **Visual Mode Buttons**: Sidereal, Solar, Lunar, Manual
- **Active Mode Highlighting**: Shows current selection
- **Mode Descriptions**: Each mode shows purpose and rate
- **Smooth Transitions**: CSS animations and hover effects

### 2. Manual Tracking Control
- **Speed Slider**: 0.1 - 5.0 deg/sec range
- **Real-time Display**: Shows current speed setting
- **Start/Stop Buttons**: Control manual tracking
- **Status Indicators**: Manual mode ON/OFF display

### 3. DEC Speed Control
- **Speed Slider**: 500-4000 range (faster default)
- **Current Setting Display**: Shows DEC speed value
- **Set Button**: Apply speed changes
- **Real-time Feedback**: Updates immediately

### 4. Status Display
- **Current Mode**: Shows active tracking mode name
- **Tracking Rate**: Displays actual rate in arcsec/sec
- **Speed Indicators**: RA/DEC motor speeds
- **Movement Status**: Moving/Not moving indicator

## Commands Used

### Tracking Mode Control
```json
// Set Sidereal
{"T": 14, "mode": 1}

// Set Solar  
{"T": 14, "mode": 2}

// Set Lunar
{"T": 14, "mode": 0}
```

### Manual Tracking Control
```json
// Start manual tracking at 1.5 deg/sec
{"T": 16, "tracking_speed": 1.5}

// Stop manual tracking
{"T": 16, "tracking_speed": 0.0}
```

### DEC Speed Control
```json
// Set DEC speed to 3000
{"T": 17, "dec_speed": 3000}
```

### Status Request
```json
{"T": 15}
```

## CSS Styling

### Mode Buttons
```css
.mode-button {
    background-color: #3d3d3d;
    border: 2px solid #555;
    color: #ffffff;
    padding: 15px;
    border-radius: 5px;
    cursor: pointer;
    transition: all 0.3s ease;
}

.mode-button.active {
    background-color: #007acc;
    border-color: #00ff88;
    box-shadow: 0 0 10px rgba(0, 255, 136, 0.3);
}
```

### Manual Control Panel
```css
.manual-control {
    background-color: #2d4a2d;
    border-radius: 8px;
    padding: 20px;
}

.control-btn.start {
    background-color: #28a745;
}

.control-btn.stop {
    background-color: #dc3545;
}
```

## JavaScript Functions

### Mode Management
```javascript
function setTrackingMode(mode) {
    const command = {"T": 14, "mode": mode};
    sendCommand(command);
}

function updateModeButtons(activeMode) {
    // Remove active class from all buttons
    document.querySelectorAll('.mode-button').forEach(btn => {
        btn.classList.remove('active');
    });
    
    // Add active class to current mode
    const modeMap = {
        0: 'mode-lunar',
        1: 'mode-sidereal', 
        2: 'mode-solar'
    };
    
    const activeButton = document.getElementById(modeMap[activeMode]);
    if (activeButton) {
        activeButton.classList.add('active');
    }
}
```

### Manual Tracking
```javascript
function startManualTracking() {
    const speed = parseFloat(document.getElementById('tracking-speed').value);
    const command = {"T": 16, "tracking_speed": speed};
    sendCommand(command);
}

function stopManualTracking() {
    const command = {"T": 16, "tracking_speed": 0.0};
    sendCommand(command);
}
```

### Status Updates
```javascript
function updateStatus(status) {
    // Update current mode display
    document.getElementById('current-mode').textContent = status.tracking_mode_name;
    
    // Update tracking rate
    document.getElementById('current-rate').textContent = 
        (status.tracking_rate * 3600).toFixed(4);
    
    // Update speed displays
    document.getElementById('ra-speed').textContent = status.ra_speed;
    document.getElementById('dec-speed').textContent = status.dec_speed;
    
    // Update DEC speed setting
    document.getElementById('dec-speed-display').textContent = status.dec_spd_setting;
}
```

## Implementation Steps

### 1. Replace Follow Mode Section
- Remove existing "Follow Mode" HTML/CSS/JS
- Add new "Tracking Mode" section with mode buttons
- Implement manual tracking panel (hidden by default)

### 2. Add Mode Selection Logic
- Create visual buttons for each tracking mode
- Add active state management
- Show/hide manual panel based on mode selection

### 3. Integrate Manual Controls
- Add speed slider with real-time display
- Implement start/stop functionality
- Add status indicators for manual mode

### 4. Update Status Display
- Show current tracking mode prominently
- Display tracking rate in arcsec/sec
- Add motor speed indicators
- Include DEC speed setting display

## Benefits

### User Experience
✅ **Clear Mode Selection**: Visual buttons instead of dropdown  
✅ **Manual Control**: Custom speed tracking like follow mode  
✅ **Real-time Feedback**: Immediate status updates  
✅ **Professional Interface**: Modern design with smooth transitions  

### Functionality
✅ **All Tracking Modes**: Sidereal, Solar, Lunar, Manual  
✅ **Speed Control**: Both tracking speed and DEC motor speed  
✅ **Status Monitoring**: Complete tracking and motor status  
✅ **Responsive Design**: Works on desktop and mobile  

## File Structure
```
web-interface/
├── index.html (main interface)
├── css/
│   └── tracking.css (styles)
├── js/
│   ├── tracking.js (main logic)
│   └── status.js (status management)
└── assets/
    └── icons/ (mode icons)
```

The new tracking interface replaces follow mode with comprehensive tracking control including all tracking modes and manual speed control!
