//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <map>
#include <set>

#include "midi_inst_list.h"

std::map<unsigned,
    std::map<unsigned, unsigned>
        > Correlate;
unsigned maxvalues[30] = { 0 };

struct insdata
{
    unsigned char data[11];
    signed char   finetune;
    bool          diff;
    bool operator==(const insdata& b) const
    {
        return std::memcmp(data, b.data, 11) == 0 && finetune == b.finetune && diff == b.diff;
    }
    bool operator< (const insdata& b) const
    {
        int c = std::memcmp(data, b.data, 11);
        if(c != 0) return c < 0;
        if(finetune != b.finetune) return finetune < b.finetune;
        if(diff != b.diff) return (!diff)==(b.diff);
        return 0;
    }
    bool operator!=(const insdata& b) const { return !operator==(b); }
};
struct ins
{
    size_t insno1, insno2;
    unsigned char notenum;
    bool pseudo4op;
    double fine_tune;

    bool operator==(const ins& b) const
    {
        return notenum == b.notenum
        && insno1 == b.insno1
        && insno2 == b.insno2
        && pseudo4op == b.pseudo4op;
    }
    bool operator< (const ins& b) const
    {
        if(insno1 != b.insno1) return insno1 < b.insno1;
        if(insno2 != b.insno2) return insno2 < b.insno2;
        if(notenum != b.notenum) return notenum < b.notenum;
        if(pseudo4op != b.pseudo4op) return pseudo4op < b.pseudo4op;
        return 0;
    }
    bool operator!=(const ins& b) const { return !operator==(b); }
};

static std::map<insdata, std::pair<size_t, std::set<std::string> > > insdatatab;

static std::map<ins, std::pair<size_t, std::set<std::string> > > instab;
static std::map<unsigned, std::map<unsigned, size_t> > progs;

static void SetBank(unsigned bank, unsigned patch, size_t insno)
{
    progs[bank][patch] = insno+1;
}

static size_t InsertIns(
    const insdata& id,
    const insdata& id2,
    ins& in,
    const std::string& name,
    const std::string& name2 = "")
{
  if(true)
  {
    std::map<insdata, std::pair<size_t,std::set<std::string> > >::iterator
        i = insdatatab.lower_bound(id);

    size_t insno = ~0;
    if(i == insdatatab.end() || i->first != id)
    {
        std::pair<insdata, std::pair<size_t,std::set<std::string> > > res;
        res.first = id;
        res.second.first = insdatatab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        insdatatab.insert(i, res);
        insno = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        insno = i->second.first;
    }

    in.insno1 = insno;
  }
  if( id != id2 )
  {
    std::map<insdata, std::pair<size_t,std::set<std::string> > >::iterator
        i = insdatatab.lower_bound(id2);
    size_t insno2 = ~0;
    if(i == insdatatab.end() || i->first != id2)
    {
        std::pair<insdata, std::pair<size_t,std::set<std::string> > > res;
        res.first = id2;
        res.second.first = insdatatab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        insdatatab.insert(i, res);
        insno2 = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        insno2 = i->second.first;
    }
    in.insno2 = insno2;
  }
  else
    in.insno2 = in.insno1;


  {
    std::map<ins, std::pair<size_t,std::set<std::string> > >::iterator
        i = instab.lower_bound(in);

    size_t resno = ~0;
    if(i == instab.end() || i->first != in)
    {
        std::pair<ins, std::pair<size_t,std::set<std::string> > > res;
        res.first = in;
        res.second.first = instab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        instab.insert(i, res);
        resno = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        resno = i->second.first;
    }
    return resno;
  }
}

// Create silent 'nosound' instrument
size_t InsertNoSoundIns()
{
    // { 0x0F70700,0x0F70710, 0xFF,0xFF, 0x0,+0 },
    insdata tmp1 = { {0x00, 0x10, 0x07, 0x07, 0xF7, 0xF7, 0x00, 0x00, 0xFF, 0xFF, 0x00}, 0, 0 };
    struct ins tmp2 = { 0, 0, 0, 0.0, 0 };
    return InsertIns(tmp1, tmp1, tmp2, "nosound");
}

static void LoadBNK(const char* fn, unsigned bank, const char* prefix, bool is_fat, bool percussive)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp),
    std::fclose(fp);

    /*printf("%s:\n", fn);*/
    unsigned short version = *(short*)&data[0]; // major,minor (2 bytes)
    //                                             "ADLIB-"    (6 bytes)
    unsigned short entries_used = *(short*)&data[8];   // entries used
    unsigned short total_entries = *(short*)&data[10]; // total entries
    unsigned       name_offset = *(unsigned*)&data[12];// name_offset
    unsigned       data_offset = *(unsigned*)&data[16];// data_offset
    // 16..23: 8 byte sof filler
    /*printf("version=%u %u %u %u %u\n",
        version, entries_used,total_entries,name_offset,data_offset);*/

    for(unsigned n=0; n<entries_used; ++n)
    {
        const size_t offset1 = name_offset + n*12;

        unsigned short data_index = data[offset1+0] + data[offset1+1]*256;
        unsigned char usage_flag = data[offset1+2];
        std::string name;
        for(unsigned p=0; p<9; ++p)
        {
            if(data[offset1+3+p] == '\0') break;
            name += char(data[offset1+3+p]);
        }

        const size_t offset2 = data_offset + data_index * 30;
        const unsigned char mode      = data[offset2+0];
        const unsigned char voice_num = data[offset2+1];
        const unsigned char* op1 = &data[offset2+2];  // 13 bytes
        const unsigned char* op2 = &data[offset2+15];
        const unsigned char waveform_mod = data[offset2+28];
        const unsigned char waveform_car = data[offset2+29];

        /*printf("%5d %3d %8s mode=%02X voice=%02X: ", data_index,usage_flag, name.c_str(),
            mode,voice_num);*/

        int gmno = is_fat
            ?   ((n & 127) + percussive*128)
            :   (n + percussive*128);

        if(is_fat && percussive)
        {
            if(name[2] == 'O'
            || name[2] == 'S')
            {
                gmno = 128 + std::stoi(name.substr(3));
            }
        }

        char name2[512];
        if(is_fat)
            sprintf(name2, "%s%c%u", prefix, percussive?'P':'M', gmno&127);
        else
            sprintf(name2, "%s%u", prefix, n);

        insdata tmp;
        tmp.data[0] = (op1[ 9] << 7) // TREMOLO FLAG
                    + (op1[10] << 6) // VIBRATO FLAG
                    + (op1[ 5] << 5) // SUSTAIN FLAG
                    + (op1[11] << 4) // SCALING FLAG
                     + op1[ 1];      // FREQMUL

        tmp.data[1] = (op2[ 9] << 7)
                    + (op2[10] << 6)
                    + (op2[ 5] << 5)
                    + (op2[11] << 4)
                     + op2[ 1];
        tmp.data[2] = op1[3]*0x10 + op1[6]; // ATTACK, DECAY
        tmp.data[3] = op2[3]*0x10 + op2[6];
        tmp.data[4] = op1[4]*0x10 + op1[7]; // SUSTAIN, RELEASE
        tmp.data[5] = op2[4]*0x10 + op2[7];
        tmp.data[6] = waveform_mod;
        tmp.data[7] = waveform_car;
        tmp.data[8] = op1[0]*0x40 + op1[8]; // KSL , LEVEL
        tmp.data[9] = op2[0]*0x40 + op2[8]; // KSL , LEVEL
        tmp.data[10] = op1[2]*2 + op1[12]; // FEEDBACK, ADDITIVEFLAG
        tmp.finetune = 0;
        tmp.diff=false;
        // Note: op2[2] and op2[12] are unused and contain garbage.
        ins tmp2;
        tmp2.notenum = is_fat ? voice_num : (percussive ? usage_flag : 0);
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        if(is_fat) tmp.data[10] ^= 1;

        size_t resno = InsertIns(tmp,tmp, tmp2, std::string(1,'\377')+name, name2);

        if(!is_fat)
        {
            SetBank(bank, gmno, resno);
        }
        else
        {
            if(name[2] == 'O' || name[1] == 'M') SetBank(bank+0, gmno, resno);
            if(name[2] == 'S' || name[1] == 'M') SetBank(bank+1, gmno, resno);
        }

        /*
        for(unsigned p=0; p<30; ++p)
        {
            unsigned char value = data[offset2+p];
            if(value > maxvalues[p]) maxvalues[p] = value;

            #define dot(maxval) if(value<=maxval) printf("."); else printf("?[%u]%X",p,value);

          {
            //if(p==6 || p==7 || p==19||p==20) value=15-value;

            if(p==4 || p==10 || p==17 || p==23)// || p==25)
                printf(" %2X", value);
            else
                printf(" %X", value);
         }
        nl:;
            //if(p == 12) printf("\n%*s", 22, "");
            //if(p == 25) printf("\n%*s", 22, "");
        }
        printf("\n");
        */
    }
}

