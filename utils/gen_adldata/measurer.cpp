#include "measurer.h"
#include <cmath>

#include "../../src/chips/opl_chip_base.h"

// Nuked OPL3 emulator, Most accurate, but requires the powerful CPU
#ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
#   include "../../src/chips/nuked_opl3.h"
#   include "../../src/chips/nuked_opl3_v174.h"
#endif

// DosBox 0.74 OPL3 emulator, Well-accurate and fast
#ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
#    include "../../src/chips/dosbox_opl3.h"
#endif

DurationInfo MeasureDurations(const ins &in)
{
    std::vector<int16_t> stereoSampleBuf;
#ifdef ADLMIDI_USE_DOSBOX_OPL
    std::vector<int32_t> stereoSampleBuf_32;
#endif
    insdata id[2];
    bool found[2] = {false, false};
    for(InstrumentDataTab::const_iterator j = insdatatab.begin();
        j != insdatatab.end();
        ++j)
    {
        if(j->second.first == in.insno1)
        {
            id[0] = j->first;
            found[0] = true;
            if(found[1]) break;
        }
        if(j->second.first == in.insno2)
        {
            id[1] = j->first;
            found[1] = true;
            if(found[0]) break;
        }
    }
    const unsigned rate = 22010;
    const unsigned interval             = 150;
    const unsigned samples_per_interval = rate / interval;
    const int notenum =
            in.notenum < 20   ? (44 + in.notenum)
                              : in.notenum >= 128 ? (44 + 128 - in.notenum)
                                                  : in.notenum;

    OPLChipBase *opl;

    //DosBoxOPL3 db; opl = &db;
    //NukedOPL3 nuke; opl = &nuke;
    NukedOPL3v174 nuke74; opl = &nuke74;

#define WRITE_REG(key, value) opl->writeReg((uint16_t)(key), (uint8_t)(value))

    static const short initdata[(2 + 3 + 2 + 2) * 2] =
    {
        0x004, 96, 0x004, 128,      // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable, leave disabled
        0x001, 32, 0x0BD, 0         // Enable wave & melodic
    };
    opl->setRate(rate);

    for(unsigned a = 0; a < 18; a += 2) WRITE_REG(initdata[a], initdata[a + 1]);

    const unsigned n_notes = in.insno1 == in.insno2 ? 1 : 2;
    unsigned x[2];

    if(n_notes == 2 && !in.pseudo4op)
    {
        WRITE_REG(0x105, 1);
        WRITE_REG(0x104, 1);
    }

    for(unsigned n = 0; n < n_notes; ++n)
    {
        static const unsigned char patchdata[11] =
        {0x20, 0x23, 0x60, 0x63, 0x80, 0x83, 0xE0, 0xE3, 0x40, 0x43, 0xC0};
        for(unsigned a = 0; a < 10; ++a) WRITE_REG(patchdata[a] + n * 8, id[n].data[a]);
        WRITE_REG(patchdata[10] + n * 8, id[n].data[10] | 0x30);
    }

    for(unsigned n = 0; n < n_notes; ++n)
    {
        double hertz = 172.00093 * std::exp(0.057762265 * (notenum + id[n].finetune));
        if(hertz > 131071)
        {
            std::fprintf(stderr, "MEASURER WARNING: Why does note %d + finetune %d produce hertz %g?          \n",
                    notenum, id[n].finetune, hertz);
            hertz = 131071;
        }
        x[n] = 0x2000;
        while(hertz >= 1023.5)
        {
            hertz /= 2.0;    // Calculate octave
            x[n] += 0x400;
        }
        x[n] += (unsigned int)(hertz + 0.5);

        // Keyon the note
        WRITE_REG(0xA0 + n * 3, x[n] & 0xFF);
        WRITE_REG(0xB0 + n * 3, x[n] >> 8);
    }

    const unsigned max_silent = 6;
    const unsigned max_on  = 40;
    const unsigned max_off = 60;

    // For up to 40 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_on;
    double highest_sofar = 0;
    short sound_min = 0, sound_max = 0;
    for(unsigned period = 0; period < max_on * interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2);
        opl->generate(stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            short s = stereoSampleBuf[c * 2];
            mean += s;
            if(sound_min > s) sound_min = s;
            if(sound_max < s) sound_max = s;
        }
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c * 2] - mean);
            std_deviation += diff * diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_on.push_back(std_deviation);
        if(std_deviation > highest_sofar)
            highest_sofar = std_deviation;

        if((period > max_silent * interval) &&
            ((std_deviation < highest_sofar * 0.2)||
             (sound_min >= -1 && sound_max <= 1))
        )
            break;
    }

    // Keyoff the note
    for(unsigned n = 0; n < n_notes; ++n)
        WRITE_REG(0xB0 + n * 3, (x[n] >> 8) & 0xDF);

    // Now, for up to 60 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_off;
    for(unsigned period = 0; period < max_off * interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2);
        opl->generate(stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            short s = stereoSampleBuf[c * 2];
            mean += s;
            if(sound_min > s) sound_min = s;
            if(sound_max < s) sound_max = s;
        }
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c * 2] - mean);
            std_deviation += diff * diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_off.push_back(std_deviation);

        if(std_deviation < highest_sofar * 0.2)
            break;

        if((period > max_silent * interval) && (sound_min >= -1 && sound_max <= 1))
            break;
    }

    /* Analyze the results */
    double begin_amplitude        = amplitudecurve_on[0];
    double peak_amplitude_value   = begin_amplitude;
    size_t peak_amplitude_time    = 0;
    size_t quarter_amplitude_time = amplitudecurve_on.size();
    size_t keyoff_out_time        = 0;

    for(size_t a = 1; a < amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] > peak_amplitude_value)
        {
            peak_amplitude_value = amplitudecurve_on[a];
            peak_amplitude_time  = a;
        }
    }
    for(size_t a = peak_amplitude_time; a < amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] <= peak_amplitude_value * 0.2)
        {
            quarter_amplitude_time = a;
            break;
        }
    }
    for(size_t a = 0; a < amplitudecurve_off.size(); ++a)
    {
        if(amplitudecurve_off[a] <= peak_amplitude_value * 0.2)
        {
            keyoff_out_time = a;
            break;
        }
    }

    if(keyoff_out_time == 0 && amplitudecurve_on.back() < peak_amplitude_value * 0.2)
        keyoff_out_time = quarter_amplitude_time;

    DurationInfo result;
    result.peak_amplitude_time = peak_amplitude_time;
    result.peak_amplitude_value = peak_amplitude_value;
    result.begin_amplitude = begin_amplitude;
    result.quarter_amplitude_time = (double)quarter_amplitude_time;
    result.keyoff_out_time = (double)keyoff_out_time;

    result.ms_sound_kon  = (int64_t)(quarter_amplitude_time * 1000.0 / interval);
    result.ms_sound_koff = (int64_t)(keyoff_out_time        * 1000.0 / interval);
    result.nosound = (peak_amplitude_value < 0.5) || ((sound_min >= -1) && (sound_max <= 1));
    return result;
}

