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

#include "adlmidi_mus2mid.h"
#include "adlmidi_xmi2mid.h"

uint64_t MIDIplay::ReadBEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = (result << 8) + data[n];

    return result;
}

uint64_t MIDIplay::ReadLEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = result + static_cast<uint64_t>(data[n] << (n * 8));

    return result;
}

uint64_t MIDIplay::ReadVarLenEx(size_t tk, bool &ok)
{
    uint64_t result = 0;
    ok = false;

    for(;;)
    {
        if(tk >= TrackData.size())
            return 1;

        if(tk >= CurrentPosition.track.size())
            return 2;

        size_t ptr = CurrentPosition.track[tk].ptr;

        if(ptr >= TrackData[tk].size())
            return 3;

        unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
        result = (result << 7) + (byte & 0x7F);

        if(!(byte & 0x80)) break;
    }

    ok = true;
    return result;
}

bool MIDIplay::LoadMIDI(const std::string &filename)
{
    fileReader file;
    file.openFile(filename.c_str());

    if(!LoadMIDI(file))
    {
        std::perror(filename.c_str());
        return false;
    }

    return true;
}

bool MIDIplay::LoadMIDI(void *data, unsigned long size)
{
    fileReader file;
    file.openData(data, size);
    return LoadMIDI(file);
}

bool MIDIplay::LoadMIDI(MIDIplay::fileReader &fr)
{
    #define qqq(x) (void)x
    size_t  fsize;
    qqq(fsize);
    //! Temp buffer for conversion
    AdlMIDI_CPtr<uint8_t> cvt_buf;

    //std::FILE* fr = std::fopen(filename.c_str(), "rb");
    if(!fr.isValid())
    {
        ADLMIDI_ErrorString = "Invalid data stream!";
        return false;
    }

    /**** Set all properties BEFORE starting of actial file reading! ****/

    config->stored_samples = 0;
    config->backup_samples_size = 0;
    opl.AdlPercussionMode = config->AdlPercussionMode;
    opl.HighTremoloMode = config->HighTremoloMode;
    opl.HighVibratoMode = config->HighVibratoMode;
    opl.ScaleModulators = config->ScaleModulators;
    opl.LogarithmicVolumes = config->LogarithmicVolumes;
    opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(config->VolumeModel));

    if(config->VolumeModel == ADLMIDI_VolumeModel_AUTO)
    {
        switch(config->AdlBank)
        {
        default:
            opl.m_volumeScale = OPL3::VOLUME_Generic;
            break;

        case 14://Doom 2
        case 15://Heretic
        case 16://Doom 1
        case 64://Raptor
            opl.m_volumeScale = OPL3::VOLUME_DMX;
            break;

        case 58://FatMan bank hardcoded in the Windows 9x drivers
        case 65://Modded Wohlstand's Fatman bank
        case 66://O'Connel's bank
            opl.m_volumeScale = OPL3::VOLUME_9X;
            break;

        case 62://Duke Nukem 3D
        case 63://Shadow Warrior
        case 69://Blood
        case 70://Lee
        case 71://Nam
            opl.m_volumeScale = OPL3::VOLUME_APOGEE;
            break;
        }
    }

    opl.NumCards    = config->NumCards;
    opl.NumFourOps  = config->NumFourOps;
    cmf_percussion_mode = false;
    opl.Reset();

    trackStart       = true;
    loopStart        = true;
    loopStart_passed = false;
    invalidLoop      = false;
    loopStart_hit    = false;

    bool is_GMF = false; // GMD/MUS files (ScummVM)
    //bool is_MUS = false; // MUS/DMX files (Doom)
    bool is_IMF = false; // IMF
    bool is_CMF = false; // Creative Music format (CMF/CTMF)

    const size_t HeaderSize = 4 + 4 + 2 + 2 + 2; // 14
    char HeaderBuf[HeaderSize] = "";
    size_t DeltaTicks = 192, TrackCount = 1;

