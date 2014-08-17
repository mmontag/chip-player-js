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
#define ABK_HEADER_SECTION_COUNT 3

static int abk_test (HIO_HANDLE *, char *, const int);
static int abk_load (struct module_data *, HIO_HANDLE *, const int);

struct format_loader abk_loader =
{
    "AMOS Music Bank",
    abk_test,
    abk_load
};

/**
 * @class abk_header
 * @brief represents the main ABK header.
 */
struct abk_header
{
    uint32	instruments_offset;
    uint32	songs_offset;
    uint32	patterns_offset;
};

/**
 * @class abk_song
 * @brief represents a song in an ABK module.
 */
struct abk_song
{
    uint32	playlist_offset[AMOS_ABK_CHANNELS];
    uint16	tempo;
    char   	song_name[AMOS_STRING_LEN];
};

/**
 * @class abk_playlist
 * @brief represents an ABK playlist.
 */
struct abk_playlist
{
    uint16	length;
    uint16	*pattern;
};

/**
 * @class abk_instrument
 * @brief represents an ABK instrument.
 */
struct abk_instrument
{
    uint32 sample_offset;
    uint32 sample_length;

    uint32 repeat_offset;
    uint16 repeat_end;

    uint16 sample_volume;

    char   sample_name[AMOS_STRING_LEN];
};


static void free_abk_playlist(struct abk_playlist *playlist)
{
    if (playlist->pattern != NULL)
    {
        free(playlist->pattern);
    }

    playlist->length = 0;
}


/**
 * @brief read the ABK playlist out from the file stream. This method malloc's some memory for the playlist
 * and can realloc if the playlist is very long.
 * @param f the stream to read from
 * @param playlist_offset the offset to the playlist sections.
 * @param playlist this structure is populated with the result.
 */
static void read_abk_playlist(HIO_HANDLE *f, uint32 playlist_offset, struct abk_playlist *playlist)
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

    while((playdata != 0xFFFF) && (playdata != 0xFFFE))
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
    (void) hio_read16b(f);

    hio_read(song->song_name, 1, AMOS_STRING_LEN, f);

    return 0;
}

/**
 * @brief reads an ABK pattern into a xmp_event structure. 
 * @param f stream to read data from.
 * @param events events object to populate.
 * @param pattern_offset_abs the absolute file offset to the start of the patter to read.
 * @return returns the size of the pattern.
 */
