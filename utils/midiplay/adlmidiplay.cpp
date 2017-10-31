
#include <vector>
#include <string>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <signal.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <adlmidi.h>

#include "wave_writer.h"

class MutexType
{
    SDL_mutex *mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType()
    {
        SDL_DestroyMutex(mut);
    }
    void Lock()
    {
        SDL_mutexP(mut);
    }
    void Unlock()
    {
        SDL_mutexV(mut);
    }
};


static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;

static void SDL_AudioCallbackX(void *, Uint8 *stream, int len)
{
    SDL_LockAudio();
    short *target = (short *) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = (unsigned)len / 2; // number of shorts
    if(ate > AudioBuffer.size())
        ate = (unsigned)AudioBuffer.size();
    for(unsigned a = 0; a < ate; ++a)
    {
        target[a] = AudioBuffer[a];
    }
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}

static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

static void printError(const char *err)
{
    std::fprintf(stderr, "\nERROR: %s\n\n", err);
    std::fflush(stderr);
}

static int stop = 0;
static void sighandler(int dum)
{
    if((dum == SIGINT)
        || (dum == SIGTERM)
    #ifndef _WIN32
        || (dum == SIGHUP)
    #endif
    )
        stop = 1;
}

int main(int argc, char **argv)
{
    std::fprintf(stdout, "==========================================\n"
                         "         libADLMIDI demo utility\n"
                         "==========================================\n\n");
    std::fflush(stdout);

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <bank> [ <numcards> [ <numfourops>] ] ]\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
            "\n"
            "Where <bank> - number of embeeded bank or filepath to custom WOPL bank file\n"
            "\n"
            "Note: To create WOPL bank files use OPL Bank Editor you can get here: \n"
            "https://github.com/Wohlstand/OPL3BankEditor\n"
            "\n"
        );

        int banksCount = adl_getBanksCount();
        const char *const *banknames = adl_getBankNames();

        if(banksCount > 0)
        {
            std::printf("    Available embedded banks by number:\n\n");

            for(int a = 0; a < banksCount; ++a)
                std::printf("%10s%2u = %s\n", a ? "" : "Banks:", a, banknames[a]);

            std::printf(
                "\n"
                "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
                "     Look up the relevant bank number from descent.sng.\n"
                "\n"
                "     The fourth parameter can be used to specify the number\n"
                "     of four-op channels to use. Each four-op channel eats\n"
                "     the room of two regular channels. Use as many as required.\n"
                "     The Doom & Hexen sets require one or two, while\n"
                "     Miles four-op set requires the maximum of numcards*6.\n"
                "\n"
            );
        }
        else
        {
            std::printf("    This build of libADLMIDI has no embedded banks!\n\n");
        }

        std::fflush(stdout);

        return 0;
    }

    //const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.08;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.
    SDL_AudioSpec spec;
    SDL_AudioSpec obtained;

    spec.freq     = 44100;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = Uint16((double)spec.freq * AudioBufferLength);
    spec.callback = SDL_AudioCallbackX;

    ADL_MIDIPlayer *myDevice;
    myDevice = adl_init(44100);
    if(myDevice == NULL)
    {
        printError("Failed to init MIDI device!\n");
        return 1;
    }

    bool recordWave = false;
    int loopEnabled = 1;

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);
        else if(!std::strcmp("-w", argv[2]))
            recordWave = true;
        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0;
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);
        else break;

        std::copy(argv + (had_option ? 4 : 3), argv + argc,
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

    //Turn loop on/off (for WAV recording loop must be disabled!)
    adl_setLoopEnabled(myDevice, recordWave ? 0 : loopEnabled);


    std::fprintf(stdout, " - %s OPL3 Emulator in use\n", adl_emulatorName());

    if(!recordWave)
    {
        // Set up SDL
        if(SDL_OpenAudio(&spec, &obtained) < 0)
        {
            std::fprintf(stderr, "\nERROR: Couldn't open audio: %s\n\n", SDL_GetError());
            //return 1;
        }
        if(spec.samples != obtained.samples)
        {
            std::fprintf(stderr, " - Audio wanted (samples=%u,rate=%u,channels=%u);\n"
                                 " - Audio obtained (samples=%u,rate=%u,channels=%u)\n",
                         spec.samples,    spec.freq,    spec.channels,
                         obtained.samples, obtained.freq, obtained.channels);
        }
    }

    if(argc >= 3)
    {
        if(is_number(argv[2]))
        {
            int bankno = std::atoi(argv[2]);
            if(adl_setBank(myDevice, bankno) != 0)
            {
                printError(adl_errorString());
                return 1;
            }
            std::fprintf(stdout, " - Use embedded bank #%d [%s]\n", bankno, adl_getBankNames()[bankno]);
        }
        else
        {
            std::fprintf(stdout, " - Use custom bank [%s]...", argv[2]);
            std::fflush(stdout);
            if(adl_openBankFile(myDevice, argv[2]) != 0)
            {
                std::fprintf(stdout, "FAILED!\n");
                std::fflush(stdout);
                printError(adl_errorString());
                return 1;
            }
            std::fprintf(stdout, "OK!\n");
        }
    }

    int numOfChips = 4;
    if(argc >= 4)
        numOfChips = std::atoi(argv[3]);

    if(adl_setNumCards(myDevice, numOfChips) != 0)
    {
        printError(adl_errorString());
        return 1;
    }
    std::fprintf(stdout, " - Number of chips %d\n", numOfChips);

    if(argc >= 5)
    {
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4])) != 0)
        {
            printError(adl_errorString());
            return 1;
        }
        std::fprintf(stdout, " - Number of four-ops %s\n", argv[4]);
    }

    if(adl_openFile(myDevice, argv[1]) != 0)
    {
        printError(adl_errorString());
        return 2;
    }

    std::fflush(stdout);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    #ifndef _WIN32
    signal(SIGHUP, sighandler);
    #endif

    double total        = adl_totalTimeLength(myDevice);
    double loopStart    = adl_loopStartTime(myDevice);
    double loopEnd      = adl_loopEndTime(myDevice);

    if(!recordWave)
    {
        std::fprintf(stdout, " - Loop is turned %s\n", loopEnabled ? "ON" : "OFF");
        if(loopStart >= 0.0 && loopEnd >= 0.0)
            std::fprintf(stdout, " - Has loop points: %10f ... %10f\n", loopStart, loopEnd);
        std::fprintf(stdout, "\n==========================================\n");
        std::fflush(stdout);

        SDL_PauseAudio(0);

        #ifdef DEBUG_SEEKING_TEST
        int delayBeforeSeek = 50;
        std::fprintf(stdout, "DEBUG: === Random position set test is active! ===\n");
        std::fflush(stdout);
        #endif

        while(!stop)
        {
            short buff[4096];
            size_t got = (size_t)adl_play(myDevice, 4096, buff);
            if(got <= 0)
                break;

            std::fprintf(stdout, "                                               \r");
            std::fprintf(stdout, "Time position: %10f / %10f\r", adl_positionTell(myDevice), total);
            std::fflush(stdout);

            AudioBuffer_lock.Lock();
            size_t pos = AudioBuffer.size();
            AudioBuffer.resize(pos + got);
            for(size_t p = 0; p < got; ++p)
                AudioBuffer[pos + p] = buff[p];
            AudioBuffer_lock.Unlock();

            const SDL_AudioSpec &spec = obtained;
            while(AudioBuffer.size() > spec.samples + (spec.freq * 2) * OurHeadRoomLength)
            {
                SDL_Delay(1);
            }

            #ifdef DEBUG_SEEKING_TEST
            if(delayBeforeSeek-- <= 0)
            {
                delayBeforeSeek = rand() % 50;
                double seekTo = double((rand() % int(adl_totalTimeLength(myDevice)) - delayBeforeSeek - 1 ));
                adl_positionSeek(myDevice, seekTo);
            }
            #endif
        }
        std::fprintf(stdout, "                                               \n\n");
        SDL_CloseAudio();
    }
    else
    {
        std::string wave_out = std::string(argv[1]) + ".wav";
        std::fprintf(stdout, " - Recording WAV file %s...\n", wave_out.c_str());
        std::fprintf(stdout, "\n==========================================\n");
        std::fflush(stdout);

        if(wave_open(spec.freq, wave_out.c_str()) == 0)
        {
            wave_enable_stereo();
            while(!stop)
            {
                short buff[4096];
                size_t got = (size_t)adl_play(myDevice, 4096, buff);
                if(got <= 0)
                    break;
                wave_write(buff, (long)got);

                double complete = std::floor(100.0 * adl_positionTell(myDevice) / total);
                std::fprintf(stdout, "                                               \r");
                std::fprintf(stdout, "Recording WAV... [%d%% completed]\r", (int)complete);
                std::fflush(stdout);
            }
            wave_close();
            std::fprintf(stdout, "                                               \n\n");

            if(stop)
                std::fprintf(stdout, "Interrupted! Recorded WAV is incomplete, but playable!\n");
            else
                std::fprintf(stdout, "Completed!\n");
            std::fflush(stdout);
        }
        else
        {
            adl_close(myDevice);
            return 1;
        }
    }

    adl_close(myDevice);

    return 0;
}

