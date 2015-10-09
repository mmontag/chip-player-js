//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <stdio.h>
#include <vector>
#include <cstring>

struct BNK1_header
{
   char maj_vers, min_vers;
   char signature[6];        // "ADLIB-"
   unsigned short ins_used, ins_entries;
   unsigned name_list, inst_data;
} __attribute__((packed));
struct BNK1_record
{
   unsigned short index;
   unsigned char used;
   char name[9];
} __attribute__((packed));
struct OPL2_op
{
   unsigned char key_scale_lvl;
   unsigned char freq_mult;
   unsigned char feedback;
   unsigned char attack;
   unsigned char sustain_lvl;
   unsigned char sustain_sound;
   unsigned char decay;
   unsigned char release;
   unsigned char out_lvl;
   unsigned char amp_vib;
   unsigned char pitch_vib;
   unsigned char env_scaling;
   unsigned char connection;
} __attribute__((packed));
struct BNK1_instrument
{
   unsigned char sound_mode;
   unsigned char voice_num;
   OPL2_op ops[2];
   unsigned char waveforms[2];
} __attribute__((packed)); // conventional Ad Lib instrument maker bankfile patch

struct BNK2_header
{
   char signature[28]; // "Accomp. Bank, (C) AdLib Inc"
   unsigned short file_ver;
   char filler[10];
   unsigned short ins_entries, ins_used;
   int lostSpace;
} __attribute__((packed));
struct BNK2_record
{
   char O3_sig[3];
   char key[12];
   char used;
   int attrib, dataOffset;
   unsigned short blockSize, allocBSize;
} __attribute__((packed));
struct OPL3_op
{
   unsigned char AVEKMMMM, KKLLLLLL;
   unsigned char AAAADDDD, SSSSRRRR;
   unsigned char DxxxxWWW, xxxxxxxx;
} __attribute__((packed));
struct BNK2_instrument
{
   OPL3_op ops[4];
   unsigned char C4xxxFFFC;
   unsigned char xxP24NNN;
   unsigned char TTTTTTTT;
   unsigned char xxxxxxxx;
} __attribute__((packed)); // Ad Lib Gold instrument maker bankfile patch

static void LoadBNK1(const std::vector<unsigned char>& data)
{
    const BNK1_header& bnk1 = *(const BNK1_header*) &data[0];
    const BNK1_record* names = (const BNK1_record*) &data[ bnk1.name_list ];
    const BNK1_instrument* ins = (const BNK1_instrument*) &data[ bnk1.inst_data ];
    printf("BNK1 version: %d.%d\n", bnk1.maj_vers, bnk1.min_vers);
    for(unsigned a=0; a<bnk1.ins_entries; ++a)
    {
        printf("%04X %02X %-9.9s: ",
            names[a].index, names[a].used, names[a].name);

        const BNK1_instrument& i = ins[ names[a].index ];
        printf("%02X %02X %X %X",
            i.sound_mode, i.voice_num, i.waveforms[0], i.waveforms[1]);
        for(unsigned b=0; b<2; ++b)
        {
            printf(" | ");
            printf("%X %X %X %X %X %X %X %X %02X %X %X %X %X",
                i.ops[b].key_scale_lvl,
                i.ops[b].freq_mult,
                b==1 ? 0 : i.ops[b].feedback,
                i.ops[b].attack,
                i.ops[b].sustain_lvl,
                i.ops[b].sustain_sound,
                i.ops[b].decay,
                i.ops[b].release,
                i.ops[b].out_lvl,
                i.ops[b].amp_vib,
                i.ops[b].pitch_vib,
                i.ops[b].env_scaling,
                b==1 ? 0 : i.ops[b].connection);
        }
        printf("\n");
    }
}
static void LoadBNK2(const std::vector<unsigned char>& data)
{
    const BNK2_header& bnk2 = *(const BNK2_header*) &data[0];
    const BNK2_record* names = (const BNK2_record*) &data[ sizeof(bnk2) ];
    printf("BNK2 version: %d, lost space %d\n", bnk2.file_ver, bnk2.lostSpace);
    for(unsigned a=0; a<bnk2.ins_entries; ++a)
    {
        printf("%3.3s %-12.12s %02X %08X %04X %04X: ",
            names[a].O3_sig,
            names[a].key,
            names[a].used,
            names[a].attrib,
            names[a].blockSize,
            names[a].allocBSize);

        const BNK2_instrument& i = *(const BNK2_instrument*) &data[ names[a].dataOffset ];
        printf("%02X %02X %02X %02X",
            i.C4xxxFFFC, i.xxP24NNN, i.TTTTTTTT, i.xxxxxxxx);
        for(unsigned b=0; b<4; ++b)
        {
            printf(" | ");
            printf("%02X %02X %02X %02X %02X %02X",
                i.ops[b].AVEKMMMM,
                i.ops[b].KKLLLLLL,
                i.ops[b].AAAADDDD,
                i.ops[b].SSSSRRRR,
                i.ops[b].DxxxxWWW,
                i.ops[b].xxxxxxxx);
        }
        printf("\n");
    }
}

static void LoadBNK(const char* fn)
{
    FILE* fp = fopen(fn, "rb");
    fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    rewind(fp);
    fread(&data[0], 1, data.size(), fp),
    fclose(fp);

    const BNK1_header& bnk1 = *(const BNK1_header*) &data[0];
    const BNK2_header& bnk2 = *(const BNK2_header*) &data[0];

    if(std::memcmp(bnk1.signature, "ADLIB-", 6) == 0)
        LoadBNK1(data);
    else if(std::memcmp(bnk2.signature, "Accomp. Bank, (C) AdLib Inc", 28) == 0)
        LoadBNK2(data);
    else
        fprintf(stderr, "%s: Unknown format\n", fn);
}

int main(int argc, const char* const* argv)
{
    LoadBNK(argv[1]);
}
