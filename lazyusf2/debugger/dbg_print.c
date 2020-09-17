#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbg_decoder.h"

int main(int argc, char ** argv)
{
    if (argc >= 3)
    {
        char * end;
        unsigned int start_address = (unsigned int) strtoul(argv[1], &end, 0);
        int i;
        
        for (i = 2; i < argc; ++i)
        {
            char opcode[64];
            char argument[64];
            memset(opcode, 0, sizeof(opcode));
            memset(argument, 0, sizeof(argument));
            
            unsigned int opcode_val = (unsigned int) strtoul(argv[i], &end, 0);
            
            r4300_decode_op( opcode_val, opcode, argument, start_address );
            
            printf("%08x: %-16s %s\n", start_address, opcode, argument);
            start_address += 4;
        }
    }
    
    return 0;
}
