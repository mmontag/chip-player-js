#ifndef __FMOPL_H__
#define __FMOPL_H__

#include "../../stdtype.h"
#include "../snddef.h"

/* --- select emulation chips --- */
#ifdef SNDDEV_YM3812
#define BUILD_YM3812 1
#else
#define BUILD_YM3812 0
#endif

#ifdef SNDDEV_YM3526
#define BUILD_YM3526 1
#else
#define BUILD_YM3526 0
#endif

#ifdef SNDDEV_Y8950
#define BUILD_Y8950  1
#else
#define BUILD_Y8950  0
#endif

typedef void (*OPL_TIMERHANDLER)(void *param,UINT8 timer,UINT32 period);
typedef void (*OPL_IRQHANDLER)(void *param,UINT8 irq);
typedef void (*OPL_UPDATEHANDLER)(void *param);
typedef void (*OPL_PORTHANDLER_W)(void *param,UINT8 data);
typedef UINT8 (*OPL_PORTHANDLER_R)(void *param);


#if BUILD_YM3812

void *ym3812_init(UINT32 clock, UINT32 rate);
void ym3812_shutdown(void *chip);
void ym3812_reset_chip(void *chip);
void ym3812_write(void *chip, UINT8 a, UINT8 v);
UINT8 ym3812_read(void *chip, UINT8 a);
UINT8 ym3812_timer_over(void *chip, UINT8 c);
void ym3812_update_one(void *chip, UINT32 length, DEV_SMPL **buffer);

void ym3812_set_timer_handler(void *chip, OPL_TIMERHANDLER TimerHandler, void *param);
void ym3812_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void ym3812_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);

#endif // BUILD_YM3812


#if BUILD_YM3526

/*
** Initialize YM3526 emulator(s).
**
** 'num' is the number of virtual YM3526's to allocate
** 'clock' is the chip clock in Hz
** 'rate' is sampling rate
*/
void *ym3526_init(UINT32 clock, UINT32 rate);
// shutdown the YM3526 emulators
void ym3526_shutdown(void *chip);
void ym3526_reset_chip(void *chip);
void ym3526_write(void *chip, UINT8 a, UINT8 v);
UINT8 ym3526_read(void *chip, UINT8 a);
UINT8 ym3526_timer_over(void *chip, UINT8 c);
/*
** Generate samples for one of the YM3526's
**
** 'which' is the virtual YM3526 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void ym3526_update_one(void *chip, UINT32 length, DEV_SMPL **buffer);

void ym3526_set_timer_handler(void *chip, OPL_TIMERHANDLER TimerHandler, void *param);
void ym3526_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void ym3526_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);

#endif // BUILD_YM3526


#if BUILD_Y8950

// Y8950 port handlers
void y8950_set_port_handler(void *chip, OPL_PORTHANDLER_W PortHandler_w, OPL_PORTHANDLER_R PortHandler_r, void *param);
void y8950_set_keyboard_handler(void *chip, OPL_PORTHANDLER_W KeyboardHandler_w, OPL_PORTHANDLER_R KeyboardHandler_r, void *param);
void y8950_alloc_pcmrom(void* chip, UINT32 memsize);
void y8950_write_pcmrom(void* chip, UINT32 offset, UINT32 length, const UINT8* data);

void * y8950_init(UINT32 clock, UINT32 rate);
void y8950_shutdown(void *chip);
void y8950_reset_chip(void *chip);
void y8950_write(void *chip, UINT8 a, UINT8 v);
UINT8 y8950_read (void *chip, UINT8 a);
UINT8 y8950_timer_over(void *chip, UINT8 c);
void y8950_update_one(void *chip, UINT32 length, DEV_SMPL **buffer);

void y8950_set_timer_handler(void *chip, OPL_TIMERHANDLER TimerHandler, void *param);
void y8950_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void y8950_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);

#endif // BUILD_Y8950

void opl_set_mute_mask(void *chip, UINT32 MuteMask);
void opl_set_log_cb(void* chip, DEVCB_LOG func, void* param);

#endif	// __FMOPL_H__
