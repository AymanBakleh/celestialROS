#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>

const char WEB_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Jarspace Telescope</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;margin:0;padding:12px;background:#1a1a2e;color:#eee}
h1{font-size:1.2rem;margin:0 0 12px 0;color:#0f0}
.card{background:#16213e;border-radius:8px;padding:12px;margin-bottom:12px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0}
label{min-width:80px}
input[type=number]{width:80px;padding:6px;background:#0f0f1a;border:1px solid #333;color:#eee;border-radius:4px}
button{padding:8px 14px;background:#0f3460;border:none;color:#eee;border-radius:4px;cursor:pointer}
button:active{background:#1a4a7a}
button.danger{background:#6b2d2d}
button.success{background:#2d5a2d}
#status{font-family:monospace;font-size:12px;white-space:pre-wrap;word-break:break-all}
</style>
</head>
<body>
<h1>Jarspace Telescope Mount</h1>
<div class="card">
  <strong>Status</strong> (auto-refresh)
  <pre id="status">Connecting...</pre>
</div>
<div class="card">
  <strong>Polar Alignment</strong>
  <div class="row">
    <button id="btnPolarAlign" class="success">Set RA=0°, DEC=90°</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    Sets the telescope to polar alignment position without moving.
  </div>
</div>
<div class="card">
  <strong>Location & Time (For Meridian Flip)</strong>
  <div class="row">
    <label>Latitude (deg)</label>
    <input type="number" step="0.0001" id="siteLat" value="33.50917">
  </div>
  <div class="row">
    <label>Longitude (deg)</label>
    <input type="number" step="0.0001" id="siteLon" value="36.31167">
  </div>
  <div class="row">
    <label>Local Time</label>
    <input type="text" id="localTime" placeholder="HH:MM:SS" value="12:00:00">
  </div>
  <div class="row">
    <button id="btnSetLocationTime" class="success">Set Location/Time</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    These values are used for automatic meridian flip calculations.
  </div>
</div>
<div class="card">
  <strong>Torque</strong>
  <div class="row">
    <button id="btnTorqueOn" class="success">Torque ON</button>
    <button id="btnTorqueOff" class="danger">Torque Off/Stop</button>
  </div>
</div>
<div class="card">
  <strong>Follow Mode</strong>
  <div class="row">
    <label>RA Speed (deg/sec)</label>
    <div style="display:flex;align-items:center;gap:4px;">
      <button id="btnFollowSpeedDown" style="padding:4px 8px;">−</button>
      <input type="number" id="followSpeed" step="0.1" min="0.1" max="50" value="1.0" style="width:80px;">
      <button id="btnFollowSpeedUp" style="padding:4px 8px;">+</button>
    </div>
    <button id="btnFollowOn" class="success">Follow ON</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    RA tracking at constant speed. Use Torque Off/Stop to stop.
  </div>
</div>
<div class="card">
  <strong>Manual Control</strong>
  <div class="row">
    <div style="display:flex;flex-direction:column;gap:4px;padding:8px;background:#0f0f1a;border-radius:4px;">
      <div style="text-align:center;font-weight:bold;">RA</div>
      <div class="row">
        <button id="btnRaMinus">−</button>
        <button id="btnRaPlus">+</button>
      </div>
    </div>
    <div style="display:flex;flex-direction:column;gap:4px;padding:8px;background:#0f0f1a;border-radius:4px;">
      <div style="text-align:center;font-weight:bold;">DEC</div>
      <div class="row">
        <button id="btnDecMinus">−</button>
        <button id="btnDecPlus">+</button>
      </div>
    </div>
  </div>
  <div class="row">
    <label>Step Size (°)</label><input type="number" id="stepSize" step="0.1" min="0.1" max="10" value="1.0">
    <button id="btnStepSize">Set Step</button>
  </div>
</div>
<div class="card">
  <strong>Angle Synchronization</strong>
  <div class="row">
    <label>RA Sync (°)</label><input type="number" id="syncRa" step="any" placeholder="0-360">
    <label>DEC Sync (°)</label><input type="number" id="syncDec" step="any" placeholder="deg">
  </div>
  <div class="row">
    <button id="btnSync" class="success">Synchronize</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    Sets the current position to these angles without moving the telescope.
  </div>
</div>
<div class="card">
  <strong>Desired angles</strong>
  <div class="row">
    <label>RA (deg)</label><input type="number" id="desRa" step="any" placeholder="0-360">
    <label>DEC (deg)</label><input type="number" id="desDec" step="any" placeholder="deg">
  </div>
  <div class="row">
    <label>RA (HH:MM:SS)</label><input type="text" id="desRaHMS" placeholder="00:00:00">
    <button id="btnConvertHMS">Convert →</button>
  </div>
  <div class="row">
    <button id="btnGoto">GOTO</button>
  </div>
  <div style="font-size:12px;opacity:0.85;margin-top:6px;">
    Conversion: 2.5&deg; axis = 4096 ticks. RA: 24h = 360&deg;
  </div>
</div>
<script>
const API = '';

function fetchStatus(){
  fetch(API + '/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('status').textContent = JSON.stringify(d,null,2);
    if (d.site_latitude_deg !== undefined) document.getElementById('siteLat').value = d.site_latitude_deg.toFixed(5);
    if (d.site_longitude_deg !== undefined) document.getElementById('siteLon').value = d.site_longitude_deg.toFixed(5);
    if (d.local_time !== undefined) document.getElementById('localTime').value = d.local_time;
  }).catch(e=>{
    document.getElementById('status').textContent = 'Error: ' + e;
  });
}
function sendCmd(obj){
  fetch(API + '/api/cmd', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(obj)
  }).then(()=> fetchStatus()).catch(e=> console.error(e));
}

// Convert HH:MM:SS to degrees
function hmsToDegrees(hmsStr) {
  const parts = hmsStr.split(':');
  if (parts.length !== 3) {
    alert('Invalid format. Use HH:MM:SS');
    return null;
  }
  
  const hours = parseFloat(parts[0]);
  const minutes = parseFloat(parts[1]);
  const seconds = parseFloat(parts[2]);
  
  if (isNaN(hours) || isNaN(minutes) || isNaN(seconds)) {
    alert('Invalid numbers. Use HH:MM:SS format');
    return null;
  }
  
  if (hours < 0 || hours >= 24 || minutes < 0 || minutes >= 60 || seconds < 0 || seconds >= 60) {
    alert('Invalid range. Hours: 0-23, Minutes: 0-59, Seconds: 0-59');
    return null;
  }
  
  // Convert to degrees: 24 hours = 360 degrees
  const totalDegrees = (hours + minutes/60 + seconds/3600) * 15;
  return totalDegrees;
}

// torque buttons
document.getElementById('btnTorqueOn').onclick = ()=> sendCmd({T:11, torque:1});
document.getElementById('btnTorqueOff').onclick = ()=> sendCmd({T:11, torque:0});

// follow mode buttons
document.getElementById('btnFollowOn').onclick = ()=>{
  const speed = parseFloat(document.getElementById('followSpeed').value);
  sendCmd({T:4, enable:true, ra_speed:speed});
};

// follow speed arrow buttons
document.getElementById('btnFollowSpeedUp').onclick = ()=>{
  const input = document.getElementById('followSpeed');
  const currentVal = parseFloat(input.value) || 0.1;
  const newVal = Math.min(currentVal + 0.1, 50.0); // Max 50 deg/sec
  input.value = newVal.toFixed(1);
};

document.getElementById('btnFollowSpeedDown').onclick = ()=>{
  const input = document.getElementById('followSpeed');
  const currentVal = parseFloat(input.value) || 0.1;
  const newVal = Math.max(currentVal - 0.1, 0.1); // Min 0.1 deg/sec
  input.value = newVal.toFixed(1);
};

// location/time set button
document.getElementById('btnSetLocationTime').onclick = ()=>{
  const lat = parseFloat(document.getElementById('siteLat').value);
  const lon = parseFloat(document.getElementById('siteLon').value);
  const time = document.getElementById('localTime').value.trim();

  if (isNaN(lat) || isNaN(lon)) {
    alert('Latitude and Longitude must be numeric');
    return;
  }
  if (!/^\d{1,2}:\d{2}:\d{2}$/.test(time)) {
    alert('Local time must be HH:MM:SS');
    return;
  }

  sendCmd({
    T: 16,
    site_latitude_deg: lat,
    site_longitude_deg: lon,
    local_time: time
  });
};

// command buttons
document.getElementById('btnGoto').onclick = ()=>{
  const cmd = {T:1};
  const raVal = document.getElementById('desRa').value.trim();
  const decVal = document.getElementById('desDec').value.trim();
  if (raVal !== '') cmd.ra_deg = parseFloat(raVal);
  if (decVal !== '') cmd.dec_deg = parseFloat(decVal);
  sendCmd(cmd);
};

// HMS conversion button
document.getElementById('btnConvertHMS').onclick = ()=>{
  const hmsValue = document.getElementById('desRaHMS').value.trim();
  if (hmsValue === '') {
    alert('Please enter HH:MM:SS format');
    return;
  }
  
  const degrees = hmsToDegrees(hmsValue);
  if (degrees !== null) {
    document.getElementById('desRa').value = degrees.toFixed(6);
    document.getElementById('desRaHMS').value = '';
  }
};

// manual control buttons
let currentStepSize = 1.0;

document.getElementById('btnRaPlus').onclick = ()=>{
  sendCmd({T:12, ra_step_deg: currentStepSize});
};

document.getElementById('btnRaMinus').onclick = ()=>{
  sendCmd({T:12, ra_step_deg: -currentStepSize});
};

document.getElementById('btnDecPlus').onclick = ()=>{
  sendCmd({T:12, dec_step_deg: currentStepSize});
};

document.getElementById('btnDecMinus').onclick = ()=>{
  sendCmd({T:12, dec_step_deg: -currentStepSize});
};

document.getElementById('btnStepSize').onclick = ()=>{
  const newSize = parseFloat(document.getElementById('stepSize').value);
  if (!isNaN(newSize) && newSize > 0) {
    currentStepSize = newSize;
    alert('Step size set to ' + newSize + '°');
  }
};

// synchronization buttons
document.getElementById('btnSync').onclick = ()=>{
  const cmd = {T:13};
  const raVal = document.getElementById('syncRa').value.trim();
  const decVal = document.getElementById('syncDec').value.trim();
  if (raVal !== '') cmd.ra_sync_deg = parseFloat(raVal);
  if (decVal !== '') cmd.dec_sync_deg = parseFloat(decVal);
  if (raVal !== '' || decVal !== '') {
    sendCmd(cmd);
  } else {
    alert('Please enter at least one angle to synchronize');
  }
};

document.getElementById('btnPolarAlign').onclick = ()=> sendCmd({T:3});

setInterval(fetchStatus, 1500);
fetchStatus();
</script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGE_H
