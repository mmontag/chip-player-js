#include <string>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <vector>
#include <algorithm>
#include <signal.h>

#if defined(__WATCOMC__)
#include <stdio.h> // snprintf is here!
#define flushout(stream)
#else
#define flushout(stream) std::fflush(stream)
#endif

#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#define HW_OPL_MSDOS
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __DJGPP__
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/exceptn.h>
#define BIOStimer _farpeekl(_dos_ds, 0x46C)
#endif//__DJGPP__

#ifdef __WATCOMC__
//#include <wdpmi.h>
#include <i86.h>
#include <bios.h>
#include <time.h>

unsigned long biostime(unsigned cmd, unsigned long lon)
{
    long tval = (long)lon;
    _bios_timeofday(cmd, &tval);
    return (unsigned long)tval;
}

#define BIOStimer   biostime(0, 0l)
#define BIOSTICK    55                  /* biostime() increases one tick about
                                   every 55 msec */

void mch_delay(int32_t msec)
{
    /*
     * We busy-wait here.  Unfortunately, delay() and usleep() have been
     * reported to give problems with the original Windows 95.  This is
     * fixed in service pack 1, but not everybody installed that.
     */
    long starttime = biostime(0, 0L);
    while(biostime(0, 0L) < starttime + msec / BIOSTICK);
}

//#define BIOStimer _farpeekl(_dos_ds, 0x46C)
//#define DOSMEM(s,o,t) *(t _far *)(SS_DOSMEM + (DWORD)((o)|(s)<<4))
//#define BIOStimer DOSMEM(0x46, 0x6C, WORD);
//#define VSYNC_STATUS    Ox3da
//#define VSYNC_MASK      Ox08
/* #define SYNC { while(inp(VSYNCSTATUS) & VSYNCMASK);\
      while (!(inp(VSYNCSTATUS) & VSYNCMASK)); } */
#endif//__WATCOMC__

#endif

#include <adlmidi.h>

#ifndef HARDWARE_OPL3

#ifndef OUTPUT_WAVE_ONLY
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#endif

#include "wave_writer.h"

#ifndef OUTPUT_WAVE_ONLY
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

typedef std::deque<uint8_t> AudioBuff;
static AudioBuff g_audioBuffer;
static MutexType g_audioBuffer_lock;
static ADLMIDI_AudioFormat g_audioFormat;

static void SDL_AudioCallbackX(void *, Uint8 *stream, int len)
{
    SDL_LockAudio();
    //short *target = (short *) stream;
    g_audioBuffer_lock.Lock();
    unsigned ate = len; // number of bytes
    if(ate > g_audioBuffer.size())
        ate = (unsigned)g_audioBuffer.size();
    for(unsigned a = 0; a < ate; ++a)
        stream[a] = g_audioBuffer[a];
    g_audioBuffer.erase(g_audioBuffer.begin(), g_audioBuffer.begin() + ate);
    g_audioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}

static const char *SDLAudioToStr(int format)
{
    switch(format)
    {
    case AUDIO_S8:
        return "S8";
    case AUDIO_U8:
        return "U8";
    case AUDIO_S16:
        return "S16";
    case AUDIO_S16MSB:
        return "S16MSB";
    case AUDIO_U16:
        return "U16";
    case AUDIO_U16MSB:
        return "U16MSB";
    case AUDIO_S32:
        return "S32";
    case AUDIO_S32MSB:
        return "S32MSB";
    case AUDIO_F32:
        return "F32";
    case AUDIO_F32MSB:
        return "F32MSB";
    default:
        return "UNK";
    }
}
#endif//OUTPUT_WAVE_ONLY

#endif //HARDWARE_OPL3

static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

static void printError(const char *err)
{
    std::fprintf(stderr, "\nERROR: %s\n\n", err);
    flushout(stderr);
}

