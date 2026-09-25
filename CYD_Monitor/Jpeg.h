// JPEG aus dem Speicher direkt aufs Display zeichnen - mit dem TJpgDec-Decoder im ROM des ESP32.
#pragma once
#include "rom/tjpgd.h"

namespace jpeg {

struct Job {
  const uint8_t *data;
  size_t len, pos;
  int x, y, maxW, maxH;
};

UINT input(JDEC *jd, BYTE *buf, UINT n) {
  Job *job = (Job *)jd->device;
  n = min((size_t)n, job->len - job->pos);
  if (buf) memcpy(buf, job->data + job->pos, n);
  job->pos += n;
  return n;
}

UINT output(JDEC *jd, void *bitmap, JRECT *r) {
  Job *job = (Job *)jd->device;
  const BYTE *src = (const BYTE *)bitmap;
  int w = r->right - r->left + 1, h = r->bottom - r->top + 1;
  int vw = min(w, job->maxW - (int)r->left), vh = min(h, job->maxH - (int)r->top);  // auf die Box zuschneiden
  if (vw <= 0 || vh <= 0) return 1;
  static uint16_t px[16 * 16];
  for (int row = 0; row < vh; ++row)
    for (int col = 0; col < vw; ++col) {
      const BYTE *p = src + (row * w + col) * 3;
      px[row * vw + col] = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
    }
  tft.drawRGBBitmap(job->x + r->left, job->y + r->top, px, vw, vh);
  return 1;
}

// Zeichnet das Bild bei (x, y), verkleinert (1/2, 1/4, 1/8) bis es in maxW x maxH passt.
bool draw(const uint8_t *data, size_t len, int x, int y, int maxW, int maxH) {
  static uint8_t work[3100];
  JDEC jd;
  Job job{data, len, 0, x, y, maxW, maxH};
  if (jd_prepare(&jd, input, work, sizeof(work), &job) != JDR_OK) return false;
  uint8_t scale = 0;
  while (scale < 3 && ((int)(jd.width >> scale) > maxW || (int)(jd.height >> scale) > maxH)) scale++;
  // Bild in der Box zentrieren
  job.x += (maxW - (int)(jd.width >> scale)) / 2 > 0 ? (maxW - (int)(jd.width >> scale)) / 2 : 0;
  job.y += (maxH - (int)(jd.height >> scale)) / 2 > 0 ? (maxH - (int)(jd.height >> scale)) / 2 : 0;
  return jd_decomp(&jd, output, scale) == JDR_OK;
}

}  // namespace jpeg
