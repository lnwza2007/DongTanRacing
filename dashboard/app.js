/* ============================================================
   BBPN EV Controller Dashboard — app.js
   Simulation engine + Canvas drawing
   ============================================================ */

// ── State ──────────────────────────────────────────────────
const state = {
  speed: 0,
  targetSpeed: 0,
  mode: 0,
  rampUp:   [30, 60, 90],
  rampDown: [25, 55, 85],
  speedLim: [40, 70, 99],
  yaw: 0, pitch: 0, roll: 0,
  rotary: 0,
  throttle: 0,
  voltage: 72.0,
  current: 0,
  startTime: Date.now(),
  speedHistory: Array(80).fill(0),
  alertElapsed: 0,
};

// ── DOM refs ───────────────────────────────────────────────
const $ = id => document.getElementById(id);

// ── Background particles ───────────────────────────────────
function spawnParticles() {
  const container = $('bgParticles');
  const colors = ['#00f5ff','#7c3aed','#10f08a','#ff7b00'];
  for (let i = 0; i < 30; i++) {
    const el = document.createElement('div');
    el.className = 'particle';
    const size = 3 + Math.random() * 8;
    const left = Math.random() * 100;
    const dur  = 8 + Math.random() * 20;
    const delay= Math.random() * -25;
    el.style.cssText = `
      width:${size}px; height:${size}px;
      left:${left}%;
      background:${colors[Math.floor(Math.random()*colors.length)]};
      animation-duration:${dur}s;
      animation-delay:${delay}s;
    `;
    container.appendChild(el);
  }
}

// ── Clock ──────────────────────────────────────────────────
function updateClock() {
  const now = new Date();
  $('timeDisplay').textContent =
    now.toLocaleTimeString('th-TH', {hour12: false});
}

