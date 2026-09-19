#ifndef WEB_PAGE_SIMPLE_H
#define WEB_PAGE_SIMPLE_H

#include <Arduino.h>

const char WEB_PAGE_SIMPLE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Jarspace Telescope Enhanced</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;margin:0;padding:12px;background:#1a1a2e;color:#eee}
h1{font-size:1.2rem;margin:0 0 12px 0;color:#0f0}
h2{font-size:1rem;margin:12px 0 8px 0;color:#4CAF50}
.card{background:#16213e;border-radius:8px;padding:12px;margin-bottom:12px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0}
label{min-width:100px}
input[type=number]{width:80px;padding:6px;background:#0f0f1a;border:1px solid #333;color:#eee;border-radius:4px}
input[type=text]{width:120px;padding:6px;background:#0f0f1a;border:1px solid #333;color:#eee;border-radius:4px}
button{padding:8px 14px;background:#0f3460;border:none;color:#eee;border-radius:4px;cursor:pointer}
button:active{background:#1a4a7a}
button.danger{background:#6b2d2d}
button.success{background:#2d5a2d}
button.warning{background:#8a6d2d}
#status{font-family:monospace;font-size:12px;white-space:pre-wrap;word-break:break-all}
.time-mode{background:#2d4a2d;padding:8px;border-radius:4px;margin:8px 0}
.coord-display{background:#0f0f1a;padding:8px;border-radius:4px;margin:4px 0;font-family:monospace;font-size:12px}
</style>
</head>
<body>
<h1>Jarspace Telescope Mount Enhanced</h1>

<div class="card">
  <strong>Status</strong> <button id="btnRefresh">Refresh</button>
  <pre id="status">Connecting...</pre>
</div>

<div class="card">
  <h2>Time Management</h2>
  <div class="time-mode">
    <strong>Current Time Mode:</strong> <span id="timeMode">Default</span><br>
    <strong>RTC Status:</strong> <span id="rtcStatus">Unknown</span>
  </div>
  
  <div class="row">
    <button id="btnTimeDefault" class="success">Use Default Time (12:00:00)</button>
  </div>
  
  <div class="row">
    <button id="btnTimeWeb" class="success">Use Web Time</button>
    <button id="btnSyncWebTime">Sync Now</button>
  </div>
  
  <div class="row">
    <button id="btnTimeRTC" class="success">Use RTC Time</button>
    <button id="btnReadRTC">Read from RTC</button>
  </div>
  
  <div class="row">
    <label>Set Time:</label>
    <input type="number" id="setHour" min="0" max="23" value="12">
    <input type="number" id="setMinute" min="0" max="59" value="0">
    <input type="number" id="setSecond" min="0" max="59" value="0">
    <button id="btnSetTime">Set Local Time</button>
  </div>
  
  <div class="row">
    <label>Set RTC:</label>
    <input type="number" id="rtcYear" min="2024" max="2100" value="2024">
    <input type="number" id="rtcMonth" min="1" max="12" value="1">
    <input type="number" id="rtcDay" min="1" max="31" value="1">
    <input type="number" id="rtcHour" min="0" max="23" value="12">
    <input type="number" id="rtcMinute" min="0" max="59" value="0">
    <input type="number" id="rtcSecond" min="0" max="59" value="0">
    <button id="btnSetRTC">Set RTC Time</button>
  </div>
</div>

<div class="card">
  <h2>Location & Time (For Meridian Flip)</h2>
  <div class="row">
    <label>Latitude (deg)</label>
    <input type="number" step="0.0001" id="siteLat" value="33.50917">
  </div>
  <div class="row">
    <label>Longitude (deg)</label>
    <input type="number" step="0.0001" id="siteLon" value="36.31167">
  </div>
  <div class="row">
    <button id="btnSetLocation">Set Location</button>
  </div>
  
  <div class="coord-display">
    <strong>Local Time:</strong> <span id="localTime">--:--:--</span><br>
    <strong>Local Sidereal Time:</strong> <span id="lstTime">---.--°</span><br>
    <strong>Julian Day:</strong> <span id="julianDay">-------.-----</span>
  </div>
</div>

<div class="card">
  <h2>Goto Coordinates</h2>
  <div class="row">
    <label>RA (hours):</label>
    <input type="number" step="0.001" id="gotoRA" value="5.5">
    <label>DEC (degrees):</label>
    <input type="number" step="0.1" id="gotoDEC" value="0.0">
    <button id="btnGoto" class="success">Goto</button>
  </div>
  
  <div class="coord-display">
    <strong>Target RA Steps:</strong> <span id="targetRASteps">-----</span><br>
    <strong>Target DEC Steps:</strong> <span id="targetDECSteps">-----</span>
  </div>
</div>

<div class="card">
  <h2>Polar Alignment</h2>
  <div class="row">
    <button id="btnPolarAlign" class="success">Set RA=0°, DEC=90°</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    Sets the telescope to polar alignment position without moving.
  </div>
</div>

<div class="card">
  <h2>Manual Control</h2>
  <div class="row">
    <label>RA Step (deg):</label>
    <input type="number" step="0.1" id="raStep" value="1.0">
    <button id="raUp">RA +</button>
    <button id="raDown">RA -</button>
  </div>
  <div class="row">
    <label>DEC Step (deg):</label>
    <input type="number" step="0.1" id="decStep" value="1.0">
    <button id="decUp">DEC +</button>
    <button id="decDown">DEC -</button>
  </div>
</div>

<div class="card">
  <h2>Torque Control</h2>
  <div class="row">
    <button id="torqueOn" class="success">Torque ON</button>
    <button id="torqueOff" class="danger">Torque OFF</button>
  </div>
</div>

<script>
let updateInterval = null;

function updateStatus() {
  fetch('/api/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('status').textContent = JSON.stringify(data, null, 2);
      
      // Update time management display
      const timeModes = ['Default', 'Web', 'RTC'];
      document.getElementById('timeMode').textContent = timeModes[data.time_mode] || 'Unknown';
      document.getElementById('rtcStatus').textContent = data.rtc_available ? 'Available' : 'Not Available';
      
      // Update time displays
      document.getElementById('localTime').textContent = data.local_time || '--:--:--';
      document.getElementById('lstTime').textContent = (data.local_sidereal_time_deg || 0).toFixed(2) + '°';
      document.getElementById('julianDay').textContent = (data.julian_day || 0).toFixed(5);
      
      // Update coordinate displays
      document.getElementById('targetRASteps').textContent = data.target_ra_steps || '0';
      document.getElementById('targetDECSteps').textContent = data.target_dec_steps || '0';
      
      // Update location inputs
      if (data.site_latitude_deg !== undefined) {
        document.getElementById('siteLat').value = data.site_latitude_deg;
      }
      if (data.site_longitude_deg !== undefined) {
        document.getElementById('siteLon').value = data.site_longitude_deg;
      }
    })
    .catch(error => {
      console.error('Error fetching status:', error);
    });
}

function sendCommand(cmd) {
  fetch('/api/cmd', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(cmd)
  })
  .then(response => response.json())
  .then(data => {
    console.log('Command sent:', data);
    // Update status after command
    setTimeout(updateStatus, 100);
  })
  .catch(error => {
    console.error('Error sending command:', error);
  });
}

// Time management functions
function setTimeMode(mode) {
  sendCommand({T: 18, time_mode: mode});
}

function setLocalTime(h, m, s) {
  sendCommand({T: 16, local_time_hours: h, local_time_minutes: m, local_time_seconds: s});
}

function setRTCTime(year, month, day, hour, minute, second) {
  sendCommand({
    T: 18, 
    rtc_set_time: true,
    year: year,
    month: month,
    day: day,
    hour: hour,
    minute: minute,
    second: second
  });
}

function readFromRTC() {
  sendCommand({T: 18, rtc_get_time: true});
}

function syncWebTime() {
  const now = new Date();
  setLocalTime(now.getHours(), now.getMinutes(), now.getSeconds());
}

// Event listeners
document.getElementById('btnRefresh').addEventListener('click', updateStatus);

document.getElementById('btnTimeDefault').addEventListener('click', () => {
  setTimeMode(0);
  setLocalTime(12, 0, 0);
});

document.getElementById('btnTimeWeb').addEventListener('click', () => {
  setTimeMode(1);
  syncWebTime();
});

document.getElementById('btnSyncWebTime').addEventListener('click', syncWebTime);

document.getElementById('btnTimeRTC').addEventListener('click', () => {
  setTimeMode(2);
  readFromRTC();
});

document.getElementById('btnReadRTC').addEventListener('click', readFromRTC);

document.getElementById('btnSetTime').addEventListener('click', () => {
  const h = parseInt(document.getElementById('setHour').value);
  const m = parseInt(document.getElementById('setMinute').value);
  const s = parseInt(document.getElementById('setSecond').value);
  setLocalTime(h, m, s);
});

document.getElementById('btnSetRTC').addEventListener('click', () => {
  const year = parseInt(document.getElementById('rtcYear').value);
  const month = parseInt(document.getElementById('rtcMonth').value);
  const day = parseInt(document.getElementById('rtcDay').value);
  const hour = parseInt(document.getElementById('rtcHour').value);
  const minute = parseInt(document.getElementById('rtcMinute').value);
  const second = parseInt(document.getElementById('rtcSecond').value);
  setRTCTime(year, month, day, hour, minute, second);
});

document.getElementById('btnSetLocation').addEventListener('click', () => {
  const lat = parseFloat(document.getElementById('siteLat').value);
  const lon = parseFloat(document.getElementById('siteLon').value);
  sendCommand({T: 16, site_latitude_deg: lat, site_longitude_deg: lon});
});

document.getElementById('btnGoto').addEventListener('click', () => {
  const ra = parseFloat(document.getElementById('gotoRA').value);
  const dec = parseFloat(document.getElementById('gotoDEC').value);
  sendCommand({T: 19, ra_hours: ra, dec_degrees: dec});
});

document.getElementById('btnPolarAlign').addEventListener('click', () => {
  sendCommand({T: 3});
});

document.getElementById('raUp').addEventListener('click', () => {
  const step = parseFloat(document.getElementById('raStep').value);
  sendCommand({T: 12, ra_step_deg: step});
});

document.getElementById('raDown').addEventListener('click', () => {
  const step = parseFloat(document.getElementById('raStep').value);
  sendCommand({T: 12, ra_step_deg: -step});
});

document.getElementById('decUp').addEventListener('click', () => {
  const step = parseFloat(document.getElementById('decStep').value);
  sendCommand({T: 12, dec_step_deg: step});
});

document.getElementById('decDown').addEventListener('click', () => {
  const step = parseFloat(document.getElementById('decStep').value);
  sendCommand({T: 12, dec_step_deg: -step});
});

document.getElementById('torqueOn').addEventListener('click', () => {
  sendCommand({T: 11, torque: 1});
});

document.getElementById('torqueOff').addEventListener('click', () => {
  sendCommand({T: 11, torque: 0});
});

// Auto-refresh status every 2 seconds
updateStatus();
setInterval(updateStatus, 2000);
</script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGE_SIMPLE_H
