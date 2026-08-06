#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include "esp_task_wdt.h"
#include <driver/gpio.h>

// M5 Libraries
#include <M5DinMeter.h>

#define KMETER_ADDR 0x66

// ------------------------------------------------
// Definitions & Constants
// ------------------------------------------------
constexpr uint8_t SSR_PIN             = 1;
constexpr float   FIXED_Y_MAX         = 1200.0f;   // Maximum temperature for Y axis display
constexpr uint8_t MAX_HOURS           = 24;
constexpr uint8_t MAX_PROGRAMS        = 10;
constexpr uint8_t MAX_CURVE_POINTS    = 20;        // points per program curve (P0 virtual excluded)
constexpr uint32_t MAX_SAMPLES        = 2000;      // history samples for the RUNNING graph
constexpr uint32_t MIN_SEGMENT_SEC    = 300;       // 5 min: min gap between consecutive points
constexpr uint32_t MAX_TOTAL_SEC      = 24UL * 3600UL;  // program total duration cap

constexpr uint16_t TEMP_READ_INTERVAL = 1000;      // read thermocouple every 1 second
constexpr uint32_t LONG_PRESS_DURATION= 1000;      // 1 second for long press
constexpr int      COUNTS_PER_DETENT  = 2;         // the encoder produces ±2 counts per detent
constexpr uint8_t  SENSOR_FAIL_THRESHOLD = 5;      // consecutive failed reads before fail-safe (≈5s)

// Sicurezza (campo 07)
constexpr float    OVER_TEMP_C        = 1250.0f;   // soglia di intervento (1200 esercizio + 50 margine)
constexpr uint32_t SLOW_RISE_WAIT_MS  = 3600000UL; // tetto attesa "Attendi arrivo": 1h, poi ri-chiede
constexpr uint32_t SLOW_RISE_EXTEND_S = 1800;      // "Allunga 30 min" per ogni pressione

// Alphabet for program-name input
constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ";

// Graph Layout Constants
constexpr int RUN_TEXT_H = 20;
constexpr int GRAPH_X    = 24;
constexpr int GRAPH_Y    = 24;
constexpr int GRAPH_W    = 210;
constexpr int GRAPH_H    = 98;