static int stop = 0;
#ifndef HARDWARE_OPL3
static void sighandler(int dum)
{
    if((dum == SIGINT)
        || (dum == SIGTERM)
    #if !defined(_WIN32) && !defined(__WATCOMC__)
        || (dum == SIGHUP)
    #endif
    )
        stop = 1;
}
#endif


static void debugPrint(void * /*userdata*/, const char *fmt, ...)
{
    char buffer[4096];
    std::va_list args;
    va_start(args, fmt);
    int rc = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if(rc > 0)
    {
        std::fprintf(stdout, " - Debug: %s\n", buffer);
        flushout(stdout);
    }
}

#ifdef DEBUG_TRACE_ALL_EVENTS
static void debugPrintEvent(void * /*userdata*/, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 * /*data*/, size_t len)
{
    std::fprintf(stdout, " - E: 0x%02X 0x%02X %02d (%d)\r\n", type, subtype, channel, (int)len);
    flushout(stdout);
}
#endif

static inline void secondsToHMSM(double seconds_full, char *hmsm_buffer, size_t hmsm_buffer_size)
{
    double seconds_integral;
    double seconds_fractional = std::modf(seconds_full, &seconds_integral);
    unsigned int milliseconds = static_cast<unsigned int>(std::floor(seconds_fractional * 1000.0));
    unsigned int seconds = static_cast<unsigned int>(std::fmod(seconds_full, 60.0));
    unsigned int minutes = static_cast<unsigned int>(std::floor(seconds_full / 60));
    unsigned int hours   = static_cast<unsigned int>(std::floor(seconds_full / 3600));
    std::memset(hmsm_buffer, 0, hmsm_buffer_size);
    if (hours > 0)
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u:%02u,%03u", hours, minutes, seconds, milliseconds);
    else
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u,%03u", minutes, seconds, milliseconds);
}

int main(int argc, char **argv)
{
    std::fprintf(stdout, "==========================================\n"
                        #ifdef HARDWARE_OPL3
                         "      libADLMIDI demo utility (HW OPL)\n"
                        #else
                         "         libADLMIDI demo utility\n"
                        #endif
                         "==========================================\n\n");
    flushout(stdout);

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <bank> [ <numchips> [ <numfourops>] ] ]\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -frb Enables full-ranged CC74 XG Brightness controller\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
            " -mb Run the test of multibank over embedded. 62, 14, 68, and 74'th banks will be combined into one\n"
            " --solo <track>             Selects a solo track to play\n"
            " --only <track1,...,trackN> Selects a subset of tracks to play\n"
            #ifndef HARDWARE_OPL3
            " -fp Enables full-panning stereo support\n"
            " --emu-nuked  Uses Nuked OPL3 v 1.8 emulator\n"
            " --emu-nuked7 Uses Nuked OPL3 v 1.7.4 emulator\n"
            " --emu-dosbox Uses DosBox 0.74 OPL3 emulator\n"
            #endif
            "\n"
            "Where <bank> - number of embeeded bank or filepath to custom WOPL bank file\n"
            "\n"
            "Note: To create WOPL bank files use OPL Bank Editor you can get here: \n"
            "https://github.com/Wohlstand/OPL3BankEditor\n"
            "\n"
        );

        // Get count of embedded banks (no initialization needed)
        int banksCount = adl_getBanksCount();
        //Get pointer to list of embedded bank names
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

        flushout(stdout);

        return 0;
    }

    long sampleRate = 44100;
    #ifndef HARDWARE_OPL3
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
    #ifndef OUTPUT_WAVE_ONLY
    SDL_AudioSpec spec;
    SDL_AudioSpec obtained;

    spec.freq     = (int)sampleRate;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = Uint16((double)spec.freq * AudioBufferLength);
    spec.callback = SDL_AudioCallbackX;
    #endif //OUTPUT_WAVE_ONLY
    #endif //HARDWARE_OPL3

    ADL_MIDIPlayer *myDevice;

    //Initialize libADLMIDI and create the instance (you can initialize multiple of them!)
    myDevice = adl_init(sampleRate);
    if(myDevice == NULL)
    {
        printError("Failed to init MIDI device!\n");
        return 1;
    }

    //Set internal debug messages hook to print all libADLMIDI's internal debug messages
    adl_setDebugMessageHook(myDevice, debugPrint, NULL);

    /*
     * Set library options by parsing of command line arguments
     */
    bool multibankFromEnbededTest = false;

