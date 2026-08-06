// ------------------------------------------------
// PID posizionale + SSR time-proportional + autocorrezione (campo 03)
// ------------------------------------------------
constexpr bool     AUTOTUNE_ENABLED    = true;
constexpr uint32_t SSR_WINDOW_MS       = 10000;   // finestra time-proportional
constexpr uint32_t CORRECTION_GATE_MS  = 120000;  // max 1 correzione / 120s
constexpr float    KP_DEFAULT = 10.0f, KI_DEFAULT = 0.2f, KD_DEFAULT = 0.1f;
constexpr float    KP_MIN = 0.5f,  KP_MAX = 50.0f;
constexpr float    KI_MIN = 0.01f, KI_MAX = 2.0f;
constexpr float    KD_MIN = 0.0f,  KD_MAX = 5.0f;

// Gain scheduling: 3 fasce di temperatura (<400 / 400-900 / >900), scelta sul target
static float pidKp[3], pidKi[3], pidKd[3];

// Stato PID
static float    integral      = 0.0f;
static float    lastMeas      = 0.0f;
float lastDuty      = 0.0f;   // duty% corrente (visibile a run per la UI)
static float    prevDuty      = 0.0f;
static uint32_t lastPidTime   = 0;

// Stato osservatori (autocorrezione)
static int      segIndex         = -1;
static int      zcCount          = 0;
static int      zcDir            = 0;
static int      nervousCount     = 0;
static int      dwellTick        = 0;
static float    errSum           = 0.0f;
static int      errSamples       = 0;
static bool     dwellOvershoot   = false;
static int      overLine8        = 0;   // T sopra la linea di >8°C
static int      underLine8       = 0;   // T sotto la linea di >8°C
static int      underLine15      = 0;   // T sotto la linea di >15°C (salita lenta)
static uint32_t lastCorrectionMs = 0;

int bandFor(float t) {
  if (t < 400.0f) return 0;
  if (t <= 900.0f) return 1;
  return 2;
}

void loadPidParams() {
  for (int i = 0; i < 3; i++) {
    pidKp[i] = preferences.getFloat(("pid_kp_" + String(i + 1)).c_str(), KP_DEFAULT);
    pidKi[i] = preferences.getFloat(("pid_ki_" + String(i + 1)).c_str(), KI_DEFAULT);
    pidKd[i] = preferences.getFloat(("pid_kd_" + String(i + 1)).c_str(), KD_DEFAULT);
  }
}

void savePidParams() {
  for (int i = 0; i < 3; i++) {
    preferences.putFloat(("pid_kp_" + String(i + 1)).c_str(), pidKp[i]);
    preferences.putFloat(("pid_ki_" + String(i + 1)).c_str(), pidKi[i]);
    preferences.putFloat(("pid_kd_" + String(i + 1)).c_str(), pidKd[i]);
  }
}

void setSsr(bool on) {
  if (ssrState == on) return;
  ssrState = on;
  digitalWrite(SSR_PIN, on ? HIGH : LOW);
  Serial.println(on ? "[SSR] ON" : "[SSR] OFF");
}

// Reset di tutto lo stato PID/osservatori (all'avvio programma)
void resetPid() {
  integral       = 0.0f;
  lastMeas       = currentTemp;
  lastDuty       = 0.0f;
  prevDuty       = 0.0f;
  lastPidTime    = 0;
  segIndex       = -1;
  zcCount        = 0;
  zcDir          = 0;
  nervousCount   = 0;
  dwellTick      = 0;
  errSum         = 0.0f;
  errSamples     = 0;
  dwellOvershoot = false;
  overLine8      = 0;
  underLine8     = 0;
  underLine15    = 0;
  lastCorrectionMs = 0;
}

