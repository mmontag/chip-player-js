#include <catch.hpp>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "wopl/wopl_file.h"

static const char *test_files[] = {
    "fm_banks/wopl_files/Apogee-IMF-90.wopl",
    "fm_banks/wopl_files/DMXOPL3-by-sneakernets-GS.wopl",
    "fm_banks/wopl_files/GM-By-J.A.Nguyen-and-Wohlstand.wopl",
    "fm_banks/wopl_files/lostvik.wopl",
    "fm_banks/wopl_files/Wohlstand's-modded-FatMan.wopl"
};

static WOPLFile *LoadBankFromFile(const char *path);

TEST_CASE("[WOPLFile] Load, Save, Load")
{
    for (int version : {1, 2}) {
        for (const char *test_file : test_files) {
            fprintf(stderr, "--- Test '%s' with version %d\n", test_file, version);

            WOPLFile *wopl = LoadBankFromFile(test_file);
            REQUIRE(wopl != nullptr);

            size_t size = WOPL_CalculateBankFileSize(wopl, version);
            char *mem = new char[size];

            REQUIRE(WOPL_SaveBankToMem(wopl, mem, size, version, 0) == 0);

            {
                int error;
                WOPLFile *wopl2 = WOPL_LoadBankFromMem(mem, size, &error);
                if(!wopl2)
                    fprintf(stderr, "--- Loading error %d\n", error);
                REQUIRE(wopl2);

                if(wopl->version == version)
                    REQUIRE(WOPL_BanksCmp(wopl, wopl2) == 1);
                else {
                    REQUIRE(wopl->banks_count_melodic == wopl2->banks_count_melodic);
                    REQUIRE(wopl->banks_count_percussion == wopl2->banks_count_percussion);
                    REQUIRE(wopl->opl_flags == wopl2->opl_flags);
                    REQUIRE(wopl->volume_model == wopl2->volume_model);
                }

                WOPL_Free(wopl2);
            }

            delete[] mem;

            {
                unsigned melo_banks = wopl->banks_count_melodic;
                unsigned drum_banks = wopl->banks_count_percussion;

                for(unsigned Bi = 0; Bi < melo_banks + drum_banks; ++Bi)
                {
                    const WOPLBank &bank = (Bi < melo_banks) ?
                        wopl->banks_melodic[Bi] : wopl->banks_percussive[Bi - melo_banks];

                    for(unsigned Pi = 0; Pi < 128; ++Pi)
                    {
                        WOPIFile opli = {};
                        opli.version = version;
                        opli.is_drum = Bi >= melo_banks;
                        opli.inst = bank.ins[Pi];

                        size = WOPL_CalculateInstFileSize(&opli, version);
                        mem = new char[size];

                        REQUIRE(WOPL_SaveInstToMem(&opli, mem, size, version) == 0);

                        WOPIFile opli2 = {};
                        int error = WOPL_LoadInstFromMem(&opli2, mem, size);
                        if(error != 0)
                            fprintf(stderr, "--- Loading error %d\n", error);
                        REQUIRE(error == 0);

                        size_t compare_size = sizeof(WOPIFile) - 2 * sizeof(uint16_t);
                        REQUIRE(memcmp(&opli, &opli2, compare_size) == 0);

                        delete[] mem;
                    }
                }
            }

            WOPL_Free(wopl);
        }
    }
}

static WOPLFile *LoadBankFromFile(const char *path)
{
    WOPLFile *file = nullptr;
    FILE *fh;
    struct stat st;
    char *mem = nullptr;

    if(!(fh = fopen(path, "rb")) || fstat(fileno(fh), &st) != 0)
        goto fail;

    mem = new char[st.st_size];
    if(fread(mem, st.st_size, 1, fh) != 1)
        goto fail;

    int error;
    file = WOPL_LoadBankFromMem(mem, st.st_size, &error);
    if(!file)
        fprintf(stderr, "--- Loading error %d\n", error);

fail:
    delete[] mem;
    if (fh) fclose(fh);
    return file;
}