static void LoadBNK2(const char* fn, unsigned bank, const char* prefix,
                     const std::string& melo_filter,
                     const std::string& perc_filter)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp),
    std::fclose(fp);

    unsigned short ins_entries = *(unsigned short*)&data[28+2+10];
    unsigned char* records = &data[48];

    for(unsigned n=0; n<ins_entries; ++n)
    {
        const size_t offset1 = n*28;
        int used     =         records[offset1 + 15];
        int attrib   = *(int*)&records[offset1 + 16];
        int offset2  = *(int*)&records[offset1 + 20];
        if(used == 0) continue;

        std::string name;
        for(unsigned p=0; p<12; ++p)
        {
            if(records[offset1+3+p] == '\0') break;
            name += char(records[offset1+3+p]);
        }

        int gmno = 0;
        if(name.substr(0, melo_filter.size()) == melo_filter)
            gmno = std::stoi(name.substr(melo_filter.size()));
        else if(name.substr(0, perc_filter.size()) == perc_filter)
            gmno = std::stoi(name.substr(perc_filter.size())) + 128;
        else
            continue;

        const unsigned char* insdata = &data[offset2];
        const unsigned char* ops[4] = { insdata+0, insdata+6, insdata+12, insdata+18 };
        unsigned char C4xxxFFFC = insdata[24];
        unsigned char xxP24NNN = insdata[25];
        unsigned char TTTTTTTT = insdata[26];
        unsigned char xxxxxxxx = insdata[27];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix, (gmno&128)?'P':'M', gmno&127);

        struct insdata tmp[2];
        for(unsigned a=0; a<2; ++a)
        {
            tmp[a].data[0] = ops[a*2+0][0];
            tmp[a].data[1] = ops[a*2+1][0];
            tmp[a].data[2] = ops[a*2+0][2];
            tmp[a].data[3] = ops[a*2+1][2];
            tmp[a].data[4] = ops[a*2+0][3];
            tmp[a].data[5] = ops[a*2+1][3];
            tmp[a].data[6] = ops[a*2+0][4] & 0x07;
            tmp[a].data[7] = ops[a*2+1][4] & 0x07;
            tmp[a].data[8] = ops[a*2+0][1];
            tmp[a].data[9] = ops[a*2+1][1];
            tmp[a].finetune = TTTTTTTT;
            tmp[a].diff=false;
        }
        tmp[0].data[10] = C4xxxFFFC & 0x0F;
        tmp[1].data[10] = (tmp[0].data[10] & 0x0E) | (C4xxxFFFC >> 7);

        ins tmp2;
        tmp2.notenum = (gmno & 128) ? 35 : 0;
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        if(xxP24NNN & 8)
        {
            // dual-op
            size_t resno = InsertIns(tmp[0],tmp[1], tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }
        else
        {
            // single-op
            size_t resno = InsertIns(tmp[0],tmp[0], tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }
    }
}

struct Doom_OPL2instrument {
  unsigned char trem_vibr_1;    /* OP 1: tremolo/vibrato/sustain/KSR/multi */
  unsigned char att_dec_1;      /* OP 1: attack rate/decay rate */
  unsigned char sust_rel_1;     /* OP 1: sustain level/release rate */
  unsigned char wave_1;         /* OP 1: waveform select */
  unsigned char scale_1;        /* OP 1: key scale level */
  unsigned char level_1;        /* OP 1: output level */
  unsigned char feedback;       /* feedback/AM-FM (both operators) */
  unsigned char trem_vibr_2;    /* OP 2: tremolo/vibrato/sustain/KSR/multi */
  unsigned char att_dec_2;      /* OP 2: attack rate/decay rate */
  unsigned char sust_rel_2;     /* OP 2: sustain level/release rate */
  unsigned char wave_2;         /* OP 2: waveform select */
  unsigned char scale_2;        /* OP 2: key scale level */
  unsigned char level_2;        /* OP 2: output level */
  unsigned char unused;
  short         basenote;       /* base note offset */
} __attribute__ ((packed));

struct Doom_opl_instr {
        unsigned short        flags;
#define FL_FIXED_PITCH  0x0001          // note has fixed pitch (drum note)
#define FL_UNKNOWN      0x0002          // ??? (used in instrument #65 only)
#define FL_DOUBLE_VOICE 0x0004          // use two voices instead of one

        unsigned char         finetune;
        unsigned char         note;
        struct Doom_OPL2instrument patchdata[2];
} __attribute__ ((packed));


