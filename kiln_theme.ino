// ------------------------------------------------
// UI helpers: palette, status bar, cards, pills
// ------------------------------------------------

void uiBattery(int x, int y) {
  auto& d = DinMeter.Display;
  int pct = battPct;
  uint16_t col = (pct < 0) ? COL_MUTED
               : (pct > 60) ? COL_GREEN
               : (pct > 25) ? COL_AMBER
                            : COL_RED;
  d.drawRoundRect(x, y, 13, 8, 2, COL_MUTED);
  d.fillRect(x + 13, y + 2, 2, 4, COL_MUTED);
  if (pct > 0) {
    int w = (pct * 9) / 100;
    if (w < 1) w = 1;
    d.fillRect(x + 2, y + 2, w, 4, col);
  }
  d.setTextSize(1);
  d.setTextColor(col);
  d.setCursor(x + 19, y);
  if (pct >= 0) d.printf("%d%%", pct);
  else          d.print("--");
}

void uiTopBar(const char* title) {
  auto& d = DinMeter.Display;
  d.fillRect(0, 0, 240, 17, COL_SURFACE);
  d.setTextSize(1);
  d.setTextColor(COL_CYAN);
  d.setCursor(6, 4);
  d.print(title);
  uiBattery(192, 4);
  d.drawFastHLine(0, 17, 240, COL_LINE);
}

void uiButton(int x, int y, int w, int h, const char* label, bool sel) {
  auto& d = DinMeter.Display;
  d.fillRoundRect(x, y, w, h, 4, sel ? COL_CYAN : COL_SURFACE);
  if (!sel) d.drawRoundRect(x, y, w, h, 4, COL_LINE);
  d.setTextSize(1);
  d.setTextColor(sel ? COL_BG : COL_TEXT);
  int tw = d.textWidth(label);
  d.setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
  d.print(label);
}

void uiPill(int x, int y, int w, int h, const char* label, uint16_t fill, uint16_t textCol) {
  auto& d = DinMeter.Display;
  d.fillRoundRect(x, y, w, h, 4, fill);
  d.setTextSize(1);
  d.setTextColor(textCol);
  int tw = d.textWidth(label);
  d.setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
  d.print(label);
}

void uiTruncate(const char* src, char* dst, size_t maxChars) {
  size_t len = strlen(src);
  if (len <= maxChars) {
    strcpy(dst, src);
    return;
  }
  if (maxChars < 3) {
    dst[0] = '\0';
    return;
  }
  memcpy(dst, src, maxChars - 2);
  dst[maxChars - 2] = '.';
  dst[maxChars - 1] = '.';
  dst[maxChars] = '\0';
}
