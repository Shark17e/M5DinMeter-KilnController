// ------------------------------------------------
// UI: menu, conferma delete, editor curva, utility
// ------------------------------------------------

// ------------------------------------------------
// BATTERIA: lettura calibrata + EMA + curva LiPo (campo 09)
// ------------------------------------------------
// Curva di scarica LiPo approssimata a tratti (mV -> %)
int batteryPctFromMv(float mv) {
  static const float PTS[][2] = {
    {4200, 100}, {4150, 95}, {4100, 90}, {4050, 85}, {4000, 80}, {3950, 72},
    {3900, 65},  {3850, 58}, {3800, 50}, {3750, 38}, {3700, 25}, {3650, 15},
    {3600, 8},   {3500, 3},  {3400, 1},  {3200, 0}
  };
  static const int N = sizeof(PTS) / sizeof(PTS[0]);
  if (mv >= PTS[0][0]) return 100;
  if (mv <= PTS[N - 1][0]) return 0;
  for (int i = 0; i < N - 1; i++) {
    if (mv <= PTS[i][0] && mv >= PTS[i + 1][0]) {
      float f = (mv - PTS[i][0]) / (PTS[i + 1][0] - PTS[i][0]);
      return (int)(PTS[i][1] + f * (PTS[i + 1][1] - PTS[i][1]) + 0.5f);
    }
  }
  return 0;
}

// Chiamata dal blocco a 1s: campiona, filtra EMA, aggiorna battPct, log ogni 30s
void updateBattery() {
  int16_t mv = DinMeter.Power.getBatteryVoltage();
  if (mv <= 0) return;                       // lettura non disponibile
  if (battMv == 0.0f) battMv = mv;
  else battMv += (mv - battMv) * 0.15f;
  battPct = batteryPctFromMv(battMv);
  if (millis() - lastBattLog >= 30000) {
    lastBattLog = millis();
    Serial.printf("[Batt] %dmV %d%%\n", (int)(battMv + 0.5f), battPct);
  }
}

// Ridisegna l'area batteria della status bar.
void drawBatteryPct() {
  DinMeter.Display.fillRect(184, 0, 56, 17, COL_SURFACE);
  uiBattery(192, 4);
  DinMeter.Display.drawFastHLine(0, 17, 240, COL_LINE);
}

// Hero del menu: etichetta, temperatura grande, stato sensore.
void drawMainMenuHero() {
  auto& d = DinMeter.Display;
  d.fillRect(0, 18, 240, 48, COL_BG);

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(10, 23);
  d.print("TEMP FORNO");

  String tStr = sensorError ? "--" : String((int)(currentTemp + 0.5f));
  d.setTextSize(4);
  d.setTextColor(sensorError ? COL_RED : COL_GREEN);
  d.setCursor(10, 33);
  d.print(tStr);
  int tw = d.textWidth(tStr.c_str());
  d.setTextSize(2);
  d.setTextColor(COL_MUTED);
  d.setCursor(10 + tw + 4, 51);
  d.print("C");

  uint16_t col = sensorError ? COL_RED : COL_GREEN;
  d.fillRoundRect(176, 28, 58, 30, 6, COL_SURFACE);
  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(184, 32);
  d.print("SENSORE");
  d.setTextColor(col);
  d.setCursor(184, 43);
  d.print(sensorError ? "ERRORE" : "OK");
}