static void LoadDoom(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp),
    std::fclose(fp);

    for(unsigned a=0; a<175; ++a)
    {
        const size_t offset1 = 0x18A4 + a*32;
        const size_t offset2 = 8      + a*36;

        std::string name;
        for(unsigned p=0; p<32; ++p)
            if(data[offset1]!='\0')
                name += char(data[offset1+p]);

        //printf("%3d %3d %3d %8s: ", a,b,c, name.c_str());
        int gmno = a<128 ? a : ((a|128)+35);

        char name2[512]; sprintf(name2, "%s%c%u", prefix, (gmno<128?'M':'P'), gmno&127);

        Doom_opl_instr& ins = *(Doom_opl_instr*) &data[offset2];

        insdata tmp[2];
        tmp[0].diff=false;
        tmp[1].diff=true;
        for(unsigned index=0; index<2; ++index)
        {
            const Doom_OPL2instrument& src = ins.patchdata[index];
            tmp[index].data[0] = src.trem_vibr_1;
            tmp[index].data[1] = src.trem_vibr_2;
            tmp[index].data[2] = src.att_dec_1;
            tmp[index].data[3] = src.att_dec_2;
            tmp[index].data[4] = src.sust_rel_1;
            tmp[index].data[5] = src.sust_rel_2;
            tmp[index].data[6] = src.wave_1;
            tmp[index].data[7] = src.wave_2;
            tmp[index].data[8] = src.scale_1 | src.level_1;
            tmp[index].data[9] = src.scale_2 | src.level_2;
            tmp[index].data[10] = src.feedback;
            tmp[index].finetune = src.basenote+12;
        }
        struct ins tmp2;
        tmp2.notenum  = ins.note;
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;
        while(tmp2.notenum && tmp2.notenum < 20)
        {
            tmp2.notenum += 12;
            tmp[0].finetune -= 12;
            tmp[1].finetune -= 12;
        }

        if(!(ins.flags&4))
        {
            size_t resno = InsertIns(tmp[0],tmp[0],tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }
        else // Double instrument
        {
            tmp2.pseudo4op = true;
            tmp2.fine_tune = ( ((double)ins.finetune-128.0)*15.625)/1000.0;
            if(ins.finetune==129)
                tmp2.fine_tune=0.000025;
            else if(ins.finetune==127)
                tmp2.fine_tune=-0.000025;
            //printf("/*DOOM FINE TUNE (flags %000X instrument is %d) IS %d -> %lf*/\n", ins.flags, a, ins.finetune, tmp2.fine_tune);
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }

        /*const Doom_OPL2instrument& A = ins.patchdata[0];
        const Doom_OPL2instrument& B = ins.patchdata[1];
        printf(
            "flags:%04X fine:%02X note:%02X\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d}\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d} ",
            ins.flags, ins.finetune, ins.note,
            A.trem_vibr_1, A.att_dec_1, A.sust_rel_1,
            A.wave_1, A.scale_1, A.level_1, A.feedback,
            A.trem_vibr_2, A.att_dec_2, A.sust_rel_2,
            A.wave_2, A.scale_2, A.level_2, A.unused, A.basenote,
            B.trem_vibr_1, B.att_dec_1, B.sust_rel_1,
            B.wave_1, B.scale_1, B.level_1, B.feedback,
            B.trem_vibr_2, B.att_dec_2, B.sust_rel_2,
            B.wave_2, B.scale_2, B.level_2, B.unused, B.basenote);
        printf(" %s VS %s\n", name.c_str(), MidiInsName[a]);
        printf("------------------------------------------------------------\n");*/
    }
}
static void LoadMiles(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp),
    std::fclose(fp);

    for(unsigned a=0; a<2000; ++a)
    {
        unsigned gm_patch  = data[a*6+0];
        unsigned gm_bank = data[a*6+1];
        unsigned offset    = *(unsigned*)&data[a*6+2];

        if(gm_patch == 0xFF)
            break;

        int gmno = gm_bank==0x7F ? gm_patch + 0x80 : gm_patch;
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;
        unsigned length = data[offset] + data[offset+1]*256;
        signed char notenum = ((signed char)data[offset+2]);

        /*printf("%02X %02X %08X ", gmnumber,gmnumber2, offset);
        for(unsigned b=0; b<length; ++b)
        {
            if(b > 3 && (b-3)%11 == 0) printf("\n                        ");
            printf("%02X ", data[offset+b]);
        }
        printf("\n");*/

        if(gm_bank != 0 && gm_bank != 0x7F)
            continue;

        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno < 128 ? 'M':'P'), gmno & 127);

        insdata tmp[200];

        const unsigned inscount = (length-3)/11;
        for(unsigned i=0; i<inscount; ++i)
        {
            unsigned o = offset + 3 + i*11;
            tmp[i].finetune = (gmno < 128 && i == 0) ? notenum : 0;
            tmp[i].diff = false;
            tmp[i].data[0] = data[o+0];  // 20
            tmp[i].data[8] = data[o+1];  // 40 (vol)
            tmp[i].data[2] = data[o+2];  // 60
            tmp[i].data[4] = data[o+3];  // 80
            tmp[i].data[6] = data[o+4];  // E0
            tmp[i].data[1] = data[o+6];  // 23
            tmp[i].data[9] = data[o+7]; // 43 (vol)
            tmp[i].data[3] = data[o+8]; // 63
            tmp[i].data[5] = data[o+9]; // 83
            tmp[i].data[7] = data[o+10]; // E3

            unsigned fb_c = data[offset+3+5];
            tmp[i].data[10] = fb_c;
            if(i == 1)
            {
                tmp[0].data[10] = fb_c & 0x0F;
                tmp[1].data[10] = (fb_c & 0x0E) | (fb_c >> 7);
            }
        }
        if(inscount == 1)
            tmp[1] = tmp[0];

        if(inscount <= 2)
        {
            struct ins tmp2;
            tmp2.notenum  = gmno < 128 ? 0 : notenum;
            tmp2.pseudo4op = false;
            tmp2.fine_tune = 0.0;
            std::string name;
            if(midi_index >= 0) name = std::string(1,'\377')+MidiInsName[midi_index];
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2);
            SetBank(bank, gmno, resno);
        }
    }
}

static void LoadIBK(const char* fn, unsigned bank, const char* prefix, bool percussive)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp),
    std::fclose(fp);

    unsigned offs1_base = 0x804, offs1_len = 9;
    unsigned offs2_base = 0x004, offs2_len = 16;

    for(unsigned a=0; a<128; ++a)
    {
        unsigned offset1 = offs1_base + a*offs1_len;
        unsigned offset2 = offs2_base + a*offs2_len;

        std::string name;
        for(unsigned p=0; p<9; ++p)
            if(data[offset1]!='\0')
                name += char(data[offset1+p]);

        int gmno = a + 128*percussive;
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;
        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno<128?'M':'P'), gmno&127);

        insdata tmp;
        tmp.data[0] = data[offset2+0];
        tmp.data[1] = data[offset2+1];
        tmp.data[8] = data[offset2+2];
        tmp.data[9] = data[offset2+3];
        tmp.data[2] = data[offset2+4];
        tmp.data[3] = data[offset2+5];
        tmp.data[4] = data[offset2+6];
        tmp.data[5] = data[offset2+7];
        tmp.data[6] = data[offset2+8];
        tmp.data[7] = data[offset2+9];
        tmp.data[10] = data[offset2+10];
        // [+11] seems to be used also, what is it for?
        tmp.finetune = 0;
        tmp.diff=false;
        struct ins tmp2;
        tmp2.notenum  = gmno < 128 ? 0 : 35;
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        size_t resno = InsertIns(tmp,tmp, tmp2, std::string(1,'\377')+name, name2);
        SetBank(bank, gmno, resno);
    }
}

