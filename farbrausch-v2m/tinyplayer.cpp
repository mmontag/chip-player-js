/*************************************************************************************/
/*************************************************************************************/
/**                                                                                 **/
/**  Tinyplayer - TibV2 example                                                     **/
/**  written by Tammo 'kb' Hinrichs 2000-2008                                       **/
/**  This file is in the public domain                                              **/
/**  "Patient Zero" is (C) Melwyn+LB 2005, do not redistribute                      **/
/**                                                                                 **/
/*************************************************************************************/
/*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "v2mplayer.h"
#include "libv2.h"
#include "v2mconv.h"
#include "sounddef.h"

static V2MPlayer player;
static SDL_AudioDeviceID dev;

////////////////////////////////////////////////////////////////////////////////////////////////////
static void V2mPlayerTitle()
{
    printf("Farbrausch Tiny Music Player v0.dontcare TWO\n");
    printf("Code and Synthesizer (C) 2000-2008 kb/Farbrausch\n");
    printf("Version: %s\n", PROJECTVERSION);
    printf("SDL Port by %s\n\n", PROJECTURL);
}
static void V2mPlayerUsage()
{
    printf("Usage : v2mplayer [options] <input_file_v2m>\n\n");
    printf("options:\n");
    printf("          -b      force power size stdin buffer (int, optional, [0..10])\n");
    printf("          -k      key/auto stop (bool, optional, default = false)\n");
    printf("          -o str  output v2m newest version (string, optional, default = none)\n");
    printf("          -h      this help\n");
}
static void sdl_callback(void *userdata, Uint8 * stream, int len) {
    player.Render((float*) stream, len / 8);
}

static bool init_sdl()
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        SDL_Log("Couldn't initialize SDL: %s\n",SDL_GetError());
        SDL_Quit();
        return false;
    }

    SDL_AudioSpec desired, actual;
    desired.channels = 2;
    desired.freq = 44100;
    desired.samples = 4096;
    desired.format = AUDIO_F32;
    desired.callback = sdl_callback;

    dev = SDL_OpenAudioDevice(NULL, 0, &desired, &actual, 0);
    if (! dev)
    {
        SDL_Log("Failed to open audio, %s\n", SDL_GetError());
        return false;
    }

    return true;
}

static unsigned char* check_and_convert(unsigned char* tune, unsigned int* length)
{
    sdInit();

    if (tune[2] != 0 || tune[3] != 0)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_INPUT, "No valid input file");
        return NULL;
    }
    int version = CheckV2MVersion(tune, *length);
    if (version < 0)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_INPUT, "Failed to Check Version on input file");
        return NULL;
    }

    uint8_t* converted;
    int converted_length;
    ConvertV2M(tune, *length, &converted, &converted_length);
    *length = converted_length;
    free(tune);

    return converted;
}

int main(int argc, char** argv)
{
    V2mPlayerTitle();
    int opt;
    int fbuf = -1;
    int fouts = 0;
    int fkey = 0;
    int fhelp = 0;
    char *foutput;
    while ((opt = getopt(argc, argv, ":b:ko:h")) != -1)
    {
        switch(opt)
        {
            case 'b':
                fbuf = atof(optarg);
                break;
            case 'k':
                fkey = 1;
                break;
            case 'o':
                foutput = optarg;
                fouts = 1;
                break;
            case 'h':
                fhelp = 1;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
        }
    }

    if(fhelp > 0)
    {
        V2mPlayerUsage();
        return 1;
    }
    unsigned char* theTune;
    FILE* file;
    uint64_t size;
    unsigned int blksz = 4096, read = 0, eofcnt = 0;
    char ch;
    if(optind + 1 > argc)
    {
        file = stdin;
        if (fbuf < 0)
        {
            eofcnt = 0;
            size = blksz;
            theTune = (unsigned char*) calloc(1, size);
            if (theTune == NULL)
            {
                fprintf(stderr, "Error memory allocator: %Ld b\n", size);
                exit(1);
            }
            ch = getc(stdin);
            while (ch != EOF || eofcnt < blksz)
            {
                if (ch != EOF) {eofcnt = 0;} else {eofcnt++;}
                if (read == size)
                {
                    size += blksz;
                    theTune = (unsigned char*)realloc(theTune, size * sizeof(unsigned char));
                    if (theTune == NULL)
                    {
                        fprintf(stderr, "Error memory allocator: %Ld b\n", size);
                        exit(1);
                    }
                }
                theTune[read] = ch;
                read++;
                ch = getc(stdin);
            }
            read -= eofcnt;
        } else {
            if (fbuf < 0 || fbuf > 10) fbuf = 4;
            fbuf += 20;
            size = 1 << fbuf;
            theTune = (unsigned char*) calloc(1, size);
            if (theTune == NULL)
            {
                fprintf(stderr, "Error memory allocator: %Ld b\n", size);
                exit(1);
            }
            read = fread(theTune, 1, size, file);
        }
        printf("Now Playing: stdin(%d[%Ld])\n", read, size);
        size = read;
    } else {
        const char *v2m_filename = argv[optind];

        file = fopen(v2m_filename, "r");
        if (file == NULL)
        {
            fprintf(stderr, "Failed to open %s\n", v2m_filename);
            return 1;
        }

        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);
        printf("Now Playing: %s\n", v2m_filename);
        theTune = (unsigned char*) calloc(1, size);
        if (theTune == NULL)
        {
            fprintf(stderr, "Error memory allocator: %Ld b\n", size);
            exit(1);
        }
        read = fread(theTune, 1, size, file);
    }
    if (size != read)
    {
        fprintf(stderr, "Invalid read size\n");
        return 1;
    }
    fclose(file);
    theTune = check_and_convert(theTune, &read);
    if (theTune == NULL)
    {
        fprintf(stderr, "Error convert to newest\n");
        exit(1);
    }
    printf("Convert: %d b\n", read);
    while (theTune[read--] == 0){}
    read++;
    read++;
    printf("Strip: %d b\n", read);

    if (fouts)
    {
        FILE* fout;
        fout = fopen(foutput, "a");
        if (fout == NULL)
        {
            fprintf(stderr, "Failed to open %s\n", foutput);
            return 1;
        }
        fwrite(theTune, 1, read, fout);
        fclose(fout);
    }

    player.Init();
    player.Open(theTune);

    if (! init_sdl()) {
        return 1;
    }

    player.Play();
    SDL_PauseAudioDevice(dev, 0);

    printf("Length: %d\n", player.Length());
    if (fkey > 0)
    {
        printf("\n\npress Enter to quit\n");
        getc(stdin);
    } else {
        while(player.IsPlaying()) { sleep(1); }
    }

    SDL_PauseAudioDevice(dev, 1);
    SDL_Quit();
    player.Close();
    free(theTune);
    return 0;
}
