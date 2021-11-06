#ifndef __AY8910_H__
#define __AY8910_H__

#include "../../stdtype.h"
#include "../snddef.h"
#include "../EmuStructs.h"

extern DEV_DEF devDef_AY8910_MAME;

/*
AY-3-8910A: 2 I/O ports
AY-3-8912A: 1 I/O port
AY-3-8913A: 0 I/O port
AY-3-8914:  same as 8910 except for different register mapping and two bit envelope enable / volume field
AY8930: upper compatible with 8910.
In extended mode, it has higher resolution and duty ratio setting
YM2149: higher resolution, selectable clock divider
YM3439: same as 2149
YMZ284: 0 I/O port, different clock divider
YMZ294: 0 I/O port
OKI M5255, Winbond WF19054, JFC 95101, File KC89C72, Toshiba T7766A : differences to be listed
*/

#define ALL_8910_CHANNELS -1

/* Internal resistance at Volume level 7. */

#define AY8910_INTERNAL_RESISTANCE	(356)
#define YM2149_INTERNAL_RESISTANCE	(353)

/*
 * The following is used by all drivers not reviewed yet.
 * This will like the old behaviour, output between
 * 0 and 7FFF
 */
#define AY8910_LEGACY_OUTPUT        (0x01)

/*
 * Specifying the next define will simulate the special
 * cross channel mixing if outputs are tied together.
 * The driver will only provide one stream in this case.
 */
#define AY8910_SINGLE_OUTPUT        (0x02)

/*
 * The following define is the default behaviour.
 * Output level 0 is 0V and 7ffff corresponds to 5V.
 * Use this to specify that a discrete mixing stage
 * follows.
 */
#define AY8910_DISCRETE_OUTPUT      (0x04)

/*
 * The following define causes the driver to output
 * resistor values. Intended to be used for
 * netlist interfacing.
 */

#define AY8910_RESISTOR_OUTPUT      (0x08)

/*
 * This define specifies the initial state of YM2149
 * pin 26 (SEL pin). By default it is set to high,
 * compatible with AY8910.
 */
/* TODO: make it controllable while it's running (used by any hw???) */
#ifndef YM2149_PIN26_HIGH
#define YM2149_PIN26_HIGH           (0x00) /* or N/C */
#endif
#ifndef YM2149_PIN26_LOW
#define YM2149_PIN26_LOW            (0x10)
#endif


/*********** An interface for SSG of YM2203 ***********/

typedef struct _ay8910_context ay8910_context;

UINT8 device_start_ay8910_mame(const AY8910_CFG* cfg, DEV_INFO* retDevInf);
UINT32 ay8910_start(void **chip, UINT32 clock, UINT8 ay_type, UINT8 ay_flags);

void ay8910_stop(void *chip);
void ay8910_reset(void *chip);
void ay8910_set_clock(void *chip, UINT32 clock);
UINT32 ay8910_get_sample_rate(void *chip);
void ay8910_write(void *chip, UINT8 addr, UINT8 data);
UINT8 ay8910_read(void *chip, UINT8 addr);
void ay8910_write_reg(ay8910_context *psg, UINT8 r, UINT8 v);

void ay8910_update_one(void *param, UINT32 samples, DEV_SMPL **outputs);

void ay8910_set_mute_mask(void *chip, UINT32 MuteMask);
void ay8910_set_stereo_mask(void *chip, UINT32 StereoMask);
void ay8910_set_srchg_cb(void *chip, DEVCB_SRATE_CHG CallbackFunc, void* DataPtr);
void ay8910_set_log_cb(void* chip, DEVCB_LOG func, void* param);

#endif	// __AY8910_H__