// ------------------------------------------------
// MAIN_MENU
// ------------------------------------------------
void drawMainMenu() {
  DinMeter.Display.fillScreen(COL_BG);
  uiTopBar("M5 KILN");
  drawMainMenuHero();

  // Lista: 4 righe visibili, selezione centrata a y=84.
  constexpr int lineHeight = 17;
  int centerY = 92;
  int maxIdx  = programCount + 2;
  for (int i = 0; i <= maxIdx; i++) {
    int y = centerY + (i - selectedIndex) * lineHeight - 8;
    if (y < 66 || y > 118) continue;

    bool sel = (i == selectedIndex);
    if (sel) {
      DinMeter.Display.fillRoundRect(6, y, 228, 16, 4, COL_SURFACE2);
      DinMeter.Display.fillRect(6, y, 3, 16, COL_CYAN);
    }
    DinMeter.Display.setTextSize(1);
    DinMeter.Display.setTextColor(sel ? COL_TEXT : COL_MUTED);
    DinMeter.Display.setCursor(14, y + 4);
    if (i < programCount) {
      DinMeter.Display.print(programs[i].name);
    } else if (i == programCount) {
      DinMeter.Display.print("Manuale");
    } else if (i == programCount + 1) {
      DinMeter.Display.print("Aggiungi Programma");
    } else {
      DinMeter.Display.print("Spegni");
    }
    if (sel) {
      DinMeter.Display.setTextColor(COL_CYAN);
      DinMeter.Display.setCursor(226, y + 4);
      DinMeter.Display.print(">");
    }
  }
}

void updateMainMenuTemp() {
  // Batteria: aggiorna l'area anche senza interazione (ogni 5 s)
  static uint32_t lastBattDraw = 0;
  if (millis() - lastBattDraw >= 5000) {
    lastBattDraw = millis();
    drawBatteryPct();
  }
  // Hero: ridisegna solo quando cambia il valore mostrato o lo stato sensore
  static int  lastShown = -9999;
  static bool lastErr   = false;
  int shown = sensorError ? -9999 : (int)(currentTemp + 0.5f);
  if (shown != lastShown || sensorError != lastErr) {
    lastShown = shown;
    lastErr   = sensorError;
    drawMainMenuHero();
  }
}

// ------------------------------------------------
// PROGRAM_ACTIONS: popup azioni sul programma
// ------------------------------------------------
void drawProgramActions() {
  auto& d = DinMeter.Display;

  // Sfondo: menu principale, poi card sovrapposta
  drawMainMenu();

  d.fillRoundRect(18, 20, 208, 98, 8, COL_BG);       // ombra
  d.fillRoundRect(16, 18, 208, 98, 8, COL_SURFACE);  // card
  d.drawRoundRect(16, 18, 208, 98, 8, COL_LINE);

  String pname = (actionProgramIndex >= 0 && actionProgramIndex < programCount)
                   ? programs[actionProgramIndex].name
                   : String("???");
  char nb[29];
  uiTruncate(pname.c_str(), nb, 27);

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(24, 22);
  d.print("AZIONI");
  d.setTextColor(COL_TEXT);
  d.setCursor(24, 32);
  d.print(nb);

  const char* labels[4] = {"Avvia", "Modifica curva", "Cancella programma", "Annulla"};
  for (int i = 0; i < 4; i++) {
    int y = 44 + i * 16;
    bool sel = (actionSel == (uint8_t)i);
    d.fillRoundRect(22, y, 196, 14, 4, sel ? COL_CYAN : COL_SURFACE2);
    if (!sel) d.drawRoundRect(22, y, 196, 14, 4, COL_LINE);
    d.setTextColor(sel ? COL_BG : COL_TEXT);
    d.setCursor(30, y + 3);
    d.print(labels[i]);
  }

  d.setTextColor(COL_MUTED);
  const char* hint = "GIRA  SHORT OK";
  int tw = d.textWidth(hint);
  d.setCursor((240 - tw) / 2, 108);
  d.print(hint);
}