#ifndef OUTPUT_WAVE_ONLY
    bool recordWave = false;
    int loopEnabled = 1;
#endif

#ifndef HARDWARE_OPL3
    int emulator = ADLMIDI_EMU_NUKED;
#endif

    size_t soloTrack = ~(size_t)0;
    std::vector<size_t> onlyTracks;

#if !defined(HARDWARE_OPL3) && !defined(OUTPUT_WAVE_ONLY)
    g_audioFormat.type = ADLMIDI_SampleType_S16;
    g_audioFormat.containerSize = sizeof(Sint16);
    g_audioFormat.sampleOffset = sizeof(Sint16) * 2;
#endif

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);//Turn on AdLib percussion mode
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);//Force turn on deep vibrato

#if !defined(OUTPUT_WAVE_ONLY) && !defined(HARDWARE_OPL3)
        else if(!std::strcmp("-w", argv[2]))
        {
            //Current Wave output implementation allows only SINT16 output
            g_audioFormat.type = ADLMIDI_SampleType_S16;
            g_audioFormat.containerSize = sizeof(Sint16);
            g_audioFormat.sampleOffset = sizeof(Sint16) * 2;
            recordWave = true;//Record library output into WAV file
        }
        else if(!std::strcmp("-s8", argv[2]) && !recordWave)
            spec.format = AUDIO_S8;
        else if(!std::strcmp("-u8", argv[2]) && !recordWave)
            spec.format = AUDIO_U8;
        else if(!std::strcmp("-s16", argv[2]) && !recordWave)
            spec.format = AUDIO_S16;
        else if(!std::strcmp("-u16", argv[2]) && !recordWave)
            spec.format = AUDIO_U16;
        else if(!std::strcmp("-s32", argv[2]) && !recordWave)
            spec.format = AUDIO_S32;
        else if(!std::strcmp("-f32", argv[2]) && !recordWave)
            spec.format = AUDIO_F32;
#endif

        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);//Force turn on deep tremolo

        else if(!std::strcmp("-frb", argv[2]))
            adl_setFullRangeBrightness(myDevice, 1);//Turn on a full-ranged XG CC74 Brightness

#ifndef OUTPUT_WAVE_ONLY
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0; //Enable loop
#endif

#ifndef HARDWARE_OPL3
        else if(!std::strcmp("--emu-nuked", argv[2]))
            emulator = ADLMIDI_EMU_NUKED;
        else if(!std::strcmp("--emu-nuked7", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_174;
        else if(!std::strcmp("--emu-dosbox", argv[2]))
            emulator = ADLMIDI_EMU_DOSBOX;
#endif
        else if(!std::strcmp("-fp", argv[2]))
            adl_setSoftPanEnabled(myDevice, 1);
        else if(!std::strcmp("-mb", argv[2]))
            multibankFromEnbededTest = true;
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);//Turn on modulators scaling by volume

        else if(!std::strcmp("--solo", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --solo requires an argument!\n");
                return 1;
            }
            soloTrack = std::strtoul(argv[3], NULL, 0);
            had_option = true;
        }
        else if(!std::strcmp("--only", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --only requires an argument!\n");
                return 1;
            }

            const char *strp = argv[3];
            unsigned long value;
            unsigned size;
            bool err = std::sscanf(strp, "%lu%n", &value, &size) != 1;
            while(!err && *(strp += size))
            {
                onlyTracks.push_back(value);
                err = std::sscanf(strp, ",%lu%n", &value, &size) != 1;
            }
            if(err)
            {
                printError("Invalid argument to --only!\n");
                return 1;
            }
            onlyTracks.push_back(value);
            had_option = true;
        }

        else break;

        std::copy(argv + (had_option ? 4 : 3),
                  argv + argc,
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

#ifndef OUTPUT_WAVE_ONLY
    //Turn loop on/off (for WAV recording loop must be disabled!)
    adl_setLoopEnabled(myDevice, recordWave ? 0 : loopEnabled);
#endif

#ifdef DEBUG_TRACE_ALL_EVENTS
    //Hook all MIDI events are ticking while generating an output buffer
    if(!recordWave)
        adl_setRawEventHook(myDevice, debugPrintEvent, NULL);
#endif

#ifndef HARDWARE_OPL3
    adl_switchEmulator(myDevice, emulator);
#endif

    std::fprintf(stdout, " - Library version %s\n", adl_linkedLibraryVersion());
#ifdef HARDWARE_OPL3
    std::fprintf(stdout, " - Hardware OPL3 chip in use\n");
#else
    std::fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));
