# How to Replace Follow Mode with Tracking Mode

## Step 1: Find Your Current Follow Mode Section

Look for HTML similar to this in your web interface:

```html
<!-- CURRENT FOLLOW MODE SECTION (TO REPLACE) -->
<div class="container">
    <h2>Follow Mode</h2>
    <div>
        <label>RA Speed (deg/sec)</label>
        <div class="speed-control">
            <button onclick="decreaseSpeed()">−</button>
            <input type="number" id="ra-speed" value="1.0" step="0.1">
            <button onclick="increaseSpeed()">+</button>
        </div>
        <button onclick="startFollow()">Follow ON</button>
    </div>
    <p>RA tracking at constant speed. Use Torque Off/Stop to stop.</p>
</div>
```

## Step 2: Replace with New Tracking Mode

### A. Add CSS Styles (in <style> section)

Add these CSS styles to your existing `<style>` section:

```css
/* NEW TRACKING MODE STYLES */
.tracking-section {
    background-color: #1e3a8f;
    border: 2px solid #007acc;
    border-radius: 8px;
    padding: 20px;
    margin-bottom: 20px;
}

.mode-selector {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 15px;
    margin-bottom: 20px;
}

.mode-button {
    background-color: #3d3d3d;
    border: 2px solid #555;
    color: #ffffff;
    padding: 15px;
    border-radius: 5px;
    cursor: pointer;
    text-align: center;
    font-weight: bold;
    transition: all 0.3s ease;
}

.mode-button:hover {
    background-color: #007acc;
    border-color: #005a9e;
}

.mode-button.active {
    background-color: #007acc;
    border-color: #00ff88;
    box-shadow: 0 0 10px rgba(0, 255, 136, 0.3);
}

.manual-control {
    background-color: #2d4a2d;
    border-radius: 8px;
    padding: 20px;
    margin-bottom: 20px;
}

.speed-control {
    display: flex;
    align-items: center;
    gap: 15px;
    margin-bottom: 15px;
}

.speed-slider {
    flex: 1;
}

.speed-value {
    min-width: 80px;
    text-align: center;
    font-weight: bold;
    color: #00ff88;
}

.control-buttons {
    display: flex;
    gap: 10px;
}

.control-btn {
    flex: 1;
    padding: 12px;
    border: none;
    border-radius: 5px;
    font-size: 16px;
    cursor: pointer;
    transition: all 0.3s ease;
}

.control-btn.start {
    background-color: #28a745;
    color: white;
}

.control-btn.start:hover {
    background-color: #1e5e3a;
}

.control-btn.stop {
    background-color: #dc3545;
    color: white;
}

.control-btn.stop:hover {
    background-color: #c82333;
}

input[type="range"] {
    width: 100%;
    height: 8px;
    border-radius: 5px;
    background: #3d3d3d;
    outline: none;
}

input[type="range"]::-webkit-slider-thumb {
    appearance: none;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: #007acc;
    cursor: pointer;
}

input[type="range"]::-moz-range-thumb {
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: #007acc;
    cursor: pointer;
    border: none;
}
```

### B. Replace HTML Section

Replace your entire "Follow Mode" section with this:

```html
<!-- NEW TRACKING MODE SECTION -->
<div class="container tracking-section">
    <h2>Tracking Mode</h2>
    <div class="mode-selector">
        <div class="mode-button" id="mode-sidereal" onclick="setTrackingMode(1)">
            <h3>Sidereal</h3>
            <p>Track stars at 15.0411 arcsec/sec</p>
            <p>Default for astrophotography</p>
        </div>
        <div class="mode-button" id="mode-solar" onclick="setTrackingMode(2)">
            <h3>Solar</h3>
            <p>Track sun at 15.0000 arcsec/sec</p>
            <p>For solar observation</p>
        </div>
        <div class="mode-button" id="mode-lunar" onclick="setTrackingMode(0)">
            <h3>Lunar</h3>
            <p>Track moon at 14.4921 arcsec/sec</p>
            <p>For lunar observation</p>
        </div>
        <div class="mode-button" id="mode-manual" onclick="setManualMode()">
            <h3>Manual</h3>
            <p>Custom tracking speed control</p>
            <p>Manual speed adjustment</p>
        </div>
    </div>
</div>

<div class="container manual-control" id="manual-panel" style="display: none;">
    <h2>Manual Tracking Control</h2>
    
    <div class="speed-control">
        <label for="tracking-speed">Tracking Speed (deg/sec):</label>
        <div class="speed-slider">
            <input type="range" id="tracking-speed" min="0.1" max="5.0" step="0.1" value="1.0">
            <div class="speed-value"><span id="speed-display">1.0</span></div>
        </div>
    </div>
    
    <div class="control-buttons">
        <button class="control-btn start" onclick="startManualTracking()">Start Tracking</button>
        <button class="control-btn stop" onclick="stopManualTracking()">Stop Tracking</button>
    </div>
</div>

<div class="container">
    <h2>DEC Speed Control</h2>
    
    <div class="speed-control">
        <label for="dec-speed">DEC Motor Speed:</label>
        <div class="speed-slider">
            <input type="range" id="dec-speed" min="500" max="4000" step="100" value="2000">
            <div class="speed-value"><span id="dec-speed-display">2000</span></div>
        </div>
    </div>
    
    <div class="control-buttons">
        <button class="control-btn start" onclick="setDecSpeed()">Set DEC Speed</button>
    </div>
</div>
```

