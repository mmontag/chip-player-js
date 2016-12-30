#ifndef __AY8910_H__
#define __AY8910_H__

#include <stdtype.h>
#include "../snddef.h"
#include "../EmuStructs.h"

/*
AY-3-8910A: 2 I/O ports
AY-3-8912A: 1 I/O port
AY-3-8913A: 0 I/O port
AY8930: upper compatible with 8910.
In extended mode, it has higher resolution and duty ratio setting
YM2149: higher resolution
YM3439: same as 2149
YMZ284: 0 I/O port, different clock divider
YMZ294: 0 I/O port
*/

#define ALL_8910_CHANNELS -1

/* Internal resistance at Volume level 7. */

#define AY8910_INTERNAL_RESISTANCE	(356)
#define YM2149_INTERNAL_RESISTANCE	(353)

/*
 * Default values for resistor loads.
 * The macro should be used in AY8910interface if
 * the real values are unknown.
 */
#define AY8910_DEFAULT_LOADS		{1000, 1000, 1000}

/*
 * The following is used by all drivers not reviewed yet.
 * This will like the old behaviour, output between
 * 0 and 7FFF
 */
#define AY8910_LEGACY_OUTPUT		(1)

/*
 * Specifing the next define will simulate the special
 * cross channel mixing if outputs are tied together.
 * The driver will only provide one stream in this case.
 */
#define AY8910_SINGLE_OUTPUT		(2)

/*
 * The follwoing define is the default behaviour.
 * Output level 0 is 0V and 7ffff corresponds to 5V.
 * Use this to specify that a discrete mixing stage
 * follows.
 */
#define AY8910_DISCRETE_OUTPUT		(4)

/*
 * The follwoing define causes the driver to output
 * raw volume levels, i.e. 0 .. 15 and 0..31.
 * This is intended to be used in a subsequent
 * mixing modul (i.e. mpatrol ties 6 channels from
 * AY-3-8910 together). Do not use it now.
 */
/* TODO: implement mixing module */
#define AY8910_RAW_OUTPUT			(8)

#define AY8910_ZX_STEREO			0x80
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

typedef struct _ay8910_interface ay8910_interface;
struct _ay8910_interface
{
	int					flags;			/* Flags */
	int					res_load[3]; 	/* Load on channel in ohms */
};


/*********** An interface for SSG of YM2203 ***********/

typedef struct _ay8910_context ay8910_context;

UINT32 ay8910_start_ym(void **chip, UINT32 clock, UINT8 ay_type, UINT8 ay_flags);

void ay8910_stop_ym(void *chip);
void ay8910_reset_ym(void *chip);
void ay8910_set_clock_ym(void *chip, UINT32 clock);
UINT32 ay8910_get_sample_rate(void *chip);
void ay8910_write_ym(void *chip, UINT8 addr, UINT8 data);
UINT8 ay8910_read_ym(void *chip, UINT8 addr);
void ay8910_write_reg(ay8910_context *psg, UINT8 r, UINT8 v);

void ay8910_update_one(void *param, UINT32 samples, DEV_SMPL **outputs);

void ay8910_set_mute_mask_ym(void *chip, UINT32 MuteMask);
void ay8910_set_srchg_cb_ym(void *chip, DEVCB_SRATE_CHG CallbackFunc, void* DataPtr);

#endif	// __AY8910_H__
