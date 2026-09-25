// Animierte Wettersymbole. Gezeichnet wird in einen Puffer (GFXcanvas16), Koordinaten fuer 100x100,
// skaliert auf die gewuenschte Groesse. phase = Zeit in Sekunden.
#pragma once

namespace wx {

enum Kind { CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, THUNDER };

// WMO-Wettercodes (Open-Meteo)
Kind kindOf(int code) {
  if (code == 0) return CLEAR;
  if (code <= 2) return PARTLY;
  if (code == 3) return CLOUDY;
  if (code == 45 || code == 48) return FOG;
  if (code >= 51 && code <= 57) return DRIZZLE;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SNOW;
  if (code >= 95) return THUNDER;
  return CLOUDY;
}

const char *describe(int code) {
  switch (code) {
    case 0: return "Klar";
    case 1: return "Überwiegend klar";
    case 2: return "Teilweise bewölkt";
    case 3: return "Bedeckt";
    case 45: case 48: return "Nebel";
    case 51: case 53: case 55: return "Nieselregen";
    case 56: case 57: return "Gefrierender Niesel";
    case 61: return "Leichter Regen";
    case 63: return "Regen";
    case 65: return "Starker Regen";
    case 66: case 67: return "Gefrierender Regen";
    case 71: return "Leichter Schneefall";
    case 73: return "Schneefall";
    case 75: return "Starker Schneefall";
    case 77: return "Schneegriesel";
    case 80: case 81: return "Regenschauer";
    case 82: return "Heftige Schauer";
    case 85: case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96: case 99: return "Gewitter mit Hagel";
    default: return "Unbekannt";
  }
}

constexpr uint16_t SUN = ui::rgb(255, 196, 40);
constexpr uint16_t MOON = ui::rgb(235, 228, 200);
constexpr uint16_t DROP = ui::rgb(80, 160, 255);
constexpr uint16_t BOLT = ui::rgb(255, 220, 60);

struct Painter {
  Adafruit_GFX &g;
  float k;  // Skalierung (Groesse / 100)
  int X(float v) const { return (int)(v * k + 0.5f); }

  void thickLine(float x1, float y1, float x2, float y2, float w, uint16_t c) {
    float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float nx = -dy / len * w / 2, ny = dx / len * w / 2;
    g.fillTriangle(X(x1 + nx), X(y1 + ny), X(x2 + nx), X(y2 + ny), X(x2 - nx), X(y2 - ny), c);
    g.fillTriangle(X(x1 + nx), X(y1 + ny), X(x2 - nx), X(y2 - ny), X(x1 - nx), X(y1 - ny), c);
  }

  void sun(float cx, float cy, float r, float phase) {
    for (int i = 0; i < 8; ++i) {
      float a = phase * 0.6f + i * PI / 4;
      float r1 = r * 1.35f, r2 = r * (1.7f + 0.1f * sinf(phase * 3 + i));
      thickLine(cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * r2, cy + sinf(a) * r2, r * 0.22f, SUN);
    }
    g.fillCircle(X(cx), X(cy), X(r), SUN);
  }

  void moon(float cx, float cy, float r, float phase) {
    static const float STARS[4][2] = {{18, 22}, {80, 18}, {86, 60}, {24, 78}};
    for (int i = 0; i < 4; ++i) {
      float tw = 0.5f + 0.5f * sinf(phase * 2 + i * 1.7f);
      int s = max(1, X(1.5f + tw * 1.5f));
      g.fillRect(X(STARS[i][0]) - s / 2, X(STARS[i][1]) - s / 2, s, s, MOON);
    }
    g.fillCircle(X(cx), X(cy), X(r), MOON);
    g.fillCircle(X(cx + r * 0.45f), X(cy - r * 0.3f), X(r * 0.85f), ui::BG);
  }

  void cloud(float cx, float cy, float s, uint16_t c) {
    g.fillCircle(X(cx - s * 0.5f), X(cy + s * 0.1f), X(s * 0.42f), c);
    g.fillCircle(X(cx + s * 0.05f), X(cy - s * 0.18f), X(s * 0.58f), c);
    g.fillCircle(X(cx + s * 0.6f), X(cy + s * 0.12f), X(s * 0.38f), c);
    g.fillRoundRect(X(cx - s * 0.92f), X(cy + s * 0.08f), X(s * 1.9f), X(s * 0.44f), X(s * 0.22f), c);
  }

  uint16_t cloudFront() { return ui::DARK ? ui::rgb(215, 220, 230) : ui::rgb(150, 158, 172); }
  uint16_t cloudBack() { return ui::DARK ? ui::rgb(120, 128, 146) : ui::rgb(196, 200, 210); }

  void scene(Kind kind, bool day, float phase) {
    float drift = sinf(phase * 0.8f) * 4;
    switch (kind) {
      case CLEAR:
        if (day) sun(50, 50, 20, phase);
        else moon(50, 50, 24, phase);
        break;
      case PARTLY:
        if (day) sun(36, 36, 14, phase);
        else moon(38, 36, 16, phase);
        cloud(56 + drift, 62, 26, cloudFront());
        break;
      case CLOUDY:
        cloud(40 + drift, 42, 22, cloudBack());
        cloud(56 - drift, 62, 28, cloudFront());
        break;
      case FOG:
        cloud(50, 36, 26, cloudBack());
        for (int i = 0; i < 3; ++i) {
          float off = sinf(phase * 1.2f + i * 1.3f) * 8;
          g.fillRoundRect(X(18 + off), X(64 + i * 11), X(64), X(6), X(3), cloudFront());
        }
        break;
      case DRIZZLE:
      case RAIN:
      case THUNDER: {
        int drops = kind == DRIZZLE ? 4 : 7;
        float speed = kind == DRIZZLE ? 30 : 55;
        for (int i = 0; i < drops; ++i) {
          float x = 26 + i * (48.0f / drops) + (i % 2) * 4;
          float y = 58 + fmodf(phase * speed + i * 13, 36);
          thickLine(x, y, x - 3, y + (kind == DRIZZLE ? 5 : 9), 2.6f, DROP);
        }
        cloud(50, 38, 28, kind == THUNDER ? cloudBack() : cloudFront());
        if (kind == THUNDER && fmodf(phase, 2.6f) < 0.35f) {
          g.fillTriangle(X(52), X(50), X(40), X(74), X(50), X(72), BOLT);
          g.fillTriangle(X(50), X(68), X(58), X(68), X(44), X(94), BOLT);
        }
        break;
      }
      case SNOW:
        for (int i = 0; i < 6; ++i) {
          float x = 28 + i * 9 + sinf(phase * 1.5f + i) * 4;
          float y = 58 + fmodf(phase * 14 + i * 11, 38);
          g.fillCircle(X(x), X(y), max(1, X(2.6f)), ui::DARK ? ui::rgb(235, 240, 255) : ui::rgb(120, 150, 200));
        }
        cloud(50, 38, 28, cloudFront());
        break;
    }
  }
};

// Zeichnet ein Symbol der Kantenlaenge size an (x, y) aufs Display.
void draw(GFXcanvas16 &canvas, int x, int y, int size, Kind kind, bool day, float phase) {
  canvas.fillScreen(ui::BG);
  Painter p{canvas, size / 100.0f};
  p.scene(kind, day, phase);
  tft.drawRGBBitmap(x, y, canvas.getBuffer(), size, size);
}

}  // namespace wx
