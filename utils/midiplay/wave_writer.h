/* WAVE sound file writer for recording 16-bit output during program development */

#pragma once
#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

int wave_open(long sample_rate, const char *filename);
void wave_enable_stereo(void);
void wave_write(short const *in, long count);
long wave_sample_count(void);
void wave_close(void);

void *ctx_wave_open(long sample_rate, const char *filename);
void ctx_wave_enable_stereo(void *ctx);
void ctx_wave_write(void *ctx, short const *in, long count);
long ctx_wave_sample_count(void *ctx);
void ctx_wave_close(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
