// ------------------------------------------------
// Programmi: setpoint curva, save/load NVS (con migrazione), start/stop/update
// ------------------------------------------------

// Curva runtime: P0 virtuale (0, startTemp) + punti del programma.
// Può essere estesa a runtime dalla salita lenta (campo 07).
void buildRuntimeCurve() {
  curveRT[0] = {0, startTemp};
  for (uint8_t i = 0; i < currentProgram->pointCount; i++) {
    curveRT[i + 1] = currentProgram->points[i];
  }
  curveRTCount = currentProgram->pointCount + 1;
}

// Inserisce un punto dopo afterIdx e slitta di shiftSec tutti i punti successivi
// (curva runtime piena: solo shift, ponytail: limite naturale, raro).
void insertAfter(int afterIdx, uint32_t insertSec, float insertTemp, uint32_t shiftSec) {
  if (afterIdx < 0) afterIdx = 0;
  if (curveRTCount >= MAX_CURVE_POINTS + MAX_RT_EXTRA) {
    for (int i = afterIdx + 1; i < curveRTCount; i++) curveRT[i].cumSec += shiftSec;
  } else {
    int ins = afterIdx + 1;
    for (int i = curveRTCount; i > ins; i--) curveRT[i] = curveRT[i - 1];
    curveRT[ins] = {insertSec, insertTemp};
    curveRTCount++;
    for (int i = ins + 1; i < curveRTCount; i++) curveRT[i].cumSec += shiftSec;
  }
  totalProgSeconds = (float)curveRT[curveRTCount - 1].cumSec;
  segIndex = -1;   // reset osservatore/integrale al prossimo tick
  needsUpdate = true;
}

// Setpoint interpolato sulla spezzata della curva runtime.
float setpointAt(uint32_t sec) {
  if (sec <= 0) return curveRT[0].temp;
  uint32_t prevCum = curveRT[0].cumSec;
  float    prevT   = curveRT[0].temp;
  for (int i = 1; i < curveRTCount; i++) {
    uint32_t cum = curveRT[i].cumSec;
    float    t   = curveRT[i].temp;
    if (sec <= cum) {
      if (cum == prevCum) return t;
      float f = (float)(sec - prevCum) / (float)(cum - prevCum);
      return prevT + f * (t - prevT);
    }
    prevCum = cum;
    prevT   = t;
  }
  return curveRT[curveRTCount - 1].temp;
}

// ------------------------------------------------
// startProgram
// ------------------------------------------------
void startProgram(int idx) {
  if (idx < 0 || idx >= programCount) return;
  currentProgram = &programs[idx];
  programStartTime = millis();
  programEndTime   = 0;
  startTemp = currentTemp;

  if (currentProgram->pointCount < 2) {
    currentState = MAIN_MENU;
    needsUpdate = true;
    return;
  }

  targetTemp = startTemp;
  currentState = RUNNING;
  needsUpdate  = true;
  programRunning = true;

  sampleCount   = 0;
  samples[0].sec  = 0;
  samples[0].temp = currentTemp;
  samples[0].ssr  = false;
  sampleCount = 1;
  lastSampleSec = 0;

  ssrOnTotalTime    = 0;
  ssrState          = false;
  sensorFailCount   = 0;
  sensorError       = false;
  digitalWrite(SSR_PIN, LOW);

  buildRuntimeCurve();
  totalProgSeconds = (float)curveRT[curveRTCount - 1].cumSec;

  slowRiseActive = false;
  slowRiseWait   = false;

  resetPid();

  Serial.printf("[startProgram] Avviato '%s'\n", currentProgram->name.c_str());

  drawRunningState(); 
}

// ------------------------------------------------
// stopProgram
// ------------------------------------------------
void stopProgram() {
  if (!programRunning) return;
  setSsr(false);
  programRunning = false;
  programEndTime = millis();
  slowRiseActive = false;
  slowRiseWait   = false;
  needsUpdate = true;
  Serial.println("[stopProgram] Program ended");
}