// ── Speedometer canvas ─────────────────────────────────────
function drawSpeedo(speed, limit) {
  const canvas = $('speedoCanvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, r = 100;

  ctx.clearRect(0,0,W,H);

  // Tick marks
  for (let i = 0; i <= 120; i += 5) {
    const angle = (Math.PI * 0.75) + (i / 120) * (Math.PI * 1.5);
    const isMajor = i % 20 === 0;
    const len = isMajor ? 14 : 7;
    const x1 = cx + (r - 2) * Math.cos(angle);
    const y1 = cy + (r - 2) * Math.sin(angle);
    const x2 = cx + (r - 2 - len) * Math.cos(angle);
    const y2 = cy + (r - 2 - len) * Math.sin(angle);
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = isMajor ? 'rgba(255,255,255,0.5)' : 'rgba(255,255,255,0.15)';
    ctx.lineWidth = isMajor ? 2 : 1;
    ctx.stroke();
  }

  // Outer ring — background arc
  ctx.beginPath();
  ctx.arc(cx, cy, r, Math.PI * 0.75, Math.PI * 2.25);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 14;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Speed arc
  const pct = Math.min(speed / 120, 1);
  const startAngle = Math.PI * 0.75;
  const endAngle   = startAngle + pct * (Math.PI * 1.5);
  const grad = ctx.createLinearGradient(cx - r, cy, cx + r, cy);
  grad.addColorStop(0,   '#00f5ff');
  grad.addColorStop(0.5, '#7c3aed');
  grad.addColorStop(1,   '#ff3b6b');
  ctx.beginPath();
  ctx.arc(cx, cy, r, startAngle, endAngle);
  ctx.strokeStyle = grad;
  ctx.lineWidth = 14;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Glow effect
  ctx.beginPath();
  ctx.arc(cx, cy, r, startAngle, endAngle);
  ctx.strokeStyle = 'rgba(0,245,255,0.25)';
  ctx.lineWidth = 22;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Speed limit marker
  if (limit > 0) {
    const limAngle = startAngle + (limit / 120) * (Math.PI * 1.5);
    const lx1 = cx + (r + 4)  * Math.cos(limAngle);
    const ly1 = cy + (r + 4)  * Math.sin(limAngle);
    const lx2 = cx + (r - 20) * Math.cos(limAngle);
    const ly2 = cy + (r - 20) * Math.sin(limAngle);
    ctx.beginPath();
    ctx.moveTo(lx1, ly1);
    ctx.lineTo(lx2, ly2);
    ctx.strokeStyle = '#ff3b6b';
    ctx.lineWidth = 3;
    ctx.lineCap = 'round';
    ctx.shadowColor = '#ff3b6b';
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  // Needle
  const needleAngle = startAngle + pct * (Math.PI * 1.5);
  ctx.save();
  ctx.translate(cx, cy);
  ctx.rotate(needleAngle);
  const ng = ctx.createLinearGradient(0, -r+20, 0, -20);
  ng.addColorStop(0, '#ffffff');
  ng.addColorStop(1, 'rgba(0,245,255,0.3)');
  ctx.beginPath();
  ctx.moveTo(0, -r + 18);
  ctx.lineTo(-3, 0);
  ctx.lineTo(3, 0);
  ctx.closePath();
  ctx.fillStyle = ng;
  ctx.fill();
  ctx.restore();

  // Center cap
  const cg = ctx.createRadialGradient(cx, cy, 0, cx, cy, 12);
  cg.addColorStop(0, '#ffffff');
  cg.addColorStop(1, '#00f5ff');
  ctx.beginPath();
  ctx.arc(cx, cy, 10, 0, Math.PI*2);
  ctx.fillStyle = cg;
  ctx.fill();
  ctx.beginPath();
  ctx.arc(cx, cy, 10, 0, Math.PI*2);
  ctx.strokeStyle = 'rgba(0,245,255,0.6)';
  ctx.lineWidth = 2;
  ctx.stroke();
}

// ── IMU mini gauge (arc) ───────────────────────────────────
function drawArcGauge(canvasId, value, min, max, color) {
  const canvas = document.getElementById(canvasId);
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, r = 45;
  ctx.clearRect(0,0,W,H);

  // bg arc
  ctx.beginPath();
  ctx.arc(cx, cy, r, Math.PI * 0.8, Math.PI * 2.2);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 10;
  ctx.lineCap = 'round';
  ctx.stroke();

  // value arc
  const pct = (value - min) / (max - min);
  const safeP = Math.max(0, Math.min(1, (pct + 1) / 2)); // center = 0.5
  const startA = Math.PI * 0.8;
  const totalArc = Math.PI * 1.4;
  const endA = startA + safeP * totalArc;
  ctx.beginPath();
  ctx.arc(cx, cy, r, startA, endA);
  ctx.strokeStyle = color;
  ctx.lineWidth = 10;
  ctx.lineCap = 'round';
  ctx.shadowColor = color;
  ctx.shadowBlur = 10;
  ctx.stroke();
  ctx.shadowBlur = 0;
}

// ── Ball (tilt indicator) ──────────────────────────────────
function drawBall(pitch, roll) {
  const canvas = $('ballCanvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, R = 55;
  ctx.clearRect(0, 0, W, H);

  // outer ring
  ctx.beginPath();
  ctx.arc(cx, cy, R, 0, Math.PI*2);
  ctx.strokeStyle = 'rgba(255,255,255,0.1)';
  ctx.lineWidth = 1.5;
  ctx.stroke();

  // crosshairs
  ctx.beginPath();
  ctx.moveTo(cx - R, cy); ctx.lineTo(cx + R, cy);
  ctx.moveTo(cx, cy - R); ctx.lineTo(cx, cy + R);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 1;
  ctx.stroke();

  // inner circles
  [R*0.35, R*0.65].forEach(rr => {
    ctx.beginPath();
    ctx.arc(cx, cy, rr, 0, Math.PI*2);
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.stroke();
  });

  // ball position
  const bx = cx + Math.max(-R+10, Math.min(R-10, roll  * 2));
  const by = cy + Math.max(-R+10, Math.min(R-10, -pitch * 2));
  const ballR = 12;
  const bg = ctx.createRadialGradient(bx-3, by-3, 1, bx, by, ballR);
  bg.addColorStop(0, '#ffffff');
  bg.addColorStop(0.5, '#00f5ff');
  bg.addColorStop(1, 'rgba(0,245,255,0.1)');
  ctx.beginPath();
  ctx.arc(bx, by, ballR, 0, Math.PI*2);
  ctx.fillStyle = bg;
  ctx.fill();
  ctx.beginPath();
  ctx.arc(bx, by, ballR, 0, Math.PI*2);
  ctx.strokeStyle = 'rgba(0,245,255,0.8)';
  ctx.lineWidth = 2;
  ctx.shadowColor = '#00f5ff';
  ctx.shadowBlur = 12;
  ctx.stroke();
  ctx.shadowBlur = 0;
}

// ── Rotary knob ────────────────────────────────────────────
function drawKnob(value) {
  const canvas = $('knobCanvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, r = 48;
  ctx.clearRect(0,0,W,H);

  // outer ring
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI*2);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 12;
  ctx.stroke();

  // fill arc based on value (mapped -100 to 100)
  const pct = (value + 100) / 200;
  const startA = -Math.PI * 0.75;
  const endA   = startA + pct * Math.PI * 1.5;
  const grad = ctx.createLinearGradient(cx-r, cy, cx+r, cy);
  grad.addColorStop(0, '#7c3aed');
  grad.addColorStop(1, '#00f5ff');
  ctx.beginPath();
  ctx.arc(cx, cy, r, startA, endA);
  ctx.strokeStyle = grad;
  ctx.lineWidth = 12;
  ctx.lineCap = 'round';
  ctx.shadowColor = '#00f5ff';
  ctx.shadowBlur = 8;
  ctx.stroke();
  ctx.shadowBlur = 0;

  // notch indicator
  const notchAngle = endA;
  const nx = cx + (r + 8) * Math.cos(notchAngle);
  const ny = cy + (r + 8) * Math.sin(notchAngle);
  ctx.beginPath();
  ctx.arc(nx, ny, 4, 0, Math.PI*2);
  ctx.fillStyle = '#00f5ff';
  ctx.shadowColor = '#00f5ff';
  ctx.shadowBlur = 10;
  ctx.fill();
  ctx.shadowBlur = 0;
}

// ── Speed history chart ────────────────────────────────────
function drawChart(history) {
  const canvas = $('chartCanvas');
  const ctx = canvas.getContext('2d');
  // Use display size
  const W = canvas.parentElement.clientWidth - 40 || 500;
  const H = 150;
  canvas.width  = W;
  canvas.height = H;
  ctx.clearRect(0, 0, W, H);

  const maxV = 120;
  const pts  = history;
  const step = W / (pts.length - 1);

  // Grid lines
  for (let i = 1; i < 4; i++) {
    const y = H - (i / 4) * H;
    ctx.beginPath();
    ctx.moveTo(0, y); ctx.lineTo(W, y);
    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = 'rgba(255,255,255,0.15)';
    ctx.font = '9px Orbitron, monospace';
    ctx.fillText(Math.round((i/4) * maxV), 4, y - 3);
  }

  // Fill gradient
  ctx.beginPath();
  pts.forEach((v, i) => {
    const x = i * step;
    const y = H - (v / maxV) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(W, H);
  ctx.lineTo(0, H);
  ctx.closePath();
  const fg = ctx.createLinearGradient(0, 0, 0, H);
  fg.addColorStop(0, 'rgba(0,245,255,0.25)');
  fg.addColorStop(1, 'rgba(0,245,255,0)');
  ctx.fillStyle = fg;
  ctx.fill();

  // Line
  ctx.beginPath();
  pts.forEach((v, i) => {
    const x = i * step;
    const y = H - (v / maxV) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#00f5ff';
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.shadowColor = '#00f5ff';
  ctx.shadowBlur = 6;
  ctx.stroke();
  ctx.shadowBlur = 0;

  // Dot at end
  const lastX = (pts.length - 1) * step;
  const lastY = H - (pts[pts.length-1] / maxV) * H;
  ctx.beginPath();
  ctx.arc(lastX, lastY, 5, 0, Math.PI*2);
  ctx.fillStyle = '#00f5ff';
  ctx.shadowColor = '#00f5ff';
  ctx.shadowBlur = 12;
  ctx.fill();
  ctx.shadowBlur = 0;
}

// ── Update UI from state ───────────────────────────────────
function updateUI() {
  const { speed, mode, rampUp, rampDown, speedLim,
          yaw, pitch, roll, rotary, throttle,
          voltage, current, speedHistory } = state;

  // Speedometer
  const lim = speedLim[mode];
  drawSpeedo(speed, lim);
  $('speedValue').textContent = Math.round(speed);
  $('speedBarFill').style.width = `${(speed / lim) * 100}%`;
  $('speedLimLabel').textContent = `${lim} km/h`;
  if (speed > lim * 0.9) {
    $('speedBarFill').style.background = 'linear-gradient(90deg,#ff7b00,#ff3b6b)';
    $('speedBarFill').style.boxShadow = '0 0 10px #ff3b6b';
  } else {
    $('speedBarFill').style.background = 'linear-gradient(90deg,#00f5ff,#7c3aed)';
    $('speedBarFill').style.boxShadow = '0 0 10px #00f5ff';
  }

  // Mode params
  $('rampUpFill').style.width   = `${rampUp[mode]}%`;
  $('rampDownFill').style.width = `${rampDown[mode]}%`;
  $('speedLimFill').style.width = `${speedLim[mode]}%`;
  $('rampUpVal').textContent    = rampUp[mode];
  $('rampDownVal').textContent  = rampDown[mode];
  $('speedLimVal').textContent  = speedLim[mode];

  // IMU
  drawArcGauge('pitchCanvas', pitch, -45, 45, '#10f08a');
  drawArcGauge('rollCanvas',  roll,  -45, 45, '#ff7b00');
  drawBall(pitch, roll);
  $('pitchVal').textContent = pitch.toFixed(1) + '°';
  $('rollVal').textContent  = roll.toFixed(1)  + '°';
  $('yawVal').textContent   = yaw.toFixed(1)   + '°';
  // yaw needle: map -180..180 → 0..100%
  $('yawNeedle').style.left = `${((yaw + 180) / 360) * 100}%`;

  // Power
  const power = voltage * current;
  const rpm   = Math.round(speed * 10);
  $('currentVal').textContent = current.toFixed(1)  + ' A';
  $('voltageVal').textContent = voltage.toFixed(1)  + ' V';
  $('powerVal').textContent   = Math.round(power)   + ' W';
  $('arrowWatt').textContent  = Math.round(power)   + ' W';
  $('motorRPM').textContent   = rpm + ' RPM';
  $('effVal').textContent     = power > 0 ? (85 + Math.random()*5).toFixed(1) + '%' : '--';

  // Rotary knob
  drawKnob(rotary);
  $('knobVal').textContent      = rotary;
  $('encoderVal').textContent   = rotary;
  $('dacVal').textContent       = ((throttle / 100) * 3.3).toFixed(2) + ' V';
  $('throttleFill').style.width = throttle + '%';
  $('dirVal').textContent       = throttle > 5 ? 'FORWARD' : throttle < -5 ? 'REGEN' : 'NEUTRAL';

  // Chart
  drawChart(speedHistory);

  // Range error
  const rangeErr = Math.abs(pitch) > 30 || Math.abs(roll) > 30;
  $('rangeBanner').style.display = rangeErr ? 'block' : 'none';
}

// ── Simulation loop ────────────────────────────────────────
let tick = 0;
function simulate() {
  tick++;

  // Smooth speed towards target
  const { rampUp, rampDown, mode, speedLim } = state;
  const ru = rampUp[mode] * 0.008;
  const rd = rampDown[mode] * 0.01;
  if (state.speed < state.targetSpeed)
    state.speed = Math.min(state.speed + ru, state.targetSpeed);
  else
    state.speed = Math.max(state.speed - rd, state.targetSpeed);

  // Auto-drive simulation (sinusoidal + noise)
  const t = tick * 0.02;
  const baseTarget = 40 + 30 * Math.sin(t * 0.5) + 15 * Math.sin(t * 1.3);
  const lim = state.speedLim[state.mode];
  state.targetSpeed = Math.max(0, Math.min(lim, baseTarget));

  // IMU simulation
  state.pitch = 15 * Math.sin(t * 0.7) + 3 * Math.sin(t * 2.1);
  state.roll  = 12 * Math.cos(t * 0.5) + 2 * Math.cos(t * 3.0);
  state.yaw   = (state.yaw + 0.5) % 360;

  // Rotary / throttle simulation
  state.rotary   = Math.round(10 * Math.sin(t * 0.9));
  state.throttle = Math.max(0, Math.round((state.speed / lim) * 100));

  // Power simulation
  state.current = (state.speed / 120) * 25 + Math.random() * 2;
  state.voltage = 72 - (state.current * 0.05);

  // Speed history
  state.speedHistory.push(state.speed);
  if (state.speedHistory.length > 80) state.speedHistory.shift();

  updateUI();
}

// ── Mode buttons ───────────────────────────────────────────
function setupModeButtons() {
  document.querySelectorAll('.mode-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const m = parseInt(btn.dataset.mode);
      state.mode = m;
      document.querySelectorAll('.mode-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      const names = ['ECO MODE', 'SPORT MODE', 'TURBO MODE'];
      addAlert('info', `Switched to ${names[m]}`, '✦');
    });
  });
}

// ── Reset ──────────────────────────────────────────────────
$('resetBtn').addEventListener('click', () => {
  state.speed = 0;
  state.targetSpeed = 0;
  state.yaw = 0; state.pitch = 0; state.roll = 0;
  state.rotary = 0; state.throttle = 0;
  state.speedHistory.fill(0);
  addAlert('ok', 'System Reset', '↺');
});

// ── Alert helper ───────────────────────────────────────────
function addAlert(type, text, icon = 'ℹ️') {
  const list = $('alertsList');
  const elapsed = Math.round((Date.now() - state.startTime) / 1000);
  const mm = String(Math.floor(elapsed/60)).padStart(2,'0');
  const ss = String(elapsed % 60).padStart(2,'0');
  const item = document.createElement('div');
  item.className = `alert-item alert-${type}`;
  item.innerHTML = `
    <span class="alert-icon">${icon}</span>
    <span class="alert-text">${text}</span>
    <span class="alert-time">${mm}:${ss}</span>
  `;
  list.prepend(item);
  // keep max 6
  while (list.children.length > 6) list.removeChild(list.lastChild);
}

// Occasional random alerts
setInterval(() => {
  const rands = [
    ['ok',   'I²C Slave ACK OK',          '📡'],
    ['info', 'EEPROM Write Complete',      '💾'],
    ['warn', 'High Current Draw Detected', '⚡'],
    ['info', 'DMP FIFO Buffer Read',       '📊'],
  ];
  const r = rands[Math.floor(Math.random() * rands.length)];
  addAlert(r[0], r[1], r[2]);
}, 8000);

// ── Init ───────────────────────────────────────────────────
spawnParticles();
setupModeButtons();
setInterval(updateClock, 1000);
updateClock();
setInterval(simulate, 50); // 20 Hz
simulate();
