// ============================================================
//  Dashboard.h  —  Beautiful 480×320 TFT Dashboard for BBPN EV
//  จอ 4" TFT SPI 480×320 V1.1 + TFT_eSPI + ESP32
//
//  Layout:
//  ┌────────────────────────────────────────────────────────┐
//  │  HEADER BAR: Brand + Mode + Clock                      │ h=28
//  ├──────────────┬───────────────────┬────────────────────┤
//  │  RING METER  │   SPEED (HUGE)    │  PARAM PANEL       │
//  │  (left col)  │   + ypr angle     │  Ramp Up/Dn/Lim    │
//  │  w=285 h=262 │  w=200            │  w=153             │
//  ├──────────────┴───────────────────┴────────────────────┤
//  │  STATUS BAR: Speed km/h | Pitch | Roll | Yaw           │ h=28
//  └────────────────────────────────────────────────────────┘
// ============================================================
#pragma once

#include <TFT_eSPI.h>

// ── Color Palette (RGB565) ──────────────────────────────────
#define DB_BG        0x0841   // Very dark navy background
#define DB_PANEL     0x1082   // Slightly lighter panel bg
#define DB_BORDER    0x2965   // Subtle border
#define DB_CYAN      0x07FF   // Accent cyan
#define DB_PURPLE    0x701F   // Accent purple
#define DB_GREEN     0x07E0   // Accent green (OK)
#define DB_ORANGE    0xFD20   // Accent orange (warn)
#define DB_RED       0xF800   // Accent red (danger)
#define DB_YELLOW    0xFFE0   // Accent yellow
#define DB_WHITE     0xFFFF   // Text bright
#define DB_GREY      0x528A   // Text dimmed
#define DB_DARKGREY  0x2945   // Dimmer bg tint

// ── Layout Constants ─────────────────────────────────────────
#define SCR_W        480
#define SCR_H        320
#define HDR_H        28
#define STB_H        28
#define BODY_Y       (HDR_H + 1)
#define BODY_H       (SCR_H - HDR_H - STB_H - 2)   // 262 px

// Left column: ring meter
#define RING_X       2
#define RING_Y       (BODY_Y)
#define RING_COL_W   285
// Centre column: big speed
#define SPD_X        (RING_COL_W + 4)
#define SPD_W        172
// Right column: param panel
#define PARAM_X      (SPD_X + SPD_W - 2)
#define PARAM_W      (SCR_W - PARAM_X - 2)

// Status bar
#define STB_Y        (SCR_H - STB_H)

// Ring meter parameters
#define RM_CX        (RING_X + RING_COL_W / 2 - 4)   // ~144
#define RM_CY        (BODY_Y + BODY_H / 2)            // ~161
#define RM_R         118                               // radius

// ── Helper: draw horizontal gradient bar ─────────────────────
static void db_gradBar(TFT_eSPI &tft,
                        int x, int y, int w, int h,
                        int fillPct,               // 0-100
                        uint16_t colFill,
                        uint16_t colEmpty = DB_DARKGREY) {
  int filled = (int)((long)w * fillPct / 100);
  if (filled > 0) tft.fillRoundRect(x, y, filled,     h, 3, colFill);
  if (filled < w) tft.fillRoundRect(x + filled, y, w - filled, h, 3, colEmpty);
  tft.drawRoundRect(x - 1, y - 1, w + 2, h + 2, 4, DB_BORDER);
}

// ── Helper: draw vertical divider line ──────────────────────
static void db_vline(TFT_eSPI &tft, int x, int y, int h) {
  tft.drawFastVLine(x, y, h, DB_BORDER);
}
static void db_hline(TFT_eSPI &tft, int y, int x, int w) {
  tft.drawFastHLine(x, y, w, DB_BORDER);
}