#endif

#if !defined(HARDWARE_OPL3) && !defined(OUTPUT_WAVE_ONLY)
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
            std::fprintf(stderr, " - Audio wanted (format=%s,samples=%u,rate=%u,channels=%u);\n"
                                 " - Audio obtained (format=%s,samples=%u,rate=%u,channels=%u)\n",
                         SDLAudioToStr(spec.format), spec.samples,    spec.freq,    spec.channels,
                         SDLAudioToStr(obtained.format), obtained.samples, obtained.freq, obtained.channels);
        }
        switch(obtained.format)
        {
        case AUDIO_S8:
            g_audioFormat.type = ADLMIDI_SampleType_S8;
            g_audioFormat.containerSize = sizeof(Sint8);
            g_audioFormat.sampleOffset = sizeof(Sint8) * 2;
            break;
        case AUDIO_U8:
            g_audioFormat.type = ADLMIDI_SampleType_U8;
            g_audioFormat.containerSize = sizeof(Uint8);
            g_audioFormat.sampleOffset = sizeof(Uint8) * 2;
            break;
        case AUDIO_S16:
            g_audioFormat.type = ADLMIDI_SampleType_S16;
            g_audioFormat.containerSize = sizeof(Sint16);
            g_audioFormat.sampleOffset = sizeof(Sint16) * 2;
            break;
        case AUDIO_U16:
            g_audioFormat.type = ADLMIDI_SampleType_U16;
            g_audioFormat.containerSize = sizeof(Uint16);
            g_audioFormat.sampleOffset = sizeof(Uint16) * 2;
            break;
        case AUDIO_S32:
            g_audioFormat.type = ADLMIDI_SampleType_S32;
            g_audioFormat.containerSize = sizeof(Sint32);
            g_audioFormat.sampleOffset = sizeof(Sint32) * 2;
            break;
        case AUDIO_F32:
            g_audioFormat.type = ADLMIDI_SampleType_F32;
            g_audioFormat.containerSize = sizeof(float);
            g_audioFormat.sampleOffset = sizeof(float) * 2;
            break;
        }
    }
