#include <SDL2/SDL.h>
#include <adlmidi.h>

/* prototype for our audio callback */
/* see the implementation for more information */
void my_audio_callback(void *midi_player, Uint8 *stream, int len);

/* variable declarations */
static Uint32 is_playing = 0; /* remaining length of the sample we have to play */
static short buffer[4096]; /* Audio buffer */


int main(int argc, char *argv[])
{
    /* local variables */
    static SDL_AudioSpec            spec; /* the specs of our piece of music */
    static struct ADL_MIDIPlayer    *midi_player = NULL; /* Instance of ADLMIDI player */
    static const char               *music_path = NULL; /* Path to music file */

    if (argc < 2)
    {
        fprintf(stderr, "\n"
                        "\n"
                        "No given files to play!\n"
                        "\n"
                        "Syntax: %s <path-to-MIDI-file>\n"
                        "\n", argv[0]);
        return 2;
    }

    music_path = argv[1];

    /* Initialize SDL.*/
    if(SDL_Init(SDL_INIT_AUDIO) < 0)
        return 1;

    spec.freq = 44100;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 2048;

    /* Initialize ADLMIDI */
    midi_player = adl_init(spec.freq);
    if (!midi_player)
    {
        fprintf(stderr, "Couldn't initialize ADLMIDI: %s\n", adl_errorString());
        return 1;
    }

    /* set the callback function */
    spec.callback = my_audio_callback;
    /* set ADLMIDI's descriptor as userdata to use it for sound generation */
    spec.userdata = midi_player;

    /* Open the audio device */
    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }

    /* Optionally Setup ADLMIDI as you want */

    /* Set using of embedded bank by ID */
    /*adl_setBank(midi_player, 68);*/

    /* Set using of custom bank (WOPL format) loaded from a file */
    /*adl_openBankFile(midi_player, "/home/vitaly/Yandex.Disk/Музыка/Wolfinstein.wopl");*/

    /* Open the MIDI (or MUS, IMF or CMF) file to play */
    if (adl_openFile(midi_player, music_path) < 0)
    {
        fprintf(stderr, "Couldn't open music file: %s\n", adl_errorInfo(midi_player));
        SDL_CloseAudio();
        adl_close(midi_player);
        return 1;
    }

    is_playing = 1;
    /* Start playing */
    SDL_PauseAudio(0);

    printf("Playing... Hit Ctrl+C to quit!\n");

    /* wait until we're don't playing */
    while (is_playing)
    {
        SDL_Delay(100);
    }

    /* shut everything down */
    SDL_CloseAudio();
    adl_close(midi_player);

    return 0;
}

/*
 audio callback function
 here you have to copy the data of your audio buffer into the
 requesting audio buffer (stream)
 you should only copy as much as the requested length (len)
*/
void my_audio_callback(void *midi_player, Uint8 *stream, int len)
{
    struct ADL_MIDIPlayer* p = (struct ADL_MIDIPlayer*)midi_player;

    /* Convert bytes length into total count of samples in all channels */
    int samples_count = len / 2;

    /* Take some samples from the ADLMIDI */
    samples_count = adl_play(p, samples_count, (short*)buffer);

    if(samples_count <= 0)
    {
        is_playing = 0;
        SDL_memset(stream, 0, len);
        return;
    }

    /* Send buffer to the audio device */
    SDL_memcpy(stream, (Uint8*)buffer, samples_count * 2);
}