static void LoadJunglevision(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp);
    std::fclose(fp);

    unsigned ins_count = data[0x20] + (data[0x21] << 8);
    unsigned drum_count = data[0x22] + (data[0x23] << 8);
    unsigned first_ins = data[0x24] + (data[0x25] << 8);
    unsigned first_drum = data[0x26] + (data[0x27] << 8);

    unsigned total_ins = ins_count + drum_count;

    for ( unsigned a = 0; a < total_ins; ++a )
    {
        unsigned offset = 0x28 + a * 0x18;
        unsigned gmno = (a < ins_count) ? (a + first_ins) : (a + first_drum);
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;

        insdata tmp[2];

        tmp[0].data[0] = data[offset + 2];
        tmp[0].data[1] = data[offset + 8];
        tmp[0].data[2] = data[offset + 4];
        tmp[0].data[3] = data[offset + 10];
        tmp[0].data[4] = data[offset + 5];
        tmp[0].data[5] = data[offset + 11];
        tmp[0].data[6] = data[offset + 6];
        tmp[0].data[7] = data[offset + 12];
        tmp[0].data[8] = data[offset + 3];
        tmp[0].data[9] = data[offset + 9];
        tmp[0].data[10] = data[offset + 7] & 0x0F;//~0x30;
        tmp[0].finetune = 0;
        tmp[0].diff=false;

        tmp[1].data[0] = data[offset + 2 + 11];
        tmp[1].data[1] = data[offset + 8 + 11];
        tmp[1].data[2] = data[offset + 4 + 11];
        tmp[1].data[3] = data[offset + 10 + 11];
        tmp[1].data[4] = data[offset + 5 + 11];
        tmp[1].data[5] = data[offset + 11 + 11];
        tmp[1].data[6] = data[offset + 6 + 11];
        tmp[1].data[7] = data[offset + 12 + 11];
        tmp[1].data[8] = data[offset + 3 + 11];
        tmp[1].data[9] = data[offset + 9 + 11];
        tmp[1].data[10] = data[offset + 7 + 11] & 0x0F;//~0x30;
        tmp[1].finetune = 0;
        tmp[1].diff=false;

        struct ins tmp2;
        tmp2.notenum  = data[offset + 1];
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        while(tmp2.notenum && tmp2.notenum < 20)
        {
            tmp2.notenum += 12;
            tmp[0].finetune -= 12;
            tmp[1].finetune -= 12;
        }

        std::string name;
        if(midi_index >= 0) name = std::string(1,'\377')+MidiInsName[midi_index];

        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno<128?'M':'P'), gmno&127);

        if(!data[offset])
        {
            size_t resno = InsertIns(tmp[0],tmp[0],tmp2, name, name2);
            SetBank(bank, gmno, resno);
        }
        else // Double instrument
        {
            size_t resno = InsertIns(tmp[0],tmp[1],tmp2, name, name2);
            SetBank(bank, gmno, resno);
        }
    }
}

static void LoadTMB(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = std::fopen(fn, "rb");
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(std::ftell(fp));
    std::rewind(fp);
    std::fread(&data[0], 1, data.size(), fp);
    std::fclose(fp);

    for ( unsigned a = 0; a < 256; ++a )
    {
        unsigned offset = a * 0x0D;
        unsigned gmno = a;
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;

        insdata tmp;

        tmp.data[0] = data[offset + 0];
        tmp.data[1] = data[offset + 1];
        tmp.data[2] = data[offset + 4];
        tmp.data[3] = data[offset + 5];
        tmp.data[4] = data[offset + 6];
        tmp.data[5] = data[offset + 7];
        tmp.data[6] = data[offset + 8];
        tmp.data[7] = data[offset + 9];
        tmp.data[8] = data[offset + 2];
        tmp.data[9] = data[offset + 3];
        tmp.data[10] = data[offset + 10];
        tmp.finetune = 0; //data[offset + 12];
        tmp.diff=false;

        struct ins tmp2;
        tmp2.notenum   = data[offset + 11];
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        std::string name;
        if(midi_index >= 0) name = std::string(1,'\377')+MidiInsName[midi_index];

        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno<128?'M':'P'), gmno&127);

        size_t resno = InsertIns(tmp,tmp,tmp2, name, name2);
        SetBank(bank, gmno, resno);
    }
}

static void LoadBisqwit(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = std::fopen(fn, "rb");
    for ( unsigned a = 0; a < 256; ++a )
    {
        unsigned offset = a * 25;
        unsigned gmno = a;
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;

        struct ins tmp2;
        tmp2.notenum = std::fgetc(fp);
        tmp2.pseudo4op = false;
        tmp2.fine_tune = 0.0;

        insdata tmp[2];
        for(int side=0; side<2; ++side)
        {
            tmp[side].finetune = std::fgetc(fp);
            tmp[side].diff=false;
            std::fread(tmp[side].data, 1, 11, fp);
        }

        std::string name;
        if(midi_index >= 0) name = std::string(1,'\377')+MidiInsName[midi_index];

        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno<128?'M':'P'), gmno&127);

        size_t resno = InsertIns(tmp[0],tmp[1],tmp2, name, name2);
        SetBank(bank, gmno, resno);
    }
    std::fclose(fp);
}

#include "../nukedopl3.h"

std::vector<int16_t> stereoSampleBuf; 

