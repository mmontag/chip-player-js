//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <cstdio>
#include <vector>
#include <stdio.h>

static void LoadMiles(const char* fn)
{
    FILE* fp = std::fopen(fn, "rb");
    if(!fp)
    {
        std::fprintf(stderr, "ERROR: Can't open %s file!", fn);
        return;
    }

    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    std::rewind(fp);
    size_t got = std::fread(&data[0], 1, data.size(), fp);
    std::fclose(fp);

    if(got == 0)
    {
        std::fprintf(stderr, "ERROR: Can't read %s file!", fn);
        return;
    }

    for(unsigned a=0; a<500; ++a)
    {
        unsigned gmnumber  = data[a*6+0];
        unsigned gmnumber2 = data[a*6+1];
        unsigned offset    = *(unsigned*)&data[a*6+2];

        if(gmnumber == 0xFF) break;
        //int gmno = gmnumber2==0x7F ? gmnumber+0x80 : gmnumber;
        unsigned length = data[offset] + data[offset+1]*256;
        //signed char notenum = data[offset+2];

        std::printf("%02X %02X ", gmnumber,gmnumber2); //, offset);
        for(unsigned b=0; b<length; ++b)
        {
            if(b > 3 && (b-3)%11 == 0) std::printf("\n                        ");
            std::printf("%02X ", data[offset+b]);
        }
        std::printf("\n");
    }
}

int main(int argc, const char* const* argv)
{
    if(argc < 2)
    {
        std::printf("Usage: \n"
                    "   %s filename.opl\n"
                    "or:\n"
                    "   %s filename.opl\n"
                    "\n", argv[0], argv[0]);
        return 1;
    }
    LoadMiles(argv[1]);
}