static uint16 read_abk_pattern(HIO_HANDLE *f, struct xmp_event *events, uint32 pattern_offset_abs)
{
    uint8 position;
    uint8 command;
    uint8 param;
    uint8 inst = 0;
    uint8 jumped = 0;
    uint8 per_command = 0;
    uint8 per_param = 0;

    uint16 delay;
    uint16 patdata;

    uint32 storepos;
    storepos = hio_tell(f);

    /* count how many abk positions are used in this pattern */
    position = 0;

    hio_seek(f, pattern_offset_abs, SEEK_SET);

    /* read the first bit of pattern data */
    patdata = hio_read16b(f);

    while ((patdata != 0x8000) && (jumped == 0))
    {
        if (patdata == 0x9100)
        {
            break;
        }

        if (patdata & 0x8000)
        {
            command = (patdata >> 8) & 0x7F;
            param = patdata & 0x007F;

            if (command != 0x09 && command != 0x0b && command != 0x0c && command < 0x10) {
                per_command = 0;
                per_param = 0;
            }

            switch (command)
            {
            case 0x01:		/* portamento up */
            case 0x0e:
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PORTA_UP;
                    events[position].fxp = param;
                }
                break;
            }
            case 0x02:		/* portamento down */
            case 0x0f:
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_PORTA_DN;
                    events[position].fxp = param;
                }
                break;
            }
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
                break;
            }
            case 0x05:		/* repeat */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    if (param == 0)
                    {
                        events[position].fxp = 0x50;
                    }
                    else
                    {
                        events[position].fxp = 0x60 | (param & 0x0f);
                    }
                }
                break;
            }
            case 0x06:		/* lowpass filter off */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    events[position].fxp = 0x00;
                }
                break;
            }
            case 0x07:		/* lowpass filter on */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_EXTENDED;
                    events[position].fxp = 0x01;
                }
                break;
            }
            case 0x08:		/* set tempo */
            {
                if (events != NULL && param != 0)
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
                    inst = param + 1;
                }

                break;
            }
            case 0x0a:		/* arpeggio */
            {
                per_command = FX_ARPEGGIO;
                per_param = param;
                break;
            }
            case 0x0b:		/* tone portamento */
            {
                per_command = FX_TONEPORTA;
                per_param = param;
                break;
            }
            case 0x0c:		/* vibrato */
            {
                per_command = FX_VIBRATO;
                per_param = param;
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
            case 0x10:		/* delay */
            {
                if (events != NULL && (per_command != 0 || per_param != 0)) {
                    int i;
                    for (i = 0; i < param; i++) {
                        events[position].fxt = per_command;
                        events[position].fxp = per_param;
                        position++;
                    }
                } else {
		    position += param;
		}
                break;
            }
            case 0x11:		/* position jump */
            {
                if (events != NULL)
                {
                    events[position].fxt = FX_JUMP;
                    events[position].fxp = param;
					/* break out of the loop because we've jumped.*/
					jumped = 1;
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
            if (patdata & 0x4000)
            {
                /* old note format.*/
                /* old note format is 2 x 2 byte words with bit 14 set on the first word */
                /* WORD 1: 0x4XDD | X = dont care, D = delay to apply after note. (Usually in 7FDD form).
                 * WORD 2: 0xXPPP | PPP = Amiga Period */

                delay = patdata;
                patdata = hio_read16b(f);
                delay = delay & 0x00FF;

                if ((patdata == 0) && (delay == 0))
                {
                    /* a zero note, with zero delay ends the pattern */
                    break;
                }

                if ((events != NULL) && (patdata != 0))
                {
                    /* convert the note from amiga period format to xmp's internal format.*/
                    events[position].note = period_to_note(patdata & 0x0fff);
                    events[position].ins = inst;
                }

                /* now add on the delay */
                position += delay;
            }
            else /* new note format */
            {
                if (events != NULL)
                {
                    /* convert the note from amiga period format to xmp's internal format.*/
                    events[position].note = period_to_note(patdata & 0x0fff);
                    events[position].ins = inst;
                }
            }
        }

        /* read the data for the next pass round the loop */
        patdata = hio_read16b(f);

        /* check for an EOF while reading */
        if (hio_eof(f))
        {
            break;
        }
    }

    hio_seek(f, storepos, SEEK_SET);

    return position;
}

/**
 * @brief Read a single abk instrument.
 * @param f the io handle
 * @param inst the instrument structure (prealloc'd)
 * @param inst_section_offset offset to the instrument section.
 */
static void read_abk_inst(HIO_HANDLE *f, struct abk_instrument *inst, uint32 inst_section_offset)
{
    uint32 sampleLength;

    inst->sample_offset = hio_read32b(f);
    inst->repeat_offset = hio_read32b(f);
    inst->sample_length = hio_read16b(f);
    inst->repeat_end = hio_read16b(f);
    inst->sample_volume = hio_read16b(f);
    sampleLength = hio_read16b(f);

    /* detect a potential bug where the sample length is not specified (and we might already know the length) */
    if (sampleLength > 4)
    {
        inst->sample_length = sampleLength;
    }

    hio_read(inst->sample_name, 1, 16, f);
/*printf("%-16.16s  %04x %04x %04x %04x %02x  %04x\n", inst->sample_name, inst->sample_offset, inst->repeat_offset, inst->sample_length, inst->repeat_end, inst->sample_volume, inst_section_offset);*/
}

/**
 * @brief takes an unsorted list of offsets, sorts them and produces a size of each one.
 * @param size the size of the list.
 * @param inptrs the list of offsets.
 * @param outptrs a preallocated list for the result.
 * @param end the very end. Used to calculate the last size.
 */