// ------------------------------------------------
// CONFIRM_DELETE
// ------------------------------------------------
void handleConfirmDelete() {
  if (!needsUpdate) return;
  auto& d = DinMeter.Display;
  d.fillScreen(COL_BG);
  uiTopBar("ELIMINA");

  d.fillRoundRect(8, 24, 224, 58, 8, COL_SURFACE);
  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(16, 30);
  d.print("PROGRAMMA SELEZIONATO");

  char nb[18];
  if (deleteIndex >= 0 && deleteIndex < programCount)
    uiTruncate(programs[deleteIndex].name.c_str(), nb, 17);
  else
    strcpy(nb, "???");
  d.setTextSize(2);
  d.setTextColor(COL_TEXT);
  int tw = d.textWidth(nb);
  d.setCursor(8 + (224 - tw) / 2, 48);
  d.print(nb);

  d.setTextSize(1);
  d.fillRoundRect(8, 90, 108, 36, 6, COL_GREEN);
  d.setTextColor(COL_BG);
  d.setCursor(8 + (108 - d.textWidth("SHORT")) / 2, 96);
  d.print("SHORT");
  d.setCursor(8 + (108 - d.textWidth("CONFERMA")) / 2, 108);
  d.print("CONFERMA");

  d.fillRoundRect(124, 90, 108, 36, 6, COL_RED);
  d.setTextColor(COL_BG);
  d.setCursor(124 + (108 - d.textWidth("LONG")) / 2, 96);
  d.print("LONG");
  d.setCursor(124 + (108 - d.textWidth("ANNULLA")) / 2, 108);
  d.print("ANNULLA");

  needsUpdate = false;
}

void deleteProgram(int idx) {
  if (idx < 0 || idx >= programCount) return;
  for (int i = idx; i < programCount - 1; i++) {
    programs[i] = programs[i + 1];
  }
  programCount--;
  if (selectedIndex >= programCount) selectedIndex = (programCount > 0) ? programCount - 1 : 0;
  savePrograms();
  currentState = MAIN_MENU;
  needsUpdate = true;
  Serial.printf("[deleteProgram] Program index=%d eliminato\n", idx);
}

// Carica una copia del programma nell'editor; Salva aggiornerà l'originale.
void beginEditProgram(int idx) {
  if (idx < 0 || idx >= programCount) return;
  newProgram = programs[idx];
  editIndex = idx;
  editingPointIndex = 0;
  editingTempField = true;
  fieldEditMode = false;
  editorSel = 0;
  currentState = ADD_PROG_EDIT;
  needsUpdate = true;
  Serial.printf("[Editor] Modifica '%s'\n", newProgram.name.c_str());
}

// ------------------------------------------------
// SLOW_RISE (campo 07): salita lenta, scelta dell'utente
// ------------------------------------------------
void drawSlowRise() {
  auto& d = DinMeter.Display;
  d.fillScreen(COL_BG);

  // Banner di allarme
  d.fillRoundRect(6, 6, 228, 30, 6, COL_SURFACE);
  d.fillTriangle(18, 12, 18, 30, 32, 21, COL_AMBER);
  d.setTextSize(2);
  d.setTextColor(COL_AMBER);
  d.setCursor(38, 14);
  d.print("SALITA LENTA");
  uiBattery(190, 12);

  // Contesto
  float gap = targetTemp - currentTemp;
  d.setTextSize(1);
  d.setTextColor(COL_TEXT);
  d.setCursor(10, 42);
  if (gap >= 0.0f)
    d.printf("Forno %.0fC sotto la curva da 1h", gap);
  else
    d.printf("Forno %.0fC sopra la curva da 1h", -gap);

  // Opzioni
  const char* opts[3] = {"Aspetta che recuperi", "Allunga di 30 min", "Annulla cottura"};
  for (int i = 0; i < 3; i++) {
    int y = 56 + i * 22;
    bool sel = (slowRiseSel == (uint8_t)i);
    d.fillRoundRect(8, y, 224, 20, 4, sel ? COL_CYAN : COL_SURFACE);
    if (!sel) d.drawRoundRect(8, y, 224, 20, 4, COL_LINE);
    d.setTextSize(1);
    d.setTextColor(sel ? COL_BG : COL_TEXT);
    d.setCursor(16, y + 6);
    d.print(opts[i]);
  }

  // Spiegazione della voce selezionata
  const char* hints[3] = {"poi riprende la curva", "aggiunge 30 min alla curva", "ferma il programma"};
  d.setTextColor(COL_MUTED);
  d.setCursor(10, 124);
  d.print(hints[slowRiseSel]);
}

