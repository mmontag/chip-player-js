/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include "wopl/wopl_file.h"


std::string ADLMIDI_ErrorString;

// Generator callback on audio rate ticks

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
void adl_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate)
{
    reinterpret_cast<MIDIplay *>(instance)->AudioTick(chipId, rate);
}
#endif

int adlCalculateFourOpChannels(MIDIplay *play, bool silent)
{
    Synth &synth = *play->m_synth;
    size_t n_fourop[2] = {0, 0}, n_total[2] = {0, 0};
    bool rhythmModeNeeded = false;

    //Automatically calculate how much 4-operator channels is necessary
//#ifndef DISABLE_EMBEDDED_BANKS
//    if(synth.m_embeddedBank == Synth::CustomBankTag)
//#endif
    {
        //For custom bank
        Synth::BankMap::iterator it = synth.m_insBanks.begin();
        Synth::BankMap::iterator end = synth.m_insBanks.end();
        for(; it != end; ++it)
        {
            size_t bank = it->first;
            size_t div = (bank & Synth::PercussionTag) ? 1 : 0;
            for(size_t i = 0; i < 128; ++i)
            {
                adlinsdata2 &ins = it->second.ins[i];
                if(ins.flags & adlinsdata::Flag_NoSound)
                    continue;
                if((ins.flags & adlinsdata::Flag_Real4op) != 0)
                    ++n_fourop[div];
                ++n_total[div];
                if(div && ((ins.flags & adlinsdata::Mask_RhythmMode) != 0))
                    rhythmModeNeeded = true;
            }
        }
    }
//#ifndef DISABLE_EMBEDDED_BANKS
//    else
//    {
//        //For embedded bank
//        for(size_t  a = 0; a < 256; ++a)
//        {
//            size_t insno = banks[play->m_setup.bankId][a];
//            size_t div = a / 128;
//            if(insno == 198)
//                continue;
//            ++n_total[div];
//            adlinsdata2 ins = adlinsdata2::from_adldata(::adlins[insno]);
//            if((ins.flags & adlinsdata::Flag_Real4op) != 0)
//                ++n_fourop[div];
//            if(div && ((ins.flags & adlinsdata::Mask_RhythmMode) != 0))
//                rhythmModeNeeded = true;
//        }
//    }
//#endif

    size_t numFourOps = 0;

    // All 2ops (no 4ops)
    if((n_fourop[0] == 0) && (n_fourop[1] == 0))
        numFourOps = 0;
    // All 2op melodics and Some (or All) 4op drums
    else if((n_fourop[0] == 0) && (n_fourop[1] > 0))
        numFourOps = 2;
    // Many 4op melodics
    else if((n_fourop[0] >= (n_total[0] * 7) / 8))
        numFourOps = 6;
    // Few 4op melodics
    else if(n_fourop[0] > 0)
        numFourOps = 4;

/* //Old formula
    unsigned NumFourOps = ((n_fourop[0] == 0) && (n_fourop[1] == 0)) ? 0
        : (n_fourop[0] >= (n_total[0] * 7) / 8) ? play->m_setup.NumCards * 6
        : (play->m_setup.NumCards == 1 ? 1 : play->m_setup.NumCards * 4);
*/

    synth.m_numFourOps = static_cast<unsigned>(numFourOps * synth.m_numChips);
    // Update channel categories and set up four-operator channels
    if(!silent)
        synth.updateChannelCategories();

    // Set rhythm mode when it needed
    synth.m_rhythmMode = rhythmModeNeeded;

    return 0;
}

#ifndef DISABLE_EMBEDDED_BANKS
void adlFromInstrument(const BanksDump::InstrumentEntry &instIn, adlinsdata2 &instOut)
{
    instOut.voice2_fine_tune = 0.0;
    if(instIn.secondVoiceDetune != 0)
        instOut.voice2_fine_tune = (double)((((int)instIn.secondVoiceDetune + 128) >> 1) - 64) / 32.0;

    instOut.midi_velocity_offset = instIn.midiVelocityOffset;
    instOut.tone = instIn.percussionKeyNumber;
    instOut.flags = (instIn.instFlags & WOPL_Ins_4op) && (instIn.instFlags & WOPL_Ins_Pseudo4op) ? adlinsdata::Flag_Pseudo4op : 0;
    instOut.flags|= (instIn.instFlags & WOPL_Ins_4op) && ((instIn.instFlags & WOPL_Ins_Pseudo4op) == 0) ? adlinsdata::Flag_Real4op : 0;
    instOut.flags|= (instIn.instFlags & WOPL_Ins_IsBlank) ? adlinsdata::Flag_NoSound : 0;
    instOut.flags|= instIn.instFlags & WOPL_RhythmModeMask;

    for(size_t op = 0; op < 2; op++)
    {
        if((instIn.ops[(op * 2) + 0] < 0) || (instIn.ops[(op * 2) + 1] < 0))
            break;
        const BanksDump::Operator &op1 = g_embeddedBanksOperators[instIn.ops[(op * 2) + 0]];
        const BanksDump::Operator &op2 = g_embeddedBanksOperators[instIn.ops[(op * 2) + 1]];
        instOut.adl[op].modulator_E862 = op1.d_E862;
        instOut.adl[op].modulator_40   = op1.d_40;
        instOut.adl[op].carrier_E862 = op2.d_E862;
        instOut.adl[op].carrier_40   = op2.d_40;
        instOut.adl[op].feedconn = (instIn.fbConn >> (op * 8)) & 0xFF;
        instOut.adl[op].finetune = static_cast<int8_t>(op == 0 ? instIn.noteOffset1 : instIn.noteOffset2);
    }

    instOut.ms_sound_kon  = instIn.delay_on_ms;
    instOut.ms_sound_koff = instIn.delay_off_ms;
}
#endif
