
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <SDL2/SDL.h>

#include <adlmidi.h>

class MutexType
{
    SDL_mutex* mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType() { SDL_DestroyMutex(mut); }
    void Lock() { SDL_mutexP(mut); }
    void Unlock() { SDL_mutexV(mut); }
};


static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;

static void SDL_AudioCallbackX(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
    {
        target[a] = AudioBuffer[a];
    }
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}



#undef main
int main(int argc, char** argv)
{

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
        );
/*
        for(unsigned a=0; a<sizeof(banknames)/sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                a?"":"Banks:",
                a,
                banknames[a]);
*/
        std::printf(
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
    spec.samples  = spec.freq * AudioBufferLength;
    spec.callback = SDL_AudioCallbackX;

    // Set up SDL
    if(SDL_OpenAudio(&spec, &obtained) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        //return 1;
    }
    if(spec.samples != obtained.samples)
        std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
            spec.samples,    spec.freq,    spec.channels,
            obtained.samples,obtained.freq,obtained.channels);

    ADL_MIDIPlayer* myDevice;
    myDevice = adl_init(44100);
    if(myDevice==NULL)
    {
        std::fprintf(stderr, "Failed to init MIDI device!\n");
        return 1;
    }


    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);
        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);
        else if(!std::strcmp("-nl", argv[2]))
            adl_setLoopEnabled(myDevice, 0);
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);
        else break;

        std::copy(argv + (had_option ? 4 : 3), argv + argc,
                  argv+2);
        argc -= (had_option ? 2 : 1);
    }

    if(argc >= 3)
    {
        int bankno = std::atoi(argv[2]);
        if(adl_setBank(myDevice, bankno)!=0)
        {
            std::fprintf(stderr,"%s", adl_errorString());
            return 0;
        }
    }

    if(argc >= 4)
    {
        if(adl_setNumCards(myDevice, std::atoi(argv[3])) != 0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }
    if(argc >= 5)
    {
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4]))!=0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }

    if(adl_openFile(myDevice, argv[1])!=0)
    {
        std::fprintf(stderr, "%s\n", adl_errorString());
        return 2;
    }

    SDL_PauseAudio(0);

    while(1)
    {
        int buff[4096];
        unsigned long gotten=adl_play(myDevice, 4096, buff);
        if(gotten<=0) break;

        AudioBuffer_lock.Lock();
            size_t pos = AudioBuffer.size();
            AudioBuffer.resize(pos + gotten);
            for(unsigned long p = 0; p < gotten; ++p)
               AudioBuffer[pos+p] = buff[p];
        AudioBuffer_lock.Unlock();

        const SDL_AudioSpec& spec_ = obtained;
        while(AudioBuffer.size() > spec_.samples + (spec_.freq*2) * OurHeadRoomLength)
        {
            SDL_Delay(1);
        }
    }

    adl_close(myDevice);
    SDL_CloseAudio();
    return 0;
}

