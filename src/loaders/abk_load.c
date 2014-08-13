/* Extended Module Player
 * AMOS/STOS Music Bank Loader
 *
 * Copyright (C) 2014 Stephen J Leary
 * Copyright (C) 1996-2014 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "effects.h"
#include "period.h"
#include "loader.h"

#define AMOS_BANK 0x416d426b
#define AMOS_MUSIC_TYPE 0x0003
#define AMOS_MUSIC_TEXT 0x4D75736963202020
#define AMOS_MAIN_HEADER 0x14L
#define AMOS_STRING_LEN 0x10
#define AMOS_BASE_FREQ 8192
#define AMOS_ABK_CHANNELS 4

static int abk_test (HIO_HANDLE *, char *, const int);
static int abk_load (struct module_data *, HIO_HANDLE *, const int);

struct format_loader abk_loader = {
    "AMOS Music Bank",
    abk_test,
    abk_load
};

struct abk_header
{
    uint32	instruments_offset;
    uint32	songs_offset;
    uint32	patterns_offset;
};

struct abk_song
{
    uint32	playlist_offset[AMOS_ABK_CHANNELS];
    uint16	tempo;
    char   	song_name[AMOS_STRING_LEN];
};

struct abk_playlist
{
    uint8	length;
    uint16	*pattern;
};

struct abk_instrument
{
    uint32 sample_offset;
    uint32 sample_length;

    uint32 repeat_offset;
    uint16 repeat_length;

    uint16 sample_volume;

    char   sample_name[AMOS_STRING_LEN];
};

/**
 * @brief read the ABK playlist out from the file stream. This method malloc's some memory for the playlist
 * and can realloc if the playlist is very long.
 * @param f the stream to read from
 * @param playlist_offset the offset to the playlist sections.
 * @param playlist this structure is populated with the result.
 */
static void read_abk_playlist(HIO_HANDLE *f, uint16 playlist_offset, struct abk_playlist *playlist)
{
    uint16 playdata;
    int arraysize;

    arraysize = 64;

    /* clear the length */
    playlist->length = 0;

    /* move to the start of the songs data section. */
    hio_seek(f, playlist_offset, SEEK_SET);

    playlist->pattern = (uint16 *) malloc(arraysize * sizeof(uint16));

    playdata = hio_read16b(f);

    while (playdata != 0xFFFF)
    {
        /* i hate doing a realloc in a loop
           but i'd rather not traverse the list twice.*/

        if (playlist->length >= arraysize)
        {
            arraysize *= 2;
            playlist->pattern = (uint16 *) realloc(playlist->pattern , arraysize * sizeof(uint16));
        }

        playlist->pattern[playlist->length++] = playdata;
        playdata = hio_read16b(f);
    };
}

static int read_abk_song(HIO_HANDLE *f, struct abk_song *song, uint32 songs_section_offset)
{
    int i;
    uint32 song_section;

    /* move to the start of the songs data sectio */
    hio_seek(f, songs_section_offset, SEEK_SET);

    if (hio_read16b(f) != 1)
    {
        /* we only support a single song.
         * in a an abk file for now */
        return -1;
    }

    song_section = hio_read32b(f);

    hio_seek(f, songs_section_offset + song_section, SEEK_SET);

    for (i=0; i<AMOS_ABK_CHANNELS; i++)
    {
        song->playlist_offset[i] = hio_read16b(f) + songs_section_offset + song_section;
    }

    song->tempo = hio_read16b(f);

    /* unused. just progress the file pointer forward */
    (void)hio_read16b(f);

    hio_read(song->song_name, 1, AMOS_STRING_LEN, f);

    return 0;
}

