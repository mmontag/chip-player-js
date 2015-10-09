//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <stdio.h>
#include <vector>

static void LoadMiles(const char* fn)
{
    FILE* fp = fopen(fn, "rb");
    fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    rewind(fp);
    fread(&data[0], 1, data.size(), fp),
    fclose(fp);

    for(unsigned a=0; a<500; ++a)
    {
        unsigned gmnumber  = data[a*6+0];
        unsigned gmnumber2 = data[a*6+1];
        unsigned offset    = *(unsigned*)&data[a*6+2];

        if(gmnumber == 0xFF) break;
        int gmno = gmnumber2==0x7F ? gmnumber+0x80 : gmnumber;
        unsigned length = data[offset] + data[offset+1]*256;
        signed char notenum = data[offset+2];

        printf("%02X %02X ", gmnumber,gmnumber2); //, offset);
        for(unsigned b=0; b<length; ++b)
        {
            if(b > 3 && (b-3)%11 == 0) printf("\n                        ");
            printf("%02X ", data[offset+b]);
        }
        printf("\n");
    }
}

int main(int argc, const char* const* argv)
{
    LoadMiles(argv[1]);
}
