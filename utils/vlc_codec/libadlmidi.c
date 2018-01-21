/*****************************************************************************
 * libadlmidi.c: Software MIDI synthesizer using OPL3 Synth emulation
 *****************************************************************************
 * Copyright © 2015-2018 Vitaly Novichkov
 * $Id$
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

//#ifdef HAVE_CONFIG_H
//# include "config.h"
//#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>

#include <unistd.h>

#ifndef N_
#define N_(x) (x)
#endif

#ifndef _
#define _(x) (x)
#endif

#include <adlmidi.h>

#define SOUNDFONT_TEXT N_("Custom bank file")
#define SOUNDFONT_LONGTEXT N_( \
    "Custom bank file to use for software synthesis." )

//#define CHORUS_TEXT N_("Chorus")

//#define GAIN_TEXT N_("Synthesis gain")
//#define GAIN_LONGTEXT N_("This gain is applied to synthesis output. " \
//    "High values may cause saturation when many notes are played at a time." )

//#define POLYPHONY_TEXT N_("Polyphony")
//#define POLYPHONY_LONGTEXT N_( \
//    "The polyphony defines how many voices can be played at a time. " \
//    "Larger values require more processing power.")

//#define REVERB_TEXT N_("Reverb")

#define SAMPLE_RATE_TEXT N_("Sample rate")

#define EMULATED_CHIPS_TEXT N_("Count of emulated chips")

#define EMBEDDED_BANK_ID_TEXT N_("Embedded bank ID")

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ADLMIDI OPL3 Synth MIDI synthesizer"))
    set_capability ("decoder", 150)
    set_shortname (N_("ADLMIDI"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACODEC)
    set_callbacks (Open, Close)
    add_loadfile ("adlmidi-custombank", "",
                  SOUNDFONT_TEXT, SOUNDFONT_LONGTEXT, false)
    //add_bool ("synth-chorus", true, CHORUS_TEXT, CHORUS_TEXT, false)
    //add_float ("synth-gain", .5, GAIN_TEXT, GAIN_LONGTEXT, false)
    //    change_float_range (0., 10.)
    //add_integer ("synth-polyphony", 256, POLYPHONY_TEXT, POLYPHONY_LONGTEXT, false)
    //    change_integer_range (1, 65535)
    //add_bool ("synth-reverb", true, REVERB_TEXT, REVERB_TEXT, true)
    add_integer ("adlmidi-sample-rate", 44100, SAMPLE_RATE_TEXT, SAMPLE_RATE_TEXT, true)
        change_integer_range (22050, 96000)
    add_integer ("adlmidi-emulated-chips", 6, EMULATED_CHIPS_TEXT, EMULATED_CHIPS_TEXT, true)
        change_integer_range (1, 100)
    add_integer ("adlmidi-internal-bank-id", 58, EMBEDDED_BANK_ID_TEXT, EMBEDDED_BANK_ID_TEXT, true)
        change_integer_range (0, 73)
vlc_module_end ()


struct decoder_sys_t
{
    struct ADL_MIDIPlayer *synth;
    int               sample_rate;
    int               soundfont;
    date_t            end_date;
};


//static int  DecodeBlock (decoder_t *p_dec, block_t *p_block); //For different version
static block_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block);
static void Flush (decoder_t *);

static int Open (vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MIDI)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->sample_rate = var_InheritInteger (p_this, "adlmidi-sample-rate");
    p_sys->synth = adl_init( p_sys->sample_rate );

    adl_setNumChips(p_sys->synth, var_InheritInteger(p_this, "adlmidi-emulated-chips"));

    char *font_path = var_InheritString (p_this, "adlmidi-custombank");
    if (font_path != NULL)
    {
        msg_Dbg (p_this, "loading custom bank file %s", font_path);
        if (adl_openBankFile(p_sys->synth, font_path))
            msg_Err (p_this, "cannot load custom bank file %s: %s", font_path, adl_errorInfo(p_sys->synth));
        free (font_path);
    }
    else
    {
        adl_setBank(p_sys->synth, var_InheritInteger(p_this, "adlmidi-internal-bank-id"));
    }

    p_dec->fmt_out.i_cat = AUDIO_ES;

    p_dec->fmt_out.audio.i_rate = p_sys->sample_rate;
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    p_dec->fmt_out.i_codec = VLC_CODEC_S16L;
    p_dec->fmt_out.audio.i_bitspersample = 16;
    date_Init (&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);
    date_Set (&p_sys->end_date, 0);

    p_dec->p_sys = p_sys;
    //==For different version==
    //p_dec->pf_decode = DecodeBlock;
    //p_dec->pf_flush  = Flush;
    p_dec->pf_decode_audio = DecodeBlock;
    return VLC_SUCCESS;//VLCDEC_SUCCESS
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    adl_close(p_sys->synth);
    free (p_sys);
}

static void Flush (decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set (&p_sys->end_date, VLC_TS_INVALID);
    adl_panic(p_sys->synth);
}



static block_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block)
{
    block_t *p_block;
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;

    if (pp_block == NULL)
        return NULL;
    p_block = *pp_block;
    if (p_block == NULL)
        return NULL;
    *pp_block = NULL;

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
        date_Set (&p_sys->end_date, 0);
        adl_panic(p_sys->synth);
    }

    if (p_block->i_pts > VLC_TS_INVALID && !date_Get (&p_sys->end_date))
        date_Set (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < date_Get (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t channel = p_block->p_buffer[0] & 0xf;
    event &= 0xF0;

    if (event == 0xF0)
        switch (channel)
        {
            case 0:
                if (p_block->p_buffer[p_block->i_buffer - 1] != 0xF7)
                {
            case 7:
                    msg_Warn (p_dec, "fragmented SysEx not implemented");
                    goto drop;
                }
                //fluid_synth_sysex (p_sys->synth, (char *)p_block->p_buffer + 1,
                //                   p_block->i_buffer - 2, NULL, NULL, NULL, 0);
                break;
            case 0xF:
                adl_rt_resetState(p_sys->synth);
                break;
        }

    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (event & 0xF0)
    {
        case 0x80:
            adl_rt_noteOff(p_sys->synth, channel, p1);
            break;
        case 0x90:
            adl_rt_noteOn(p_sys->synth, channel, p1, p2);
            break;
        /*case 0xA0: note aftertouch not implemented */
        case 0xB0:
            adl_rt_controllerChange(p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            adl_rt_patchChange(p_sys->synth, channel, p1);
            break;
        case 0xD0:
            adl_rt_channelAfterTouch(p_sys->synth, channel, p1);
            break;
        case 0xE0:
            adl_rt_pitchBendML(p_sys->synth, channel, p2, p2);
            break;
    }

    unsigned samples =
        (p_block->i_pts - date_Get (&p_sys->end_date)) * 441 / 10000;
    if (samples == 0)
        goto drop;

    p_out = decoder_NewAudioBuffer (p_dec, samples);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get (&p_sys->end_date );
    samples = adl_generate(p_sys->synth, samples * 2, (short*)p_out->p_buffer);
    samples /= 2;
    p_out->i_length = date_Increment (&p_sys->end_date, samples) - p_out->i_pts;