struct DurationInfo
{
    long ms_sound_kon;
    long ms_sound_koff;
    bool nosound;
};
static DurationInfo MeasureDurations(const ins& in)
{
    insdata id[2];
    bool found[2] = {false,false};
    for(std::map<insdata, std::pair<size_t, std::set<std::string> > >
        ::const_iterator
        j = insdatatab.begin();
        j != insdatatab.end();
        ++j)
    {
        if(j->second.first == in.insno1) { id[0] = j->first; found[0]=true; if(found[1]) break; }
        if(j->second.first == in.insno2) { id[1] = j->first; found[1]=true; if(found[0]) break; }
    }
    const unsigned rate = 22050;
    const unsigned interval             = 150;
    const unsigned samples_per_interval = rate / interval;
    const int notenum =
        in.notenum < 20   ? (44 + in.notenum)
      : in.notenum >= 128 ? (44 + 128 - in.notenum)
      : in.notenum;

    _opl3_chip opl;
    static const short initdata[(2+3+2+2)*2] =
    { 0x004,96, 0x004,128,        // Pulse timer
      0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable, leave disabled
      0x001,32, 0x0BD,0           // Enable wave & melodic
    };
    OPL3_Reset(&opl, rate);
    for(unsigned a=0; a<18; a+=2) OPL3_WriteReg(&opl, initdata[a], initdata[a+1]);

    const unsigned n_notes = in.insno1 == in.insno2 ? 1 : 2;
    unsigned x[2];

    if (n_notes == 2 && !in.pseudo4op)
    {
        OPL3_WriteReg(&opl, 0x105, 1);
        OPL3_WriteReg(&opl, 0x104, 1);
    }

    for(unsigned n=0; n<n_notes; ++n)
    {
        static const unsigned char patchdata[11] =
            {0x20,0x23,0x60,0x63,0x80,0x83,0xE0,0xE3,0x40,0x43,0xC0};
        for(unsigned a=0; a<10; ++a) OPL3_WriteReg(&opl, patchdata[a]+n*8, id[n].data[a]);
        OPL3_WriteReg(&opl, patchdata[10]+n*8, id[n].data[10] | 0x30);
    }
    for(unsigned n=0; n<n_notes; ++n)
    {
        double hertz = 172.00093 * std::exp(0.057762265 * (notenum + id[n].finetune));
        if(hertz > 131071)
        {
            fprintf(stderr, "Why does note %d + finetune %d produce hertz %g?\n",
                notenum, id[n].finetune, hertz);
            hertz = 131071;
        }
        x[n] = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x[n] += 0x400; } // Calculate octave
        x[n] += (int)(hertz + 0.5);

        // Keyon the note
        OPL3_WriteReg(&opl, 0xA0+n*3, x[n]&0xFF);
        OPL3_WriteReg(&opl, 0xB0+n*3, x[n]>>8);
    }

    const unsigned max_on = 40;
    const unsigned max_off = 60;

    // For up to 40 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_on;
    double highest_sofar = 0;
    for(unsigned period=0; period<max_on*interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2);
        OPL3_GenerateStream(&opl, stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;
        for(unsigned long c=0; c<samples_per_interval; ++c)
            mean += stereoSampleBuf[c*2];
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c=0; c<samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c*2]-mean);
            std_deviation += diff*diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_on.push_back(std_deviation);
        if(std_deviation > highest_sofar)
            highest_sofar = std_deviation;

        if(period > 6*interval && std_deviation < highest_sofar*0.2)
            break;
    }

    // Keyoff the note
    for(unsigned n=0; n<n_notes; ++n)
        OPL3_WriteReg(&opl, 0xB0+n, (x[n]>>8) & 0xDF);

    // Now, for up to 60 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_off;
    for(unsigned period=0; period<max_off*interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2);
        OPL3_GenerateStream(&opl, stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;
        for(unsigned long c=0; c<samples_per_interval; ++c)
            mean += stereoSampleBuf[c*2];
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c=0; c<samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c*2]-mean);
            std_deviation += diff*diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_off.push_back(std_deviation);

        if(std_deviation < highest_sofar*0.2) break;
    }

    /* Analyze the results */
    double begin_amplitude        = amplitudecurve_on[0];
    double peak_amplitude_value   = begin_amplitude;
    size_t peak_amplitude_time    = 0;
    size_t quarter_amplitude_time = amplitudecurve_on.size();
    size_t keyoff_out_time        = 0;

    for(size_t a=1; a<amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] > peak_amplitude_value)
        {
            peak_amplitude_value = amplitudecurve_on[a];
            peak_amplitude_time  = a;
        }
    }
    for(size_t a=peak_amplitude_time; a<amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] <= peak_amplitude_value * 0.2)
        {
            quarter_amplitude_time = a;
            break;
        }
    }
    for(size_t a=0; a<amplitudecurve_off.size(); ++a)
    {
        if(amplitudecurve_off[a] <= peak_amplitude_value * 0.2)
        {
            keyoff_out_time = a;
            break;
        }
    }

    if(keyoff_out_time == 0 && amplitudecurve_on.back() < peak_amplitude_value*0.2)
        keyoff_out_time = quarter_amplitude_time;

    if(peak_amplitude_time == 0)
    {
        printf(
            "    // Amplitude begins at %6.1f,\n"
            "    // fades to 20%% at %.1fs, keyoff fades to 20%% in %.1fs.\n",
            begin_amplitude,
            quarter_amplitude_time / double(interval),
            keyoff_out_time / double(interval));
    }
    else
    {
        printf(
            "    // Amplitude begins at %6.1f, peaks %6.1f at %.1fs,\n"
            "    // fades to 20%% at %.1fs, keyoff fades to 20%% in %.1fs.\n",
            begin_amplitude,
            peak_amplitude_value,
            peak_amplitude_time / double(interval),
            quarter_amplitude_time / double(interval),
            keyoff_out_time / double(interval));
    }

    DurationInfo result;
    result.ms_sound_kon  = quarter_amplitude_time * 1000.0 / interval;
    result.ms_sound_koff = keyoff_out_time        * 1000.0 / interval;
    result.nosound = (peak_amplitude_value < 0.5);
    return result;
}