riffskip:
    fsize = fr.read(HeaderBuf, 1, HeaderSize);

    if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
    {
        fr.seek(6l, SEEK_CUR);
        goto riffskip;
    }

    if(std::memcmp(HeaderBuf, "GMF\x1", 4) == 0)
    {
        // GMD/MUS files (ScummVM)
        fr.seek(7 - static_cast<long>(HeaderSize), SEEK_CUR);
        is_GMF = true;
    }
    else if(std::memcmp(HeaderBuf, "MUS\x1A", 4) == 0)
    {
        // MUS/DMX files (Doom)
        //uint64_t start = ReadLEint(HeaderBuf + 6, 2);
        //is_MUS = true;
        fr.seek(0, SEEK_END);
        size_t mus_len = fr.tell();
        fr.seek(0, SEEK_SET);
        uint8_t *mus = (uint8_t*)malloc(mus_len);
        if(!mus)
        {
            ADLMIDI_ErrorString = "Out of memory!";
            return false;
        }
        fr.read(mus, 1, mus_len);
        //Close source stream
        fr.close();

        uint8_t *mid = NULL;
        uint32_t mid_len = 0;
        int m2mret = AdlMidi_mus2midi(mus, static_cast<uint32_t>(mus_len),
                                      &mid, &mid_len, 0);
        if(mus) free(mus);
        if(m2mret < 0)
        {
            ADLMIDI_ErrorString = "Invalid MUS/DMX data format!";
            return false;
        }
        cvt_buf.reset(mid);
        //Open converted MIDI file
        fr.openData(mid, static_cast<size_t>(mid_len));
        //Re-Read header again!
        goto riffskip;
    }
    else if(std::memcmp(HeaderBuf, "FORM", 4) == 0)
    {
        if(std::memcmp(HeaderBuf + 8, "XDIR", 4) != 0)
        {
            fr.close();
            ADLMIDI_ErrorString = fr._fileName + ": Invalid format\n";
            return false;
        }

        fr.seek(0, SEEK_END);
        size_t mus_len = fr.tell();
        fr.seek(0, SEEK_SET);
        uint8_t *mus = (uint8_t*)malloc(mus_len);
        if(!mus)
        {
            ADLMIDI_ErrorString = "Out of memory!";
            return false;
        }
        fr.read(mus, 1, mus_len);
        //Close source stream
        fr.close();

        uint8_t *mid = NULL;
        uint32_t mid_len = 0;
        int m2mret = AdlMidi_xmi2midi(mus, static_cast<uint32_t>(mus_len),
                                      &mid, &mid_len, XMIDI_CONVERT_NOCONVERSION);
        if(mus) free(mus);
        if(m2mret < 0)
        {
            ADLMIDI_ErrorString = "Invalid XMI data format!";
            return false;
        }
        cvt_buf.reset(mid);
        //Open converted MIDI file
        fr.openData(mid, static_cast<size_t>(mid_len));
        //Re-Read header again!
        goto riffskip;
    }
    else if(std::memcmp(HeaderBuf, "CTMF", 4) == 0)
    {
        opl.dynamic_instruments.clear();
        opl.dynamic_metainstruments.clear();
        // Creative Music Format (CMF).
        // When playing CTMF files, use the following commandline:
        // adlmidi song8.ctmf -p -v 1 1 0
        // i.e. enable percussion mode, deeper vibrato, and use only 1 card.
        is_CMF = true;
        //unsigned version   = ReadLEint(HeaderBuf+4, 2);
        uint64_t ins_start = ReadLEint(HeaderBuf + 6, 2);
        uint64_t mus_start = ReadLEint(HeaderBuf + 8, 2);
        //unsigned deltas    = ReadLEint(HeaderBuf+10, 2);
        uint64_t ticks     = ReadLEint(HeaderBuf + 12, 2);
        // Read title, author, remarks start offsets in file
        fr.read(HeaderBuf, 1, 6);
        //unsigned long notes_starts[3] = {ReadLEint(HeaderBuf+0,2),ReadLEint(HeaderBuf+0,4),ReadLEint(HeaderBuf+0,6)};
        fr.seek(16, SEEK_CUR); // Skip the channels-in-use table
        fr.read(HeaderBuf, 1, 4);
        uint64_t ins_count =  ReadLEint(HeaderBuf + 0, 2); //, basictempo = ReadLEint(HeaderBuf+2, 2);
        fr.seek(static_cast<long>(ins_start), SEEK_SET);

        //std::printf("%u instruments\n", ins_count);
        for(unsigned i = 0; i < ins_count; ++i)
        {
            unsigned char InsData[16];
            fr.read(InsData, 1, 16);
            /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                        i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                           InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
            struct adldata    adl;
            struct adlinsdata adlins;
            adl.modulator_E862 =
                ((static_cast<uint32_t>(InsData[8] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(InsData[6]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(InsData[4]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(InsData[0]) << 0) & 0x000000FF); //MultKEVA
            adl.carrier_E862 =
                ((static_cast<uint32_t>(InsData[9] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(InsData[7]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(InsData[5]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(InsData[1]) << 0) & 0x000000FF); //MultKEVA
            adl.modulator_40 = InsData[2];
            adl.carrier_40   = InsData[3];
            adl.feedconn     = InsData[10] & 0x0F;
            adl.finetune = 0;
            adlins.adlno1 = static_cast<uint16_t>(opl.dynamic_instruments.size() | opl.DynamicInstrumentTag);
            adlins.adlno2 = adlins.adlno1;
            adlins.ms_sound_kon  = 1000;
            adlins.ms_sound_koff = 500;
            adlins.tone  = 0;
            adlins.flags = 0;
            adlins.voice2_fine_tune = 0.0;
            opl.dynamic_metainstruments.push_back(adlins);
            opl.dynamic_instruments.push_back(adl);
        }

        fr.seeku(mus_start, SEEK_SET);
        TrackCount = 1;
        DeltaTicks = ticks;
        opl.AdlBank    = ~0u; // Ignore AdlBank number, use dynamic banks instead
        //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
        opl.LogarithmicVolumes = true;
        opl.AdlPercussionMode = true;
        opl.m_volumeScale = OPL3::VOLUME_CMF;
    }
    else
    {
        // Try parsing as an IMF file
        {
            size_t end = static_cast<uint8_t>(HeaderBuf[0]) + 256 * static_cast<uint8_t>(HeaderBuf[1]);

            if(!end || (end & 3))
                goto not_imf;

            size_t backup_pos = fr.tell();
            int64_t sum1 = 0, sum2 = 0;
            fr.seek(2, SEEK_SET);

            for(unsigned n = 0; n < 42; ++n)
            {
                int64_t value1 = fr.getc();
                value1 += fr.getc() << 8;
                sum1 += value1;
                int64_t value2 = fr.getc();
                value2 += fr.getc() << 8;
                sum2 += value2;
            }

            fr.seek(static_cast<long>(backup_pos), SEEK_SET);

            if(sum1 > sum2)
            {
                is_IMF = true;
                DeltaTicks = 1;
            }
        }

        if(!is_IMF)
        {
not_imf:

            if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
            {
InvFmt:
                fr.close();
                ADLMIDI_ErrorString = fr._fileName + ": Invalid format\n";
                return false;
            }

            /*size_t  Fmt =*/ ReadBEint(HeaderBuf + 8,  2);
            TrackCount = ReadBEint(HeaderBuf + 10, 2);
            DeltaTicks = ReadBEint(HeaderBuf + 12, 2);
        }
    }

    TrackData.clear();
    TrackData.resize(TrackCount, std::vector<uint8_t>());
    CurrentPosition.track.clear();
    CurrentPosition.track.resize(TrackCount);
    InvDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(DeltaTicks));
    //Tempo       = 1000000l * InvDeltaTicks;
    Tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(DeltaTicks));
    static const unsigned char EndTag[4] = {0xFF, 0x2F, 0x00, 0x00};
    int totalGotten = 0;

    for(size_t tk = 0; tk < TrackCount; ++tk)
    {
        // Read track header
        size_t TrackLength;

        if(is_IMF)
        {
            //std::fprintf(stderr, "Reading IMF file...\n");
            size_t end = static_cast<uint8_t>(HeaderBuf[0]) + 256 * static_cast<uint8_t>(HeaderBuf[1]);
            unsigned IMF_tempo = 1428;
            static const unsigned char imf_tempo[] = {0xFF, 0x51, 0x4,
                                                      static_cast<uint8_t>(IMF_tempo >> 24),
                                                      static_cast<uint8_t>(IMF_tempo >> 16),
                                                      static_cast<uint8_t>(IMF_tempo >> 8),
                                                      static_cast<uint8_t>(IMF_tempo)
                                                     };
            TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
            TrackData[tk].push_back(0x00);
            fr.seek(2, SEEK_SET);

            while(fr.tell() < end && !fr.eof())
            {
                uint8_t special_event_buf[5];
                special_event_buf[0] = 0xFF;
                special_event_buf[1] = 0xE3;
                special_event_buf[2] = 0x02;
                special_event_buf[3] = static_cast<uint8_t>(fr.getc()); // port index
                special_event_buf[4] = static_cast<uint8_t>(fr.getc()); // port value
                uint32_t delay = static_cast<uint16_t>(fr.getc());
                delay += 256 * static_cast<uint16_t>(fr.getc());
                totalGotten += 4;
                //if(special_event_buf[3] <= 8) continue;
                //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);
                TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf + 5);

                //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                if(delay >> 14)
                    TrackData[tk].push_back(0x80 | ((delay >> 14) & 0x7F));

                if(delay >> 7)
                    TrackData[tk].push_back(0x80 | ((delay >> 7) & 0x7F));

                TrackData[tk].push_back(((delay >> 0) & 0x7F));
            }

            TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);
            CurrentPosition.track[tk].delay = 0;
            CurrentPosition.began = true;
            //std::fprintf(stderr, "Done reading IMF file\n");
            opl.NumFourOps = 0; //Don't use 4-operator channels for IMF playing!
        }
        else
        {
            if(is_GMF || is_CMF) // Take the rest of the file
            {
                size_t pos = fr.tell();
                fr.seek(0, SEEK_END);
                TrackLength = fr.tell() - pos;
                fr.seek(static_cast<long>(pos), SEEK_SET);
            }
            //else if(is_MUS) // Read TrackLength from file position 4
            //{
            //    size_t pos = fr.tell();
            //    fr.seek(4, SEEK_SET);
            //    TrackLength = static_cast<size_t>(fr.getc());
            //    TrackLength += static_cast<size_t>(fr.getc() << 8);
            //    fr.seek(static_cast<long>(pos), SEEK_SET);
            //}
            else
            {
                fsize = fr.read(HeaderBuf, 1, 8);

                if(std::memcmp(HeaderBuf, "MTrk", 4) != 0)
                    goto InvFmt;

                TrackLength = ReadBEint(HeaderBuf + 4, 4);
            }

            // Read track data
            TrackData[tk].resize(TrackLength);
            fsize = fr.read(&TrackData[tk][0], 1, TrackLength);
            totalGotten += fsize;

            if(is_GMF /*|| is_MUS*/) // Note: CMF does include the track end tag.
                TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);

            bool ok = false;
            // Read next event time
            uint64_t tkDelay = ReadVarLenEx(tk, ok);

            if(ok)
                CurrentPosition.track[tk].delay = tkDelay;
            else
            {
                std::stringstream msg;
                msg << fr._fileName << ": invalid variable length in the track " << tk << "! (error code " << tkDelay << ")";
                ADLMIDI_ErrorString = msg.str();
                return false;
            }
        }
    }

    for(size_t tk = 0; tk < TrackCount; ++tk)
        totalGotten += TrackData[tk].size();

    if(totalGotten == 0)
    {
        ADLMIDI_ErrorString = fr._fileName + ": Empty track data";
        return false;
    }

    opl.Reset(); // Reset AdLib
    //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
    ch.clear();
    ch.resize(opl.NumChannels);
    return true;
}
