// ------------------------------------------------
// UI: menu, conferma delete, editor curva, utility
// ------------------------------------------------

// ------------------------------------------------
// MAIN_MENU (campo 05): batteria a sinistra, TEMP GRANDE a destra, righe 24px
// ------------------------------------------------
void drawMainMenu() {
  DinMeter.Display.fillScreen(TFT_BLACK);

  // Batteria a sinistra (size 2), rossa ovunque (campo 05)
  int batt = DinMeter.Power.getBatteryLevel();
  DinMeter.Display.setTextColor(TFT_RED, TFT_BLACK);
  DinMeter.Display.setTextSize(2);
  DinMeter.Display.setCursor(4, 0);
  if (batt >= 0) DinMeter.Display.printf("%d%%", batt);
  else           DinMeter.Display.print("--%");

  // TEMP GRANDE a destra (size 4, intero, right-aligned, a metà altezza)
  String tStr = sensorError ? "--" : (String((int)(currentTemp + 0.5f)) + "C");
  DinMeter.Display.setTextSize(4);
  DinMeter.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  int tw = DinMeter.Display.textWidth(tStr.c_str());
  DinMeter.Display.setCursor(236 - tw, 51);
  DinMeter.Display.print(tStr);

  // Lista programmi, righe 24px, ~4 visibili
  constexpr int lineHeight = 24;
  int screenH = DinMeter.Display.height();
  int centerY = 84;
  int maxIdx  = programCount + 2;
  for (int i = 0; i <= maxIdx; i++) {
    int y = centerY + (i - selectedIndex) * lineHeight;
    if (y < 34 || y > screenH) continue;

    if (i < programCount) {
      bool sel = (i == selectedIndex);
      DinMeter.Display.setTextSize(1);
      DinMeter.Display.setTextColor(TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
      DinMeter.Display.setCursor(10, y);
      DinMeter.Display.print(programs[i].name);
    } else {
      bool sel = (i == selectedIndex);
      DinMeter.Display.setTextSize(1);
      DinMeter.Display.setTextColor(TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
      DinMeter.Display.setCursor(10, y);
      switch (i - programCount) {
        case 0: DinMeter.Display.print("Manuale"); break;
        case 1: DinMeter.Display.print("Aggiungi Programma"); break;
        default: DinMeter.Display.print("Spegni"); break;
      }
    }
  }
}

void updateMainMenuTemp() {
  if (millis() - lastTempDraw < 400) return;
  lastTempDraw = millis();
  String tStr = sensorError ? "--" : (String((int)(currentTemp + 0.5f)) + "C");
  DinMeter.Display.setTextSize(4);
  DinMeter.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  int tw = DinMeter.Display.textWidth(tStr.c_str());
  DinMeter.Display.fillRect(236 - tw, 51, 240, 32, TFT_BLACK);
  DinMeter.Display.setCursor(236 - tw, 51);
  DinMeter.Display.print(tStr);
}

// ------------------------------------------------
// CONFIRM_DELETE
// ------------------------------------------------
void handleConfirmDelete() {
  if (needsUpdate) {
    DinMeter.Display.fillScreen(TFT_BLACK);
    DinMeter.Display.setTextColor(TFT_RED);
    DinMeter.Display.setTextFont(1);
    DinMeter.Display.setTextSize(1);

    DinMeter.Display.setCursor(10, 10);
    if (deleteIndex >= 0 && deleteIndex < programCount) {
      DinMeter.Display.println("Cancellare il Programma:");
      DinMeter.Display.setCursor(10, 30);
      DinMeter.Display.print("'");
      DinMeter.Display.print(programs[deleteIndex].name);
      DinMeter.Display.println("'");
    } else {
      DinMeter.Display.println("Cancellare ???");
    }
    DinMeter.Display.setCursor(10, 60);
    DinMeter.Display.println("Short => Conferma");
    DinMeter.Display.setCursor(70, 72);
    DinMeter.Display.println("Long => Annulla");
    needsUpdate = false;
  }
}

void deleteProgram(int idx) {
  if (idx < 0 || idx >= programCount) return;
  for (int i = idx; i < programCount - 1; i++) {
    programs[i] = programs[i + 1];
  }
  programCount--;
  savePrograms();
  currentState = MAIN_MENU;
  needsUpdate = true;
  Serial.printf("[deleteProgram] Program index=%d eliminato\n", idx);
}

// ------------------------------------------------
// SLOW_RISE (campo 07): salita lenta, scelta dell'utente
// ------------------------------------------------
void drawSlowRise() {
  DinMeter.Display.fillScreen(TFT_BLACK);
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  DinMeter.Display.setCursor(10, 8);
  DinMeter.Display.println("SALITA LENTA");

  DinMeter.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  DinMeter.Display.setCursor(10, 24);
  DinMeter.Display.printf("T e %.0fC sotto la curva", currentTemp - targetTemp);

  const char* opts[3] = {"Attendi arrivo", "Allunga 30 min", "Annulla cottura"};
  for (int i = 0; i < 3; i++) {
    int y = 46 + i * 26;
    bool sel = (slowRiseSel == (uint8_t)i);
    DinMeter.Display.fillRect(10, y, 220, 22, sel ? TFT_BLUE : TFT_BLACK);
    DinMeter.Display.setCursor(16, y + 5);
    DinMeter.Display.setTextColor(TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
    DinMeter.Display.print(opts[i]);
  }
}

// ------------------------------------------------
// SOVRATEMP (campo 07): allarme, si esce solo a mano
// ------------------------------------------------
void drawOverTemp() {
  DinMeter.Display.fillScreen(TFT_RED);
  DinMeter.Display.setTextColor(TFT_BLACK, TFT_RED);
  DinMeter.Display.setCursor(30, 40);
  DinMeter.Display.setTextSize(3);
  DinMeter.Display.print("SOVRATEMP!");
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.setCursor(30, 88);
  DinMeter.Display.print("SSR spento.");
  DinMeter.Display.setCursor(30, 100);
  DinMeter.Display.print("Long => esci");
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
  DinMeter.Display.fillScreen(TFT_BLACK);
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.setTextColor(TFT_WHITE);
  DinMeter.Display.setCursor(10, 10);
  DinMeter.Display.println("Inserisci Nome Programma:");

  DinMeter.Display.setTextColor(TFT_GREEN);
  DinMeter.Display.setCursor(10, 30);
  DinMeter.Display.println("Nome attuale:");
  DinMeter.Display.setCursor(10, 42);
  DinMeter.Display.println(inputName);

  int alphaLen = strlen(ALPHABET);
  char c = ALPHABET[currentCharIndex % alphaLen];

  DinMeter.Display.setTextColor(TFT_WHITE);
  DinMeter.Display.setCursor(10, 60);
  DinMeter.Display.print("Caratt. attuale: ");
  DinMeter.Display.println(String(c));

  DinMeter.Display.setTextColor(TFT_GREEN);
  DinMeter.Display.setCursor(10, 80);
  DinMeter.Display.println("Short => Aggiunge il carattere");
  DinMeter.Display.setCursor(10, 92);
  DinMeter.Display.println("Long  => Conferma il nome");
}

// ------------------------------------------------
// ADD_PROG_EDIT (editor ibrido: grafico + barra comandi)
// ------------------------------------------------
// Barra comandi = 8 celle: < T t > +P -P + Salva/Annulla
// Ruota barra per muovere la selezione; T/t = modifica valore (encoder ±, short conferma).
void drawAddProgEdit() {
  DinMeter.Display.fillScreen(TFT_BLACK);
  DinMeter.Display.setTextSize(1);
  DinMeter.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  DinMeter.Display.setCursor(4, 2);
  DinMeter.Display.printf("Editor: %s  (%d pt)", newProgram.name.c_str(), newProgram.pointCount);

  // Grafico: asse Y 0..1200, asse X 0..durata totale
  constexpr int gx = 30, gy = 12, gw = 206, gh = 56;
  uint32_t total = (newProgram.pointCount > 0) ? newProgram.points[newProgram.pointCount - 1].cumSec : 1;
  if (total == 0) total = 1;

  auto xOf = [&](uint32_t s) -> int { return gx + (int)((long)gw * (long)s / (long)total); };
  auto yOf = [&](float t) -> int {
    int y = gy + gh - (int)(gh * t / FIXED_Y_MAX);
    if (y < gy) y = gy;
    if (y > gy + gh) y = gy + gh;
    return y;
  };

  // assi e tacche Y
  DinMeter.Display.drawLine(gx - 1, gy, gx - 1, gy + gh, TFT_WHITE);
  DinMeter.Display.drawLine(gx - 1, gy + gh, gx + gw, gy + gh, TFT_WHITE);
  for (int i = 0; i <= 4; i++) {
    float val = FIXED_Y_MAX * i / 4;
    int yy = yOf(val);
    DinMeter.Display.drawLine(gx - 3, yy, gx - 1, yy, TFT_WHITE);
  }

  // P0 grigio: (0, startTemp)
  int p0x = xOf(0);
  int p0y = yOf(startTemp);
  DinMeter.Display.fillCircle(p0x, p0y, 2, TFT_DARKGREY);

  // spezzata della curva
  int prevX = p0x, prevY = p0y;
  for (uint8_t i = 0; i < newProgram.pointCount; i++) {
    int cx = xOf(newProgram.points[i].cumSec);
    int cy = yOf(newProgram.points[i].temp);
    uint16_t col = (i == editingPointIndex) ? TFT_BLUE : TFT_CYAN;
    DinMeter.Display.drawLine(prevX, prevY, cx, cy, col);
    DinMeter.Display.fillCircle(cx, cy, (i == editingPointIndex) ? 3 : 2, col);
    prevX = cx;
    prevY = cy;
  }

  // riga valore del punto selezionato: evidenzia il campo attivo (T o t)
  Point &p = newProgram.points[editingPointIndex];
  String tLabel = shortTimeLabel(p.cumSec);
  DinMeter.Display.setCursor(3, 72);
  DinMeter.Display.setTextColor(editingTempField ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
  DinMeter.Display.print(String(editingPointIndex + 1) + ": T=");
  DinMeter.Display.print((int)p.temp);
  DinMeter.Display.setTextColor(editingTempField ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  DinMeter.Display.print("C  t=");
  DinMeter.Display.print(tLabel);
  DinMeter.Display.print(fieldEditMode ? " (RUOTA)" : "");

  // barra comandi riga 1: <  T  t  >  +P  -P
  // (solo caratteri ASCII: le frecce Unicode ◀▶ non esistono nel font 6x8)
  const char* labels1[6] = {"<", "T", "t", ">", "+P", "-P"};
  for (int i = 0; i < 6; i++) {
    int bx = i * 40;
    bool sel = (editorSel == i);
    uint16_t fill = sel ? TFT_BLUE : TFT_DARKGREY;
    DinMeter.Display.fillRect(bx, 90, 40, 20, fill);
    DinMeter.Display.drawRect(bx, 90, 40, 20, TFT_WHITE);
    DinMeter.Display.setCursor(bx + 4, 94);
    DinMeter.Display.setTextColor(TFT_WHITE, fill);
    DinMeter.Display.print(labels1[i]);
  }
  // riga 2: [Salva][Annulla]
  const char* labels2[2] = {"Salva", "Annulla"};
  for (int i = 0; i < 2; i++) {
    int bx = i * 120;
    bool sel = (editorSel == 6 + i);
    DinMeter.Display.fillRect(bx, 115, 118, 18, sel ? TFT_BLUE : TFT_DARKGREY);
    DinMeter.Display.drawRect(bx, 115, 118, 18, TFT_WHITE);
    DinMeter.Display.setCursor(bx + 40, 118);
    DinMeter.Display.setTextColor(TFT_WHITE, sel ? TFT_BLUE : TFT_DARKGREY);
    DinMeter.Display.print(labels2[i]);
  }
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
    if (programCount >= MAX_PROGRAMS) {
      Serial.println("[Editor] Memoria piena, salvataggio non riuscito");
      needsUpdate = true;
      return;
    }
    programs[programCount++] = newProgram;
    savePrograms();
    Serial.println("[Editor] Program salvato con successo");
    currentState = MAIN_MENU;
  } else if (sel == 7) {               // Annulla
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