static void sortandsize(uint32 size, uint32 *inptrs, uint32 *outptrs, uint32 end)
{
    uint32 i,j,tmp;

    /* dumb bubble sort */
    for (i = 0; i < size; i++)
    {
        for (j = i+1; j < size; j++)
        {
            if (inptrs[j] < inptrs[i])
            {
                /* exchange items */
                tmp = inptrs[j];
                inptrs[j] = inptrs[i];
                inptrs[i] = tmp;
            }
        }
    }

    for (i = 0; i < (size-1); i++)
    {
        outptrs[i] = inptrs[i+1] - inptrs[i];
    }

    outptrs[size - 1] = end - inptrs[size-1];
}

static struct abk_instrument* read_abk_insts(HIO_HANDLE *f, uint32 inst_section_offset, uint32 inst_section_size)
{
    uint16 i;
    uint16 count;
    uint32 savepos;
    struct abk_instrument *inst;

    savepos = hio_tell(f);

    hio_seek(f, inst_section_offset, SEEK_SET);
    count = hio_read16b(f);

    inst = (struct abk_instrument*) malloc(count * sizeof(struct abk_instrument));
    memset(inst, 0, count * sizeof(struct abk_instrument));

    uint32 *offsets = (uint32 *) malloc(count * sizeof(uint32));
    uint32 *sizes = (uint32 *) malloc(count * sizeof(uint32));

    for (i = 0; i < count; i++)
    {
        read_abk_inst(f, &inst[i], AMOS_MAIN_HEADER + inst_section_offset);
        offsets[i] = inst[i].sample_offset;
    }

    /* should have all the instruments.
     * now we need to fix all the lengths */
    sortandsize(count, offsets, sizes, inst_section_size);

#if 0
    /* update the ordered struct. */
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < count; j++)
        {
            if (inst[i].sample_offset == offsets[j])
            {
                inst[i].sample_length = sizes[j]/2;
                break;
            }
        }
    }
#endif

    free(offsets);
    free(sizes);

    hio_seek(f, savepos, SEEK_SET);
    return inst;
}

/**
 * @brief compute the size of the instrument data section.
 * @param head the main ABK header (populated)
 * @param size the size of the entire file from the start of the main header.
 * @return the size of the instruments section.
 */
