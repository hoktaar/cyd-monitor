// Minimaler QR-Code-Generator: Version 3 (29x29), Fehlerkorrektur M, Byte-Modus, Maske 0.
// Reicht fuer URLs bis 42 Zeichen. Algorithmus nach Nayuki (QR Code generator library).
#pragma once

namespace qr {

constexpr int SIZE = 29;
constexpr int DATA_CW = 44, EC_CW = 26;

bool modules[SIZE][SIZE];
bool isFunction[SIZE][SIZE];

uint8_t gfMul(uint8_t x, uint8_t y) {
  int z = 0;
  for (int i = 7; i >= 0; --i) {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((y >> i) & 1) * x;
  }
  return z;
}

void setFunction(int x, int y, bool dark) {
  modules[y][x] = dark;
  isFunction[y][x] = true;
}

void drawFinder(int cx, int cy) {
  for (int dy = -4; dy <= 4; ++dy)
    for (int dx = -4; dx <= 4; ++dx) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) continue;
      int d = max(abs(dx), abs(dy));
      setFunction(x, y, d != 2 && d != 4);
    }
}

void drawFormat(int mask) {
  int data = (0 << 3) | mask;  // Fehlerkorrektur M = 00
  int rem = data;
  for (int i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
  int bits = ((data << 10) | rem) ^ 0x5412;
  auto formatBit = [&](int i) { return ((bits >> i) & 1) != 0; };  // nicht "bit": Arduino-Makro!
  for (int i = 0; i <= 5; ++i) setFunction(8, i, formatBit(i));
  setFunction(8, 7, formatBit(6));
  setFunction(8, 8, formatBit(7));
  setFunction(7, 8, formatBit(8));
  for (int i = 9; i < 15; ++i) setFunction(14 - i, 8, formatBit(i));
  for (int i = 0; i < 8; ++i) setFunction(SIZE - 1 - i, 8, formatBit(i));
  for (int i = 8; i < 15; ++i) setFunction(8, SIZE - 15 + i, formatBit(i));
  setFunction(8, SIZE - 8, true);
}

bool encode(const char *text) {
  int len = strlen(text);
  if (len > 42) return false;
  memset(modules, 0, sizeof(modules));
  memset(isFunction, 0, sizeof(isFunction));

  // Funktionsmuster
  for (int i = 0; i < SIZE; ++i) {
    setFunction(6, i, i % 2 == 0);
    setFunction(i, 6, i % 2 == 0);
  }
  drawFinder(3, 3);
  drawFinder(SIZE - 4, 3);
  drawFinder(3, SIZE - 4);
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) setFunction(22 + dx, 22 + dy, max(abs(dx), abs(dy)) != 1);
  drawFormat(0);

  // Datenbits
  uint8_t cw[DATA_CW + EC_CW] = {0};
  int bitLen = 0;
  auto put = [&](uint32_t val, int n) {
    for (int i = n - 1; i >= 0; --i) {
      if ((val >> i) & 1) cw[bitLen >> 3] |= 0x80 >> (bitLen & 7);
      bitLen++;
    }
  };
  put(4, 4);
  put(len, 8);
  for (int i = 0; i < len; ++i) put((uint8_t)text[i], 8);
  const int cap = DATA_CW * 8;
  put(0, min(4, cap - bitLen));
  put(0, (8 - bitLen % 8) % 8);
  for (uint8_t pad = 0xEC; bitLen < cap; pad ^= 0xEC ^ 0x11) put(pad, 8);

  // Reed-Solomon-Fehlerkorrektur
  uint8_t div[EC_CW] = {0};
  div[EC_CW - 1] = 1;
  uint8_t root = 1;
  for (int i = 0; i < EC_CW; ++i) {
    for (int j = 0; j < EC_CW; ++j) {
      div[j] = gfMul(div[j], root);
      if (j + 1 < EC_CW) div[j] ^= div[j + 1];
    }
    root = gfMul(root, 0x02);
  }
  uint8_t *ec = cw + DATA_CW;
  for (int i = 0; i < DATA_CW; ++i) {
    uint8_t factor = cw[i] ^ ec[0];
    memmove(ec, ec + 1, EC_CW - 1);
    ec[EC_CW - 1] = 0;
    for (int j = 0; j < EC_CW; ++j) ec[j] ^= gfMul(div[j], factor);
  }

  // Codewoerter im Zickzack platzieren
  int i = 0;
  const int total = (DATA_CW + EC_CW) * 8;
  for (int right = SIZE - 1; right >= 1; right -= 2) {
    if (right == 6) right = 5;
    for (int vert = 0; vert < SIZE; ++vert)
      for (int j = 0; j < 2; ++j) {
        int x = right - j;
        bool upward = ((right + 1) & 2) == 0;
        int y = upward ? SIZE - 1 - vert : vert;
        if (!isFunction[y][x] && i < total) {
          modules[y][x] = (cw[i >> 3] >> (7 - (i & 7))) & 1;
          i++;
        }
      }
  }

  // Maske 0
  for (int y = 0; y < SIZE; ++y)
    for (int x = 0; x < SIZE; ++x)
      if (!isFunction[y][x] && (x + y) % 2 == 0) modules[y][x] = !modules[y][x];
  drawFormat(0);
  return true;
}

// Zeichnet den zuletzt erzeugten Code inkl. 2 Module Ruhezone; Kantenlaenge (SIZE + 4) * scale.
void draw(int x0, int y0, int scale) {
  const int quiet = 2;
  tft.fillRect(x0, y0, (SIZE + 2 * quiet) * scale, (SIZE + 2 * quiet) * scale, 0xFFFF);
  for (int y = 0; y < SIZE; ++y)
    for (int x = 0; x < SIZE; ++x)
      if (modules[y][x]) tft.fillRect(x0 + (x + quiet) * scale, y0 + (y + quiet) * scale, scale, scale, 0x0000);
}

}  // namespace qr