#endif

    if(argc >= 3)
    {
        if(is_number(argv[2]))
        {
            int bankno = std::atoi(argv[2]);
            //Choose one of embedded banks
            if(adl_setBank(myDevice, bankno) != 0)
            {
                printError(adl_errorInfo(myDevice));
                return 1;
            }
            std::fprintf(stdout, " - Use embedded bank #%d [%s]\n", bankno, adl_getBankNames()[bankno]);
        }
        else
        {
            std::string bankPath = argv[2];
            std::fprintf(stdout, " - Use custom bank [%s]...", bankPath.c_str());
            flushout(stdout);
            //Open external bank file (WOPL format is supported)
            //to create or edit them, use OPL3 Bank Editor you can take here https://github.com/Wohlstand/OPL3BankEditor
            if(adl_openBankFile(myDevice, bankPath.c_str()) != 0)
            {
                std::fprintf(stdout, "FAILED!\n");
                flushout(stdout);
                printError(adl_errorInfo(myDevice));
                return 1;
            }
            std::fprintf(stdout, "OK!\n");
        }
    }

    if(multibankFromEnbededTest)
    {
        ADL_BankId id[] =
        {
            {0, 0, 0}, /*62*/ // isPercussion, MIDI bank MSB, LSB
            {0, 8, 0}, /*14*/ // Use as MSB-8
            {1, 0, 0}, /*68*/
            {1, 0, 25} /*74*/
        };
        int banks[] =
        {
            62, 14, 68, 74
        };

        for(size_t i = 0; i < 4; i++)
        {
            ADL_Bank bank;
            if(adl_getBank(myDevice, &id[i], ADLMIDI_Bank_Create, &bank) < 0)
            {
                printError(adl_errorInfo(myDevice));
                return 1;
            }

            if(adl_loadEmbeddedBank(myDevice, &bank, banks[i]) < 0)
            {
                printError(adl_errorInfo(myDevice));
                return 1;
            }
        }

        std::fprintf(stdout, " - Ran a test of multibank over embedded\n");
    }

#ifndef HARDWARE_OPL3
    int numOfChips = 4;
    if(argc >= 4)
        numOfChips = std::atoi(argv[3]);

    //Set count of concurrent emulated chips count to excite channels limit of one chip
    if(adl_setNumChips(myDevice, numOfChips) != 0)
    {
        printError(adl_errorInfo(myDevice));
        return 1;
    }
#else
    int numOfChips = 1;
    adl_setNumChips(myDevice, numOfChips);
#endif

    if(argc >= 5)
    {
        //Set total count of 4-operator channels between all emulated chips
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4])) != 0)
        {
            printError(adl_errorInfo(myDevice));
            return 1;
        }
    }

    std::string musPath = argv[1];
    //Open MIDI file to play
    if(adl_openFile(myDevice, musPath.c_str()) != 0)
    {
        printError(adl_errorInfo(myDevice));
        return 2;
    }

    std::fprintf(stdout, " - Number of chips %d\n", adl_getNumChipsObtained(myDevice));
    std::fprintf(stdout, " - Number of four-ops %d\n", adl_getNumFourOpsChnObtained(myDevice));
    std::fprintf(stdout, " - Track count: %lu\n", (unsigned long)adl_trackCount(myDevice));

    if(soloTrack != ~(size_t)0)
    {
        std::fprintf(stdout, " - Solo track: %lu\n", (unsigned long)soloTrack);
        adl_setTrackOptions(myDevice, soloTrack, ADLMIDI_TrackOption_Solo);
    }

    if(!onlyTracks.empty())
    {
        size_t count = adl_trackCount(myDevice);
        for(size_t track = 0; track < count; ++track)
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_Off);
        std::fprintf(stdout, " - Only tracks:");
        for(size_t i = 0, n = onlyTracks.size(); i < n; ++i)
        {
            size_t track = onlyTracks[i];
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_On);
            std::fprintf(stdout, " %lu", (unsigned long)track);
        }
        std::fprintf(stdout, "\n");
    }

    std::fprintf(stdout, " - File [%s] opened!\n", musPath.c_str());
    flushout(stdout);

#ifndef HARDWARE_OPL3
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#   if !defined(_WIN32) && !defined(__WATCOMC__)
    signal(SIGHUP, sighandler);
#   endif

