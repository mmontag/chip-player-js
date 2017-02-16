/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2017 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
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
 */

#ifndef ADLMIDI_H
#define ADLMIDI_H

#ifdef __cplusplus
extern "C" {
#endif

enum ADLMIDI_VolumeModels
{
    ADLMIDI_VolumeModel_AUTO = 0,
    ADLMIDI_VolumeModel_Generic,
    ADLMIDI_VolumeModel_CMF,
    ADLMIDI_VolumeModel_DMX,
    ADLMIDI_VolumeModel_APOGEE,
    ADLMIDI_VolumeModel_9X
};

struct ADL_MIDIPlayer
{
    unsigned int AdlBank;
    unsigned int NumFourOps;
    unsigned int NumCards;
    unsigned int HighTremoloMode;
    unsigned int HighVibratoMode;
    unsigned int AdlPercussionMode;
    unsigned int LogarithmicVolumes;
    int VolumeModel;
    unsigned int QuitFlag;
    unsigned int SkipForward;
    unsigned int QuitWithoutLooping;
    unsigned int ScaleModulators;
    double delay;
    double carry;

    /* The lag between visual content and audio content equals */
    /* the sum of these two buffers. */
    double mindelay;
    double maxdelay;

    /* For internal usage */
    int     stored_samples; /* num of collected samples */
    short   backup_samples[1024]; /* Backup sample storage. */
    int     backup_samples_size; /* Backup sample storage. */
    /* For internal usage */

    void *adl_midiPlayer;
    unsigned long PCM_RATE;
};

/* Sets number of emulated sound cards (from 1 to 100). Emulation of multiple sound cards exchanges polyphony limits*/
extern int adl_setNumCards(struct ADL_MIDIPlayer *device, int numCards);

/* Sets a number of the patches bank from 0 to N banks */
extern int adl_setBank(struct ADL_MIDIPlayer *device, int bank);

/* Returns total number of available banks */
extern int adl_getBanksCount();

/* Returns pointer to array of names of every bank */
extern const char *const *adl_getBankNames();

/*Sets number of 4-chan operators*/
extern int adl_setNumFourOpsChn(struct ADL_MIDIPlayer *device, int ops4);

/*Enable or disable AdLib percussion mode*/
extern void adl_setPercMode(struct ADL_MIDIPlayer *device, int percmod);

/*Enable or disable deep vibrato*/
extern void adl_setHVibrato(struct ADL_MIDIPlayer *device, int hvibro);

/*Enable or disable deep tremolo*/
extern void adl_setHTremolo(struct ADL_MIDIPlayer *device, int htremo);

/*Enable or disable Enables scaling of modulator volumes*/
extern void adl_setScaleModulators(struct ADL_MIDIPlayer *device, int smod);

/*Enable or disable built-in loop (built-in loop supports 'loopStart' and 'loopEnd' tags to loop specific part)*/
extern void adl_setLoopEnabled(struct ADL_MIDIPlayer *device, int loopEn);

/*Enable or disable Logariphmic volume changer */
extern void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol);

/*Set different volume range model */
extern void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel);


/*Returns string which contains last error message*/
extern const char *adl_errorString();

/*Initialize ADLMIDI Player device*/
extern struct ADL_MIDIPlayer *adl_init(long sample_rate);

/*Load MIDI file from File System*/
extern int adl_openFile(struct ADL_MIDIPlayer *device, char *filePath);

/*Load MIDI file from memory data*/
extern int adl_openData(struct ADL_MIDIPlayer *device, void *mem, long size);

/*Resets MIDI player*/
extern void adl_reset(struct ADL_MIDIPlayer *device);

/*Close and delete ADLMIDI device*/
extern void adl_close(struct ADL_MIDIPlayer *device);

/*Take a sample buffer*/
extern int  adl_play(struct ADL_MIDIPlayer *device, int sampleCount, short out[]);

#ifdef __cplusplus
}
#endif

#endif /* ADLMIDI_H */