static const char* spinner = "-\\|/";

void MeasureThreaded::LoadCache(const char *fileName)
{
    FILE *in = std::fopen(fileName, "rb");
    if(!in)
    {
        std::printf("Failed to load cache: file is not exists.\n"
               "Complete data will be generated from scratch.\n");
        std::fflush(stdout);
        return;
    }

    char magic[32];
    if(std::fread(magic, 1, 32, in) != 32)
    {
        std::fclose(in);
        std::printf("Failed to load cache: can't read magic.\n"
               "Complete data will be generated from scratch.\n");
        std::fflush(stdout);
        return;
    }

    if(memcmp(magic, "ADLMIDI-DURATION-CACHE-FILE-V1.0", 32) != 0)
    {
        std::fclose(in);
        std::printf("Failed to load cache: magic missmatch.\n"
               "Complete data will be generated from scratch.\n");
        std::fflush(stdout);
        return;
    }

    while(!std::feof(in))
    {
        DurationInfo info;
        ins inst;
        //got by instrument
        insdata id[2];
        size_t insNo[2] = {0, 0};
        bool found[2] = {false, false};
        //got from file
        insdata id_f[2];
        bool found_f[2] = {false, false};
        bool isMatches = false;

        memset(id, 0, sizeof(insdata) * 2);
        memset(id_f, 0, sizeof(insdata) * 2);
        memset(&info, 0, sizeof(DurationInfo));
        memset(&inst, 0, sizeof(ins));

        //Instrument
        uint64_t inval;
        if(std::fread(&inval, 1, sizeof(uint64_t), in) != sizeof(uint64_t))
            break;
        inst.insno1 = inval;
        if(std::fread(&inval, 1, sizeof(uint64_t), in) != sizeof(uint64_t))
            break;
        inst.insno2 = inval;
        if(std::fread(&inst.notenum, 1, 1, in) != 1)
            break;
        if(std::fread(&inst.pseudo4op, 1, 1, in) != 1)
            break;
        if(std::fread(&inst.voice2_fine_tune, sizeof(double), 1, in) != 1)
            break;

        //Instrument data
        if(fread(found_f, 1, 2 * sizeof(bool), in) != sizeof(bool) * 2)
            break;
        for(size_t i = 0; i < 2; i++)
        {
            if(fread(id_f[i].data, 1, 11, in) != 11)
                break;
            if(fread(&id_f[i].finetune, 1, 1, in) != 1)
                break;
            if(fread(&id_f[i].diff, 1, sizeof(bool), in) != sizeof(bool))
                break;
        }

        if(found_f[0] || found_f[1])
        {
            for(InstrumentDataTab::const_iterator j = insdatatab.begin(); j != insdatatab.end(); ++j)
            {
                if(j->second.first == inst.insno1)
                {
                    id[0] = j->first;
                    found[0] = (id[0] == id_f[0]);
                    insNo[0] = inst.insno1;
                    if(found[1]) break;
                }
                if(j->second.first == inst.insno2)
                {
                    id[1] = j->first;
                    found[1] = (id[1] == id_f[1]);
                    insNo[1] = inst.insno2;
                    if(found[0]) break;
                }
            }

            //Find instrument entries are matching
            if((found[0] != found_f[0]) || (found[1] != found_f[1]))
            {
                for(InstrumentDataTab::const_iterator j = insdatatab.begin(); j != insdatatab.end(); ++j)
                {
                    if(found_f[0] && (j->first == id_f[0]))
                    {
                        found[0] = true;
                        insNo[0] = j->second.first;
                    }
                    if(found_f[1] && (j->first == id_f[1]))
                    {
                        found[1] = true;
                        insNo[1] = j->second.first;
                    }
                    if(found[0] && !found_f[1])
                    {
                        isMatches = true;
                        break;
                    }
                    if(found[0] && found[1])
                    {
                        isMatches = true;
                        break;
                    }
                }
            }
            else
            {
                isMatches = true;
            }

            //Then find instrument entry that uses found instruments
            if(isMatches)
            {
                inst.insno1 = insNo[0];
                inst.insno2 = insNo[1];
                InstrumentsData::iterator d = instab.find(inst);
                if(d == instab.end())
                    isMatches = false;
            }
        }

        //Duration data
        if(std::fread(&info.peak_amplitude_time, 1, sizeof(uint64_t), in) != sizeof(uint64_t))
            break;
        if(std::fread(&info.peak_amplitude_value, 1, sizeof(double), in) != sizeof(double))
            break;
        if(std::fread(&info.quarter_amplitude_time, 1, sizeof(double), in) != sizeof(double))
            break;
        if(std::fread(&info.begin_amplitude, 1, sizeof(double), in) != sizeof(double))
            break;
        if(std::fread(&info.interval, 1, sizeof(double), in) != sizeof(double))
            break;
        if(std::fread(&info.keyoff_out_time, 1, sizeof(double), in) != sizeof(double))
            break;
        if(std::fread(&info.ms_sound_kon, 1, sizeof(int64_t), in) != sizeof(int64_t))
            break;
        if(std::fread(&info.ms_sound_koff, 1, sizeof(int64_t), in) != sizeof(int64_t))
            break;
        if(std::fread(&info.nosound, 1, sizeof(bool), in) != sizeof(bool))
            break;

        if(isMatches)//Store only if cached entry matches actual raw instrument data
            m_durationInfo.insert({inst, info});
    }

    std::printf("Cache loaded!\n");
    std::fflush(stdout);

    std::fclose(in);
}

