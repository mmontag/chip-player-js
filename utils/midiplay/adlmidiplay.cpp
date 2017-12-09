#include <string>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <algorithm>
#include <signal.h>

#if defined(__WATCOMC__)
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

typedef std::deque<int16_t> AudioBuff;
static AudioBuff g_audioBuffer;
static MutexType g_audioBuffer_lock;

static void SDL_AudioCallbackX(void *, Uint8 *stream, int len)
{
    SDL_LockAudio();
    short *target = (short *) stream;
    g_audioBuffer_lock.Lock();
    unsigned ate = (unsigned)len / 2; // number of shorts
    if(ate > g_audioBuffer.size())
        ate = (unsigned)g_audioBuffer.size();
    for(unsigned a = 0; a < ate; ++a)
        target[a] = g_audioBuffer[a];
    g_audioBuffer.erase(g_audioBuffer.begin(), g_audioBuffer.begin() + ate);
    g_audioBuffer_lock.Unlock();
    SDL_UnlockAudio();
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
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
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
    #ifndef OUTPUT_WAVE_ONLY
    bool recordWave = false;
    int loopEnabled = 1;
    #endif
    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);//Turn on AdLib percussion mode
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);//Turn on deep vibrato
        #ifndef OUTPUT_WAVE_ONLY
        else if(!std::strcmp("-w", argv[2]))
            recordWave = true;//Record library output into WAV file
        #endif
        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);//Turn on deep tremolo
        #ifndef OUTPUT_WAVE_ONLY
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0; //Enable loop
        #endif
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);//Turn on modulators scaling by volume
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

    #ifdef HARDWARE_OPL3
    std::fprintf(stdout, " - Hardware OPL3 chip in use\n");
    #else
    std::fprintf(stdout, " - %s OPL3 Emulator in use\n", adl_emulatorName());
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
            std::fprintf(stderr, " - Audio wanted (samples=%u,rate=%u,channels=%u);\n"
                                 " - Audio obtained (samples=%u,rate=%u,channels=%u)\n",
                         spec.samples,    spec.freq,    spec.channels,
                         obtained.samples, obtained.freq, obtained.channels);
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
    std::fprintf(stdout, " - Number of chips %d\n", adl_getNumChips(myDevice));
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
    std::fprintf(stdout, " - Number of four-ops %d\n", adl_getNumFourOpsChn(myDevice));

    std::string musPath = argv[1];
    //Open MIDI file to play
    if(adl_openFile(myDevice, musPath.c_str()) != 0)
    {
        printError(adl_errorInfo(myDevice));
        return 2;
    }

    std::fprintf(stdout, " - File [%s] opened!\n", musPath.c_str());
    flushout(stdout);

    #ifndef HARDWARE_OPL3
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    #if !defined(_WIN32) && !defined(__WATCOMC__)
    signal(SIGHUP, sighandler);
    #endif
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

    #ifndef HARDWARE_OPL3
    if(!recordWave)
    #endif
    {
        std::fprintf(stdout, " - Loop is turned %s\n", loopEnabled ? "ON" : "OFF");
        if(loopStart >= 0.0 && loopEnd >= 0.0)
            std::fprintf(stdout, " - Has loop points: %10f ... %10f\n", loopStart, loopEnd);
        std::fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

        #ifndef HARDWARE_OPL3
        SDL_PauseAudio(0);
        #endif

        #ifdef DEBUG_SEEKING_TEST
        int delayBeforeSeek = 50;
        std::fprintf(stdout, "DEBUG: === Random position set test is active! ===\n");
        flushout(stdout);
        #endif

        while(!stop)
        {
            #ifndef HARDWARE_OPL3
            short buff[4096];
            size_t got = (size_t)adl_play(myDevice, 4096, buff);
            if(got <= 0)
                break;
            #endif

            #ifndef DEBUG_TRACE_ALL_EVENTS
            std::fprintf(stdout, "                                               \r");
            std::fprintf(stdout, "Time position: %10f / %10f\r", adl_positionTell(myDevice), total);
            flushout(stdout);
            #endif

            #ifndef HARDWARE_OPL3
            g_audioBuffer_lock.Lock();
            size_t pos = g_audioBuffer.size();
            g_audioBuffer.resize(pos + got);
            for(size_t p = 0; p < got; ++p)
                g_audioBuffer[pos + p] = buff[p];
            g_audioBuffer_lock.Unlock();

            const SDL_AudioSpec &spec = obtained;
            while(g_audioBuffer.size() > spec.samples + (spec.freq * 2) * OurHeadRoomLength)
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

            #else//HARDWARE_OPL3
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

            #endif//HARDWARE_OPL3
        }
        std::fprintf(stdout, "                                               \n\n");
        #ifndef HARDWARE_OPL3
        SDL_CloseAudio();
        #endif
    }
    #endif //OUTPUT_WAVE_ONLY

    #ifndef HARDWARE_OPL3

    #ifndef OUTPUT_WAVE_ONLY
    else
    #endif //OUTPUT_WAVE_ONLY
    {
        std::string wave_out = musPath + ".wav";
        std::fprintf(stdout, " - Recording WAV file %s...\n", wave_out.c_str());
        std::fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

        if(wave_open(sampleRate, wave_out.c_str()) == 0)
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
                flushout(stdout);
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