static uint16 read_abk_pattern(HIO_HANDLE *f, struct xmp_event *events, uint32 pattern_offset_abs)
{
    uint8 position;
    uint8 command;
    uint8 param;
    uint16 patdata;

    uint32 storepos;
    storepos = hio_tell(f);

    /* count how many abk positions are used in this pattern */
    position = 0;
    hio_seek(f, pattern_offset_abs, SEEK_SET);

    patdata = hio_read16b(f);

    while (patdata != 0x8000)
    {
        if (patdata == 0x9100)
        {
            break;
        }

        if (patdata & 0x8000)
        {
            command = (patdata >> 8) & 0x7F;
            param = patdata & 0x007F;

            switch (command)
            {
            case 0x03:		/* set volume */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_VOLSET;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x04:		/* stop effect */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PER_CANCEL;
                }
                break;
            }
            case 0x05:		/* repeat */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    if (param == 0) {
                        events[position].fxp = 0x50;
                    } else {
                        events[position].fxp = 0x60 | (param & 0x0f);
                    }
                }
                break;
            }
            case 0x06:		/* lowpass filter on */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    events[position].fxp = 0x01;
                }
                break;
            }
            case 0x07:		/* lowpass filter off */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    events[position].fxp = 0x00;
                }
                break;
            }
            case 0x08:		/* set tempo */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_SPEED;
                    events[position].fxp = 100/param;
                }
                break;
            }
            case 0x09:		/* set instrument */
            {
                if (events != NULL)
                {
                    events[position].ins = param+1;
                }

                break;
            }
            case 0x0a:		/* arpeggio */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_ARPEGGIO;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x0B:		/* tone portamento */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PER_TPORTA;
                }
                break;
            }
            case 0x0d:		/* volume slide */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_VOLSLIDE;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x0e:		/* portamento up */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PORTA_UP;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x0F:		/* portamento down */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PORTA_DN;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x10:		/* delay */
            {
                position += param;
                break;
            }
            case 0x11:		/* position jump */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_JUMP;
                    events[position].fxp = param;
                }
                break;
            }
            default:
            {
                /* write out an error for any unprocessed commands.*/
                fprintf(stderr, "ABK UNPROCESSED COMMAND: %x,%x\n", command, param);
                break;
            }
        }
        }
        else
        {
            if (events != NULL)
            {
                /* convert the note from amiga period format to xmp's internal format.*/
                events[position].note = period_to_note(patdata & 0x0fff);
            }
        }

        patdata = hio_read16b(f);
    }

    hio_seek(f, storepos, SEEK_SET);

    return position;
}

static void read_abk_inst(HIO_HANDLE *f, struct abk_instrument *inst, uint32 inst_section_offset)
{
    uint32 sampleLength;
    uint32 savepos;
    uint16 repeat;

    inst->sample_offset = hio_read32b(f);
    inst->repeat_offset = hio_read32b(f);

    savepos = hio_tell(f);

    hio_seek(f, inst_section_offset + inst->repeat_offset, SEEK_SET);

    repeat = hio_read16b(f);

    hio_seek(f, savepos, SEEK_SET);

    if (repeat == 0)
    {
        /* the sample does not repeat*/
        inst->sample_length = hio_read16b(f);
        (void) hio_read16b(f);
    }
    else
    {
        inst->repeat_length = hio_read32b(f);
    }

    inst->sample_volume = hio_read16b(f);
    sampleLength = hio_read16b(f);

    /* detect a potential bug where the sample length is not specified (and we might already know the length) */
    if (sampleLength > 4)
    {
        inst->sample_length = sampleLength;
    }

    hio_read(inst->sample_name, 1, 16, f);
}
static int abk_test (HIO_HANDLE *f, char *t, const int start)
{
    uint64 music;

    if (hio_read32b(f) != AMOS_BANK)
    {
        return -1;
    }

    if (hio_read16b(f) != AMOS_MUSIC_TYPE)
    {
        return -1;
    }

    /* skip over length and chip/fastmem.*/
    hio_seek(f, 6, SEEK_CUR);

    music = hio_read32b(f); /* get the "Music   " */
    music = music << 32;
    music |= hio_read32b(f);

    if (music != (unsigned long long)AMOS_MUSIC_TEXT)
    {
        return -1;
    }

    return 0;
}