// ------------------------------------------------
// Global Objects / Variables
// ------------------------------------------------
// I2C read helper (ponytail: STOP required by KMeterISO, repeated start fails)
uint8_t kmReadReg(uint8_t reg) {
  Wire.beginTransmission(KMETER_ADDR);
  Wire.write(reg);
  Wire.endTransmission(true);
  delayMicroseconds(50);
  Wire.requestFrom(KMETER_ADDR, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

int32_t kmReadTemp() {
  Wire.beginTransmission(KMETER_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(true);
  delayMicroseconds(50);
  Wire.requestFrom(KMETER_ADDR, (uint8_t)4);
  if (Wire.available() < 4) return INT32_MIN;
  union { int32_t v; uint8_t b[4]; } u;
  for (int i = 0; i < 4; i++) u.b[i] = Wire.read();
  return u.v;
}
Preferences preferences;
const char* const NAMESPACE = "kilnprogs";

// Temperature variables
static float currentTemp = 0.0f;
static float targetTemp  = 0.0f;

// ------------------------------------------------
// Data Structures
// ------------------------------------------------
struct Point {
  uint32_t cumSec;     // cumulative seconds from program start
  float    temp;       // target temperature at this point
};

struct Program {
  String name;
  Point points[MAX_CURVE_POINTS];
  uint8_t pointCount;
};

// History samples for the RUNNING graph
struct Sample {
  uint32_t sec;        // seconds from program start
  float    temp;
  bool     ssr;
};
static Sample samples[MAX_SAMPLES];
static int      sampleCount   = 0;
static uint32_t lastSampleSec = 0;

// Programs storage
Program programs[MAX_PROGRAMS];
uint8_t programCount = 0;

// ------------------------------------------------
// Application State
// ------------------------------------------------
enum AppState {
  MAIN_MENU,
  RUNNING,
  SLOW_RISE,
  SOVRATEMP,
  CONFIRM_DELETE,
  ADD_PROG_NAME,
  ADD_PROG_EDIT
};
AppState currentState = MAIN_MENU;

// Running program info
Program* currentProgram   = nullptr;
uint32_t programStartTime = 0;
uint32_t programEndTime   = 0;    // freeze the elapsed time once done
static float startTemp    = 0.0f; // P0 virtual: measured temp at program start

// Curva runtime: P0 virtuale + punti programma, estendibile a runtime
// (salita lenta: allunga / attesa). Ricostruita a ogni avvio programma.
constexpr uint8_t MAX_RT_EXTRA = 20;
Point curveRT[MAX_CURVE_POINTS + MAX_RT_EXTRA];
int   curveRTCount = 0;

// Sicurezza (campo 07)
static bool    overTemp           = false;
static bool    slowRiseActive     = false;  // schermata SALITA LENTA attiva
static uint8_t slowRiseSel        = 0;      // scelta selezionata 0..2
static bool    slowRiseWait       = false;  // in attesa che T arrivi al punto
static float   slowRiseWaitTarget = 0.0f;   // T del punto finale del segmento
static uint32_t slowRiseWaitAnchor = 0;     // cumSec del punto di ancoraggio
static uint32_t slowRiseWaitStart = 0;

// SSR control
static bool ssrState           = false;
static bool programRunning     = false;
static uint32_t ssrOnTotalTime = 0;

// Fail-safe sensor: consecutive failed reads => SSR forced OFF
static uint8_t sensorFailCount = 0;
static bool    sensorError     = false;

// Display update flag
static bool needsUpdate = true;

// Main menu
static int selectedIndex = 0;

// Add Program (new program creation)
Program newProgram;
uint8_t editingPointIndex = 0;
bool editingTempField = true;    // true = editing temperature, false = editing time
bool fieldEditMode = false;      // true = encoder regola il valore, short conferma
int  editorSel = 0;              // selected command bar cell 0..7
String inputName = "";
int currentCharIndex = 0;

// Confirm delete state
int deleteIndex = -1;

// Debounce and ignore buttons until timeout
static uint32_t ignoreButtonUntil = 0;

// Button press detection
static bool pressInProgress = false;
static bool longPressFired  = false;
static uint32_t pressStartTime = 0;

// Partial update of main menu temperature
static uint32_t lastTempDraw = 0;

// ------------------------------------------------
// X-axis uses real time in seconds
// ------------------------------------------------
static float totalProgSeconds = 0.0f;  // total time of the program (in seconds)

// ------------------------------------------------
// Function Prototypes
// ------------------------------------------------
void loadPrograms();
void savePrograms();

void updateSSR();
void setSsr(bool on);
void resetPid();
void loadPidParams();
void startProgram(int idx);
void stopProgram();
void updateProgram();
void deleteProgram(int idx);
float setpointAt(uint32_t sec);
int currentSegmentIndex(uint32_t sec);
void insertAfter(int afterIdx, uint32_t insertSec, float insertTemp, uint32_t shiftSec);
void buildRuntimeCurve();

void checkButton();
void handleShortPress();
void handleLongPress();
void handleEncoder();

void drawMainMenu();
void updateMainMenuTemp();

void drawRunningState();
void partialUpdateRunningState();
void drawSlowRise();
void drawOverTemp();
String elapsedTimeString(uint32_t ms);
String shortTimeLabel(uint32_t sec);

void handleConfirmDelete();

// Add Program screens
void drawAddProgName();

void drawAddProgEdit();
void handleAddProgEditShortPress();
void adjustPoint(uint8_t idx, int dir);
void adjustPoint(uint8_t idx, int dir);

// Utility for time-based X-axis
float computeTotalProgramTime(const Program &prog);

// ------------------------------------------------
// setup
// ------------------------------------------------
void setup() {
  Serial.begin(115200);
  M5.begin();

  auto cfg = M5.config();
  DinMeter.begin(cfg, true);
  DinMeter.update();

  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW);
  // Drive max: il 3.3V puro è marginale per l'ingresso 3-32VDC del Fotek
  // (vedi 01: ci vuole il driver a transistor; qui alziamo la corrente disponibile)
  gpio_set_drive_capability((gpio_num_t)SSR_PIN, GPIO_DRIVE_CAP_3);

  DinMeter.Display.setRotation(1);
  DinMeter.Display.fillScreen(TFT_BLACK);
  DinMeter.Display.setTextColor(TFT_GREEN);
  DinMeter.Display.setTextFont(1);
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.drawString("Inizializzazione...", 10, 10);
  delay(1500);

  Wire.begin(13, 15, 400000L);
  delay(10);
  // Probe KMeter
  bool kmOk = false;
  for (int i = 0; i < 5 && !kmOk; i++) {
    Wire.beginTransmission(KMETER_ADDR);
    kmOk = (Wire.endTransmission(true) == 0);
    if (!kmOk) {
      Serial.printf("[Setup] Kmeter fail attempt %d\n", i + 1);
      delay(1000);
    }
  }
  if (!kmOk) {
    Serial.println("[Setup] Kmeter not found!");
    while (true) { delay(1000); }
  }

  preferences.begin(NAMESPACE, false);
  loadPidParams();
  loadPrograms();

  // Watchdog (campo 07): se il loop si blocca, reboot in 10s; SSR LOW al boot
  esp_task_wdt_config_t wdtCfg;
  wdtCfg.timeout_ms     = 10000;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic  = true;
  esp_task_wdt_init(&wdtCfg);
  esp_task_wdt_add(NULL);

  DinMeter.Encoder.write(0);

  sampleCount = 0;
  lastSampleSec = 0;

  ssrState = false;
  programRunning = false;
  ssrOnTotalTime = 0;

  Serial.println("[Setup] Complete");
}

// ------------------------------------------------
// loop
// ------------------------------------------------
void loop() {
  static uint32_t lastTempRead = 0;
  uint32_t now = millis();

  esp_task_wdt_reset();

  if (now - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = now;

    // Read thermocouple always (menu too), track consecutive failures
    if (kmReadReg(0x20) == 0) {
      int32_t rawT = kmReadTemp();
      if (rawT != INT32_MIN) {
        currentTemp = rawT / 100.0f;
        sensorFailCount = 0;
        sensorError = false;
      } else {
        sensorFailCount++;
        if (sensorFailCount >= SENSOR_FAIL_THRESHOLD) sensorError = true;
      }
    } else {
      sensorFailCount++;
      if (sensorFailCount >= SENSOR_FAIL_THRESHOLD) sensorError = true;
    }

    // SOVRATEMPERATURA (campo 07): rete di salvataggio sempre attiva, in ogni stato
    if (currentTemp > OVER_TEMP_C) overTemp = true;
    else if (currentTemp < OVER_TEMP_C - 10.0f) overTemp = false;
    if (overTemp && currentState != SOVRATEMP) {
      if (programRunning) stopProgram();
      currentState = SOVRATEMP;
      needsUpdate = true;
    }

    if (programRunning) {
      updateSSR();

      // Record sample adaptively (whole-program timeline)
      if (currentState == RUNNING && currentProgram) {
        uint32_t elapsedSec = (millis() - programStartTime) / 1000;
        uint32_t desiredInterval = (uint32_t)(totalProgSeconds / MAX_SAMPLES);
        if (desiredInterval < 1) desiredInterval = 1;
        if (elapsedSec - lastSampleSec >= desiredInterval && sampleCount < (int)MAX_SAMPLES) {
          samples[sampleCount].sec  = elapsedSec;
          samples[sampleCount].temp = currentTemp;
          samples[sampleCount].ssr  = ssrState;
          sampleCount++;
          lastSampleSec = elapsedSec;
        }
      }

      if (currentState == RUNNING) {
        partialUpdateRunningState();
      }
    }
    else {
      // Program is not running => force SSR OFF
      setSsr(false);
    }

    if (currentState == MAIN_MENU) {
      updateMainMenuTemp();
    }
  }

  DinMeter.update();
  checkButton();
  handleEncoder();

  switch (currentState) {
    case MAIN_MENU:
      if (needsUpdate) {
        drawMainMenu();
        needsUpdate = false;
      }
      break;
    case RUNNING:
      updateProgram();
      if (needsUpdate) {
        drawRunningState();
        needsUpdate = false;
      }
      break;
    case SLOW_RISE:
      if (needsUpdate) {
        drawSlowRise();
        needsUpdate = false;
      }
      break;
    case SOVRATEMP:
      if (needsUpdate) {
        drawOverTemp();
        needsUpdate = false;
      }
      break;
    case CONFIRM_DELETE:
      handleConfirmDelete();
      break;
    case ADD_PROG_NAME:
      if (needsUpdate) {
        drawAddProgName();
        needsUpdate = false;
      }
      break;
    case ADD_PROG_EDIT:
      if (needsUpdate) {
        drawAddProgEdit();
        needsUpdate = false;
      }
      break;
    default:
      break;
  }

  delay(2);
}

// ------------------------------------------------
// checkButton
// ------------------------------------------------
void checkButton() {
  if (millis() < ignoreButtonUntil) return;
  bool isPressed = DinMeter.BtnA.isPressed();
  uint32_t now = millis();

  if (!pressInProgress && isPressed) {
    pressInProgress = true;
    pressStartTime = now;
    longPressFired = false;
  }
  else if (pressInProgress && isPressed) {
    if (!longPressFired && (now - pressStartTime >= LONG_PRESS_DURATION)) {
      longPressFired = true;
      handleLongPress();
    }
  }
  else if (pressInProgress && !isPressed) {
    pressInProgress = false;
    if (!longPressFired) {
      handleShortPress();
    }
  }
}

void handleShortPress() {
  Serial.println("[Button] SHORT Press");
  switch (currentState) {
    case MAIN_MENU:
      if (selectedIndex < programCount) {
        startProgram(selectedIndex);
      } else if (selectedIndex == programCount) {
        // Begin new program creation
        newProgram.name = "";
        newProgram.pointCount = 0;
        currentCharIndex = 0;
        inputName = "";
        editingPointIndex = 0;
        editingTempField = true;
        fieldEditMode = false;
        editorSel = 0;
        currentState = ADD_PROG_NAME;
        needsUpdate = true;
        ignoreButtonUntil = millis() + 300;
      } else {
        DinMeter.Power.powerOff();
      }
      break;

    case CONFIRM_DELETE:
      // short => confirm
      if (deleteIndex >= 0 && deleteIndex < programCount) {
        deleteProgram(deleteIndex);
      }
      break;

    case ADD_PROG_NAME: {
      // add current char
      if (inputName.length() < 20) {
        int alphaLen = strlen(ALPHABET);
        char c = ALPHABET[currentCharIndex % alphaLen];
        inputName += c;
      }
      needsUpdate = true;
    } break;

    case ADD_PROG_EDIT: {
      handleAddProgEditShortPress();
    } break;

    case SLOW_RISE: {
      // conferma la scelta selezionata
      switch (slowRiseSel) {
        case 0: {  // Attendi arrivo
          uint32_t elapsedNow = (millis() - programStartTime) / 1000;
          int seg = currentSegmentIndex(elapsedNow);
          slowRiseWaitTarget = curveRT[seg].temp;
          slowRiseWaitAnchor = curveRT[seg].cumSec;
          slowRiseWaitStart  = millis();
          slowRiseWait       = true;
          currentState       = RUNNING;
        } break;
        case 1: {  // Allunga 30 min
          uint32_t elapsedNow = (millis() - programStartTime) / 1000;
          int seg = currentSegmentIndex(elapsedNow);
          insertAfter(seg - 1, elapsedNow, currentTemp, SLOW_RISE_EXTEND_S);
          slowRiseActive = false;
          currentState   = RUNNING;
        } break;
        default: { // Annulla cottura
          slowRiseActive = false;
          stopProgram();
          currentState = RUNNING;
        } break;
      }
      needsUpdate = true;
    } break;

    case RUNNING: {
      // zoom/pan rimossi (campo 05): short press nel RUNNING non fa nulla
    } break;

    default:
      break;
  }
}

void handleLongPress() {
  Serial.println("[Button] LONG Press");
  switch (currentState) {
    case MAIN_MENU:
      if (selectedIndex < programCount) {
        deleteIndex = selectedIndex;
        currentState = CONFIRM_DELETE;
        needsUpdate = true;
      }
      break;
    case CONFIRM_DELETE:
      // cancel
      currentState = MAIN_MENU;
      needsUpdate = true;
      break;
    case ADD_PROG_NAME: {
      newProgram.name = inputName;
      inputName = "";
      currentCharIndex = 0;
      // curva iniziale: 2 punti (4h totali), l'utente li aggiunge con [+P]
      newProgram.pointCount = 2;
      newProgram.points[0].cumSec = 2UL * 3600UL;
      newProgram.points[0].temp   = 0;
      newProgram.points[1].cumSec = 4UL * 3600UL;
      newProgram.points[1].temp   = 0;
      editingPointIndex = 0;
      editingTempField = true;
      fieldEditMode = false;
      editorSel = 0;
      currentState = ADD_PROG_EDIT;
      needsUpdate = true;
    } break;
    case ADD_PROG_EDIT: {
      // do nothing
    } break;
    case SLOW_RISE: {
      // do nothing
    } break;
    case SOVRATEMP: {
      // si esce solo a mano
      currentState = MAIN_MENU;
      needsUpdate = true;
    } break;
    case RUNNING: {
      // stop or main menu
      if (programRunning) stopProgram();
      else {
        currentState = MAIN_MENU;
        needsUpdate = true;
      }
    } break;

    default:
      break;
  }
}

// ------------------------------------------------
// handleEncoder
// ------------------------------------------------
void handleEncoder() {
  // readAndReset: delta esatto dall'ultimo poll (l'ISR PJRC conta ogni
  // fronte); il resto accumulato conserva i conteggi dispari (±1 sub-detent)
  int32_t delta = DinMeter.Encoder.readAndReset();
  if (delta == 0) return;

  static int32_t encRemainder = 0;
  encRemainder += delta;
  int32_t detents = encRemainder / COUNTS_PER_DETENT;
  encRemainder -= detents * COUNTS_PER_DETENT;
  detents = -detents;  // orario = aumenta (campo 01)
  if (detents == 0) return;

  switch (currentState) {
    case MAIN_MENU: {
      int maxIdx = programCount + 1;
      selectedIndex += detents;
      if (selectedIndex < 0) selectedIndex = maxIdx;
      if (selectedIndex > maxIdx) selectedIndex = 0;
      needsUpdate = true;
    } break;

    case ADD_PROG_NAME: {
      int alphaLen = strlen(ALPHABET);
      currentCharIndex += detents;
      while (currentCharIndex < 0) {
        currentCharIndex += alphaLen;
      }
      currentCharIndex %= alphaLen;
      needsUpdate = true;
    } break;

    case ADD_PROG_EDIT: {
      if (fieldEditMode) {
        adjustPoint(editingPointIndex, detents);
      } else {
        editorSel += detents;
        if (editorSel < 0) editorSel = 7;
        if (editorSel > 7) editorSel = 0;
      }
      needsUpdate = true;
    } break;

    case SLOW_RISE: {
      slowRiseSel += detents;
      if (slowRiseSel < 0) slowRiseSel = 2;
      if (slowRiseSel > 2) slowRiseSel = 0;
      needsUpdate = true;
    } break;

    default:
      break;
  }

  Serial.printf("[Encoder] Detents:%d | State:%d\n",
                (int)detents, (int)currentState);
}
