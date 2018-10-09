/*
* Copyright (c) 2017 Muhammad Faiz <mfcc64@gmail.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

/* Audio visualization based on showcqtbar mpv/ffmpeg audio visualization */
/* See https://github.com/FFmpeg/FFmpeg/blob/master/libavfilter/avf_showcqt.c */

/*
 * Modified for chip-player-js by Matt Montag, October 6, 2018.
 * from https://github.com/mfcc64/html5-showcqtbar
 *
 * Notes:
 * - removed all instances of C99 'restrict' keyword
 * - added extern "C" { ... }
 * - added 'cqt_' prefix to public functions
 *
 */

#include <math.h>
#include <stdint.h>

#define MAX_FFT_SIZE 32768
#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define MAX_KERNEL_SIZE 200000
#define MIN_VOL 1.0f
#define MAX_VOL 100.0f

typedef struct Complex {
  float re, im;
} Complex;

typedef struct Color {
  uint8_t r, g, b, a;
} Color;

typedef struct ColorF {
  float r, g, b, h;
} ColorF;

typedef union Kernel {
  float f;
  int i;
} Kernel;

typedef struct ShowCQT {
  /* args */
  // stereo input signal
  float input[2][MAX_FFT_SIZE];
  float out[2][MAX_WIDTH];
  Color output[MAX_WIDTH];

  /* tables */
  Complex exp_tbl[MAX_FFT_SIZE + MAX_FFT_SIZE / 4];
  int16_t reversed_bit_tbl[MAX_FFT_SIZE / 4];
  float attack_tbl[MAX_FFT_SIZE / 8]; // windowing table?
  uint8_t padding[1024];

  /* buffers */
  Complex fft_buf[MAX_FFT_SIZE];
  ColorF color_buf[MAX_WIDTH * 2];
  float rcp_h_buf[MAX_WIDTH];

  /* props */
  int width;
  int height;
  int fft_size;
  int t_size;
  int attack_size; // num samples in 1/30th of a second
  float sono_v;
  float bar_v;

  /* kernel */
  Kernel kernel[MAX_KERNEL_SIZE];
} ShowCQT;

static __attribute__((__aligned__(128))) ShowCQT cqt;