// ------------------------------------------------
// SOVRATEMP (campo 07): allarme, si esce solo a mano
// ------------------------------------------------
void drawOverTemp() {
  auto& d = DinMeter.Display;
  d.fillScreen(COL_RED);
  d.fillRoundRect(10, 14, 220, 107, 8, COL_BG);
  d.drawRoundRect(10, 14, 220, 107, 8, COL_RED);

  // Triangolo di allarme
  d.fillTriangle(120, 26, 102, 56, 138, 56, COL_RED);

  d.setTextSize(3);
  d.setTextColor(COL_RED);
  const char* title = "SOVRATEMP!";
  int tw = d.textWidth(title);
  d.setCursor((240 - tw) / 2, 64);
  d.print(title);

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  const char* l1 = "SSR SPENTO";
  tw = d.textWidth(l1);
  d.setCursor((240 - tw) / 2, 96);
  d.print(l1);

  d.setTextColor(COL_TEXT);
  const char* l2 = "LONG PRESS PER USCIRE";
  tw = d.textWidth(l2);
  d.setCursor((240 - tw) / 2, 110);
  d.print(l2);
}

// ------------------------------------------------
// Utility stringhe
// ------------------------------------------------
String elapsedTimeString(uint32_t ms) {
  uint32_t sec = ms / 1000;
  uint32_t h = sec / 3600;
  uint32_t m = (sec % 3600) / 60;
  uint32_t s = sec % 60;
  char buf[16];
  if (h > 0)
    sprintf(buf, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else
    sprintf(buf, "%02u:%02u", (unsigned)m, (unsigned)s);
  return String(buf);
}

String shortTimeLabel(uint32_t sec) {
  if (sec < 60) {
    return String(sec) + "s";
  } else if (sec < 3600) {
    uint32_t mm = sec / 60;
    uint32_t ss = sec % 60;
    if (ss == 0) return String(mm) + "m";
    return String(mm) + "m" + String(ss) + "s";
  } else {
    uint32_t hh = sec / 3600;
    uint32_t rr = sec % 3600;
    uint32_t mm = rr / 60;
    if (mm == 0) return String(hh) + "h";
    return String(hh) + "h" + String(mm) + "m";
  }
}

// ------------------------------------------------
// ADD_PROG_NAME
// ------------------------------------------------
void drawAddProgName() {
  auto& d = DinMeter.Display;
  d.fillScreen(COL_BG);
  uiTopBar("NUOVO PROGRAMMA");

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(10, 24);
  d.print("NOME");

  d.fillRoundRect(8, 34, 224, 20, 4, COL_SURFACE);
  d.setCursor(14, 40);
  if (inputName.length() == 0) {
    d.setTextColor(COL_MUTED);
    d.print("(vuoto)");
  } else {
    d.setTextColor(COL_TEXT);
    d.print(inputName);
  }

  int alphaLen = strlen(ALPHABET);
  char c = ALPHABET[currentCharIndex % alphaLen];

  d.fillRoundRect(8, 62, 46, 46, 8, COL_SURFACE2);
  d.setTextSize(4);
  d.setTextColor(COL_CYAN);
  char cs[2] = { (char)(c == ' ' ? '_' : c), 0 };
  int tw = d.textWidth(cs);
  d.setCursor(8 + (46 - tw) / 2, 69);
  d.print(cs);

  d.setTextSize(1);
  d.setTextColor(COL_MUTED);
  d.setCursor(64, 66);
  d.print("CARATTERE");
  d.setTextColor(COL_TEXT);
  d.setCursor(64, 78);
  d.print(c == ' ' ? "SPAZIO" : "RUOTA per cambiare");

  d.setTextColor(COL_MUTED);
  d.setCursor(10, 116);
  d.print("RUOTA cambia  SHORT aggiunge");
  d.setCursor(10, 126);
  d.print("LONG conferma nome");
}

// ------------------------------------------------
// ADD_PROG_EDIT (editor ibrido: grafico + barra comandi)
// ------------------------------------------------
// Barra comandi = 8 celle: < T t > +P -P + Salva/Annulla
// Ruota barra per muovere la selezione; T/t = modifica valore (encoder ±, short conferma).
void drawAddProgEdit() {
  auto& d = DinMeter.Display;
  d.fillScreen(COL_BG);
  uiTopBar(editIndex >= 0 ? "MODIFICA" : "NUOVO");

  // Grafico in una card
  d.fillRoundRect(4, 18, 232, 62, 6, COL_SURFACE);
  constexpr int gx = 30, gy = 22, gw = 204, gh = 52;
  uint32_t total = (newProgram.pointCount > 0) ? newProgram.points[newProgram.pointCount - 1].cumSec : 1;
  if (total == 0) total = 1;

  auto xOf = [&](uint32_t s) -> int { return gx + (int)((long)gw * (long)s / (long)total); };
  auto yOf = [&](float t) -> int {
    int y = gy + gh - (int)(gh * t / FIXED_Y_MAX);
    if (y < gy) y = gy;
    if (y > gy + gh) y = gy + gh;
    return y;
  };

  // Griglia orizzontale
  for (int i = 1; i < 4; i++) {
    int yy = yOf(FIXED_Y_MAX * i / 4);
    d.drawFastHLine(gx, yy, gw, COL_LINE);
  }
  // Assi
  d.drawFastHLine(gx, gy + gh, gw, COL_LINE);
  d.drawFastVLine(gx, gy, gh, COL_LINE);

  // P0 grigio: (0, startTemp)
  int p0x = xOf(0);
  int p0y = yOf(startTemp);
  d.fillCircle(p0x, p0y, 2, COL_MUTED);

  // Spezzata della curva
  int prevX = p0x, prevY = p0y;
  for (uint8_t i = 0; i < newProgram.pointCount; i++) {
    int cx = xOf(newProgram.points[i].cumSec);
    int cy = yOf(newProgram.points[i].temp);
    d.drawWideLine(prevX, prevY, cx, cy, 1.0f, COL_CYAN);
    if (i == editingPointIndex) {
      d.fillCircle(cx, cy, 3, COL_AMBER);
      d.drawCircle(cx, cy, 5, COL_AMBER);
    } else {
      d.fillCircle(cx, cy, 2, COL_CYAN);
    }
    prevX = cx;
    prevY = cy;
  }

  // Riga valore del punto selezionato
  Point &p = newProgram.points[editingPointIndex];
  String tLabel = shortTimeLabel(p.cumSec);
  d.fillRoundRect(4, 82, 232, 16, 4, COL_SURFACE);
  d.setTextSize(1);
  d.setCursor(10, 86);
  d.setTextColor(COL_MUTED);
  d.printf("P%d", editingPointIndex + 1);
  d.setCursor(34, 86);
  d.setTextColor((fieldEditMode && editingTempField) ? COL_AMBER : COL_TEXT);
  d.printf("T %.0fC", p.temp);
  d.setCursor(104, 86);
  d.setTextColor((fieldEditMode && !editingTempField) ? COL_CYAN : COL_TEXT);
  d.printf("t %s", tLabel.c_str());
  if (fieldEditMode) {
    d.setTextColor(COL_MUTED);
    d.setCursor(190, 86);
    d.print("RUOTA");
  }

  // Barra comandi
  const char* labels1[6] = {"<", "T", "t", ">", "+P", "-P"};
  for (int i = 0; i < 6; i++) {
    uiButton(2 + i * 39, 100, 38, 16, labels1[i], editorSel == i);
  }
  uiButton(2, 118, 116, 16, "Salva", editorSel == 6);
  uiButton(120, 118, 116, 16, "Annulla", editorSel == 7);
}

void handleAddProgEditShortPress() {
  uint8_t sel = editorSel;
  if (fieldEditMode) {                 // short = conferma modifica valore
    fieldEditMode = false;
    needsUpdate = true;
    return;
  } else if (sel == 0) {               // <
    if (editingPointIndex > 0) editingPointIndex--;
  } else if (sel == 1) {               // T: modifica temperatura
    editingTempField = true;
    fieldEditMode = true;
  } else if (sel == 2) {               // t: modifica tempo
    editingTempField = false;
    fieldEditMode = true;
  } else if (sel == 3) {               // >
    if (editingPointIndex < newProgram.pointCount - 1) editingPointIndex++;
  } else if (sel == 4) {               // +P: aggiunge un punto
    addPoint();
  } else if (sel == 5) {               // -P: elimina il punto
    removePoint();
  } else if (sel == 6) {               // Salva
    if (editIndex >= 0) {
      if (editIndex < programCount) {
        programs[editIndex] = newProgram;
        savePrograms();
        Serial.println("[Editor] Program modificato");
      }
      editIndex = -1;
      currentState = MAIN_MENU;
    } else {
      if (programCount >= MAX_PROGRAMS) {
        Serial.println("[Editor] Memoria piena, salvataggio non riuscito");
        needsUpdate = true;
        return;
      }
      programs[programCount++] = newProgram;
      savePrograms();
      Serial.println("[Editor] Program salvato con successo");
      currentState = MAIN_MENU;
    }
  } else if (sel == 7) {               // Annulla
    editIndex = -1;
    Serial.println("[Editor] annullato => MAIN_MENU");
    currentState = MAIN_MENU;
  }
  needsUpdate = true;
}

// Aggiunge un punto dopo quello selezionato: stessa T, tempo a metà verso il
// successivo (o +5 min dall'attuale se è l'ultimo), poi lo seleziona.
void addPoint() {
  if (newProgram.pointCount >= MAX_CURVE_POINTS) {
    Serial.println("[Editor] Max punti raggiunto");
    return;
  }
  uint8_t at = editingPointIndex;
  for (uint8_t i = newProgram.pointCount; i > at + 1; i--) {
    newProgram.points[i] = newProgram.points[i - 1];
  }
  Point &prev = newProgram.points[at];
  uint32_t nextSec = (at + 1 < newProgram.pointCount) ? newProgram.points[at + 1].cumSec : 0;
  uint32_t newSec = (nextSec > 0) ? (prev.cumSec + nextSec) / 2 : prev.cumSec + MIN_SEGMENT_SEC;
  if (newSec > MAX_TOTAL_SEC) newSec = MAX_TOTAL_SEC;
  newProgram.points[at + 1].cumSec = newSec;
  newProgram.points[at + 1].temp   = prev.temp;
  newProgram.pointCount++;
  editingPointIndex = at + 1;
  Serial.printf("[Editor] Punto %d aggiunto\n", editingPointIndex + 1);
}

// Elimina il punto selezionato; minimo 2 punti (P1/P2).
void removePoint() {
  if (newProgram.pointCount <= 2) {
    Serial.println("[Editor] Minimo 2 punti");
    return;
  }
  uint8_t at = editingPointIndex;
  for (uint8_t i = at; i < newProgram.pointCount - 1; i++) {
    newProgram.points[i] = newProgram.points[i + 1];
  }
  newProgram.pointCount--;
  if (editingPointIndex >= newProgram.pointCount) editingPointIndex = newProgram.pointCount - 1;
  Serial.printf("[Editor] Punto %d eliminato\n", at + 1);
}

// Regola punto: T passo 5°C (0..1200), t passo 5 min,
// tempo sempre crescente: t_i in [t_prev+5m, t_next-5m] (t_prev=0 per P1).
void adjustPoint(uint8_t idx, int dir) {
  Point &p = newProgram.points[idx];
  if (editingTempField) {
    p.temp = constrain(p.temp + dir * 5.0f, 0.0f, FIXED_Y_MAX);
    return;
  }
  uint32_t prevCum = (idx == 0) ? 0 : newProgram.points[idx - 1].cumSec;
  uint32_t nextCum = (idx == newProgram.pointCount - 1)
                        ? MAX_TOTAL_SEC : newProgram.points[idx + 1].cumSec;
  long nv = (long)p.cumSec + (long)dir * (long)MIN_SEGMENT_SEC;
  long tMinv = (long)prevCum + (long)MIN_SEGMENT_SEC;
  long tMaxv = (long)nextCum - (long)MIN_SEGMENT_SEC;
  if (nv < tMinv) nv = tMinv;
  if (nv > tMaxv) nv = tMaxv;
  p.cumSec = (uint32_t)nv;
}