// PID posizionale, dt=1s, derivata sulla misura, anti-windup condizionale
float runPID() {
  int b = bandFor(targetTemp);
  float kp = pidKp[b], ki = pidKi[b], kd = pidKd[b];

  float err  = targetTemp - currentTemp;
  float dMeas = currentTemp - lastMeas;   // dt = 1 tick = 1s
  lastMeas = currentTemp;

  // Anti-windup: integra solo se |e|>deadband e se l'uscita col nuovo integrale
  // non satura (trial output)
  if (fabs(err) > 0.2f) {
    float trial = kp * err - kd * dMeas + ki * (integral + err);
    if (trial > 0.0f && trial < 100.0f) integral += err;
  }
  integral = constrain(integral, -300.0f, 300.0f);

  float out = kp * err - kd * dMeas + ki * integral;
  return constrain(out, 0.0f, 100.0f);
}

int currentSegmentIndex(uint32_t sec) {
  for (int i = 1; i < curveRTCount; i++) {
    if (sec <= curveRT[i].cumSec) return i;
  }
  return curveRTCount - 1;
}

// Segmento 0 = rampa da P0 (temperatura misurata) al primo punto
bool isDwellSegment(int seg) {
  if (seg < 1) return false;
  return (curveRT[seg].temp == curveRT[seg - 1].temp);
}

// Applica una correzione di gain alla fascia attiva (gate 120s + clamp)
void correct(float kpMul, float kiMul, float kdMul) {
  uint32_t now = millis();
  if (now - lastCorrectionMs < CORRECTION_GATE_MS) return;
  lastCorrectionMs = now;
  int b = bandFor(targetTemp);
  pidKp[b] = constrain(pidKp[b] * kpMul, KP_MIN, KP_MAX);
  pidKi[b] = constrain(pidKi[b] * kiMul, KI_MIN, KI_MAX);
  pidKd[b] = constrain(pidKd[b] * kdMul, KD_MIN, KD_MAX);
  savePidParams();
  Serial.printf("[Autotune] fascia %d: Kp=%.2f Ki=%.3f Kd=%.3f\n", b, pidKp[b], pidKi[b], pidKd[b]);
}