static uint32 abk_inst_section_size(struct abk_header *head, int size)
{
    int i;
    uint32 result = 0;

    uint32 offsets[ABK_HEADER_SECTION_COUNT];
    uint32 sizes[ABK_HEADER_SECTION_COUNT];

    offsets[0] = head->instruments_offset;
    offsets[1] = head->patterns_offset;
    offsets[2] = head->songs_offset;

    memset(sizes, 0, ABK_HEADER_SECTION_COUNT*sizeof(uint32));

    sortandsize(ABK_HEADER_SECTION_COUNT, offsets, sizes, size);

    for (i=0; i<3; i++)
    {
        if (offsets[i] == head->instruments_offset)
        {
            result = sizes[i];
        }
    }

    return result;
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
    int i,j,k;
    uint16 pattern;
    uint32 first_sample_offset;
    uint32 inst_section_size;
    uint32 file_size;

    uint8 *bad_patterns; /* skip these patterns and dont encode them */
    struct xmp_module *mod = &m->mod;

    struct abk_header main_header;
    struct abk_instrument *ci;
    struct abk_song song;
    struct abk_playlist playlist;

    ci = NULL;
    pattern = 0;
    first_sample_offset = 0;

    hio_seek(f, 0, SEEK_END);
    file_size = hio_tell(f);

    hio_seek(f, AMOS_MAIN_HEADER, SEEK_SET);

    main_header.instruments_offset = hio_read32b(f);
    main_header.songs_offset = hio_read32b(f);
    main_header.patterns_offset = hio_read32b(f);

    inst_section_size = abk_inst_section_size(&main_header, file_size - AMOS_MAIN_HEADER);
    D_(D_INFO "Sample Bytes: %d", inst_section_size);

    LOAD_INIT();

    set_type(m, "AMOS Music Bank");

    if (read_abk_song(f, &song, AMOS_MAIN_HEADER + main_header.songs_offset) < 0)
    {
        return -1;
    }

    copy_adjust(mod->name, (uint8*) song.song_name, AMOS_STRING_LEN);

    MODULE_INFO();

    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset, SEEK_SET);

    mod->chn = AMOS_ABK_CHANNELS;
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

    /* read all the instruments in */
    ci = read_abk_insts(f, AMOS_MAIN_HEADER + main_header.instruments_offset, inst_section_size);

    for (i = 0; i < mod->ins; i++)
    {
        if (subinstrument_alloc(mod, i, 1) < 0)
        {
            return -1;
        }

        mod->xxs[i].len = (ci[i].sample_length)<<1;

        if (mod->xxs[i].len > 0)
        {
            mod->xxi[i].nsm = 1;
        }

        /* store the location of the first sample so we can read them later. */
        if (first_sample_offset == 0)
        {
            first_sample_offset = AMOS_MAIN_HEADER + main_header.instruments_offset + ci[0].sample_offset;
        }

        /* the repeating stuff. */
        if (ci[i].repeat_offset > ci[i].sample_offset)
        {
            mod->xxs[i].lps = (ci[i].repeat_offset - ci[i].sample_offset) << 1;
        }
        else
        {
            mod->xxs[i].lps = 0;
        }
        mod->xxs[i].lpe = ci[i].repeat_end;
        if (mod->xxs[i].lpe > 2) {
            mod->xxs[i].lpe <<= 1;
            mod->xxs[i].flg = XMP_SAMPLE_LOOP;
        }
/*printf("%02x lps=%04x lpe=%04x\n", i,  mod->xxs[i].lps, mod->xxs[i].lpe);*/

        mod->xxi[i].sub[0].vol = ci[i].sample_volume;
        mod->xxi[i].sub[0].pan = 0x80;
        mod->xxi[i].sub[0].sid = i;

        instrument_name(mod, i, (uint8*)ci[i].sample_name, 16);

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

    /* move to the start of the instruments section */
    /* then convert the patterns one at a time. there is a pattern for each channel.*/
    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset + 2, SEEK_SET);

    mod->len = 0;

    bad_patterns = (uint8 *) malloc(sizeof(int)*mod->pat);
    memset(bad_patterns, 0, sizeof(int)*mod->pat);

    i = 0;
    for (j = 0; j < mod->pat; j++)
    {
        uint32 savepos = hio_tell(f);

        /* just read the size first time.*/
        /* we'll use that size to define the row size then do the conversion.*/

        uint16 patternsize = 0;

        for (k = 0; k  < mod->chn; k++)
        {
            pattern = hio_read16b(f);
            uint16 tmp = read_abk_pattern(f, NULL, AMOS_MAIN_HEADER + main_header.patterns_offset + pattern);

            if ((k > 0) && (tmp != patternsize))
            {
                D_(D_WARN "Pattern lengths do not match for pattern: %d", j);
            }

            patternsize = tmp > patternsize ? tmp : patternsize;
        }

        hio_seek(f, savepos, SEEK_SET);

        if (patternsize == 0)
        {
            bad_patterns[j] = 1;
            D_(D_WARN "Zero length pattern detected: %d", j);
            continue;
        }

        if (pattern_tracks_alloc(mod, i, patternsize) < 0)
        {
            free(bad_patterns);
            return -1;
        }

        for (k = 0; k  < mod->chn; k++)
        {
            pattern = hio_read16b(f);
            read_abk_pattern(f,  mod->xxt[(i*mod->chn)+k]->event, AMOS_MAIN_HEADER + main_header.patterns_offset + pattern);
        }

        i++;
    }

	/* now push all the patterns into the module and set the length */
    i = 0;
	
    for (j=0; j< playlist.length; j++)
    {
        if (bad_patterns[playlist.pattern[j]] == 0)
        {
            /* increment the length */
            mod->len++;
            mod->xxo[i++] = playlist.pattern[j];
        }
        else
        {
            D_(D_WARN "Skipping playlist item: %i", playlist.pattern[j]);
        }
    }

    /* free up some memory here */
    free(bad_patterns);
    free(ci);
    free_abk_playlist(&playlist);

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
