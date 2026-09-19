#ifndef WEB_PAGE_ENHANCED_H
#define WEB_PAGE_ENHANCED_H

#include <Arduino.h>

const char WEB_PAGE_ENHANCED_HTML[] PROGMEM = R"rawliteral(
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
input[type=text]{width:100px;padding:6px;background:#0f0f1a;border:1px solid #333;color:#eee;border-radius:4px}
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
<h1>Jarspace Telescope Mount Enhanced (R2)</h1>

<div class="card">
  <strong>Status</strong> (auto-refresh)
  <pre id="status">Connecting...</pre>
</div>

<div class="card">
  <h2>Time Management</h2>
  <div class="time-mode">
    <strong>Firmware:</strong> <span id="firmwareRev">Unknown</span><br>
    <strong>Current Time Mode:</strong> <span id="timeMode">Default</span><br>
    <strong>RTC Status:</strong> <span id="rtcStatus">Unknown</span><br>
    <strong>Tracking Mode:</strong> <span id="trackingMode">Sidereal</span><br>
    <strong>Tracking Enabled:</strong> <span id="trackingEnabled">No</span><br>
    <strong>Tracking Rate:</strong> <span id="trackingRate">--</span> arcsec/sec
  </div>
  
  <div class="row">
    <button id="btnTimeDefault" class="success">Use Default Time (00:00:00)</button>
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
    <label>Tracking Mode:</label>
    <button id="btnTrackSidereal" class="success">Sidereal</button>
    <button id="btnTrackSolar" class="warning">Solar</button>
    <button id="btnTrackLunar" class="warning">Lunar</button>
  </div>
  
  <div class="row">
    <label>Set Time:</label>
    <input type="number" id="setHour" min="0" max="23" value="0">
    <input type="number" id="setMinute" min="0" max="59" value="0">
    <input type="number" id="setSecond" min="0" max="59" value="0">
    <button id="btnSetTime">Set Local Time</button>
  </div>
  
  <div class="row">
    <label>Set RTC:</label>
    <input type="number" id="rtcYear" min="2026" max="2100" value="2026">
    <input type="number" id="rtcMonth" min="1" max="12" value="3">
    <input type="number" id="rtcDay" min="1" max="31" value="27">
    <input type="number" id="rtcHour" min="0" max="23" value="0">
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
    <label>RA (HH:MM:SS):</label>
    <input type="text" id="gotoRA" value="00:00:00" placeholder="HH:MM:SS">
    <label>RA (degrees):</label>
    <input type="number" step="0.1" id="gotoRADeg" value="0">
    <button id="btnConvertRA" class="warning">Convert</button>
  </div>
  <div class="row">
    <label>DEC Degrees:</label>
    <input type="number" id="decDeg" value="0" style="width:60px;">
    <span>°</span>
    <input type="number" id="decMin" value="0" min="0" max="59" style="width:50px;">
    <span>'</span>
    <input type="number" id="decSec" value="0" min="0" max="59" step="0.1" style="width:60px;">
    <span>"</span>
    <span id="decDecimal" style="min-width:80px;text-align:right;color:#0f0;">= 0.00°</span>
    <button id="btnGoto" class="success">Goto</button>
  </div>
  
  <div class="coord-display">
    <strong>Calculated Hour Angle:</strong> <span id="hourAngle">---.--°</span><br>
    <strong>Hour Angle (hours):</strong> <span id="hourAngleHours">--.--h</span><br>
    <strong>Hour Angle (HH:MM:SS):</strong> <span id="hourAngleHms">--:--:--</span><br>
    <strong>Target RA Steps:</strong> <span id="targetRASteps">-----</span><br>
    <strong>Target DEC Steps:</strong> <span id="targetDECSteps">-----</span>
  </div>
</div>

<div class="card">
  <h2>Counter Debug (No Motor Move)</h2>
  <div class="row">
    <label>Set RA (deg):</label>
    <input type="number" step="0.01" id="setCurrentRA" value="0">
    <label>Set DEC (deg):</label>
    <input type="number" step="0.01" id="setCurrentDEC" value="0">
    <button id="btnSetCounters" class="warning">Set Counters</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    Updates current RA/DEC counters for debugging HA math without moving the motors.
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

<div class="card">
  <h2>Motor Direction</h2>
  <div class="row">
    <label>RA Motor:</label>
    <button id="raNormal" class="success">Normal</button>
    <button id="raInverted" class="warning">Inverted</button>
    <span id="raStatus" style="margin-left:10px;">Normal</span>
  </div>
  <div class="row">
    <label>DEC Motor:</label>
    <button id="decNormal" class="success">Normal</button>
    <button id="decInverted" class="warning">Inverted</button>
    <span id="decStatus" style="margin-left:10px;">Normal</span>
  </div>
</div>

<script>
let reconnectTimer = null;

function connect() {
  // No WebSocket needed - using HTTP polling instead
  console.log('Using HTTP polling for status updates');
  if (reconnectTimer) {
    clearInterval(reconnectTimer);
    reconnectTimer = null;
  }
}

function fetchStatus() {
  fetch('/api/status')
    .then(response => response.json())
    .then(data => {
      updateStatus(data);
    })
    .catch(error => {
      console.error('Error fetching status:', error);
    });
}

function updateStatus(data) {
  // Update basic status
  document.getElementById('status').textContent = JSON.stringify(data, null, 2);

  const haDegRaw = Number(data.hour_angle_deg || 0);
  const haDegWrapped = ((haDegRaw % 360) + 360) % 360;
  const haHoursWrapped = haDegWrapped / 15.0;
  const haSignedDeg = Number(data.hour_angle_signed_deg ?? ((haDegWrapped > 180) ? (haDegWrapped - 360) : haDegWrapped));
  const haSignedHours = haSignedDeg / 15.0;
  const haSignedAbsHours = Math.abs(haSignedHours);
  let derivedHaHms = '--:--:--';
  {
    const sign = haSignedHours < 0 ? '-' : '+';
    const h = Math.floor(haSignedAbsHours);
    const mFloat = (haSignedAbsHours - h) * 60.0;
    const m = Math.floor(mFloat);
    const s = Math.floor((mFloat - m) * 60.0);
    derivedHaHms = `${sign}${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  }
  
  // Update time management display
  const timeModes = ['Default', 'Web', 'RTC'];
  document.getElementById('timeMode').textContent = timeModes[data.time_mode] || 'Unknown';
  document.getElementById('rtcStatus').textContent = data.rtc_available ? 'Available' : 'Not Available';
  
  // Update tracking mode display
  const trackingModes = {0: 'Lunar', 1: 'Sidereal', 2: 'Solar'};
  document.getElementById('trackingMode').textContent = trackingModes[data.tracking_mode] || 'Unknown';
  document.getElementById('trackingEnabled').textContent = data.tracking_enabled ? 'Yes' : 'No';
  document.getElementById('trackingRate').textContent = (data.tracking_rate * 3600.0).toFixed(2); // Convert to arcsec/sec
  document.getElementById('firmwareRev').textContent = data.firmware_rev || 'Unknown';
  
  // Update time displays
  document.getElementById('localTime').textContent = data.local_time || '--:--:--';
  document.getElementById('lstTime').textContent = (data.local_sidereal_time_deg || 0).toFixed(2) + '°';
  document.getElementById('julianDay').textContent = (data.julian_day || 0).toFixed(5);
  
  // Update coordinate displays
  document.getElementById('hourAngle').textContent = haSignedDeg.toFixed(2) + '°';
  document.getElementById('hourAngleHours').textContent = haSignedHours.toFixed(3) + 'h';
  document.getElementById('hourAngleHms').textContent = data.hour_angle_signed_hms || derivedHaHms;
  document.getElementById('targetRASteps').textContent = data.target_ra_steps || '0';
  document.getElementById('targetDECSteps').textContent = data.target_dec_steps || '0';

  if (data.ra_deg !== undefined) {
    document.getElementById('setCurrentRA').value = Number(data.ra_deg).toFixed(3);
  }
  if (data.dec_deg !== undefined) {
    document.getElementById('setCurrentDEC').value = Number(data.dec_deg).toFixed(3);
  }
  
  // Update location inputs
  if (data.site_latitude_deg !== undefined) {
    document.getElementById('siteLat').value = data.site_latitude_deg;
  }
  if (data.site_longitude_deg !== undefined) {
    document.getElementById('siteLon').value = data.site_longitude_deg;
  }
  
  // Update motor direction status
  if (data.ra_inverted !== undefined) {
    document.getElementById('raStatus').textContent = data.ra_inverted ? 'Inverted' : 'Normal';
  }
  if (data.dec_inverted !== undefined) {
    document.getElementById('decStatus').textContent = data.dec_inverted ? 'Inverted' : 'Normal';
  }
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
    console.log('Command sent:', cmd, 'Response:', data);
    // Refresh status after command
    fetchStatus();
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

// Tracking speed constants (deg/sec)
const TRACKING_SPEED_SIDEREAL = 0.004178;
const TRACKING_SPEED_SOLAR = 0.004167;
const TRACKING_SPEED_LUNAR = 0.004156;

function startConstantTracking(speed) {
  // Ensure motors are enabled for movement
  sendCommand({T: 11, torque: 1});
  sendCommand({T: 16, tracking_speed: speed});
}

function stopConstantTracking() {
  sendCommand({T: 16, tracking_speed: 0});
}

function setupTrackingButton(buttonId, modeValue, speed) {
  const button = document.getElementById(buttonId);

  button.addEventListener('click', () => {
    sendCommand({T: 14, mode: modeValue});
  });

  button.addEventListener('mousedown', () => {
    startConstantTracking(speed);
  });

  button.addEventListener('mouseup', () => {
    stopConstantTracking();
  });

  button.addEventListener('mouseleave', () => {
    stopConstantTracking();
  });

  // Mobile / touch support
  button.addEventListener('touchstart', (event) => {
    event.preventDefault();
    startConstantTracking(speed);
  });

  button.addEventListener('touchend', (event) => {
    event.preventDefault();
    stopConstantTracking();
  });

  button.addEventListener('touchcancel', (event) => {
    event.preventDefault();
    stopConstantTracking();
  });
}

// Event listeners
document.getElementById('btnTimeDefault').addEventListener('click', () => {
  setTimeMode(0);
  setLocalTime(0, 0, 0);
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

setupTrackingButton('btnTrackSidereal', 1, TRACKING_SPEED_SIDEREAL);
setupTrackingButton('btnTrackSolar', 2, TRACKING_SPEED_SOLAR);
setupTrackingButton('btnTrackLunar', 0, TRACKING_SPEED_LUNAR);

document.getElementById('btnConvertRA').addEventListener('click', () => {
  const raTimeStr = document.getElementById('gotoRA').value;
  const parts = raTimeStr.split(':');
  
  if (parts.length === 3) {
    const hours = parseInt(parts[0]);
    const minutes = parseInt(parts[1]);
    const seconds = parseInt(parts[2]);
    
    if (!isNaN(hours) && !isNaN(minutes) && !isNaN(seconds)) {
      const decimalHours = hours + (minutes / 60) + (seconds / 3600);
      const raDegrees = decimalHours * 15.0; // Convert hours to degrees
      document.getElementById('gotoRADeg').value = raDegrees.toFixed(2);
      console.log('RA converted:', raTimeStr, '->', decimalHours.toFixed(4), 'hours ->', raDegrees.toFixed(2), 'degrees');
    } else {
      alert('Invalid RA time format. Use HH:MM:SS');
    }
  } else {
    alert('Invalid RA time format. Use HH:MM:SS');
  }
});

document.getElementById('btnSetLocation').addEventListener('click', () => {
  const lat = parseFloat(document.getElementById('siteLat').value);
  const lon = parseFloat(document.getElementById('siteLon').value);
  sendCommand({T: 16, site_latitude_deg: lat, site_longitude_deg: lon});
});

document.getElementById('btnGoto').addEventListener('click', () => {
  const raTimeStr = document.getElementById('gotoRA').value;
  
  // Convert DEC DMS to decimal degrees: Degrees + (Minutes/60) + (Seconds/3600)
  const decDeg = parseFloat(document.getElementById('decDeg').value) || 0;
  const decMin = parseFloat(document.getElementById('decMin').value) || 0;
  const decSec = parseFloat(document.getElementById('decSec').value) || 0;
  const dec = decDeg + (decMin / 60) + (decSec / 3600);
  
  // Convert HH:MM:SS to decimal hours
  let raHours = 0;
  const parts = raTimeStr.split(':');
  
  if (parts.length === 3) {
    const hours = parseInt(parts[0]);
    const minutes = parseInt(parts[1]);
    const seconds = parseInt(parts[2]);
    
    if (!isNaN(hours) && !isNaN(minutes) && !isNaN(seconds)) {
      raHours = hours + (minutes / 60) + (seconds / 3600);
    } else {
      alert('Invalid RA time format. Use HH:MM:SS');
      return;
    }
  } else {
    alert('Invalid RA time format. Use HH:MM:SS');
    return;
  }
  
  sendCommand({T: 19, ra_hours: raHours, dec_degrees: dec});
});

document.getElementById('btnPolarAlign').addEventListener('click', () => {
  sendCommand({T: 3});
});

document.getElementById('btnSetCounters').addEventListener('click', () => {
  const ra = parseFloat(document.getElementById('setCurrentRA').value);
  const dec = parseFloat(document.getElementById('setCurrentDEC').value);
  sendCommand({T: 22, set_ra_deg: ra, set_dec_deg: dec});
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

// Motor direction controls
document.getElementById('raNormal').addEventListener('click', () => {
  sendCommand({T: 21, motor: 'ra', direction: 'normal'});
  document.getElementById('raStatus').textContent = 'Normal';
});

document.getElementById('raInverted').addEventListener('click', () => {
  sendCommand({T: 21, motor: 'ra', direction: 'inverted'});
  document.getElementById('raStatus').textContent = 'Inverted';
});

document.getElementById('decNormal').addEventListener('click', () => {
  sendCommand({T: 21, motor: 'dec', direction: 'normal'});
  document.getElementById('decStatus').textContent = 'Normal';
});

document.getElementById('decInverted').addEventListener('click', () => {
  sendCommand({T: 21, motor: 'dec', direction: 'inverted'});
  document.getElementById('decStatus').textContent = 'Inverted';
});

// DEC DMS to decimal live update
function updateDecDecimal() {
  const decDeg = parseFloat(document.getElementById('decDeg').value) || 0;
  const decMin = parseFloat(document.getElementById('decMin').value) || 0;
  const decSec = parseFloat(document.getElementById('decSec').value) || 0;
  const decDecimal = decDeg + (decMin / 60) + (decSec / 3600);
  document.getElementById('decDecimal').textContent = '= ' + decDecimal.toFixed(4) + '°';
}

document.getElementById('decDeg').addEventListener('input', updateDecDecimal);
document.getElementById('decMin').addEventListener('input', updateDecDecimal);
document.getElementById('decSec').addEventListener('input', updateDecDecimal);

// Start connection and polling
connect();
setInterval(fetchStatus, 2000); // Update status every 2 seconds
fetchStatus(); // Initial fetch
</script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGE_ENHANCED_H
