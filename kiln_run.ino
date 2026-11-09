// ------------------------------------------------
// RUNNING + MANUAL
// ------------------------------------------------

// Passo etichette X automatico: minimo 45px, valori "belli"
const uint32_t NICE_TIME_STEPS[] = {60, 300, 600, 900, 1800, 3600, 7200, 14400, 21600, 43200, 86400};
constexpr int  NICE_TIME_STEP_COUNT = sizeof(NICE_TIME_STEPS) / sizeof(NICE_TIME_STEPS[0]);

uint32_t niceTimeStep(float windowSec) {
  float minStep = 45.0f * windowSec / GRAPH_W;
  for (int i = 0; i < NICE_TIME_STEP_COUNT; i++) {
    if ((float)NICE_TIME_STEPS[i] >= minStep) return NICE_TIME_STEPS[i];
  }
  return NICE_TIME_STEPS[NICE_TIME_STEP_COUNT - 1];
}

void drawRunningState() {
  DinMeter.Display.fillScreen(COL_BG);
  partialUpdateRunningState();
}

void partialUpdateRunningState() {
  auto& d = DinMeter.Display;
  uint32_t now = millis();

  // ---------- Status bar: nome + batteria ----------
  char nameBuf[30];
  if (currentProgram) uiTruncate(currentProgram->name.c_str(), nameBuf, 28);
  else                strcpy(nameBuf, "--");
  uiTopBar(nameBuf);

  int seg = 0;
  if (currentProgram && programRunning) {
    seg = slowRiseWait ? slowRiseWaitSeg
                       : currentSegmentIndex((now - programStartTime) / 1000);
  }

  // ---------- Riga 2: setpoint, misura, tempo ----------
  d.fillRect(0, 18, 240, 18, COL_BG);
  d.setTextSize(1);
  if (programRunning) {
    d.setTextColor(COL_MUTED);
    d.setCursor(6, 22);
    d.print("Tgt");
    d.setTextColor(COL_AMBER);
    d.setCursor(26, 22);
    d.printf("%.0fC", targetTemp);

    d.setTextColor(COL_MUTED);
    d.setCursor(82, 22);
    d.print("Tsens");
    d.setTextColor(COL_GREEN);
    d.setCursor(114, 22);
    d.printf("%.1fC", currentTemp);

    String el = shortTimeLabel((now - programStartTime) / 1000);
    String tot = shortTimeLabel((uint32_t)totalProgSeconds);
    String tstr = el + "/" + tot;
    int tw = d.textWidth(tstr.c_str());
    d.setTextColor(COL_TEXT);
    d.setCursor(234 - tw, 22);
    d.print(tstr);
  } else if (programEndTime > programStartTime) {
    d.setTextColor(COL_GREEN);
    d.setCursor(6, 22);
    d.print("FINE");
    String dur = "Durata ";
    dur += shortTimeLabel((programEndTime - programStartTime) / 1000);
    d.setTextColor(COL_MUTED);
    d.setCursor(40, 22);
    d.print(dur);
    String ssr = "SSR ";
    ssr += shortTimeLabel(ssrOnTotalTime / 1000);
    d.setTextColor(COL_GREEN);
    d.setCursor(150, 22);
    d.print(ssr);
  }

  if (sensorError) {
    d.fillRect(0, 18, 240, 18, COL_BG);
    d.fillRoundRect(4, 19, 232, 16, 4, COL_RED);
    d.setTextColor(COL_BG);
    d.setCursor(10, 23);
    d.print("SENSORE ERRORE!  SSR OFF");
  }

  // ---------- Grafico ----------
  d.fillRoundRect(4, 36, 232, 97, 6, COL_SURFACE);

  // Intestazione del grafico: segmento + SSR
  if (currentProgram && programRunning) {
    bool dwell = isDwellSegment(seg);
    char segText[20];
    snprintf(segText, sizeof(segText), "%s %d/%d", dwell ? "TENUTA" : "RAMPA", seg, curveRTCount - 1);
    d.setTextSize(1);
    d.setTextColor(dwell ? COL_AMBER : COL_CYAN);
    d.setCursor(6, 38);
    d.print(segText);

    char ssrTxt[20];
    snprintf(ssrTxt, sizeof(ssrTxt), "SSR %s %d%%", ssrState ? "ON" : "OFF", (int)(lastDuty + 0.5f));
    int tw = d.textWidth(ssrTxt);
    uiPill(230 - (tw + 12), 37, tw + 12, 13, ssrTxt,
           ssrState ? COL_GREEN : COL_SURFACE2,
           ssrState ? COL_BG : COL_MUTED);
  } else if (currentProgram && programEndTime > programStartTime) {
    d.setTextSize(1);
    d.setTextColor(COL_GREEN);
    d.setCursor(6, 38);
    d.print("COTTURA COMPLETATA");
  }

  if (!currentProgram) return;

  float tMin = 0.0f;
  float tMax = totalProgSeconds;
  float window = tMax - tMin;
  if (window <= 0.0f) return;

  auto mapTempToY = [&](float T) -> int16_t {
    float clamped = T;
    if (clamped < 0.0f)            clamped = 0.0f;
    else if (clamped > FIXED_Y_MAX) clamped = FIXED_Y_MAX;
    float ratio = clamped / FIXED_Y_MAX;
    return (int16_t)(GRAPH_Y + GRAPH_H - 1 - (GRAPH_H - 1) * ratio);
  };

  auto mapTimeToX = [&](float sec) -> int16_t {
    if (sec < tMin) sec = tMin;
    if (sec > tMax) sec = tMax;
    float frac = (sec - tMin) / window;
    return (int16_t)(GRAPH_X + frac * GRAPH_W);
  };

  // Griglia orizzontale (300/600/900)
  for (int i = 1; i < 4; i++) {
    float val = FIXED_Y_MAX * i / 4;
    int yy = mapTempToY(val);
    d.drawFastHLine(GRAPH_X, yy, GRAPH_W, COL_LINE);
  }

  // Griglia verticale ai tick temporali
  uint32_t timeStep = niceTimeStep(window);
  for (uint32_t tTick = timeStep; tTick < (uint32_t)window; tTick += timeStep) {
    int xx = mapTimeToX((float)tTick);
    d.drawFastVLine(xx, GRAPH_Y, GRAPH_H, COL_LINE);
  }

  // Assi
  d.drawFastHLine(GRAPH_X, GRAPH_Y + GRAPH_H, GRAPH_W, COL_LINE);
  d.drawFastVLine(GRAPH_X, GRAPH_Y, GRAPH_H, COL_LINE);

  // Etichette Y
  for (int i = 0; i <= 4; i++) {
    float val = FIXED_Y_MAX * i / 4;
    int yy = mapTempToY(val);
    char buf[8];
    sprintf(buf, "%.0f", val);
    d.setTextSize(1);
    d.setTextColor(COL_MUTED);
    int tw = d.textWidth(buf);
    int ty = (i == 0) ? yy - 9 : yy - 4;
    int tx = GRAPH_X - tw - 2;
    if (tx < 4) tx = 4;
    d.setCursor(tx, ty);
    d.print(buf);
  }

  // Curva programmata (ambra)
  {
    bool hasFirst = false;
    int16_t prevX = 0, prevY = 0;
    for (int i = 0; i < curveRTCount; i++) {
      int16_t x = mapTimeToX((float)curveRT[i].cumSec);
      int16_t y = mapTempToY(curveRT[i].temp);
      if (hasFirst) d.drawWideLine(prevX, prevY, x, y, 1.0f, COL_AMBER);
      prevX = x;
      prevY = y;
      hasFirst = true;
    }
  }

  // Storia misurata (verde)
  if (sampleCount >= 2) {
    bool hasFirst = false;
    int16_t prevX = 0, prevY = 0;
    for (int c = 0; c < sampleCount; c++) {
      int16_t x = mapTimeToX((float)samples[c].sec);
      int16_t y = mapTempToY(samples[c].temp);
      if (hasFirst) d.drawWideLine(prevX, prevY, x, y, 0.5f, COL_GREEN);
      prevX = x;
      prevY = y;
      hasFirst = true;
    }
  }

  // Pallino di posizione
  if (programRunning) {
    float elapsed = (now - programStartTime) / 1000.0f;
    int16_t px = mapTimeToX(elapsed);
    int16_t py = mapTempToY(targetTemp);
    if (px >= GRAPH_X && px <= GRAPH_X + GRAPH_W) {
      d.fillCircle(px, py, 3, COL_TEXT);
      d.drawCircle(px, py, 4, COL_CYAN);
    }
  }

  // Etichette X
  for (uint32_t tTick = 0; tTick <= (uint32_t)window; tTick += timeStep) {
    int xx = mapTimeToX((float)tTick);
    d.drawFastVLine(xx, GRAPH_Y + GRAPH_H, 3, COL_LINE);
    String lbl = shortTimeLabel(tTick);
    int lxx = xx - 10;
    if (lxx < GRAPH_X) lxx = GRAPH_X;
    int tw = d.textWidth(lbl.c_str());
    if (lxx + tw > GRAPH_X + GRAPH_W) lxx = GRAPH_X + GRAPH_W - tw;
    d.setTextColor(COL_MUTED);
    d.setCursor(lxx, GRAPH_Y + GRAPH_H + 3);
    d.print(lbl);
  }
}

