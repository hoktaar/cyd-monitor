// Farben, Schriften und Zeichenhelfer, die alle Module gemeinsam nutzen.
#pragma once
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

namespace ui {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Themenfarben (per applyTheme umschaltbar)
uint16_t BG, PANEL, TEXT, MUTED, DIM;
bool DARK = true;

void applyTheme(bool dark) {
  DARK = dark;
  if (dark) {
    BG = rgb(10, 12, 18);
    PANEL = rgb(30, 34, 46);
    TEXT = rgb(235, 238, 245);
    MUTED = rgb(130, 138, 155);
    DIM = rgb(26, 30, 40);
  } else {
    BG = rgb(244, 243, 239);
    PANEL = rgb(222, 224, 230);
    TEXT = rgb(22, 24, 30);
    MUTED = rgb(100, 106, 120);
    DIM = rgb(218, 220, 226);
  }
}

constexpr uint16_t ACCENT = rgb(217, 119, 87);  // Claude-Orange
constexpr uint16_t OK = rgb(80, 200, 120);
constexpr uint16_t WARN = rgb(240, 190, 60);
constexpr uint16_t BAD = rgb(235, 80, 70);

constexpr int W = 320, H = 240;
constexpr int HEADER_H = 28;

enum Align { LEFT, CENTER, RIGHT };

// Die GFX-Schriften kennen nur ASCII: Umlaute werden als Grundbuchstabe + zwei Punkte gezeichnet.
String fold(const char *s) {
  String out;
  for (const uint8_t *p = (const uint8_t *)s; *p; ++p) {
    if (*p == 0xC3 && p[1]) {
      switch (*++p) {
        case 0xA4: out += 'a'; break;
        case 0xB6: out += 'o'; break;
        case 0xBC: out += 'u'; break;
        case 0x84: out += 'A'; break;
        case 0x96: out += 'O'; break;
        case 0x9C: out += 'U'; break;
        case 0x9F: out += "ss"; break;
        default: out += '?';
      }
    } else if (*p == 0xC2 && p[1]) {
      out += *++p == 0xB0 ? "o" : ".";  // Gradzeichen / Mittelpunkt (nur fuer die Breitenmessung)
    } else if (*p < 0x80) {
      out += (char)*p;
    }
  }
  return out;
}

void printDE(Adafruit_GFX &g, int x, int y, const char *s, uint16_t color) {
  g.setCursor(x, y);
  for (const uint8_t *p = (const uint8_t *)s; *p; ++p) {
    if (*p == 0xC3 && p[1]) {
      char base;
      switch (*++p) {
        case 0xA4: base = 'a'; break;
        case 0xB6: base = 'o'; break;
        case 0xBC: base = 'u'; break;
        case 0x84: base = 'A'; break;
        case 0x96: base = 'O'; break;
        case 0x9C: base = 'U'; break;
        case 0x9F: g.print("ss"); continue;
        default: base = '?';
      }
      char str[2] = {base, 0};
      int16_t bx, by;
      uint16_t bw, bh;
      g.getTextBounds(str, g.getCursorX(), y, &bx, &by, &bw, &bh);
      g.write(base);
      int d = max(2, (int)bh / 7);
      int top = by - d - max(1, d / 2);
      g.fillRect(bx + bw / 4 - d / 2, top, d, d, color);
      g.fillRect(bx + (3 * bw) / 4 - d / 2, top, d, d, color);
    } else if (*p == 0xC2 && p[1]) {
      int16_t bx, by;
      uint16_t bw, bh;
      int cx = g.getCursorX();
      if (*++p == 0xB0) {  // Gradzeichen als kleiner Ring auf Versalhoehe
        g.getTextBounds("0", cx, y, &bx, &by, &bw, &bh);
        int r = max(2, (int)bh / 5), t = max(1, r / 2);
        for (int k = 0; k < t; ++k) g.drawCircle(cx + r + 1, by + r, r - k, color);
        g.setCursor(cx + 2 * r + 3, y);
      } else {  // Mittelpunkt
        g.getTextBounds("x", cx, y, &bx, &by, &bw, &bh);
        int d = max(2, (int)bh / 5);
        g.fillRect(cx + d, by + bh / 2 - d / 2, d, d, color);
        g.setCursor(cx + 3 * d, y);
      }
    } else if (*p < 0x80) {
      g.write(*p);
    }
  }
}

// Text flackerfrei in eine Box zeichnen (erst in einen 1-Bit-Puffer, dann zeilenweise aufs Display).
void textBox(int x, int y, int w, int h, int baseline, const char *s, const GFXfont *font,
             uint16_t fg, uint16_t bg, Align align = LEFT) {
  GFXcanvas1 canvas(w, h);
  uint8_t *buf = canvas.getBuffer();
  if (!buf) {
    tft.fillRect(x, y, w, h, bg);
    return;
  }
  canvas.setFont(font);
  canvas.setTextWrap(false);
  canvas.setTextColor(1);
  int16_t bx, by;
  uint16_t bw, bh;
  canvas.getTextBounds(fold(s), 0, baseline, &bx, &by, &bw, &bh);
  int cx = align == LEFT ? 0 : align == CENTER ? (w - (int)bw) / 2 - bx : w - (int)bw - bx;
  printDE(canvas, cx, baseline, s, 1);

  static uint16_t line[W];
  int stride = (w + 7) / 8;
  tft.startWrite();
  for (int row = 0; row < h; ++row) {
    const uint8_t *src = buf + row * stride;
    for (int col = 0; col < w; ++col) line[col] = (src[col >> 3] & (0x80 >> (col & 7))) ? fg : bg;
    tft.setAddrWindow(x, y + row, w, 1);
    tft.writePixels(line, w);
  }
  tft.endWrite();
}

uint16_t levelColor(int percent) {
  if (percent >= 90) return BAD;
  if (percent >= 70) return WARN;
  return ACCENT;
}

void bar(int x, int y, int w, int h, int percent, uint16_t color) {
  percent = constrain(percent, 0, 100);
  int fill = (w * percent) / 100;
  int r = h / 2;
  tft.fillRoundRect(x, y, w, h, r, PANEL);
  if (fill > 0) tft.fillRoundRect(x, y, max(fill, h), h, r, color);
}

void header(const char *title, int page, int pages) {
  tft.fillRect(0, 0, W, HEADER_H, PANEL);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(TEXT);
  printDE(tft, 10, 19, title, TEXT);
  for (int i = 0; i < pages; ++i)
    tft.fillCircle(W - 30 - (pages - 1 - i) * 12, HEADER_H / 2, 3, i == page ? TEXT : MUTED);
}

void connectionDot(bool connected) {
  tft.fillCircle(W - 12, HEADER_H / 2, 4, connected ? OK : MUTED);
}

// Siebensegment-Ziffer; digit -1 zeichnet einen Strich.
void sevenSeg(int x, int y, int w, int h, int t, int digit, uint16_t on, uint16_t off) {
  static const uint8_t MAP[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  uint8_t bits = digit < 0 ? 0x40 : MAP[digit % 10];
  int hx = x + t + 1, hw = w - 2 * t - 2;
  int vh = (h - 3 * t) / 2 - 2;
  int r = t / 2;
  auto seg = [&](int bit, int sx, int sy, int sw, int sh) {
    tft.fillRoundRect(sx, sy, sw, sh, r, (bits >> bit) & 1 ? on : off);
  };
  seg(0, hx, y, hw, t);                              // a
  seg(1, x + w - t, y + t + 1, t, vh);               // b
  seg(2, x + w - t, y + (h + t) / 2 + 1, t, vh);     // c
  seg(3, hx, y + h - t, hw, t);                      // d
  seg(4, x, y + (h + t) / 2 + 1, t, vh);             // e
  seg(5, x, y + t + 1, t, vh);                       // f
  seg(6, hx, y + (h - t) / 2, hw, t);                // g
}

int textWidth(const String &s, const GFXfont *font) {
  static GFXcanvas1 probe(8, 8);  // nur zum Messen
  probe.setFont(font);
  int16_t bx, by;
  uint16_t bw, bh;
  probe.getTextBounds(fold(s.c_str()), 0, 30, &bx, &by, &bw, &bh);
  return bw;
}

// Bricht Text an Wortgrenzen auf maxLines Zeilen um; die letzte Zeile wird bei Bedarf mit "..." gekuerzt.
int wrap(const String &text, const GFXfont *font, int width, String *lines, int maxLines) {
  int n = 0, start = 0;
  String cur;
  bool truncated = false;
  while (start <= (int)text.length()) {
    int sp = text.indexOf(' ', start);
    if (sp < 0) sp = text.length();
    String word = text.substring(start, sp);
    start = sp + 1;
    if (!word.length()) continue;
    String cand = cur.length() ? cur + " " + word : word;
    if (!cur.length() || textWidth(cand, font) <= width) {
      cur = cand;
      continue;
    }
    if (n == maxLines - 1) {
      truncated = true;
      break;
    }
    lines[n++] = cur;
    cur = word;
  }
  if (cur.length()) {
    if (truncated || textWidth(cur, font) > width) {
      while (cur.length() > 1 && textWidth(cur + "...", font) > width) cur.remove(cur.length() - 1);
      cur += "...";
    }
    lines[n++] = cur;
  }
  return n;
}

// Zahl mit deutschem Dezimalkomma
String fmt(double v, int decimals = 1) {
  String s = String(v, decimals);
  s.replace('.', ',');
  return s;
}

// Bytes -> "320 GB" / "4,2 TB" (dezimal wie in der Unraid-Oberflaeche)
String fmtBytes(double bytes) {
  double gb = bytes / 1e9;
  if (gb >= 1000) return fmt(gb / 1000, 1) + " TB";
  return fmt(gb, gb < 10 ? 1 : 0) + " GB";
}

}  // namespace ui
