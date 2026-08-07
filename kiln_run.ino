// ------------------------------------------------
// RUNNING: testata 2 righe + grafico (campo 05)
// ------------------------------------------------
void drawRunningState() {
  DinMeter.Display.fillScreen(TFT_BLACK);
  partialUpdateRunningState();
}

// Passo etichette X automatico (campo 05): minimo 45px, arrotondato ai valori "belli"
const uint32_t NICE_TIME_STEPS[] = {60, 300, 600, 900, 1800, 3600, 7200, 14400, 21600, 43200, 86400};
constexpr int  NICE_TIME_STEP_COUNT = sizeof(NICE_TIME_STEPS) / sizeof(NICE_TIME_STEPS[0]);

uint32_t niceTimeStep(float windowSec) {
  float minStep = 45.0f * windowSec / GRAPH_W;   // 45px minimi tra due etichette
  for (int i = 0; i < NICE_TIME_STEP_COUNT; i++) {
    if ((float)NICE_TIME_STEPS[i] >= minStep) return NICE_TIME_STEPS[i];
  }
  return NICE_TIME_STEPS[NICE_TIME_STEP_COUNT - 1];
}

void partialUpdateRunningState() {
  uint32_t now = millis();

  // ---------- Testata: 2 righe (campo 05) ----------
  DinMeter.Display.fillRect(0, 0, 240, RUN_TEXT_H, TFT_BLACK);
  DinMeter.Display.setTextFont(1);

  // R1: nome+segmento | SSR+duty | batteria
  int seg = 0;
  if (currentProgram && programRunning) {
    seg = slowRiseWait ? slowRiseWaitSeg
                       : currentSegmentIndex((now - programStartTime) / 1000);
  }
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  DinMeter.Display.setCursor(4, 1);
  if (currentProgram) {
    if (programRunning) {
      bool dwell = isDwellSegment(seg);
      DinMeter.Display.printf("%s %s%d/%d", currentProgram->name.c_str(),
                              dwell ? "TENUTA" : "RAMPA", seg, curveRTCount - 1);
    } else if (programEndTime > programStartTime) {
      // fine cottura: FINE · durata · SSR on-time
      String t = elapsedTimeString(programEndTime - programStartTime);
      String s = elapsedTimeString(ssrOnTotalTime * 1000UL);
      DinMeter.Display.print("FINE ");
      DinMeter.Display.print(t);
      DinMeter.Display.print(" SSR ");
      DinMeter.Display.print(s);
    } else {
      DinMeter.Display.print("Programma Terminato");
    }
  }
  DinMeter.Display.setCursor(150, 1);
  if (programRunning) {
    DinMeter.Display.print(ssrState ? "SSR:ON " : "SSR:OFF ");
    DinMeter.Display.print((int)(lastDuty + 0.5f));
    DinMeter.Display.print("%");
  }
  int batt = DinMeter.Power.getBatteryLevel();
  if (batt >= 0) {
    DinMeter.Display.setTextColor(TFT_RED, TFT_BLACK);
    DinMeter.Display.setCursor(214, 1);
    DinMeter.Display.print(batt);
    DinMeter.Display.print("%");
  }

  // R2: Tgt giallo | temp verde | tempo — tutto size 1 (campo 05)
  DinMeter.Display.setCursor(4, 11);
  DinMeter.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  DinMeter.Display.printf("Tgt %.1fC", targetTemp);
  DinMeter.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  DinMeter.Display.printf("  TSens %.1fC", currentTemp);

  if (programRunning) {
    String totalStr = elapsedTimeString((uint32_t)(totalProgSeconds * 1000.0f));
    String el = elapsedTimeString(now - programStartTime);
    DinMeter.Display.setCursor(148, 11);
    DinMeter.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    DinMeter.Display.print(el);
    DinMeter.Display.print("/");
    DinMeter.Display.print(totalStr);
  }

  if (sensorError) {
    DinMeter.Display.fillRect(0, 11, 240, 8, TFT_BLACK);
    DinMeter.Display.setTextColor(TFT_RED, TFT_BLACK);
    DinMeter.Display.setCursor(4, 11);
    DinMeter.Display.print("SENSORE ERRORE!");
  }

  // ---------- Grafico (campo 05: curva gialla + T verde + pallino, niente etichette Y) ----------
  DinMeter.Display.fillRect(0, RUN_TEXT_H, 240, 135 - RUN_TEXT_H, TFT_BLACK);
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

  // assi Y e X
  DinMeter.Display.drawLine(GRAPH_X - 1, GRAPH_Y, GRAPH_X - 1, GRAPH_Y + GRAPH_H, TFT_WHITE);
  DinMeter.Display.drawLine(GRAPH_X - 1, GRAPH_Y + GRAPH_H, GRAPH_X + GRAPH_W, GRAPH_Y + GRAPH_H, TFT_WHITE);

  // Etichette Y (temperatura), size 1, a sinistra dell'asse
  constexpr int nTicks = 5;
  float yStep = FIXED_Y_MAX / nTicks;
  for (int i = 0; i <= nTicks; i++) {
    float val = i * yStep;
    int yy = mapTempToY(val);
    DinMeter.Display.drawLine(GRAPH_X - 3, yy, GRAPH_X - 1, yy, TFT_WHITE);
    char buf[8];
    sprintf(buf, "%.0f", val);
    DinMeter.Display.setCursor(GRAPH_X - 24, yy - 3);
    DinMeter.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    DinMeter.Display.print(buf);
  }

  // Curva programmata (gialla)
  {
    bool hasFirst = false;
    int16_t prevX = 0, prevY = 0;
    for (int i = 0; i < curveRTCount; i++) {
      int16_t x = mapTimeToX((float)curveRT[i].cumSec);
      int16_t y = mapTempToY(curveRT[i].temp);
      if (hasFirst) DinMeter.Display.drawLine(prevX, prevY, x, y, TFT_YELLOW);
      prevX = x; prevY = y;
      hasFirst = true;
    }
  }

  // Storia temperatura (verde)
  if (sampleCount >= 2) {
    bool hasFirst = false;
    int16_t prevX = 0, prevY = 0;
    for (int c = 0; c < sampleCount; c++) {
      int16_t x = mapTimeToX((float)samples[c].sec);
      int16_t y = mapTempToY(samples[c].temp);
      if (hasFirst) DinMeter.Display.drawLine(prevX, prevY, x, y, TFT_GREEN);
      prevX = x; prevY = y;
      hasFirst = true;
    }
  }

  // Pallino bianco: posizione sulla curva (tempo trascorso, setpoint attuale)
  if (programRunning) {
    float elapsed = (now - programStartTime) / 1000.0f;
    int16_t px = mapTimeToX(elapsed);
    int16_t py = mapTempToY(targetTemp);
    if (px >= GRAPH_X && px <= GRAPH_X + GRAPH_W) {
      DinMeter.Display.fillCircle(px, py, 3, TFT_WHITE);
      DinMeter.Display.drawCircle(px, py, 3, TFT_BLACK);
    }
  }

  // Etichette X a passo auto (min 45px), formato 2h30m, taglio bordi
  uint32_t timeStep = niceTimeStep(window);
  for (uint32_t tTick = 0; tTick <= (uint32_t)window; tTick += timeStep) {
    int xx = mapTimeToX((float)tTick);
    DinMeter.Display.drawLine(xx, GRAPH_Y + GRAPH_H, xx, GRAPH_Y + GRAPH_H + 3, TFT_WHITE);
    String lbl = shortTimeLabel(tTick);
    int lxx = xx - 10;
    if (lxx < GRAPH_X) lxx = GRAPH_X;
    if (lxx + 30 > GRAPH_X + GRAPH_W) lxx = GRAPH_X + GRAPH_W - 30;
    DinMeter.Display.setCursor(lxx, GRAPH_Y + GRAPH_H + 4);
    DinMeter.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    DinMeter.Display.print(lbl);
  }
}