#else//HARDWARE_OPL3
    static const unsigned NewTimerFreq = 209;
    unsigned TimerPeriod = 0x1234DDul / NewTimerFreq;

    #ifdef __DJGPP__
    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, TimerPeriod & 0xFF);
    outportb(0x40, TimerPeriod >>   8);
    //enable();
    #endif//__DJGPP__

    #ifdef __WATCOMC__
    std::fprintf(stdout, " - Initializing BIOS timer...\n");
    flushout(stdout);
    //disable();
    outp(0x43, 0x34);
    outp(0x40, TimerPeriod & 0xFF);
    outp(0x40, TimerPeriod >>   8);
    //enable();
    std::fprintf(stdout, " - Ok!\n");
    flushout(stdout);
    #endif//__WATCOMC__

    unsigned long BIOStimer_begin = BIOStimer;
    double tick_delay = 0.0;
#endif//HARDWARE_OPL3

    double total        = adl_totalTimeLength(myDevice);

#ifndef OUTPUT_WAVE_ONLY
    double loopStart    = adl_loopStartTime(myDevice);
    double loopEnd      = adl_loopEndTime(myDevice);
    char totalHMS[25];
    char loopStartHMS[25];
    char loopEndHMS[25];
    secondsToHMSM(total, totalHMS, 25);
    if(loopStart >= 0.0 && loopEnd >= 0.0)
    {
        secondsToHMSM(loopStart, loopStartHMS, 25);
        secondsToHMSM(loopEnd, loopEndHMS, 25);
    }

#   ifndef HARDWARE_OPL3
    if(!recordWave)
#   endif
    {
        std::fprintf(stdout, " - Loop is turned %s\n", loopEnabled ? "ON" : "OFF");
        if(loopStart >= 0.0 && loopEnd >= 0.0)
            std::fprintf(stdout, " - Has loop points: %s ... %s\n", loopStartHMS, loopEndHMS);
        std::fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

#   ifndef HARDWARE_OPL3
        SDL_PauseAudio(0);
#   endif

#   ifdef DEBUG_SEEKING_TEST
        int delayBeforeSeek = 50;
        std::fprintf(stdout, "DEBUG: === Random position set test is active! ===\n");
        flushout(stdout);
#   endif

#   ifndef HARDWARE_OPL3
        Uint8 buff[16384];
#   endif
        char posHMS[25];
        uint64_t milliseconds_prev = -1;
        while(!stop)
        {
#   ifndef HARDWARE_OPL3
            size_t got = (size_t)adl_playFormat(myDevice, 4096,
                                                buff,
                                                buff + g_audioFormat.containerSize,
                                                &g_audioFormat) * g_audioFormat.containerSize;
            if(got <= 0)
                break;
#   endif

#   ifdef DEBUG_TRACE_ALL_CHANNELS
            enum { TerminalColumns = 80 };
            char channelText[TerminalColumns + 1];
            char channelAttr[TerminalColumns + 1];
            adl_describeChannels(myDevice, channelText, channelAttr, sizeof(channelText));
            std::fprintf(stdout, "%*s\r", TerminalColumns, "");  // erase the line
            std::fprintf(stdout, "%s\n", channelText);
#   endif

#   ifndef DEBUG_TRACE_ALL_EVENTS
            double time_pos = adl_positionTell(myDevice);
            std::fprintf(stdout, "                                               \r");
            uint64_t milliseconds = static_cast<uint64_t>(time_pos * 1000.0);
            if(milliseconds != milliseconds_prev)
            {
                secondsToHMSM(time_pos, posHMS, 25);
                std::fprintf(stdout, "                                               \r");
                std::fprintf(stdout, "Time position: %s / %s\r", posHMS, totalHMS);
                flushout(stdout);
                milliseconds_prev = milliseconds;
            }
#   endif

#   ifndef HARDWARE_OPL3
            g_audioBuffer_lock.Lock();
            size_t pos = g_audioBuffer.size();
            g_audioBuffer.resize(pos + got);
            for(size_t p = 0; p < got; ++p)
                g_audioBuffer[pos + p] = buff[p];
            g_audioBuffer_lock.Unlock();

            const SDL_AudioSpec &spec = obtained;
            while(g_audioBuffer.size() > static_cast<size_t>(spec.samples + (spec.freq * g_audioFormat.sampleOffset) * OurHeadRoomLength))
            {
                SDL_Delay(1);
            }

#       ifdef DEBUG_SEEKING_TEST
            if(delayBeforeSeek-- <= 0)
            {
                delayBeforeSeek = rand() % 50;
                double seekTo = double((rand() % int(adl_totalTimeLength(myDevice)) - delayBeforeSeek - 1 ));
                adl_positionSeek(myDevice, seekTo);
            }
#       endif

#   else//HARDWARE_OPL3
            const double mindelay = 1.0 / NewTimerFreq;

            //__asm__ volatile("sti\nhlt");
            //usleep(10000);
            #ifdef __DJGPP__
            __dpmi_yield();
            #endif
            #ifdef __WATCOMC__
            //dpmi_dos_yield();
            mch_delay((unsigned int)(tick_delay * 1000.0));
            #endif
            static unsigned long PrevTimer = BIOStimer;
            const unsigned long CurTimer = BIOStimer;
            const double eat_delay = (CurTimer - PrevTimer) / (double)NewTimerFreq;
            PrevTimer = CurTimer;
            tick_delay = adl_tickEvents(myDevice, eat_delay, mindelay);
            if(adl_atEnd(myDevice) && tick_delay <= 0)
                stop = true;

            if(kbhit())
            {   // Quit on ESC key!
                int c = getch();
                if(c == 27)
                    stop = true;
            }

#   endif//HARDWARE_OPL3
        }
        std::fprintf(stdout, "                                               \n\n");
#   ifndef HARDWARE_OPL3
        SDL_CloseAudio();
#   endif
    }