drop:
    block_Release (p_block);
    return p_out;
}

#if 0
static int DecodeBlock (decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;

    if (p_block == NULL) /* No Drain */
        return VLC_SUCCESS;//VLCDEC_SUCCESS

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
        Flush (p_dec);
        if (p_block->i_flags & BLOCK_FLAG_CORRUPTED)
        {
            block_Release(p_block);
            return VLC_SUCCESS;//VLCDEC_SUCCESS
        }
    }

    if (p_block->i_pts > VLC_TS_INVALID && !date_Get (&p_sys->end_date))
        date_Set (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < date_Get (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t channel = p_block->p_buffer[0] & 0xf;
    event &= 0xF0;

    if (event == 0xF0)
        switch (channel)
        {
            case 0:
                if (p_block->p_buffer[p_block->i_buffer - 1] != 0xF7)
                {
            case 7:
                    msg_Warn (p_dec, "fragmented SysEx not implemented");
                    goto drop;
                }
                //fluid_synth_sysex (p_sys->synth, (char *)p_block->p_buffer + 1,
                //                   p_block->i_buffer - 2, NULL, NULL, NULL, 0);
                break;
            case 0xF:
                adl_rt_resetState(p_sys->synth);
                break;
        }

    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (event & 0xF0)
    {
        case 0x80:
            adl_rt_noteOff(p_sys->synth, channel, p1);
            break;
        case 0x90:
            adl_rt_noteOn(p_sys->synth, channel, p1, p2);
            break;
        /*case 0xA0: note aftertouch not implemented */
        case 0xB0:
            adl_rt_controllerChange(p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            adl_rt_patchChange(p_sys->synth, channel, p1);
            break;
        case 0xD0:
            adl_rt_channelAfterTouch(p_sys->synth, channel, p1);
            break;
        case 0xE0:
            adl_rt_pitchBendML(p_sys->synth, channel, p2, p1);
            break;
    }

    unsigned samples =
        (p_block->i_pts - date_Get (&p_sys->end_date)) * 441 / 10000;
    if (samples == 0)
        goto drop;

    if (decoder_UpdateAudioFormat (p_dec))
        goto drop;
    p_out = decoder_NewAudioBuffer (p_dec, samples);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get (&p_sys->end_date );
    p_out->i_length = date_Increment (&p_sys->end_date, samples)
                      - p_out->i_pts;
    adl_generate(p_sys->synth, samples * 2, p_out->p_buffer);
drop:
    block_Release (p_block);
    if (p_out != NULL)
        decoder_QueueAudio (p_dec, p_out);
    return VLC_SUCCESS;//VLCDEC_SUCCESS
}

#endif