### C. Add JavaScript Functions

Add these JavaScript functions to your existing `<script>` section:

```javascript
// NEW TRACKING MODE JAVASCRIPT

let currentTrackingMode = 1; // Default to sidereal
let manualTrackingActive = false;

// Set tracking mode
function setTrackingMode(mode) {
    const command = {
        "T": 14,
        "mode": mode
    };
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(command)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Tracking mode set:', data);
        currentTrackingMode = mode;
        updateStatus(data);
    })
    .catch(error => {
        console.error('Error setting tracking mode:', error);
    });
}

// Manual mode functions
function setManualMode() {
    updateModeButtons('manual');
    manualTrackingActive = true;
}

function startManualTracking() {
    const speed = parseFloat(document.getElementById('tracking-speed').value);
    const command = {
        "T": 16,
        "tracking_speed": speed
    };
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(command)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Manual tracking started:', data);
        updateStatus(data);
    })
    .catch(error => {
        console.error('Error starting manual tracking:', error);
    });
}

function stopManualTracking() {
    const command = {
        "T": 16,
        "tracking_speed": 0.0
    };
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(command)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Manual tracking stopped:', data);
        updateStatus(data);
    })
    .catch(error => {
        console.error('Error stopping manual tracking:', error);
    });
}

// DEC speed control
function setDecSpeed() {
    const speed = parseInt(document.getElementById('dec-speed').value);
    const command = {
        "T": 17,
        "dec_speed": speed
    };
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(command)
    })
    .then(response => response.json())
    .then(data => {
        console.log('DEC speed set:', data);
        updateStatus(data);
    })
    .catch(error => {
        console.error('Error setting DEC speed:', error);
    });
}

// Update mode button states
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
    
    // Show/hide manual panel
    const manualPanel = document.getElementById('manual-panel');
    if (activeMode === 'manual') {
        manualPanel.style.display = 'block';
    } else {
        manualPanel.style.display = 'none';
        manualTrackingActive = false;
    }
}

// Update speed display when slider changes
document.getElementById('tracking-speed').addEventListener('input', function() {
    document.getElementById('speed-display').textContent = this.value;
});

document.getElementById('dec-speed').addEventListener('input', function() {
    document.getElementById('dec-speed-display').textContent = this.value;
});
```

### D. Update Status Function

Modify your existing `updateStatus()` function to include tracking mode:

```javascript
// UPDATE YOUR EXISTING updateStatus FUNCTION
function updateStatus(status) {
    // YOUR EXISTING STATUS UPDATES HERE...
    
    // ADD THESE NEW UPDATES:
    // Update current mode display
    document.getElementById('current-mode').textContent = status.tracking_mode_name || 'Unknown';
    document.getElementById('current-rate').textContent = 
        ((status.tracking_rate || 0) * 3600).toFixed(4);
    
    // Update speed displays
    document.getElementById('ra-speed').textContent = status.ra_speed || 0;
    document.getElementById('dec-speed').textContent = status.dec_speed || 0;
    
    // Update DEC speed display
    document.getElementById('dec-speed-display').textContent = status.dec_spd_setting || 2000;
    
    // Update tracking mode buttons
    updateModeButtons(status.tracking_mode || 1);
    
    // Update manual panel
    if (manualTrackingActive) {
        document.getElementById('manual-status').textContent = 'ON';
        document.getElementById('speed-setting').textContent = 
            document.getElementById('tracking-speed').value;
    }
}
```

## Step 3: Remove Old Follow Mode Functions

Delete or comment out these old functions:
```javascript
// DELETE THESE OLD FUNCTIONS:
function decreaseSpeed() { ... }
function increaseSpeed() { ... }
function startFollow() { ... }
function stopFollow() { ... }
```

## Step 4: Test the Replacement

1. **Save your HTML file**
2. **Refresh the web page**
3. **Test each tracking mode**:
   - Click Sidereal, Solar, Lunar buttons
   - Click Manual button (should show speed controls)
   - Test manual tracking start/stop
   - Test DEC speed adjustment

## Quick Copy-Paste Version

If you want to replace everything at once, just:

1. **Copy the CSS** from Step 2A into your `<style>` section
2. **Replace the HTML** from Step 2B (delete old Follow Mode, paste new)
3. **Copy the JavaScript** from Step 2C into your `<script>` section
4. **Update your status function** from Step 2D
5. **Delete old functions** from Step 3

That's it! Your Follow Mode is now replaced with the comprehensive Tracking Mode interface.
