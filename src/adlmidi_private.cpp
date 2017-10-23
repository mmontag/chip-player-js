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

std::string ADLMIDI_ErrorString;

int adlRefreshNumCards(ADL_MIDIPlayer *device)
{
    unsigned n_fourop[2] = {0, 0}, n_total[2] = {0, 0};
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);

    //Automatically calculate how much 4-operator channels is necessary
    if(play->opl.AdlBank == ~0u)
    {
        //For custom bank
        for(size_t a = 0; a < play->opl.dynamic_metainstruments.size(); ++a)
        {
            ++n_total[a / 128];
            adlinsdata &ins = play->opl.dynamic_metainstruments[a];
            if((ins.adlno1 != ins.adlno2) && ((ins.flags & adlinsdata::Flag_Pseudo4op) == 0))
                ++n_fourop[a / 128];
        }
    }
    else
    {
        //For embedded bank
        for(unsigned a = 0; a < 256; ++a)
        {
            unsigned insno = banks[device->AdlBank][a];
            if(insno == 198)
                continue;
            ++n_total[a / 128];
            const adlinsdata &ins = adlins[insno];
            if((ins.adlno1 != ins.adlno2) && ((ins.flags & adlinsdata::Flag_Pseudo4op) == 0))
                ++n_fourop[a / 128];
        }
    }

    device->NumFourOps =
        (n_fourop[0] >= n_total[0] * 7 / 8) ? device->NumCards * 6
        : (n_fourop[0] < n_total[0] * 1 / 8) ? 0
        : (device->NumCards == 1 ? 1 : device->NumCards * 4);
    play->opl.NumFourOps = device->NumFourOps;

    if(n_fourop[0] >= n_total[0] * 15 / 16 && device->NumFourOps == 0)
    {
        ADLMIDI_ErrorString = "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
                              "       The results (silence + much cpu load) would be probably\n"
                              "       not what you want, therefore ignoring the request.\n";
        return -1;
    }

    return 0;
}