#ifdef __cplusplus
extern "C" {
#endif

float *cqt_get_input_array(int index) {
  return cqt.input[!!index];
}

Color *cqt_get_output_array(void) {
  return cqt.output;
}

// See https://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
static unsigned reverse_bits(unsigned x, int bits) {
  unsigned m = 0x55555555;
  x = ((x & m) << 1) | ((x & ~m) >> 1);
  m = 0x33333333;
  x = ((x & m) << 2) | ((x & ~m) >> 2);
  m = 0x0F0F0F0F;
  x = ((x & m) << 4) | ((x & ~m) >> 4);
  m = 0x00FF00FF;
  x = ((x & m) << 8) | ((x & ~m) >> 8);
  m = 0x0000FFFF;
  x = ((x & m) << 16) | ((x & ~m) >> 16);
  return (x >> (32 - bits)) & ((1 << bits) - 1);
}

static void gen_reversed_bits_tbl(int bits) {
  int n = 1 << bits;
  for (int x = 0; x < n; x++)
    cqt.reversed_bit_tbl[x] = reverse_bits(x, bits);
}

static void gen_exp_tbl(int n) {
  double mul;
  for (int k = 2; k < n; k *= 2) {
    mul = 2.0 * M_PI / k;
    for (int x = 0; x < k / 2; x++)
      cqt.exp_tbl[k + x] = (Complex) {cos(mul * x), -sin(mul * x)};
    mul = 3.0 * M_PI / k;
    for (int x = 0; x < k / 2; x++)
      cqt.exp_tbl[k + k / 2 + x] = (Complex) {cos(mul * x), -sin(mul * x)};
  }
  mul = 2.0 * M_PI / n;
  for (int x = 0; x < n / 4; x++)
    cqt.exp_tbl[n + x] = (Complex) {cos(mul * x), -sin(mul * x)};
}

#define C_ADD(a, b) (Complex){ (a).re + (b).re, (a).im + (b).im }
#define C_SUB(a, b) (Complex){ (a).re - (b).re, (a).im - (b).im }
#define C_MUL(a, b) (Complex){ (a).re * (b).re - (a).im * (b).im, (a).re * (b).im + (a).im * (b).re }
#define C_AIM(a, b) (Complex){ (a).re - (b).im, (a).im + (b).re }
#define C_SIM(a, b) (Complex){ (a).re + (b).im, (a).im - (b).re }

#define FFT_CALC_FUNC(n, q)                                                     \
static void fft_calc_ ## n(Complex * v)                                 \
{                                                                               \
    const Complex *e2 = cqt.exp_tbl + 2*q;                             \
    const Complex *e3 = cqt.exp_tbl + 3*q;                             \
    const Complex *e1 = cqt.exp_tbl + 4*q;                             \
    Complex v0, v1, v2, v3;                                                     \
    Complex a02, a13, s02, s13;                                                 \
                                                                                \
    fft_calc_ ## q(v);                                                          \
    fft_calc_ ## q(q+v);                                                        \
    fft_calc_ ## q(2*q+v);                                                      \
    fft_calc_ ## q(3*q+v);                                                      \
                                                                                \
    v0 = v[0];                                                                  \
    v2 = v[q]; /* bit reversed */                                               \
    v1 = v[2*q];                                                                \
    v3 = v[3*q];                                                                \
    a02 = C_ADD(v0, v2);                                                        \
    s02 = C_SUB(v0, v2);                                                        \
    a13 = C_ADD(v1, v3);                                                        \
    s13 = C_SUB(v1, v3);                                                        \
    v[0] = C_ADD(a02, a13);                                                     \
    v[q] = C_SIM(s02, s13);                                                     \
    v[2*q] = C_SUB(a02, a13);                                                   \
    v[3*q] = C_AIM(s02, s13);                                                   \
                                                                                \
    for (int x = 1; x < q; x++) {                                               \
        v0 = v[x];                                                              \
        v2 = C_MUL(e2[x], v[q+x]); /* bit reversed */                           \
        v1 = C_MUL(e1[x], v[2*q+x]);                                            \
        v3 = C_MUL(e3[x], v[3*q+x]);                                            \
        a02 = C_ADD(v0, v2);                                                    \
        s02 = C_SUB(v0, v2);                                                    \
        a13 = C_ADD(v1, v3);                                                    \
        s13 = C_SUB(v1, v3);                                                    \
        v[x] = C_ADD(a02, a13);                                                 \
        v[q+x] = C_SIM(s02, s13);                                               \
        v[2*q+x] = C_SUB(a02, a13);                                             \
        v[3*q+x] = C_AIM(s02, s13);                                             \
    }                                                                           \
}

static inline void fft_calc_1(Complex *v) {}

static inline void fft_calc_2(Complex *v) {
  Complex v0 = v[0], v1 = v[1];
  v[0] = C_ADD(v0, v1);
  v[1] = C_SUB(v0, v1);
}

FFT_CALC_FUNC(4, 1)
FFT_CALC_FUNC(8, 2)
FFT_CALC_FUNC(16, 4)
FFT_CALC_FUNC(32, 8)
FFT_CALC_FUNC(64, 16)
FFT_CALC_FUNC(128, 32)
FFT_CALC_FUNC(256, 64)
FFT_CALC_FUNC(512, 128)
FFT_CALC_FUNC(1024, 256)
FFT_CALC_FUNC(2048, 512)
FFT_CALC_FUNC(4096, 1024)
FFT_CALC_FUNC(8192, 2048)
FFT_CALC_FUNC(16384, 4096)
FFT_CALC_FUNC(32768, 8192)

static void fft_calc(Complex *v, int n) {
  switch (n) {
    case 1024:
      fft_calc_1024(v);
      break;
    case 2048:
      fft_calc_2048(v);
      break;
    case 4096:
      fft_calc_4096(v);
      break;
    case 8192:
      fft_calc_8192(v);
      break;
    case 16384:
      fft_calc_16384(v);
      break;
    case 32768:
      fft_calc_32768(v);
      break;
  }
}

int cqt_init(int rate, int width, int height, float bar_v, float sono_v, int super) {
  if (height <= 0 || height > MAX_HEIGHT || width <= 0 || width > MAX_WIDTH)
    return 0;

  cqt.width = width;
  cqt.height = height;

  cqt.bar_v = (bar_v > MAX_VOL) ? MAX_VOL : (bar_v > MIN_VOL) ? bar_v : MIN_VOL;
  cqt.sono_v = (sono_v > MAX_VOL) ? MAX_VOL : (sono_v > MIN_VOL) ? sono_v : MIN_VOL;

  if (rate < 8000 || rate > 100000)
    return 0;

  int bits = ceil(log(rate * 0.33) / M_LN2);
  if (bits > 20 || bits < 10)
    return 0;
  cqt.fft_size = 1 << bits;
  if (cqt.fft_size > MAX_FFT_SIZE)
    return 0;

  gen_reversed_bits_tbl(bits - 2);
  gen_exp_tbl(cqt.fft_size);

  cqt.attack_size = ceil(rate * 0.033);
  for (int x = 0; x < cqt.attack_size; x++) {
    double y = M_PI * x / (rate * 0.033);
    /* nuttall window */
    cqt.attack_tbl[x] = 0.355768 + 0.487396 * cos(y) + 0.144232 * cos(2 * y) + 0.012604 * cos(3 * y);
  }

  cqt.t_size = cqt.width * (1 + !!super);
  double log_base = log(20.01523126408007475);
  double log_end = log(20495.59681441799654);
  for (int f = 0, idx = 0; f < cqt.t_size; f++) {
    double freq = exp(log_base + (f + 0.5) * (log_end - log_base) * (1.0 / cqt.t_size));

    if (freq >= 0.5 * rate) {
      cqt.kernel[idx].i = 0;
      break;
    }

    double tlen = 384 * 0.33 / (384 / 0.17 + 0.33 * freq / (1 - 0.17)) +
                  384 * 0.33 / (0.33 * freq / 0.17 + 384 / (1 - 0.17));
    double flen = 8.0 * cqt.fft_size / (tlen * rate);
    double center = freq * cqt.fft_size / rate;
    int start = ceil(center - 0.5 * flen);
    int end = floor(center + 0.5 * flen);
    int len = end - start + 1;

    if (idx + len + 1000 > MAX_KERNEL_SIZE)
      return 0;
    cqt.kernel[idx].i = len;
    cqt.kernel[idx + 1].i = start;

    for (int x = start; x <= end; x++) {
      int sign = (x & 1) ? (-1) : 1;
      double y = 2.0 * M_PI * (x - center) * (1.0 / flen);
      /* nuttall window */
      double w = 0.355768 + 0.487396 * cos(y) + 0.144232 * cos(2 * y) + 0.012604 * cos(3 * y);
      w *= sign * (1.0 / cqt.fft_size);
      cqt.kernel[idx + 2 + x - start].f = w;
    }
    idx += len + 2;
  }
  return cqt.fft_size;
}

void cqt_calc2(const float *input_L, const float *input_R) {
  int fft_size_h = cqt.fft_size >> 1;
  int fft_size_q = cqt.fft_size >> 2;
  int shift = fft_size_h - cqt.attack_size;

  for (int x = 0; x < cqt.attack_size; x++) {
    int i = 4 * cqt.reversed_bit_tbl[x];
    cqt.fft_buf[i] = (Complex) {input_L[shift + x], input_R[shift + x]};
    cqt.fft_buf[i + 1].re = cqt.attack_tbl[x] * input_L[fft_size_h + shift + x];
    cqt.fft_buf[i + 1].im = cqt.attack_tbl[x] * input_R[fft_size_h + shift + x];
    cqt.fft_buf[i + 2] = (Complex) {input_L[fft_size_q + shift + x], input_R[fft_size_q + shift + x]};
    cqt.fft_buf[i + 3] = (Complex) {0, 0};
  }

  for (int x = cqt.attack_size; x < fft_size_q; x++) {
    int i = 4 * cqt.reversed_bit_tbl[x];
    cqt.fft_buf[i] = (Complex) {input_L[shift + x], input_R[shift + x]};
    cqt.fft_buf[i + 1] = (Complex) {0, 0};
    cqt.fft_buf[i + 2] = (Complex) {input_L[fft_size_q + shift + x], input_R[fft_size_q + shift + x]};
    cqt.fft_buf[i + 3] = (Complex) {0, 0};
  }

  fft_calc(cqt.fft_buf, cqt.fft_size);

  for (int x = 0, m = 0; x < cqt.t_size; x++) {
    int len = cqt.kernel[m].i;
    int start = cqt.kernel[m + 1].i;
    if (!len) {
      cqt.color_buf[x] = (ColorF) {0, 0, 0, 0};
      continue;
    }
    Complex a = {0, 0}, b = {0, 0};
    for (int y = 0; y < len; y++) {
      int i = start + y;
      int j = cqt.fft_size - i;
      float u = cqt.kernel[m + 2 + y].f;
      a.re += u * cqt.fft_buf[i].re;
      a.im += u * cqt.fft_buf[i].im;
      b.re += u * cqt.fft_buf[j].re;
      b.im += u * cqt.fft_buf[j].im;
    }
    Complex v0 = {a.re + b.re, a.im - b.im};
    Complex v1 = {b.im + a.im, b.re - a.re};
    float r0 = v0.re * v0.re + v0.im * v0.im;
    float r1 = v1.re * v1.re + v1.im * v1.im;

    float c = 255.0f * sqrtf(cqt.sono_v * sqrtf(r0));
    cqt.color_buf[x].r = (c < 255.0f) ? c : 255.0f;
    c = 255.0f * sqrtf(cqt.sono_v * sqrtf(0.5f * (r0 + r1)));
    cqt.color_buf[x].g = (c < 255.0f) ? c : 255.0f;
    c = 255.0f * sqrtf(cqt.sono_v * sqrtf(r1));
    cqt.color_buf[x].b = (c < 255.0f) ? c : 255.0f;
    cqt.color_buf[x].h = cqt.bar_v * sqrtf(0.5f * (r0 + r1));

    m += len + 2;
  }

  if (cqt.t_size != cqt.width) {
    for (int x = 0; x < cqt.width; x++) {
      cqt.color_buf[x].r = 0.5f * (cqt.color_buf[2 * x].r + cqt.color_buf[2 * x + 1].r);
      cqt.color_buf[x].g = 0.5f * (cqt.color_buf[2 * x].g + cqt.color_buf[2 * x + 1].g);
      cqt.color_buf[x].b = 0.5f * (cqt.color_buf[2 * x].b + cqt.color_buf[2 * x + 1].b);
      cqt.color_buf[x].h = 0.5f * (cqt.color_buf[2 * x].h + cqt.color_buf[2 * x + 1].h);
    }
  }

  for (int x = 0; x < cqt.width; x++)
    cqt.rcp_h_buf[x] = 1.0f / (cqt.color_buf[x].h + 0.0001f);
}

void cqt_calc(void) {
  int fft_size_h = cqt.fft_size >> 1;
  int fft_size_q = cqt.fft_size >> 2;
  int shift = fft_size_h - cqt.attack_size;

  for (int x = 0; x < cqt.attack_size; x++) {
    int i = 4 * cqt.reversed_bit_tbl[x];
    cqt.fft_buf[i] = (Complex) {cqt.input[0][shift + x], cqt.input[1][shift + x]};
    cqt.fft_buf[i + 1].re = cqt.attack_tbl[x] * cqt.input[0][fft_size_h + shift + x];
    cqt.fft_buf[i + 1].im = cqt.attack_tbl[x] * cqt.input[1][fft_size_h + shift + x];
    cqt.fft_buf[i + 2] = (Complex) {cqt.input[0][fft_size_q + shift + x], cqt.input[1][fft_size_q + shift + x]};
    cqt.fft_buf[i + 3] = (Complex) {0, 0};
  }

  for (int x = cqt.attack_size; x < fft_size_q; x++) {
    int i = 4 * cqt.reversed_bit_tbl[x];
    cqt.fft_buf[i] = (Complex) {cqt.input[0][shift + x], cqt.input[1][shift + x]};
    cqt.fft_buf[i + 1] = (Complex) {0, 0};
    cqt.fft_buf[i + 2] = (Complex) {cqt.input[0][fft_size_q + shift + x], cqt.input[1][fft_size_q + shift + x]};
    cqt.fft_buf[i + 3] = (Complex) {0, 0};
  }

  fft_calc(cqt.fft_buf, cqt.fft_size);

  for (int x = 0, m = 0; x < cqt.t_size; x++) {
    int len = cqt.kernel[m].i;
    int start = cqt.kernel[m + 1].i;
    if (!len) {
      cqt.color_buf[x] = (ColorF) {0, 0, 0, 0};
      continue;
    }
    Complex a = {0, 0}, b = {0, 0};
    for (int y = 0; y < len; y++) {
      int i = start + y;
      int j = cqt.fft_size - i;
      float u = cqt.kernel[m + 2 + y].f;
      a.re += u * cqt.fft_buf[i].re;
      a.im += u * cqt.fft_buf[i].im;
      b.re += u * cqt.fft_buf[j].re;
      b.im += u * cqt.fft_buf[j].im;
    }
    Complex v0 = {a.re + b.re, a.im - b.im};
    Complex v1 = {b.im + a.im, b.re - a.re};
    float r0 = v0.re * v0.re + v0.im * v0.im;
    float r1 = v1.re * v1.re + v1.im * v1.im;

    float c = 255.0f * sqrtf(cqt.sono_v * sqrtf(r0));
    cqt.color_buf[x].r = (c < 255.0f) ? c : 255.0f;
    c = 255.0f * sqrtf(cqt.sono_v * sqrtf(0.5f * (r0 + r1)));
    cqt.color_buf[x].g = (c < 255.0f) ? c : 255.0f;
    c = 255.0f * sqrtf(cqt.sono_v * sqrtf(r1));
    cqt.color_buf[x].b = (c < 255.0f) ? c : 255.0f;
    cqt.color_buf[x].h = cqt.bar_v * sqrtf(0.5f * (r0 + r1));

    m += len + 2;
  }

  if (cqt.t_size != cqt.width) {
    for (int x = 0; x < cqt.width; x++) {
      cqt.color_buf[x].r = 0.5f * (cqt.color_buf[2 * x].r + cqt.color_buf[2 * x + 1].r);
      cqt.color_buf[x].g = 0.5f * (cqt.color_buf[2 * x].g + cqt.color_buf[2 * x + 1].g);
      cqt.color_buf[x].b = 0.5f * (cqt.color_buf[2 * x].b + cqt.color_buf[2 * x + 1].b);
      cqt.color_buf[x].h = 0.5f * (cqt.color_buf[2 * x].h + cqt.color_buf[2 * x + 1].h);
    }
  }

  for (int x = 0; x < cqt.width; x++)
    cqt.rcp_h_buf[x] = 1.0f / (cqt.color_buf[x].h + 0.0001f);
}

void cqt_render_line2(float *out) {
  for (int x = 0; x < cqt.width; x++)
    out[x] = cqt.color_buf[x].g + 0.5f;
}

void cqt_render_line(int y, int alpha) {
  alpha = (alpha < 0) ? 0 : (alpha > 255) ? 255 : alpha;
  if (y >= 0 && y < cqt.height) {
    float ht = (cqt.height - y) / (float) cqt.height;
    for (int x = 0; x < cqt.width; x++) {
      if (cqt.color_buf[x].h <= ht) {
        cqt.output[x] = (Color) {0, 0, 0, alpha};
      } else {
        float mul = (cqt.color_buf[x].h - ht) * cqt.rcp_h_buf[x];
        cqt.output[x] = (Color) {mul * cqt.color_buf[x].r + 0.5f, mul * cqt.color_buf[x].g + 0.5f,
                                 mul * cqt.color_buf[x].b + 0.5f, alpha};
      }
    }
  } else {
    for (int x = 0; x < cqt.width; x++)
      cqt.output[x] = (Color) {cqt.color_buf[x].r + 0.5f, cqt.color_buf[x].g + 0.5f, cqt.color_buf[x].b + 0.5f, alpha};
  }
}

void cqt_set_volume(float bar_v, float sono_v) {
  cqt.bar_v = (bar_v > MAX_VOL) ? MAX_VOL : (bar_v > MIN_VOL) ? bar_v : MIN_VOL;
  cqt.sono_v = (sono_v > MAX_VOL) ? MAX_VOL : (sono_v > MIN_VOL) ? sono_v : MIN_VOL;
}

void cqt_set_height(int height) {
  cqt.height = (height > MAX_HEIGHT) ? MAX_HEIGHT : (height > 1) ? height : 1;
}

#ifdef __cplusplus
}
#endif