// ------------------------------------------------
// updateProgram
// ------------------------------------------------
void updateProgram() {
  if (!currentProgram || !programRunning) return;
  uint32_t elapsedSec = (millis() - programStartTime) / 1000;
  if (elapsedSec >= totalProgSeconds) {
    Serial.println("[updateProgram] Program finito");
    stopProgram();
    return;
  }
  if (slowRiseWait) targetTemp = slowRiseWaitTarget;   // attesa: setpoint congelato al punto
  else targetTemp = setpointAt(elapsedSec);
  updateSSR();
}

// ------------------------------------------------
// Save & Load from Preferences
// ------------------------------------------------
void savePrograms() {
  preferences.clear();
  preferences.putUInt("count", programCount);
  for (uint8_t i = 0; i < programCount; i++) {
    String prefix = "prog" + String(i);
    preferences.putString((prefix + "_name").c_str(), programs[i].name);
    preferences.putUInt((prefix + "_points").c_str(), programs[i].pointCount);
    for (uint8_t s = 0; s < programs[i].pointCount; s++) {
      preferences.putULong((prefix + "_p" + String(s) + "c").c_str(), programs[i].points[s].cumSec);
      preferences.putFloat((prefix + "_p" + String(s) + "t").c_str(), programs[i].points[s].temp);
    }
  }
  Serial.println("[savePrograms] completato");
}

void loadPrograms() {
  programCount = 0;
  uint c = preferences.getUInt("count", 0);
  Serial.printf("[loadPrograms] Carico %d programs\n", c);
  for (uint8_t i = 0; i < c && i < MAX_PROGRAMS; i++) {
    Program &p = programs[i];
    String prefix = "prog" + String(i);
    p.name = preferences.getString((prefix + "_name").c_str(), "");
    if (p.name.isEmpty()) continue;

    uint32_t np = preferences.getUInt((prefix + "_points").c_str(), 0xFFFFFFFF);
    if (np == 0xFFFFFFFF) {
      // Vecchio formato (step): migra ogni step in 2 punti (inizio e fine tenuta)
      uint8_t st = preferences.getUInt((prefix + "_steps").c_str(), 0);
      if (st == 0) continue;
      uint32_t cum = 0;
      uint8_t np2 = 0;
      for (uint8_t s = 0; s < st && np2 < MAX_CURVE_POINTS; s++) {
        float    t = preferences.getFloat((prefix + "_s" + String(s) + "t").c_str(), 0.0f);
        uint32_t d = preferences.getULong((prefix + "_s" + String(s) + "d").c_str(), 0);
        if (np2 < MAX_CURVE_POINTS) p.points[np2++] = {cum, t};
        cum += d;
        if (np2 < MAX_CURVE_POINTS) p.points[np2++] = {cum, t};
      }
      np = np2;
    } else {
      if (np > MAX_CURVE_POINTS) np = MAX_CURVE_POINTS;  // ponytail: curve oltre 20 punti, conserva i primi 20
      for (uint8_t s = 0; s < np; s++) {
        p.points[s].cumSec = preferences.getULong((prefix + "_p" + String(s) + "c").c_str(), 0);
        p.points[s].temp   = preferences.getFloat((prefix + "_p" + String(s) + "t").c_str(), 0.0f);
      }
    }
    if (np < 2) continue;
    p.pointCount = (uint8_t)np;
    programCount++;
  }
  if (programCount == 0) {
    // create default: (0,200) -> (1h,200) -> (3h,500) -> (4h,500)
    Program def;
    def.name = "Default";
    def.pointCount = 4;
    def.points[0] = {0, 200};
    def.points[1] = {3600, 200};
    def.points[2] = {10800, 500};
    def.points[3] = {14400, 500};
    programs[programCount++] = def;
    savePrograms();
    Serial.println("[loadPrograms] creato Default");
  }
}

// ------------------------------------------------
// Helper functions for time-based X axis
// ------------------------------------------------
float computeTotalProgramTime(const Program &prog) {
  if (prog.pointCount == 0) return 0.0f;
  return (float)prog.points[prog.pointCount - 1].cumSec;
}