void MeasureThreaded::SaveCache(const char *fileName)
{
    FILE *out = std::fopen(fileName, "wb");
    fprintf(out, "ADLMIDI-DURATION-CACHE-FILE-V1.0");
    for(DurationInfoCache::iterator it = m_durationInfo.begin(); it != m_durationInfo.end(); it++)
    {
        const ins &in = it->first;
        insdata id[2];
        bool found[2] = {false, false};
        memset(id, 0, sizeof(insdata) * 2);

        uint64_t outval;
        outval = in.insno1;
        fwrite(&outval, 1, sizeof(uint64_t), out);
        outval = in.insno2;
        fwrite(&outval, 1, sizeof(uint64_t), out);
        fwrite(&in.notenum, 1, 1, out);
        fwrite(&in.pseudo4op, 1, 1, out);
        fwrite(&in.voice2_fine_tune, sizeof(double), 1, out);

        for(InstrumentDataTab::const_iterator j = insdatatab.begin(); j != insdatatab.end(); ++j)
        {
            if(j->second.first == in.insno1)
            {
                id[0] = j->first;
                found[0] = true;
                if(found[1]) break;
            }
            if(j->second.first == in.insno2)
            {
                id[1] = j->first;
                found[1] = true;
                if(found[0]) break;
            }
        }

        fwrite(found, 1, 2 * sizeof(bool), out);
        for(size_t i = 0; i < 2; i++)
        {
            fwrite(id[i].data, 1, 11, out);
            fwrite(&id[i].finetune, 1, 1, out);
            fwrite(&id[i].diff, 1, sizeof(bool), out);
        }

        fwrite(&it->second.peak_amplitude_time, 1, sizeof(uint64_t), out);
        fwrite(&it->second.peak_amplitude_value, 1, sizeof(double), out);
        fwrite(&it->second.quarter_amplitude_time, 1, sizeof(double), out);
        fwrite(&it->second.begin_amplitude, 1, sizeof(double), out);
        fwrite(&it->second.interval, 1, sizeof(double), out);
        fwrite(&it->second.keyoff_out_time, 1, sizeof(double), out);
        fwrite(&it->second.ms_sound_kon, 1, sizeof(int64_t), out);
        fwrite(&it->second.ms_sound_koff, 1, sizeof(int64_t), out);
        fwrite(&it->second.nosound, 1, sizeof(bool), out);
    }
    std::fclose(out);
}

