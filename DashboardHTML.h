// Auto-generated from DashboardHTML.h
// This is the full dashboard HTML served by the Ground Station ESP32 in AP mode.
// Stored in PROGMEM to avoid consuming precious RAM.
#pragma once
#include <pgmspace.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Rocket Ground Station</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: 'Segoe UI', monospace;
    background: #0d1117;
    color: #c9d1d9;
    min-height: 100vh;
    padding: 16px;
  }
  h1 {
    color: #58a6ff;
    font-size: 1.4rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    border-bottom: 1px solid #21262d;
    padding-bottom: 10px;
    margin-bottom: 16px;
  }
  h1 span { color: #3fb950; font-size: 0.8rem; margin-left: 12px; }
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 12px;
    margin-bottom: 16px;
  }
  @media(max-width: 700px) { .grid { grid-template-columns: 1fr; } }
  .card {
    background: #161b22;
    border: 1px solid #21262d;
    border-radius: 8px;
    padding: 14px 18px;
  }
  .card label {
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: #8b949e;
    display: block;
    margin-bottom: 4px;
  }
  .card .val {
    font-size: 2rem;
    font-weight: bold;
    color: #f0f6fc;
  }
  .card .val.apogee { color: #f85149; }
  .card .val.good   { color: #3fb950; }
  .card .val.warn   { color: #d29922; }
  .card .unit { font-size: 0.85rem; color: #8b949e; margin-left: 4px; }

  /* RSSI Bar */
  .rssi-bar-wrap {
    background: #0d1117;
    border-radius: 4px;
    height: 18px;
    width: 100%;
    margin-top: 8px;
    overflow: hidden;
  }
  .rssi-bar {
    height: 100%;
    border-radius: 4px;
    transition: width 0.5s ease, background 0.5s ease;
    width: 0%;
    background: #3fb950;
  }
  .rssi-label { font-size: 0.7rem; color: #8b949e; margin-top: 4px; }

  /* Chart */
  .chart-card { background: #161b22; border: 1px solid #21262d; border-radius: 8px; padding: 14px 18px; margin-bottom: 16px; }
  .chart-card label { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 1px; color: #8b949e; display: block; margin-bottom: 8px; }
  canvas#altChart { width: 100% !important; height: 200px !important; }

  /* Table */
  .table-card { background: #161b22; border: 1px solid #21262d; border-radius: 8px; padding: 14px 18px; }
  .table-card label { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 1px; color: #8b949e; display: block; margin-bottom: 8px; }
  table { width: 100%; border-collapse: collapse; font-size: 0.82rem; }
  th { color: #8b949e; border-bottom: 1px solid #21262d; padding: 6px 8px; text-align: left; font-weight: 500; }
  td { padding: 5px 8px; border-bottom: 1px solid #161b22; }
  tr:hover td { background: #21262d; }
  .badge {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 0.7rem;
    font-weight: bold;
  }
  .badge.normal { background: #1f3a1f; color: #3fb950; }
  .badge.apogee { background: #3a1f1f; color: #f85149; }
  .badge.ejection { background: #3a2f1f; color: #d29922; }
  .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; background: #3fb950; animation: blink 1s infinite; }
  @keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.2} }

  /* Status bar */
  .statusbar { font-size: 0.72rem; color: #8b949e; text-align: right; margin-top: 12px; }
</style>
</head>
<body>

<h1>&#x1F680; Rocket Ground Station <span><span class="dot"></span> LIVE</span></h1>

<div class="grid">
  <div class="card">
    <label>Latest Altitude</label>
    <div><span class="val" id="latestAlt">--</span><span class="unit">m</span></div>
  </div>
  <div class="card">
    <label>Max Altitude (Apogee)</label>
    <div><span class="val apogee" id="maxAlt">--</span><span class="unit">m</span></div>
  </div>
  <div class="card">
    <label>Packets Received</label>
    <div><span class="val good" id="pktCount">0</span></div>
  </div>
  <div class="card">
    <label>RSSI (Signal Strength)</label>
    <div><span class="val" id="rssiVal">--</span><span class="unit">dBm</span></div>
    <div class="rssi-bar-wrap"><div class="rssi-bar" id="rssiBar"></div></div>
    <div class="rssi-label" id="rssiLabel">Waiting...</div>
  </div>
  <div class="card">
    <label>SNR</label>
    <div><span class="val" id="snrVal">--</span><span class="unit">dB</span></div>
  </div>
  <div class="card">
    <label>Last Rocket ID</label>
    <div><span class="val" id="rocketId">--</span></div>
  </div>
</div>

<div class="chart-card">
  <label>Altitude Profile</label>
  <canvas id="altChart"></canvas>
</div>

<div class="table-card">
  <label>Packet Log</label>
  <table>
    <thead><tr>
      <th>Time</th><th>ID</th><th>Type</th><th>Seq</th><th>Alt (m)</th><th>RSSI</th><th>SNR</th>
    </tr></thead>
    <tbody id="pktTable"></tbody>
  </table>
</div>

<div class="statusbar" id="statusBar">Connecting...</div>

<script>
// ---- Chart Setup ----
const canvas = document.getElementById('altChart');
const ctx = canvas.getContext('2d');
const MAX_POINTS = 120;
let altHistory = [];
let maxAltSeen = 0;
let totalPackets = 0;
let lastSeq = -1;

function drawChart() {
  const W = canvas.offsetWidth;
  const H = canvas.offsetHeight;
  canvas.width = W;
  canvas.height = H;

  ctx.clearRect(0, 0, W, H);

  // Grid
  ctx.strokeStyle = '#21262d';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = H - (i / 4) * H;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }

  if (altHistory.length < 2) return;

  const maxVal = Math.max(...altHistory, 10);
  const minVal = 0;
  const range = maxVal - minVal || 1;

  // Fill gradient
  const grad = ctx.createLinearGradient(0, 0, 0, H);
  grad.addColorStop(0, 'rgba(88,166,255,0.25)');
  grad.addColorStop(1, 'rgba(88,166,255,0)');

  ctx.beginPath();
  ctx.moveTo(0, H);
  altHistory.forEach((v, i) => {
    const x = (i / (MAX_POINTS - 1)) * W;
    const y = H - ((v - minVal) / range) * H;
    i === 0 ? ctx.lineTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(W, H);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line
  ctx.beginPath();
  ctx.strokeStyle = '#58a6ff';
  ctx.lineWidth = 2;
  altHistory.forEach((v, i) => {
    const x = (i / (MAX_POINTS - 1)) * W;
    const y = H - ((v - minVal) / range) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.stroke();

  // Apogee line
  if (maxAltSeen > 0) {
    const ay = H - ((maxAltSeen - minVal) / range) * H;
    ctx.beginPath();
    ctx.strokeStyle = '#f85149';
    ctx.setLineDash([4, 4]);
    ctx.lineWidth = 1;
    ctx.moveTo(0, ay); ctx.lineTo(W, ay);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#f85149';
    ctx.font = '10px monospace';
    ctx.fillText('APOGEE ' + maxAltSeen.toFixed(1) + 'm', 6, ay - 4);
  }
}

// ---- RSSI Bar ----
function updateRSSI(rssi) {
  // RSSI typically -120 (terrible) to -40 (excellent)
  const clamped = Math.max(-120, Math.min(-40, rssi));
  const pct = ((clamped + 120) / 80) * 100;
  const bar = document.getElementById('rssiBar');
  bar.style.width = pct + '%';
  if (pct > 65)      { bar.style.background = '#3fb950'; }
  else if (pct > 35) { bar.style.background = '#d29922'; }
  else               { bar.style.background = '#f85149'; }

  let label = 'Poor';
  if (pct > 65) label = 'Good';
  else if (pct > 35) label = 'Fair';
  document.getElementById('rssiLabel').textContent = label + ' (' + pct.toFixed(0) + '%)';
}

// ---- Packet Poll ----
async function fetchPackets() {
  try {
    const res = await fetch('/api/packets');
    if (!res.ok) return;
    const packets = await res.json();
    if (!packets || packets.length === 0) return;

    const latest = packets[packets.length - 1];

    // Stats
    document.getElementById('latestAlt').textContent = latest.alt.toFixed(1);
    document.getElementById('rssiVal').textContent = latest.rssi;
    document.getElementById('snrVal').textContent = latest.snr.toFixed(1);
    document.getElementById('rocketId').textContent = latest.rocket_id;
    updateRSSI(latest.rssi);

    // Count only genuinely new packets
    packets.forEach(p => {
      if (p.seq !== lastSeq) {
        totalPackets++;
        lastSeq = p.seq;
        if (p.alt > maxAltSeen) maxAltSeen = p.alt;
        altHistory.push(p.alt);
        if (altHistory.length > MAX_POINTS) altHistory.shift();
      }
    });

    document.getElementById('pktCount').textContent = totalPackets;
    document.getElementById('maxAlt').textContent = maxAltSeen.toFixed(1);

    // Table (show latest 25 in reverse)
    const tbody = document.getElementById('pktTable');
    tbody.innerHTML = '';
    const recent = [...packets].reverse().slice(0, 25);
    recent.forEach(p => {
      const tr = document.createElement('tr');
      let typeLabel;
      if (p.type === 2) {
        typeLabel = '<span class="badge apogee">APOGEE</span>';
      } else if (p.type === 3) {
        typeLabel = '<span class="badge ejection">EJECTION</span>';
      } else {
        typeLabel = '<span class="badge normal">NORMAL</span>';
      }
      tr.innerHTML = `<td>${p.ts}</td><td>${p.rocket_id}</td><td>${typeLabel}</td>
        <td>${p.seq}</td><td><b>${p.alt.toFixed(1)}</b></td>
        <td>${p.rssi} dBm</td><td>${p.snr.toFixed(1)} dB</td>`;
      tbody.appendChild(tr);
    });

    drawChart();
    document.getElementById('statusBar').textContent =
      'Last update: ' + new Date().toLocaleTimeString() + ' | Polling /api/packets';

  } catch(e) {
    document.getElementById('statusBar').textContent = 'Connection error: ' + e.message;
  }
}

setInterval(fetchPackets, 500);
setInterval(drawChart, 1000);
fetchPackets();
</script>
</body>
</html>
)rawhtml";