// ── Draw static chrome (only called once on init) ────────────
static void db_drawChrome(TFT_eSPI &tft) {
  // Full background
  tft.fillScreen(DB_BG);

  // ── Header bar ──
  tft.fillRect(0, 0, SCR_W, HDR_H, DB_PANEL);
  db_hline(tft, HDR_H, 0, SCR_W);

  // Brand text
  tft.setTextColor(DB_CYAN, DB_PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("BBPN EV", 6, HDR_H / 2, 2);

  // Divider lines
  db_vline(tft, RING_COL_W,       BODY_Y, BODY_H);
  db_vline(tft, RING_COL_W + SPD_W, BODY_Y, BODY_H);

  // ── Status bar ──
  db_hline(tft, STB_Y - 1, 0, SCR_W);
  tft.fillRect(0, STB_Y, SCR_W, STB_H, DB_PANEL);

  // ── Right panel: static labels ──
  int px = PARAM_X + 6;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(DB_GREY, DB_BG);

  tft.drawString("RAMP UP",  px, BODY_Y + 14, 1);
  tft.drawString("RAMP DWN", px, BODY_Y + 74, 1);
  tft.drawString("SPD LIM",  px, BODY_Y + 134, 1);

  // ── Centre panel: static labels ──
  tft.setTextColor(DB_GREY, DB_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("km/h", SPD_X + SPD_W / 2, BODY_Y + BODY_H - 38, 2);

  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("TILT", SPD_X + SPD_W / 2, BODY_Y + BODY_H - 14, 1);
}

// ── Draw the ring meter (re-uses existing ringMeter logic) ────
//    Draws a coloured arc ring; pass TFT ref and value 0-99
static void db_ringMeter(TFT_eSPI &tft,
                          int value, int vmin, int vmax,
                          int cx, int cy, int r,
                          uint16_t colScheme) {
  // colScheme: 0=fixed green, 1=green→red (like GREEN2RED)
  int angle = 150;
  int v = map(value, vmin, vmax, -angle, angle);
  int w = r / 4;
  byte seg = 5, inc = 8;

  for (int i = -angle + inc / 2; i < angle - inc / 2; i += inc) {
    float sx  = cos((i - 90)       * 0.0174532925f);
    float sy  = sin((i - 90)       * 0.0174532925f);
    float sx2 = cos((i + seg - 90) * 0.0174532925f);
    float sy2 = sin((i + seg - 90) * 0.0174532925f);

    uint16_t x0 = sx  * (r - w) + cx,  y0 = sy  * (r - w) + cy;
    uint16_t x1 = sx  *  r      + cx,  y1 = sy  *  r      + cy;
    uint16_t x2 = sx2 * (r - w) + cx,  y2 = sy2 * (r - w) + cy;
    uint16_t x3 = sx2 *  r      + cx,  y3 = sy2 *  r      + cy;

    uint16_t col;
    if (i < v) {
      // Active segment colour
      if (colScheme == 1) {
        // Green → Yellow → Red
        int pct = map(i, -angle, angle, 0, 127);
        if      (pct < 42)  col = DB_GREEN;
        else if (pct < 85)  col = DB_YELLOW;
        else                col = DB_RED;
      } else {
        col = DB_CYAN;
      }
    } else {
      col = DB_DARKGREY;
    }
    tft.fillTriangle(x0,y0, x1,y1, x2,y2, col);
    tft.fillTriangle(x1,y1, x2,y2, x3,y3, col);
  }

  // Inner dark circle to clean up center
  tft.fillCircle(cx, cy, r - w - 2, DB_BG);
}

// ── Draw centre speed panel ──────────────────────────────────
static void db_drawSpeed(TFT_eSPI &tft,
                          int speed,        // 0-99
                          float pitchDeg,   // from ypr[1]*180/PI
                          const char* modeName) {
  int cx = SPD_X + SPD_W / 2;

  // Clear speed area (avoid full screen clear)
  tft.fillRect(SPD_X + 1, BODY_Y + 1, SPD_W - 2, BODY_H - 2, DB_BG);

  // Big speed number
  tft.setTextDatum(MC_DATUM);
  uint16_t spdCol = (speed > 80) ? DB_RED :
                    (speed > 55) ? DB_ORANGE : DB_WHITE;
  tft.setTextColor(spdCol, DB_BG);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", speed);
  tft.drawString(buf, cx, BODY_Y + BODY_H / 2 - 20, 8);

  // Mode name below speed
  tft.setTextColor(DB_CYAN, DB_BG);
  tft.drawString(modeName, cx, BODY_Y + BODY_H / 2 + 70, 4);

  // Tilt indicator bar (pitch)
  int tiltBarW = SPD_W - 20;
  int tiltBarX = SPD_X + 10;
  int tiltBarY = BODY_Y + BODY_H - 58;
  // background
  tft.fillRoundRect(tiltBarX, tiltBarY, tiltBarW, 10, 3, DB_DARKGREY);
  tft.drawRoundRect(tiltBarX - 1, tiltBarY - 1, tiltBarW + 2, 12, 4, DB_BORDER);
  // centre mark
  int midX = tiltBarX + tiltBarW / 2;
  tft.drawFastVLine(midX, tiltBarY - 3, 16, DB_GREY);
  // needle (pitch mapped ±45° → bar width)
  float clampedPitch = constrain(pitchDeg, -45.0f, 45.0f);
  int needleX = midX + (int)(clampedPitch / 45.0f * (tiltBarW / 2));
  needleX = constrain(needleX, tiltBarX + 2, tiltBarX + tiltBarW - 6);
  uint16_t needleCol = (abs(pitchDeg) > 30) ? DB_RED :
                       (abs(pitchDeg) > 15) ? DB_ORANGE : DB_CYAN;
  tft.fillRoundRect(needleX - 2, tiltBarY + 1, 6, 8, 2, needleCol);
}

// ── Draw right param panel ────────────────────────────────────
static void db_drawParams(TFT_eSPI &tft,
                           int rampUp, int rampDown, int speedLim,
                           int mode) {
  int px = PARAM_X + 4;
  int bx = px;
  int bw = PARAM_W - 10;

  // Section: RAMP UP
  int sy = BODY_Y + 24;
  db_gradBar(tft, bx, sy, bw, 18, rampUp, DB_GREEN);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(DB_WHITE, DB_BG);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d", rampUp);
  tft.fillRect(PARAM_X + PARAM_W - 30, sy - 2, 28, 22, DB_BG);
  tft.drawString(tmp, PARAM_X + PARAM_W - 3, sy + 9, 2);

  // Section: RAMP DOWN
  sy = BODY_Y + 84;
  db_gradBar(tft, bx, sy, bw, 18, rampDown, DB_ORANGE);
  snprintf(tmp, sizeof(tmp), "%d", rampDown);
  tft.fillRect(PARAM_X + PARAM_W - 30, sy - 2, 28, 22, DB_BG);
  tft.drawString(tmp, PARAM_X + PARAM_W - 3, sy + 9, 2);

  // Section: SPEED LIMIT
  sy = BODY_Y + 144;
  db_gradBar(tft, bx, sy, bw, 18, speedLim, DB_PURPLE);
  snprintf(tmp, sizeof(tmp), "%d", speedLim);
  tft.fillRect(PARAM_X + PARAM_W - 30, sy - 2, 28, 22, DB_BG);
  tft.drawString(tmp, PARAM_X + PARAM_W - 3, sy + 9, 2);

  // Mode dots (3 circles)
  int dotY = BODY_Y + BODY_H - 28;
  uint16_t dotCols[3] = {DB_GREY, DB_GREY, DB_GREY};
  dotCols[mode] = DB_CYAN;
  const char* dotLabels[3] = {"N", "S", "E"};
  for (int i = 0; i < 3; i++) {
    int dx = PARAM_X + 18 + i * 38;
    tft.fillCircle(dx, dotY, 10, dotCols[i]);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(DB_BG, dotCols[i]);
    tft.drawString(dotLabels[i], dx, dotY, 2);
  }
}

// ── Draw header (mode name + clock ticks) ────────────────────
static void db_drawHeader(TFT_eSPI &tft,
                            const char* modeName,
                            unsigned long ms) {
  // Clear header right area
  tft.fillRect(80, 1, SCR_W - 82, HDR_H - 2, DB_PANEL);

  // Mode name centre
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_YELLOW, DB_PANEL);
  tft.drawString(modeName, SCR_W / 2, HDR_H / 2, 2);

  // Uptime right
  unsigned long sec = ms / 1000;
  char tbuf[16];
  snprintf(tbuf, sizeof(tbuf), "%02lu:%02lu:%02lu",
           sec / 3600, (sec % 3600) / 60, sec % 60);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(DB_CYAN, DB_PANEL);
  tft.drawString(tbuf, SCR_W - 4, HDR_H / 2, 2);
}

// ── Draw status bar ───────────────────────────────────────────
static void db_drawStatusBar(TFT_eSPI &tft,
                               int speed,
                               float pitch, float roll, float yaw) {
  tft.fillRect(0, STB_Y + 1, SCR_W, STB_H - 2, DB_PANEL);
  tft.setTextDatum(ML_DATUM);

  char buf[80];
  snprintf(buf, sizeof(buf),
           "  SPD %3d km/h   PITCH %+5.1f    ROLL %+5.1f    YAW %+6.1f",
           speed, pitch, roll, yaw);
  tft.setTextColor(DB_GREY, DB_PANEL);
  tft.drawString(buf, 4, STB_Y + STB_H / 2, 1);
}

// ── Full dashboard redraw (for ring meter, called on value change) ─
//    ringValue: whatever is being shown in the ring (speed / rampUp etc.)
static void db_drawRing(TFT_eSPI &tft, int ringValue, int vmin, int vmax) {
  // Clear left panel
  tft.fillRect(RING_X, BODY_Y, RING_COL_W - 1, BODY_H, DB_BG);
  // Draw ring
  db_ringMeter(tft, ringValue, vmin, vmax, RM_CX, RM_CY, RM_R, 0);
  // Value in ring centre
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_WHITE, DB_BG);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", ringValue);
  tft.drawString(buf, RM_CX, RM_CY, 6);
  // Small label at bottom of ring
  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("SPEED", RM_CX, RM_CY + RM_R - 16, 1);
}

