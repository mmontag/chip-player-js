#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "matrix.h"

int main(void)
{
    uint32_t i;
    uint32_t buffer[0x1000 / 4];
    FILE * f = fopen("out.pj", "rb");
    fseek(f, 0x40175C, SEEK_SET);
    fread(buffer, 1, 0x1000, f);
    fclose(f);
    
    for (i = 0; i < 0x1000/4; ++i)
    {
        char opcode[32];
        memset(opcode, 0, sizeof(opcode));
        disassemble(opcode,buffer[i]);
        printf("%03x: %08x %s\n", i * 4, buffer[i], opcode);
    }
    
    return 0;
}