//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <cstdio>
#include <string>
#include <cstring>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "ini/ini_processing.h"

#include "progs_cache.h"
#include "measurer.h"

#include "midi_inst_list.h"

int main(int argc, char**argv)
{
    if(argc == 1)
    {
        std::printf("Usage:\n"
               "\n"
               "bin/gen_adldata src/adldata.cpp\n"
               "\n");
        return 1;
    }

    const char *outFile_s = argv[1];

    BanksDump db;

    {
        IniProcessing ini;
        if(!ini.open("banks.ini"))
        {
            std::fprintf(stderr, "Can't open banks.ini!\n");
            return 1;
        }

        uint32_t banks_count;
        ini.beginGroup("General");
        ini.read("banks", banks_count, 0);
        ini.endGroup();

        if(!banks_count)
        {
            std::fprintf(stderr, "Zero count of banks found in banks.ini!\n");
            return 1;
        }

        for(uint32_t bank = 0; bank < banks_count; bank++)
        {
            if(!ini.beginGroup(std::string("bank-") + std::to_string(bank)))
            {
                std::fprintf(stderr, "Failed to find bank %u!\n", bank);
                return 1;
            }
            std::string bank_name;
            std::string filepath;
            std::string filepath_d;
            std::string prefix;
            std::string prefix_d;
            std::string filter_m;
            std::string filter_p;
            std::string format;
            bool noRhythmMode = false;

            ini.read("name",     bank_name, "Untitled");
            ini.read("format",   format,    "Unknown");
            ini.read("file",     filepath,  "");
            ini.read("file-p",   filepath_d,  "");
            ini.read("prefix",   prefix, "");
            ini.read("prefix-p", prefix_d, "");
            ini.read("filter-m", filter_m, "");
            ini.read("filter-p", filter_p, "");
            ini.read("no-rhythm-mode", noRhythmMode, false);

            if(filepath.empty())
            {
                std::fprintf(stderr, "Failed to load bank %u, file is empty!\n", bank);
                return 1;
            }

//            banknames.push_back(bank_name);

            //printf("Loading %s...\n", filepath.c_str());

            if(format == "AIL")
            {
                if(!BankFormats::LoadMiles(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "Bisqwit")
            {
                if(!BankFormats::LoadBisqwit(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "WOPL")
            {
                if(!BankFormats::LoadWopl(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "OP2")
            {
                if(!BankFormats::LoadDoom(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "EA")
            {
                if(!BankFormats::LoadEA(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "TMB")
            {
                if(!BankFormats::LoadTMB(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "Junglevision")
            {
                if(!BankFormats::LoadJunglevision(db, filepath.c_str(), bank, bank_name, prefix.c_str()))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "AdLibGold")
            {
                if(!BankFormats::LoadBNK2(db, filepath.c_str(), bank, bank_name, prefix.c_str(), filter_m, filter_p))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
            }
            else
            if(format == "HMI")
            {
                if(!BankFormats::LoadBNK(db, filepath.c_str(),  bank, bank_name, prefix.c_str(),   false, false))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
                if(!filepath_d.empty())
                {
                    if(!BankFormats::LoadBNK(db, filepath_d.c_str(), bank, bank_name, prefix_d.c_str(), false, true))
                    {
                        std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                        return 1;
                    }
                }
            }
            else
            if(format == "IBK")
            {
                if(!BankFormats::LoadIBK(db, filepath.c_str(),  bank, bank_name, prefix.c_str(),   false))
                {
                    std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                    return 1;
                }
                if(!filepath_d.empty())
                {
                    //printf("Loading %s... \n", filepath_d.c_str());
                    if(!BankFormats::LoadIBK(db, filepath_d.c_str(),bank, bank_name, prefix_d.c_str(), true, noRhythmMode))
                    {
                        std::fprintf(stderr, "Failed to load bank %u, file %s!\n", bank, filepath.c_str());
                        return 1;
                    }
                }
            }
            else
            {
                std::fprintf(stderr, "Failed to load bank %u, file %s!\nUnknown format type %s\n",
                        bank,
                        filepath.c_str(),
                        format.c_str());
                return 1;
            }


            ini.endGroup();
        }

        std::printf("Loaded %u banks!\n", banks_count);
        std::fflush(stdout);
    }

    MeasureThreaded measureCounter;

    {
        measureCounter.LoadCache("fm_banks/adldata-cache.dat");
        measureCounter.m_cache_matches = 0;
        measureCounter.m_done = 0;
        measureCounter.m_total = db.instruments.size();
        std::printf("Beginning to generate measures data... (hardware concurrency of %d)\n", std::thread::hardware_concurrency());
        std::fflush(stdout);
        for(size_t b = 0; b < db.instruments.size(); ++b)
        {
            assert(db.instruments[b].instId == b);
            measureCounter.run(db, db.instruments[b]);
        }
        std::fflush(stdout);
        measureCounter.waitAll();
        measureCounter.SaveCache("fm_banks/adldata-cache.dat");
    }

    db.exportBanks(std::string(outFile_s));

    std::printf("Generation of ADLMIDI data has been completed!\n");
    std::fflush(stdout);
}