int main()
{
    printf("\
#include \"adldata.hh\"\n\
\n\
/* THIS ADLIB FM INSTRUMENT DATA IS AUTOMATICALLY GENERATED\n\
 * FROM A NUMBER OF SOURCES, MOSTLY PC GAMES.\n\
 * PREPROCESSED, CONVERTED, AND POSTPROCESSED OFF-SCREEN.\n\
 */\n\
");
    size_t nosound = InsertNoSoundIns();

    LoadMiles("fm_banks/opl_files/sc3.opl",  0, "G"); // Our "standard" bank! Same as file22.opl

    LoadBisqwit("fm_banks/op3_files/bisqwit.adlraw", 1, "Bisq");

    LoadBNK("fm_banks/bnk_files/melodic.bnk", 2, "HMIGM", false, false); // same as file156.bnk
    LoadBNK("fm_banks/bnk_files/drum.bnk",    2, "HMIGP", false, true);
    LoadBNK("fm_banks/bnk_files/intmelo.bnk", 3, "intM", false, false);
    LoadBNK("fm_banks/bnk_files/intdrum.bnk", 3, "intP", false, true);
    LoadBNK("fm_banks/bnk_files/hammelo.bnk", 4, "hamM", false, false);
    LoadBNK("fm_banks/bnk_files/hamdrum.bnk", 4, "hamP", false, true);
    LoadBNK("fm_banks/bnk_files/rickmelo.bnk",5, "rickM", false, false);
    LoadBNK("fm_banks/bnk_files/rickdrum.bnk",5, "rickP", false, true);

    LoadBNK("fm_banks/bnk_files/d2melo.bnk",  6, "b6M", false, false);
    LoadBNK("fm_banks/bnk_files/d2drum.bnk",  6, "b6P", false, true);
    LoadBNK("fm_banks/bnk_files/normmelo.bnk", 7, "b7M", false, false);
    LoadBNK("fm_banks/bnk_files/normdrum.bnk", 7, "b7P", false, true); // same as file122.bnk
    LoadBNK("fm_banks/bnk_files/ssmelo.bnk",  8, "b8M", false, false);
    LoadBNK("fm_banks/bnk_files/ssdrum.bnk",  8, "b8P", false, true);

    LoadTMB("fm_banks/bnk_files/themepark.tmb", 9, "b9MP");
    //LoadBNK("fm_banks/bnk_files/file131.bnk", 9, "b9M", false, false);
    //LoadBNK("fm_banks/bnk_files/file132.bnk", 9, "b9P", false, true);
    LoadBNK("fm_banks/bnk_files/file133.bnk", 10,"b10P", false, true);
    LoadBNK("fm_banks/bnk_files/file134.bnk", 10,"b10M", false, false);
    LoadBNK("fm_banks/bnk_files/file142.bnk", 11, "b11P", false, true);
    LoadBNK("fm_banks/bnk_files/file143.bnk", 11, "b11M", false, false);
    LoadBNK("fm_banks/bnk_files/file145.bnk", 12, "b12M", false, false);//file145 is MELODIC
    LoadBNK("fm_banks/bnk_files/file144.bnk", 12, "b12P", false, true);//file144  is DRUMS
    LoadBNK("fm_banks/bnk_files/file167.bnk", 13, "b13P", false, true);
    LoadBNK("fm_banks/bnk_files/file168.bnk", 13, "b13M", false, false);

    LoadDoom("fm_banks/doom2/genmidi.op2", 14, "dM");
    LoadDoom("fm_banks/doom2/genmidi.htc", 15, "hxM"); // same as genmidi.hxn
    LoadDoom("fm_banks/doom2/default.op2", 16, "mus");

    LoadMiles("fm_banks/opl_files/file17.opl", 17, "f17G");
    LoadMiles("fm_banks/opl_files/warcraft.ad", 18, "sG"); // same as file44, warcraft.opl
    LoadMiles("fm_banks/opl_files/file19.opl", 19, "f19G");
    LoadMiles("fm_banks/opl_files/file20.opl", 20, "f20G");
    LoadMiles("fm_banks/opl_files/file21.opl", 21, "f21G");
    LoadMiles("fm_banks/opl_files/nemesis.opl", 22, "nem");
    LoadMiles("fm_banks/opl_files/file23.opl", 23, "f23G");
    LoadMiles("fm_banks/opl_files/file24.opl", 24, "f24G");
    LoadMiles("fm_banks/opl_files/file25.opl", 25, "f25G");
    LoadMiles("fm_banks/opl_files/file26.opl", 26, "f26G");
    LoadMiles("fm_banks/opl_files/file27.opl", 27, "f27G");
    LoadMiles("fm_banks/opl_files/nhlpa.opl", 28, "nhl");
    LoadMiles("fm_banks/opl_files/file29.opl", 29, "f29G");
    LoadMiles("fm_banks/opl_files/file30.opl", 30, "f30G");
    LoadMiles("fm_banks/opl_files/file31.opl", 31, "f31G");
    LoadMiles("fm_banks/opl_files/file32.opl", 32, "f32G");
    LoadMiles("fm_banks/opl_files/file13.opl", 33, "f13G");
    LoadMiles("fm_banks/opl_files/file34.opl", 34, "f34G");
    LoadMiles("fm_banks/opl_files/file35.opl", 35, "f35G");
    LoadMiles("fm_banks/opl_files/file36.opl", 36, "f36G");
    LoadMiles("fm_banks/opl_files/file37.opl", 37, "f37G");
    LoadMiles("fm_banks/opl_files/simfarm.opl", 38, "qG");
    LoadMiles("fm_banks/opl_files/simfarm.ad", 39, "mG"); // same as file18.opl
    LoadMiles("fm_banks/opl_files/file12.opl", 40, "f12G");
    LoadMiles("fm_banks/opl_files/file41.opl", 41, "f41G");
    LoadMiles("fm_banks/opl_files/file42.opl", 42, "f42G");
    LoadMiles("fm_banks/opl_files/file47.opl", 43, "f47G");
    LoadMiles("fm_banks/opl_files/file48.opl", 44, "f48G");
    LoadMiles("fm_banks/opl_files/file49.opl", 45, "f49G");
    LoadMiles("fm_banks/opl_files/file50.opl", 46, "f50G");
    LoadMiles("fm_banks/opl_files/file53.opl", 47, "f53G");
        LoadBNK("fm_banks/bnk_files/file144.bnk", 47, "f53GD", false, true);//Attempt to append missing drums
    LoadMiles("fm_banks/opl_files/file54.opl", 48, "f54G");

    LoadMiles("fm_banks/opl_files/sample.ad",  49, "MG"); // same as file51.opl
    LoadMiles("fm_banks/opl_files/sample.opl", 50, "oG"); // same as file40.opl
    LoadMiles("fm_banks/opl_files/file15.opl", 51, "f15G");
    LoadMiles("fm_banks/opl_files/file16.opl", 52, "f16G");

    LoadBNK2("fm_banks/bnk_files/file159.bnk", 53, "b50", "gm","gps"); // fat-opl3
    LoadBNK2("fm_banks/bnk_files/file159.bnk", 54, "b51", "gm","gpo");

    LoadIBK("fm_banks/ibk_files/soccer-genmidi.ibk", 55, "b55M", false);
    LoadIBK("fm_banks/ibk_files/soccer-percs.ibk",   55, "b55P", true);
    LoadIBK("fm_banks/ibk_files/game.ibk",           56, "b56", false);
    LoadIBK("fm_banks/ibk_files/mt_fm.ibk",          57, "b57", false);

    LoadJunglevision("fm_banks/op3_files/fat2.op3", 58, "fat2");
    LoadJunglevision("fm_banks/op3_files/fat4.op3", 59, "fat4");
    LoadJunglevision("fm_banks/op3_files/jv_2op.op3", 60, "b60");
    LoadJunglevision("fm_banks/op3_files/wallace.op3", 61, "b61");

    LoadTMB("fm_banks/tmb_files/d3dtimbr.tmb", 62, "duke");
    LoadTMB("fm_banks/tmb_files/swtimbr.tmb",  63, "sw");

    LoadDoom("fm_banks/raptor/genmidi.op2", 64, "rapt");

	//LoadJunglevision("fm_banks/op3_files/fat2_modded.op3", 65, "b65M");
    LoadTMB("fm_banks/op3_files/gmopl_wohl_mod.tmb",  65, "b65");

	//LoadIBK("fm_banks/ibk_files/JOconnel.IBK", 		66, "b66M", false);
    //LoadIBK("fm_banks/ibk_files/my-gmopldrums.IBK", 66, "b66P", true);
    LoadTMB("fm_banks/op3_files/gmoconel.tmb",  66, "b66");

    LoadTMB("fm_banks/tmb_files/default.tmb",  67, "3drm67");
    //LoadDoom("fm_banks/doom2/wolfinstein.op2", 67, "wolf"); //Small experiment!

    //LoadJunglevision("fm_banks/op3_files/2x2.op3", 68, "2x2byJAN");
    LoadMiles("fm_banks/op3_files/2x2.opl", 68, "2x2byJAN");

    LoadTMB("fm_banks/tmb_files/bloodtmb.tmb",  69, "apgblood");
    LoadTMB("fm_banks/tmb_files/lee.tmb",  70, "apglee");
    LoadTMB("fm_banks/tmb_files/nam.tmb",  71, "apgnam");

    LoadDoom("fm_banks/doom2/DMXOPL-by-sneakernets.op2", 72, "skeakernets");

    //LoadBNK("bnk_files/grassman1.bnk", 63, "b63", false);
    //LoadBNK("bnk_files/grassman2.bnk", 64, "b64", false);

    //LoadIBK("ibk_files/nitemare_3d.ibk",  65, "b65G", false); // Seems to be identical to wallace.op3 despite different format!

    static const char* const banknames[] =
    {// 0
     "AIL (Star Control 3, Albion, Empire 2, Sensible Soccer, Settlers 2, many others)",
     "Bisqwit (selection of 4op and 2op)",
     "HMI (Descent, Asterix)", //melodic,drum
     "HMI (Descent:: Int)",    //intmelo,intdrum
     "HMI (Descent:: Ham)",    //hammelo,hamdrum
     "HMI (Descent:: Rick)",   //rickmelo,rickdrum
     "HMI (Descent 2)",        //d2melo,d2drum
     "HMI (Normality)",        //normmelo,normdrum
     "HMI (Shattered Steel)",  //ssmelo,ssdrum
     "HMI (Theme Park)", // file131, file132
     // 10
     "HMI (3d Table Sports, Battle Arena Toshinden)", //file133, file134
     "HMI (Aces of the Deep)", //file142, file143
     "HMI (Earthsiege)", //file144, file145
     "HMI (Anvil of Dawn)", //file167,file168
     "DMX (Doom           :: partially pseudo 4op)",
     "DMX (Hexen, Heretic :: partially pseudo 4op)",
     "DMX (MUS Play       :: partially pseudo 4op)",
     "AIL (Discworld, Grandest Fleet, Pocahontas, Slob Zone 3d, Ultima 4, Zorro)", // file17
     "AIL (Warcraft 2)",
     "AIL (Syndicate)", // file19
     // 20
     "AIL (Guilty, Orion Conspiracy, Terra Nova Strike Force Centauri :: 4op)", // file20
     "AIL (Magic Carpet 2)", // file21
     "AIL (Nemesis)",
     "AIL (Jagged Alliance)", //file23
     "AIL (When Two Worlds War :: 4op, MISSING INSTRUMENTS)", //file24
     "AIL (Bards Tale Construction :: MISSING INSTRUMENTS)", //file25
     "AIL (Return to Zork)", //file26
     "AIL (Theme Hospital)", //file27
     "AIL (National Hockey League PA)",
     "AIL (Inherit The Earth)", //file29
     // 30
     "AIL (Inherit The Earth, file two)", //file30
     "AIL (Little Big Adventure :: 4op)", //file31
     "AIL (Wreckin Crew)", //file32
     "AIL (Death Gate)", // file13
     "AIL (FIFA International Soccer)", //file34
     "AIL (Starship Invasion)", //file35
     "AIL (Super Street Fighter 2 :: 4op)", //file36
     "AIL (Lords of the Realm :: MISSING INSTRUMENTS)", //file37
     "AIL (SimFarm, SimHealth :: 4op)",
     "AIL (SimFarm, Settlers, Serf City)",
     // 40
     "AIL (Caesar 2 :: partially 4op, MISSING INSTRUMENTS)",   // file12
     "AIL (Syndicate Wars)", //file41
     "AIL (Bubble Bobble Feat. Rainbow Islands, Z)", //file42
     "AIL (Warcraft)", //file47
     "AIL (Terra Nova Strike Force Centuri :: partially 4op)", //file48
     "AIL (System Shock :: partially 4op)", //file49
     "AIL (Advanced Civilization)", //file50
     "AIL (Battle Chess 4000 :: partially 4op, melodic only)", //file53
     "AIL (Ultimate Soccer Manager :: partially 4op)", //file54
     "AIL (Air Bucks, Blue And The Gray, America Invades, Terminator 2029)", // sample.ad
     // 50
     "AIL (Ultima Underworld 2)", // sample.opl
     "AIL (Kasparov's Gambit)", // file15
     "AIL (High Seas Trader :: MISSING INSTRUMENTS)", // file16
     "AIL (Master of Magic, Master of Orion 2 :: 4op, std percussion)", //file159
     "AIL (Master of Magic, Master of Orion 2 :: 4op, orchestral percussion)", //file159
     "SB (Action Soccer)",
     "SB (3d Cyberpuck :: melodic only)",
     "SB (Simon the Sorcerer :: melodic only)",
     "OP3 (The Fat Man 2op set)",
     "OP3 (The Fat Man 4op set)",
     // 60
     "OP3 (JungleVision 2op set :: melodic only)",
     "OP3 (Wallace 2op set, Nitemare 3D :: melodic only)",
     "TMB (Duke Nukem 3D)",
     "TMB (Shadow Warrior)",
     "DMX (Raptor)",
	 "OP3 (Modded GMOPL by Wohlstand)",
	 "SB (Jammey O'Connel's bank)",
	 "TMB (Default bank of Build Engine)",
     "OP3 (4op bank by James Alan Nguyen)",
	 "TMB (Blood)",
     // 70
	 "TMB (Lee)",
	 "TMB (Nam)",
     "DMX (Bank by Sneakernets)"
    };

#if 0
    for(unsigned a=0; a<36*8; ++a)
    {
        if( (1 << (a%8)) > maxvalues[a/8]) continue;

        const std::map<unsigned,unsigned>& data = Correlate[a];
        if(data.empty()) continue;
        std::vector< std::pair<unsigned,unsigned> > correlations;
        for(std::map<unsigned,unsigned>::const_iterator
            i = data.begin();
            i != data.end();
            ++i)
        {
            correlations.push_back( std::make_pair( i->second,i->first ) );
        }
        std::sort(correlations.begin(), correlations.end());
        printf("Byte %2u bit %u=mask %02X:\n", a/8, a%8, 1<<(a%8));
        for(size_t c=0; c<correlations.size() && c < 10; ++c)
        {
            unsigned count = correlations[correlations.size()-1-c ].first;
            unsigned index = correlations[correlations.size()-1-c ].second;
            printf("\tAdldata index %u, bit %u=mask %02X (%u matches)\n",
                index/8, index%8, 1 << (index%8), count);
        }
    }
#endif

    printf(
        /*
        "static const struct\n"
        "{\n"
        "    unsigned modulator_E862, carrier_E862;  // See below\n"
        "    unsigned char modulator_40, carrier_40; // KSL/attenuation settings\n"
        "    unsigned char feedconn; // Feedback/connection bits for the channel\n"
        "    signed char finetune;   // Finetune\n"
        "} adl[] =\n"*/
        "const adldata adl[%u] =\n"
        "{ //    ,---------+-------- Wave select settings\n"
        "  //    | ,-------ч-+------ Sustain/release rates\n"
        "  //    | | ,-----ч-ч-+---- Attack/decay rates\n"
        "  //    | | | ,---ч-ч-ч-+-- AM/VIB/EG/KSR/Multiple bits\n"
        "  //    | | | |   | | | |\n"
        "  //    | | | |   | | | |     ,----+-- KSL/attenuation settings\n"
        "  //    | | | |   | | | |     |    |    ,----- Feedback/connection bits\n"
        "  //    | | | |   | | | |     |    |    |\n", (unsigned)insdatatab.size());
    for(size_t b=insdatatab.size(), c=0; c<b; ++c)
        for(std::map<insdata,std::pair<size_t,std::set<std::string> > >
            ::const_iterator
            i = insdatatab.begin();
            i != insdatatab.end();
            ++i)
        {
            if(i->second.first != c) continue;
            printf("    { ");

            unsigned carrier_E862 =
                (i->first.data[6] << 24)
              + (i->first.data[4] << 16)
              + (i->first.data[2] << 8)
              + (i->first.data[0] << 0);
            unsigned modulator_E862 =
                (i->first.data[7] << 24)
              + (i->first.data[5] << 16)
              + (i->first.data[3] << 8)
              + (i->first.data[1] << 0);
            printf("0x%07X,0x%07X, 0x%02X,0x%02X, 0x%X, %+d, %s",
                carrier_E862,
                modulator_E862,
                i->first.data[8],
                i->first.data[9],
                i->first.data[10],
                i->first.finetune,
                i->first.diff?"true":"false");

            std::string names;
            for(std::set<std::string>::const_iterator
                j = i->second.second.begin();
                j != i->second.second.end();
                ++j)
            {
                if(!names.empty()) names += "; ";
                if((*j)[0] == '\377')
                    names += j->substr(1);
                else
                    names += *j;
            }
            printf(" }, // %u: %s\n", (unsigned)c, names.c_str());
        }
    printf("};\n");
    /*printf("static const struct\n"
           "{\n"
           "    unsigned short adlno1, adlno2;\n"
           "    unsigned char tone;\n"
           "    unsigned char flags;\n"
           "    long ms_sound_kon;  // Number of milliseconds it produces sound;\n"
           "    long ms_sound_koff;\n"
           "} adlins[] =\n");*/
    printf("const struct adlinsdata adlins[%u] =\n", (unsigned)instab.size());
    printf("{\n");
    std::vector<unsigned> adlins_flags;
    for(size_t b=instab.size(), c=0; c<b; ++c)
        for(std::map<ins,std::pair<size_t,std::set<std::string> > >
            ::const_iterator
            i = instab.begin();
            i != instab.end();
            ++i)
        {
            if(i->second.first != c) continue;

            DurationInfo info = MeasureDurations(i->first);
            unsigned flags = (i->first.pseudo4op ? 1 : 0) | (info.nosound ? 2 : 0);

            printf("    {");
            printf("%4d,%4d,%3d, %d, %6ld,%6ld,%lf",
                (unsigned) i->first.insno1,
                (unsigned) i->first.insno2,
                (int)(i->first.notenum),
                flags,
                info.ms_sound_kon,
                info.ms_sound_koff,
                   i->first.fine_tune);
            std::string names;
            for(std::set<std::string>::const_iterator
                j = i->second.second.begin();
                j != i->second.second.end();
                ++j)
            {
                if(!names.empty()) names += "; ";
                if((*j)[0] == '\377')
                    names += j->substr(1);
                else
                    names += *j;
            }
            printf(" }, // %u: %s\n\n", (unsigned)c, names.c_str());
            fflush(stdout);
            adlins_flags.push_back(flags);
        }
    printf("};\n\n");

    //printf("static const unsigned short banks[][256] =\n");
    const unsigned bankcount = sizeof(banknames)/sizeof(*banknames);
    std::map<unsigned, std::vector<unsigned> > bank_data;
    for(unsigned bank=0; bank<bankcount; ++bank)
    {
        //bool redundant = true;
        std::vector<unsigned> data(256);
        for(unsigned p=0; p<256; ++p)
        {
            unsigned v = progs[bank][p];
            if(v == 0 || (adlins_flags[v-1]&2))
                v = nosound; // Blank.in
            else
                v -= 1;
            data[p] = v;
        }
        bank_data[bank] = data;
    }
    std::set<unsigned> listed;

    printf(
        "\n\n//Returns total number of generated banks\n"
        "int  maxAdlBanks()\n"
        "{"
        "   return %u;\n"
        "}\n\n"
        "const char* const banknames[%u] =\n", bankcount, bankcount);
    printf("{\n");
    for(unsigned bank=0; bank<bankcount; ++bank)
        printf("    \"%s\",\n", banknames[bank]);
    printf("};\n");

    printf("const unsigned short banks[%u][256] =\n", bankcount);
    printf("{\n");
    for(unsigned bank=0; bank<bankcount; ++bank)
    {
        printf("    { // bank %u, %s\n", bank, banknames[bank]);
        bool redundant = true;
        for(unsigned p=0; p<256; ++p)
        {
            unsigned v = bank_data[bank][p];
            if(listed.find(v) == listed.end())
            {
                listed.insert(v);
                redundant = false;
            }
            printf("%4d,", v);
            if(p%16 == 15) printf("\n");
        }
        printf("    },\n");
        if(redundant)
        {
            printf("    // Bank %u defines nothing new.\n", bank);
            for(unsigned refbank=0; refbank<bank; ++refbank)
            {
                bool match = true;
                for(unsigned p=0; p<256; ++p)
                    if(bank_data[bank][p] != nosound
                    && bank_data[bank][p] != bank_data[refbank][p])
                    {
                        match=false;
                        break;
                    }
                if(match)
                    printf("    // Bank %u is just a subset of bank %u!\n",
                        bank, refbank);
            }
        }
    }

    printf("};\n");
}
