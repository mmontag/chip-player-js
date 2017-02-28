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

#include "adlmidi_private.hpp"

#ifdef ADLMIDI_buildAsApp
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#endif

static const unsigned MaxCards = 100;

/*---------------------------EXPORTS---------------------------*/

ADLMIDI_EXPORT struct ADL_MIDIPlayer *adl_init(long sample_rate)
{
    ADL_MIDIPlayer *_device;
    _device = (ADL_MIDIPlayer *)malloc(sizeof(ADL_MIDIPlayer));
    _device->PCM_RATE = static_cast<unsigned long>(sample_rate);
    _device->AdlBank    = 0;
    _device->NumFourOps = 7;
    _device->NumCards   = 2;
    _device->HighTremoloMode   = 0;
    _device->HighVibratoMode   = 0;
    _device->AdlPercussionMode = 0;
    _device->LogarithmicVolumes = 0;
    _device->QuitFlag = 0;
    _device->SkipForward = 0;
    _device->QuitWithoutLooping = 0;
    _device->ScaleModulators = 0;
    _device->delay = 0.0;
    _device->carry = 0.0;
    _device->mindelay = 1.0 / (double)_device->PCM_RATE;
    _device->maxdelay = 512.0 / (double)_device->PCM_RATE;
    _device->stored_samples = 0;
    _device->backup_samples_size = 0;
    MIDIplay *player = new MIDIplay;
    _device->adl_midiPlayer = player;
    player->config = _device;
    player->opl._parent = _device;
    player->opl.NumCards = _device->NumCards;
    player->opl.AdlBank = _device->AdlBank;
    player->opl.NumFourOps = _device->NumFourOps;
    player->opl.LogarithmicVolumes = (bool)_device->LogarithmicVolumes;
    player->opl.HighTremoloMode = (bool)_device->HighTremoloMode;
    player->opl.HighVibratoMode = (bool)_device->HighVibratoMode;
    player->opl.AdlPercussionMode = (bool)_device->AdlPercussionMode;
    player->opl.ScaleModulators = (bool)_device->ScaleModulators;
    player->ChooseDevice("none");
    adlRefreshNumCards(_device);
    return _device;
}