void MeasureThreaded::printProgress()
{
    std::printf("Calculating measures... [%c %3u%% (%4u/%4u) Threads %3u, Matches %u]       \r",
            spinner[m_done.load() % 4],
            (unsigned int)(((double)m_done.load() / (double)(m_total)) * 100),
            (unsigned int)m_done.load(),
            (unsigned int)m_total,
            (unsigned int)m_threads.size(),
            (unsigned int)m_cache_matches
            );
    std::fflush(stdout);
}

void MeasureThreaded::printFinal()
{
    std::printf("Calculating measures completed! [Total entries %4u with %u cache matches]\n",
            (unsigned int)m_total,
            (unsigned int)m_cache_matches);
    std::fflush(stdout);
}

void MeasureThreaded::run(InstrumentsData::const_iterator i)
{
    m_semaphore.wait();
    if(m_threads.size() > 0)
    {
        for(std::vector<destData *>::iterator it = m_threads.begin(); it != m_threads.end();)
        {
            if(!(*it)->m_works)
            {
                delete(*it);
                it = m_threads.erase(it);
            }
            else
                it++;
        }
    }

    destData *dd = new destData;
    dd->i = i;
    dd->myself = this;
    dd->start();
    m_threads.push_back(dd);
    printProgress();
}

void MeasureThreaded::waitAll()
{
    for(auto &th : m_threads)
    {
        printProgress();
        delete th;
    }
    m_threads.clear();
    printFinal();
}

void MeasureThreaded::destData::start()
{
    m_work = std::thread(&destData::callback, this);
}

void MeasureThreaded::destData::callback(void *myself)
{
    destData *s = reinterpret_cast<destData *>(myself);
    DurationInfo info;
    DurationInfoCache::iterator cachedEntry = s->myself->m_durationInfo.find(s->i->first);

    if(cachedEntry != s->myself->m_durationInfo.end())
    {
        s->myself->m_cache_matches++;
        goto endWork;
    }

    info = MeasureDurations(s->i->first);
    s->myself->m_durationInfo_mx.lock();
    s->myself->m_durationInfo.insert({s->i->first, info});
    s->myself->m_durationInfo_mx.unlock();

endWork:
    s->myself->m_semaphore.notify();
    s->myself->m_done++;
    s->m_works = false;
}