#endif //OUTPUT_WAVE_ONLY

#ifndef HARDWARE_OPL3

#   ifndef OUTPUT_WAVE_ONLY
    else
#   endif //OUTPUT_WAVE_ONLY
    {
        std::string wave_out = musPath + ".wav";
        std::fprintf(stdout, " - Recording WAV file %s...\n", wave_out.c_str());
        std::fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

        if(wave_open(sampleRate, wave_out.c_str()) == 0)
        {
            wave_enable_stereo();
            short buff[4096];
            int complete_prev = -1;
            while(!stop)
            {
                size_t got = (size_t)adl_play(myDevice, 4096, buff);
                if(got <= 0)
                    break;
                wave_write(buff, (long)got);

                int complete = static_cast<int>(std::floor(100.0 * adl_positionTell(myDevice) / total));
                flushout(stdout);
                if(complete_prev != complete)
                {
                    std::fprintf(stdout, "                                               \r");
                    std::fprintf(stdout, "Recording WAV... [%d%% completed]\r", complete);
                    std::fflush(stdout);
                    complete_prev = complete;
                }
            }
            wave_close();
            std::fprintf(stdout, "                                               \n\n");

            if(stop)
                std::fprintf(stdout, "Interrupted! Recorded WAV is incomplete, but playable!\n");
            else
                std::fprintf(stdout, "Completed!\n");
            flushout(stdout);
        }
        else
        {
            adl_close(myDevice);
            return 1;
        }
    }
#endif //HARDWARE_OPL3

#ifdef HARDWARE_OPL3

    #ifdef __DJGPP__
    // Fix the skewed clock and reset BIOS tick rate
    _farpokel(_dos_ds, 0x46C, BIOStimer_begin +
              (BIOStimer - BIOStimer_begin)
              * (0x1234DD / 65536.0) / NewTimerFreq);

    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, 0);
    outportb(0x40, 0);
    //enable();
    #endif

    #ifdef __WATCOMC__
    outp(0x43, 0x34);
    outp(0x40, 0);
    outp(0x40, 0);
    #endif

    adl_panic(myDevice); //Shut up all sustaining notes

#endif

    adl_close(myDevice);

    return 0;
}