ADLMIDI_EXPORT int adl_setNumCards(ADL_MIDIPlayer *device, int numCards)
{
    if(device == NULL)
        return -2;

    device->NumCards = static_cast<unsigned int>(numCards);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.NumCards = device->NumCards;

    if(device->NumCards < 1 || device->NumCards > MaxCards)
    {
        std::stringstream s;
        s << "number of cards may only be 1.." << MaxCards << ".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
    const uint32_t NumBanks = static_cast<uint32_t>(maxAdlBanks());
    int32_t bankno = bank;

    if(bankno < 0)
        bankno = 0;

    device->AdlBank = static_cast<uint32_t>(bankno);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.AdlBank = device->AdlBank;

    if(device->AdlBank >= NumBanks)
    {
        std::stringstream s;
        s << "bank number may only be 0.." << (NumBanks - 1) << ".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getBanksCount()
{
    return maxAdlBanks();
}

ADLMIDI_EXPORT const char *const *adl_getBankNames()
{
    return banknames;
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    device->NumFourOps = static_cast<unsigned int>(ops4);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.NumFourOps = device->NumFourOps;

    if(device->NumFourOps > 6 * device->NumCards)
    {
        std::stringstream s;
        s << "number of four-op channels may only be 0.." << (6 * (device->NumCards)) << " when " << device->NumCards << " OPL3 cards are used.\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}


ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    if(!device) return;

    device->AdlPercussionMode = static_cast<unsigned int>(percmod);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.AdlPercussionMode = (bool)device->AdlPercussionMode;
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;

    device->HighVibratoMode = static_cast<unsigned int>(hvibro);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.HighVibratoMode = (bool)device->HighVibratoMode;
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;

    device->HighTremoloMode = static_cast<unsigned int>(htremo);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.HighTremoloMode = (bool)device->HighTremoloMode;
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device) return;

    device->ScaleModulators = static_cast<unsigned int>(smod);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.ScaleModulators = (bool)device->ScaleModulators;
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
    if(!device) return;

    device->QuitWithoutLooping = (loopEn == 0);
}

ADLMIDI_EXPORT void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol)
{
    if(!device) return;

    device->LogarithmicVolumes = static_cast<unsigned int>(logvol);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.LogarithmicVolumes = (bool)device->LogarithmicVolumes;
}

ADLMIDI_EXPORT void adl_setVolumeRangeModel(ADL_MIDIPlayer *device, int volumeModel)
{
    if(!device) return;

    device->VolumeModel = volumeModel;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(volumeModel));
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, char *filePath)
{
    ADLMIDI_ErrorString.clear();

    if(device && device->adl_midiPlayer)
    {
        device->stored_samples = 0;
        device->backup_samples_size = 0;

        if(!reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->LoadMIDI(filePath))
        {
            if(ADLMIDI_ErrorString.empty())
                ADLMIDI_ErrorString = "ADL MIDI: Can't load file";

            return -1;
        }
        else return 0;
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer *device, void *mem, long size)
{
    ADLMIDI_ErrorString.clear();

    if(device && device->adl_midiPlayer)
    {
        device->stored_samples = 0;
        device->backup_samples_size = 0;

        if(!reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->LoadMIDI(mem, static_cast<size_t>(size)))
        {
            if(ADLMIDI_ErrorString.empty())
                ADLMIDI_ErrorString = "ADL MIDI: Can't load data from memory";

            return -1;
        }
        else return 0;
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}


ADLMIDI_EXPORT const char *adl_errorString()
{
    return ADLMIDI_ErrorString.c_str();
}

ADLMIDI_EXPORT const char *adl_getMusicTitle(ADL_MIDIPlayer *device)
{
    if(!device) return "";

    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTitle.c_str();
}

ADLMIDI_EXPORT void adl_close(ADL_MIDIPlayer *device)
{
    if(device->adl_midiPlayer)
        delete reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);

    device->adl_midiPlayer = NULL;
    free(device);
    device = NULL;
}

ADLMIDI_EXPORT void adl_reset(ADL_MIDIPlayer *device)
{
    if(!device)
        return;

    device->stored_samples = 0;
    device->backup_samples_size = 0;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.Reset();
}

#ifdef ADLMIDI_USE_DOSBOX_OPL

#define ADLMIDI_CLAMP(V, MIN, MAX) std::max(std::min(V, (MAX)), (MIN))

inline static void SendStereoAudio(ADL_MIDIPlayer *device,
                                   int      &samples_requested,
                                   ssize_t  &in_size,
                                   int      *_in,
                                   ssize_t  out_pos,
                                   short    *_out)
{
    if(!in_size) return;

    device->stored_samples = 0;
    ssize_t out;
    ssize_t offset;
    ssize_t pos = static_cast<ssize_t>(out_pos);

    for(ssize_t p = 0; p < in_size; ++p)
    {
        for(ssize_t w = 0; w < 2; ++w)
        {
            out    = _in[p * 2 + w];
            offset = pos + p * 2 + w;

            if(offset < samples_requested)
                _out[offset] = static_cast<short>(ADLMIDI_CLAMP(out, static_cast<ssize_t>(INT16_MIN), static_cast<ssize_t>(INT16_MAX)));
            else
            {
                device->backup_samples[device->backup_samples_size] = static_cast<short>(out);
                device->backup_samples_size++;
                device->stored_samples++;
            }
        }
    }
}
#else
inline static void SendStereoAudio(ADL_MIDIPlayer *device,
                                   int      &samples_requested,
                                   ssize_t  &in_size,
                                   short    *_in,
                                   ssize_t   out_pos,
                                   short    *_out)
{
    if(!in_size)
        return;

    device->stored_samples = 0;
    size_t offset       = static_cast<size_t>(out_pos);
    size_t inSamples    = static_cast<size_t>(in_size * 2);
    size_t maxSamples   = static_cast<size_t>(samples_requested) - offset;
    size_t toCopy       = std::min(maxSamples, inSamples);
    memcpy(_out + out_pos, _in, toCopy * sizeof(short));

    if(maxSamples < inSamples)
    {
        size_t appendSize = inSamples - maxSamples;
        memcpy(device->backup_samples + device->backup_samples_size,
               maxSamples + _in, appendSize * sizeof(short));
        device->backup_samples_size += appendSize;
        device->stored_samples += appendSize;
    }
}
#endif


ADLMIDI_EXPORT int adl_play(ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    if(!device)
        return 0;

    sampleCount -= sampleCount % 2; //Avoid even sample requests

    if(sampleCount < 0)
        return 0;

    if(device->QuitFlag)
        return 0;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;
    //ssize_t n_periodCountPhys = n_periodCountStereo * 2;
    int left = sampleCount;

    while(left > 0)
    {
        if(device->backup_samples_size > 0)
        {
            //Send reserved samples if exist
            ssize_t ate = 0;

            while((ate < device->backup_samples_size) && (ate < left))
            {
                out[ate] = device->backup_samples[ate];
                ate++;
            }

            left -= ate;
            gotten_len += ate;

            if(ate < device->backup_samples_size)
            {
                for(int j = 0; j < ate; j++)
                    device->backup_samples[(ate - 1) - j] = device->backup_samples[(device->backup_samples_size - 1) - j];
            }

            device->backup_samples_size -= ate;
        }
        else
        {
            const long double eat_delay = device->delay < device->maxdelay ? device->delay : device->maxdelay;
            device->delay -= eat_delay;
            device->carry += device->PCM_RATE * eat_delay;
            n_periodCountStereo = static_cast<ssize_t>(device->carry);
            //n_periodCountPhys = n_periodCountStereo * 2;
            device->carry -= n_periodCountStereo;

            if(device->SkipForward > 0)
                device->SkipForward -= 1;
            else
            {
                #ifdef ADLMIDI_USE_DOSBOX_OPL
                std::vector<int>     out_buf;
                #else
                std::vector<int16_t> out_buf;
                #endif
                out_buf.resize(1024 /*n_samples * 2*/);

                ssize_t in_generatedStereo = (n_periodCountStereo > 512) ? 512 : n_periodCountStereo;
                ssize_t in_generatedPhys = in_generatedStereo * 2;

                //fill buffer with zeros
                size_t in_countStereoU = static_cast<size_t>(in_generatedStereo * 2);

                if(device->NumCards == 1)
                {
                    #ifdef ADLMIDI_USE_DOSBOX_OPL
                    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.cards[0].GenerateArr(out_buf.data(), &in_generatedStereo);
                    in_generatedPhys = in_generatedStereo * 2;
                    #else
                    OPL3_GenerateStream(&(reinterpret_cast<MIDIplay *>(device->adl_midiPlayer))->opl.cards[0], out_buf.data(), static_cast<Bit32u>(in_generatedStereo));
                    #endif
                    /* Process it */
                    SendStereoAudio(device, sampleCount, in_generatedStereo, out_buf.data(), gotten_len, out);
                }
                else if(n_periodCountStereo > 0)
                {
                    #ifdef ADLMIDI_USE_DOSBOX_OPL
                    std::vector<int32_t> in_mixBuffer;
                    in_mixBuffer.resize(1024); //n_samples * 2
                    ssize_t in_generatedStereo = n_periodCountStereo;
                    #endif
                    memset(out_buf.data(), 0, in_countStereoU * sizeof(short));

                    /* Generate data from every chip and mix result */
                    for(unsigned card = 0; card < device->NumCards; ++card)
                    {
                        #ifdef ADLMIDI_USE_DOSBOX_OPL
                        reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.cards[card].GenerateArr(in_mixBuffer.data(), &in_generatedStereo);
                        in_generatedPhys = in_generatedStereo * 2;
                        size_t in_samplesCount = static_cast<size_t>(in_generatedPhys);
                        for(size_t a = 0; a < in_samplesCount; ++a)
                            out_buf[a] += in_mixBuffer[a];
                        #else
                        OPL3_GenerateStreamMix(&(reinterpret_cast<MIDIplay *>(device->adl_midiPlayer))->opl.cards[card], out_buf.data(), static_cast<Bit32u>(in_generatedStereo));
                        #endif
                    }

                    /* Process it */
                    SendStereoAudio(device, sampleCount, in_generatedStereo, out_buf.data(), gotten_len, out);
                }

                left -= in_generatedPhys;
                gotten_len += (in_generatedPhys) - device->stored_samples;
            }

            device->delay = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->Tick(eat_delay, device->mindelay);
        }
    }

    return static_cast<int>(gotten_len);
}

#ifdef ADLMIDI_buildAsApp
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
    unsigned ate = len / 2; // number of shorts

    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();

    for(unsigned a = 0; a < ate; ++a)
        target[a] = AudioBuffer[a];

    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}

int main(int argc, char **argv)
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

        for(unsigned a = 0; a < sizeof(banknames) / sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                        a ? "" : "Banks:",
                        a,
                        banknames[a]);

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
                     obtained.samples, obtained.freq, obtained.channels);

    ADL_MIDIPlayer *myDevice;
    myDevice = adl_init(44100);

    if(myDevice == NULL)
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
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

    if(argc >= 3)
    {
        int bankno = std::atoi(argv[2]);

        if(adl_setBank(myDevice, bankno) != 0)
        {
            std::fprintf(stderr, "%s", adl_errorString());
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
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4])) != 0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }

    if(adl_openFile(myDevice, argv[1]) != 0)
    {
        std::fprintf(stderr, "%s\n", adl_errorString());
        return 2;
    }

    SDL_PauseAudio(0);

    while(1)
    {
        int buff[4096];
        unsigned long gotten = adl_play(myDevice, 4096, buff);

        if(gotten <= 0) break;

        AudioBuffer_lock.Lock();
        size_t pos = AudioBuffer.size();
        AudioBuffer.resize(pos + gotten);

        for(unsigned long p = 0; p < gotten; ++p)
            AudioBuffer[pos + p] = buff[p];

        AudioBuffer_lock.Unlock();
        const SDL_AudioSpec &spec_ = obtained;

        while(AudioBuffer.size() > spec_.samples + (spec_.freq * 2) * OurHeadRoomLength)
            SDL_Delay(1);
    }

    adl_close(myDevice);
    SDL_CloseAudio();
    return 0;
}
#endif