// Osservatori (un tick = 1s): tenuta e rampa (campo 03/07)
void observerTick() {
  // Salita lenta (campo 07): gestione sempre attiva, anche senza autotune
  if (slowRiseWait) {
    // Attendi arrivo: T arrivata al punto (±2°C) -> riparte il segmento successivo
    if (fabs(currentTemp - slowRiseWaitTarget) < 2.0f) {
      uint32_t elapsedNow = (millis() - programStartTime) / 1000;
      int a = -1;
      for (int i = 1; i < curveRTCount; i++) {
        if (curveRT[i].cumSec == slowRiseWaitAnchor) { a = i; break; }
      }
      if (a >= 0) insertAfter(a, elapsedNow, slowRiseWaitTarget, elapsedNow - slowRiseWaitAnchor);
      else        insertAfter(0, elapsedNow, slowRiseWaitTarget, elapsedNow);
      slowRiseWait = false;
      slowRiseActive = false;
    } else if (millis() - slowRiseWaitStart >= SLOW_RISE_WAIT_MS) {
      // tetto 1h superato: la schermata ricompare
      slowRiseWait = false;
      slowRiseActive = true;
      currentState = SLOW_RISE;
      needsUpdate = true;
    }
    return;
  }

  if (!AUTOTUNE_ENABLED) {
    prevDuty = lastDuty;
    return;
  }
  uint32_t elapsedSec = (millis() - programStartTime) / 1000;
  int seg = currentSegmentIndex(elapsedSec);
  if (seg != segIndex) {
    segIndex = seg;
    integral = 0.0f;   // integrale azzerato a ogni cambio segmento
    zcCount = 0; zcDir = 0; nervousCount = 0; dwellTick = 0;
    errSum = 0.0f; errSamples = 0; dwellOvershoot = false;
    overLine8 = 0; underLine8 = 0;
  }

  float err = targetTemp - currentTemp;

  // Zero-crossing del segnale di errore (oscillazione / zigzag)
  int dir = (err > 0.0f) ? 1 : ((err < 0.0f) ? -1 : 0);
  if (dir != 0) {
    if (zcDir != 0 && dir != zcDir) zcCount++;
    zcDir = dir;
  }

  if (isDwellSegment(seg)) {
    // TENUTA
    dwellTick++;
    if (err < -3.0f) dwellOvershoot = true;                 // overshoot >3°C
    if (fabs(lastDuty - prevDuty) > 20.0f) nervousCount++;  // uscita nervosa ±20%

    errSum += err; errSamples++;
    if (errSamples >= 60) {                                  // offset medio >2°C
      float meanErr = errSum / 60;
      if (meanErr > 2.0f) correct(1.0f, 1.1f, 1.0f);         // Ki×1.1
      errSum = 0.0f; errSamples = 0;
    }

    if (dwellTick >= 180 && fabs(err) < 5.0f) {              // tenuta ≥3min e |e|<5
      if (zcCount >= 4) { correct(0.9f, 1.0f, 1.0f); zcCount = 0; }      // oscillazione → Kp×0.9
      if (nervousCount >= 4) { correct(1.0f, 1.0f, 0.9f); nervousCount = 0; } // nervosa → Kd×0.9
    }
    if (dwellOvershoot) { correct(1.0f, 0.9f, 1.0f); dwellOvershoot = false; } // overshoot → Ki×0.9
  } else {
    // RAMPA
    if (err < -8.0f) overLine8++; else overLine8 = 0;        // sopra linea >8°C
    if (err > 8.0f)  underLine8++; else underLine8 = 0;      // sotto linea >8°C
    if (overLine8 >= 60) { correct(1.0f, 0.9f, 1.0f); overLine8 = 0; }                 // 60s sopra → Ki×0.9
    if (underLine8 >= 120 && elapsedSec > 600) { correct(1.1f, 1.0f, 1.0f); underLine8 = 0; } // 120s sotto (dopo 10min) → Kp×1.1
    if (zcCount >= 4) { correct(0.9f, 1.0f, 1.1f); zcCount = 0; }                      // zigzag → Kp×0.9 e Kd×1.1

    // SALITA LENTA (campo 07): T più di 15°C sotto la linea per 60 min
    if (err > 15.0f) underLine15++;
    else underLine15 = 0;
    if (underLine15 >= 3600 && !slowRiseActive) {
      underLine15 = 0;
      slowRiseActive = true;
      slowRiseSel = 0;
      currentState = SLOW_RISE;
      needsUpdate = true;
      Serial.println("[Safety] Salita lenta rilevata");
    }
  }
  prevDuty = lastDuty;
}

// Chiamato a ogni giro; il PID gira 1 volta al secondo.
// Fuori dal RUNNING e su errore sensore: SSR forzato OFF.
void updateSSR() {
  if (!programRunning) {
    setSsr(false);
    return;
  }

  // Fail-safe: sensor error => force SSR OFF, never heat on stale/frozen temp
  if (sensorError) {
    if (ssrState) Serial.println("[SSR] OFF (sensore errore)");
    setSsr(false);
    return;
  }

  uint32_t now = millis();
  if (now - lastPidTime >= TEMP_READ_INTERVAL) {
    lastPidTime = now;
    lastDuty = runPID();
    observerTick();
    if (ssrState) ssrOnTotalTime += TEMP_READ_INTERVAL;  // conteggio on-time per tick
  }

  // SSR time-proportional: duty% della finestra SSR_WINDOW_MS
  if (lastDuty <= 0.0f) {
    setSsr(false);
  } else if (lastDuty >= 100.0f) {
    setSsr(true);
  } else {
    uint32_t cyclePos = now % SSR_WINDOW_MS;
    bool shouldOn = (cyclePos < (uint32_t)(SSR_WINDOW_MS * lastDuty / 100.0f));
    setSsr(shouldOn);
  }
}