// ============================================================
//  db_init()  —  Call once in setup() AFTER tft.begin()
// ============================================================
static void db_init(TFT_eSPI &tft) {
  tft.setRotation(1);        // landscape
  tft.fillScreen(DB_BG);
  db_drawChrome(tft);
}

// ============================================================
//  db_update()  —  Call in Main_task() instead of old drawing
//
//  Parameters:
//    speed       — actual speed from I2C slave (0-99)
//    rampUp/Down/SpeedLim — EEPROM values for current mode
//    mode        — 0=NORMAL, 1=SPORT, 2=ECO
//    ypr[3]      — from MPU6050 dmpGetYawPitchRoll, in radians
// ============================================================
static void db_update(TFT_eSPI &tft,
                       int speed,
                       int rampUp, int rampDown, int speedLim,
                       int mode,
                       float yprRad[3]) {
  static int  prevSpeed    = -1;
  static int  prevRampUp   = -1;
  static int  prevRampDown = -1;
  static int  prevSpeedLim = -1;
  static int  prevMode     = -1;
  static unsigned long prevSec = 0;

  const char* modeNames[3] = {"NORMAL", "SPORT", "ECO"};

  float pitchDeg = yprRad[1] * 57.2957795f;
  float rollDeg  = yprRad[2] * 57.2957795f;
  float yawDeg   = yprRad[0] * 57.2957795f;

  unsigned long ms = millis();
  unsigned long sec = ms / 1000;

  bool paramsChanged = (rampUp   != prevRampUp   ||
                        rampDown != prevRampDown  ||
                        speedLim != prevSpeedLim  ||
                        mode     != prevMode);

  bool speedChanged = (speed != prevSpeed);
  bool timeChanged  = (sec   != prevSec);

  // Always update ring if speed changed
  if (speedChanged) {
    db_drawRing(tft, speed, 0, 99);
    prevSpeed = speed;
  }

  // Update centre panel if speed or mode changed
  if (speedChanged || mode != prevMode) {
    db_drawSpeed(tft, speed, pitchDeg, modeNames[mode]);
  }

  // Update right panel only if params/mode changed
  if (paramsChanged) {
    db_drawParams(tft, rampUp, rampDown, speedLim, mode);
    prevRampUp   = rampUp;
    prevRampDown = rampDown;
    prevSpeedLim = speedLim;
    prevMode     = mode;
  }

  // Update header every second
  if (timeChanged) {
    db_drawHeader(tft, modeNames[mode], ms);
    prevSec = sec;
  }

  // Status bar: update when speed or angle changes
  if (speedChanged || timeChanged) {
    db_drawStatusBar(tft, speed, pitchDeg, rollDeg, yawDeg);
  }
}