// ------------------------------------------------
// MANUAL: interruttore SSR manuale
// short = esci, long = toggle SSR (bloccato senza sensore)
// ------------------------------------------------
void drawManualTempValue() {
  auto& d = DinMeter.Display;
  d.fillRect(12, 88, 216, 22, COL_BG);
  d.setTextSize(2);
  d.setTextColor(sensorError ? COL_RED : COL_GREEN);
  d.setCursor(16, 90);
  if (sensorError) d.print("SENSORE ERRORE!");
  else             d.printf("Tsens %.1fC", currentTemp);
}

void updateManualTempDraw() {
  // Batteria: aggiorna l'area anche senza interazione (ogni 5 s)
  static uint32_t lastBattDraw = 0;
  if (millis() - lastBattDraw >= 5000) {
    lastBattDraw = millis();
    drawBatteryPct();
  }
  if (millis() - lastManualTempDraw < 400) return;
  lastManualTempDraw = millis();
  drawManualTempValue();
}

void drawManualScreen() {
  auto& d = DinMeter.Display;
  d.fillScreen(COL_BG);
  uiTopBar("MANUALE");

  uint16_t col = sensorError ? COL_RED : (ssrState ? COL_GREEN : COL_MUTED);
  d.fillRoundRect(12, 26, 216, 54, 8, COL_SURFACE);
  d.fillCircle(38, 53, 11, col);
  d.setTextSize(3);
  d.setTextColor(col);
  d.setCursor(58, 40);
  d.print(ssrState ? "SSR ON" : "SSR OFF");

  drawManualTempValue();

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(12, 120);
  d.print("SHORT  Esci");
  d.setCursor(132, 120);
  d.print("LONG  SSR");
}