static int abk_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    int i,j;
    uint16 pattern;
    uint32 first_sample_offset;
	
	struct xmp_module *mod = &m->mod;

    struct abk_header main_header;
    struct abk_instrument ci;
    struct abk_song song;
    struct abk_playlist playlist;

    pattern = 0;
    first_sample_offset = 0;

    hio_seek(f, AMOS_MAIN_HEADER, SEEK_SET);

    main_header.instruments_offset = hio_read32b(f);
    main_header.songs_offset = hio_read32b(f);
    main_header.patterns_offset = hio_read32b(f);

    LOAD_INIT();

    set_type(m, "AMOS Music Bank");

    if (read_abk_song(f, &song, AMOS_MAIN_HEADER + main_header.songs_offset) < 0)
    {
        return -1;
    }

    copy_adjust(mod->name, (uint8*) song.song_name, AMOS_STRING_LEN);

    MODULE_INFO();

    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset, SEEK_SET);

    mod->chn = 4;
    mod->pat = hio_read16b(f);
    mod->trk = mod->chn * mod->pat;

    /* move to the start of the instruments section. */
    hio_seek(f, AMOS_MAIN_HEADER + main_header.instruments_offset, SEEK_SET);
    mod->ins = hio_read16b(f);
    mod->smp = mod->ins;

    hio_seek(f, AMOS_MAIN_HEADER + main_header.instruments_offset + 2, SEEK_SET);

	 /* Read and convert instruments and samples */
	 
    if (instrument_init(mod) < 0)
	{
		return -1;
	}

    D_(D_INFO "Instruments: %d", mod->ins);
	
    for (i = 0; i < mod->ins; i++)
    {
        /* allocate an instrment */
        memset(&ci, 0, sizeof(ci));
		
		if (subinstrument_alloc(mod, i, 1) < 0)
	    {
			return -1;
		}

        read_abk_inst(f, &ci, AMOS_MAIN_HEADER + main_header.instruments_offset);

        mod->xxs[i].len = (ci.sample_length)<<1;
        
		if (mod->xxs[i].len > 0)
		{
			mod->xxi[i].nsm = 1;
		}
		
        /* store the location of the first sample so we can read them later. */
        if (first_sample_offset == 0)
        {
            first_sample_offset = AMOS_MAIN_HEADER + main_header.instruments_offset + ci.sample_offset;
        }

        /* TODO: do the repeating stuff. */
        mod->xxs[i].lps = 0;
        mod->xxs[i].lpe = 0;
        mod->xxs[i].flg = 0;

		mod->xxi[i].sub[0].vol = ci.sample_volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		instrument_name(mod, i, (uint8*)ci.sample_name, 16);

		D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ');
    }

	 if (pattern_init(mod) < 0)
	 {
		return -1;
	 }
	 
    /* figure out the playlist order.
     * TODO: if the 4 channels arent in the same order then
     * we need to fail here. */

    read_abk_playlist(f, song.playlist_offset[0], &playlist);

    for (j=0; j< playlist.length; j++)
    {
		mod->xxo[j] = playlist.pattern[j];
    }

    /* move to the start of the instruments section */
    /* then convert the patterns one at a time. there is a pattern for each channel.*/
    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset + 2, SEEK_SET);

    mod->len = 0;

    for (i = 0; i < mod->pat; i++)
    {
        /* increment the length */
        mod->len++;
		
        pattern = hio_read16b(f);
        hio_seek(f, -2L, SEEK_CUR);

        /* just read the size first time.*/
        /* we'll use that size to define the row size then do the conversion.*/
        uint8 patternsize = read_abk_pattern(f, NULL, AMOS_MAIN_HEADER + main_header.patterns_offset + pattern);
		
		if (pattern_tracks_alloc(mod, i, patternsize) < 0)
		{
			return -1;
		}
		
        for (j = 0; j  < mod->chn; j++)
        {
            pattern = hio_read16b(f);
            read_abk_pattern(f,  mod->xxt[(i*mod->chn)+j]->event, AMOS_MAIN_HEADER + main_header.patterns_offset + pattern);
        }
    }
	
    D_(D_INFO "Stored patterns: %d", mod->pat);

   	D_(D_INFO "Stored tracks: %d", mod->trk);

    /* Read samples */
    hio_seek(f, first_sample_offset, SEEK_SET);
	
    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++)
    {
        if (mod->xxs[i].len <= 2)
            continue;
			
		if (load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
	    {
			return -1;
		}
    }

    return 0;
}