// ============================================================
//  db_drawSetParam()  —  Replaces Set_Parameter() display
//  Shows which param is being edited with a large ring + label
// ============================================================
static void db_drawSetParam(TFT_eSPI &tft,
                              int paramMode,   // 0=UP, 1=DOWN, 2=SPD
                              int value,
                              int mode) {
  static int prevVal = -99;
  if (value == prevVal) return;
  prevVal = value;

  tft.fillScreen(DB_BG);

  // Large ring
  db_ringMeter(tft, value, 0, 99, SCR_W / 2, SCR_H / 2 - 20, 130, 1);

  // Value in centre
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_WHITE, DB_BG);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", value);
  tft.drawString(buf, SCR_W / 2, SCR_H / 2 - 20, 7);

  // Param name
  const char* paramNames[3] = {"RAMP UP", "RAMP DOWN", "SPEED LIMIT"};
  tft.setTextColor(DB_CYAN, DB_BG);
  tft.drawString(paramNames[paramMode], SCR_W / 2, SCR_H / 2 + 120, 4);

  // Instruction
  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("HOLD BTN 2s TO SAVE", SCR_W / 2, SCR_H - 18, 1);
}

// ============================================================
//  db_drawModeSelect()  —  Replaces displayMode()
// ============================================================
static void db_drawModeSelect(TFT_eSPI &tft, int mode) {
  tft.fillScreen(DB_BG);

  // Title
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("SELECT DRIVE MODE", SCR_W / 2, 30, 2);

  const char* names[3] = {"NORMAL", "SPORT", "ECO"};
  const uint16_t cols[3] = {DB_CYAN, DB_ORANGE, DB_GREEN};
  const char* icons[3] = {"N", "S", "E"};

  for (int i = 0; i < 3; i++) {
    int boxX = 30 + i * 148;
    int boxY = 70;
    int boxW = 130, boxH = 160;
    uint16_t borderCol = (i == mode) ? cols[i] : DB_BORDER;
    uint16_t bgCol     = (i == mode) ? (cols[i] & 0x1082) : DB_PANEL;

    tft.fillRoundRect(boxX, boxY, boxW, boxH, 12, bgCol);
    tft.drawRoundRect(boxX, boxY, boxW, boxH, 12, borderCol);
    if (i == mode) {
      // Second outline glow effect (inner)
      tft.drawRoundRect(boxX + 2, boxY + 2, boxW - 4, boxH - 4, 10, borderCol);
    }

    // Icon circle
    tft.fillCircle(boxX + boxW/2, boxY + 55, 32, borderCol);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(DB_BG, borderCol);
    tft.drawString(icons[i], boxX + boxW / 2, boxY + 55, 4);

    // Mode name
    tft.setTextColor((i == mode) ? cols[i] : DB_GREY, bgCol);
    tft.drawString(names[i], boxX + boxW / 2, boxY + 110, 2);
  }

  // Instruction
  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("TURN ROTARY  |  PRESS TO CONFIRM", SCR_W / 2, SCR_H - 20, 1);
}

// ============================================================
//  db_drawSaving()  —  Dark "Saving..." screen shown after
//  user holds button to commit a parameter change.
//  Replaces every tft.fillScreen(WHITE_GREY) that preceded
//  the loading bar animation.
// ============================================================
static void db_drawSaving(TFT_eSPI &tft) {
  tft.fillScreen(DB_BG);

  // Decorative ring
  tft.drawCircle(SCR_W / 2, SCR_H / 2 - 30, 60, DB_BORDER);

  // Icon
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_GREEN, DB_BG);
  tft.drawString("SAVING", SCR_W / 2, SCR_H / 2 - 30, 4);

  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("Writing to EEPROM...", SCR_W / 2, SCR_H / 2 + 20, 2);

  // Progress bar background
  int bx = 80, by = SCR_H / 2 + 55, bw = SCR_W - 160, bh = 18;
  tft.fillRoundRect(bx - 1, by - 1, bw + 2, bh + 2, 5, DB_BORDER);
  tft.fillRoundRect(bx, by, bw, bh, 4, DB_DARKGREY);

  // Animate progress bar
  for (int i = 0; i <= bw; i += 4) {
    tft.fillRoundRect(bx, by, i, bh, 4, DB_GREEN);
    delay(8);
  }

  tft.setTextColor(DB_GREEN, DB_BG);
  tft.drawString("DONE ✓", SCR_W / 2, SCR_H / 2 + 90, 2);
  delay(400);
}

// ============================================================
//  db_drawHello()  —  Replaces HELLO() splash screen
// ============================================================
static void db_drawHello(TFT_eSPI &tft) {
  tft.fillScreen(DB_BG);

  // Decorative rings
  tft.drawCircle(SCR_W / 2, SCR_H / 2, 120, DB_BORDER);
  tft.drawCircle(SCR_W / 2, SCR_H / 2, 90,  DB_BORDER);
  tft.drawCircle(SCR_W / 2, SCR_H / 2,  5,  DB_CYAN);

  // Brand
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DB_CYAN, DB_BG);
  tft.drawString("BBPN", SCR_W / 2, SCR_H / 2 - 30, 7);

  tft.setTextColor(DB_WHITE, DB_BG);
  tft.drawString("EV CONTROLLER", SCR_W / 2, SCR_H / 2 + 35, 2);

  tft.setTextColor(DB_GREY, DB_BG);
  tft.drawString("INITIALIZING...", SCR_W / 2, SCR_H / 2 + 65, 1);
}
