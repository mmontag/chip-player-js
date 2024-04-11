/*********
 NLG.h by BouKiCHi 2013-2014

 this file is no warranty,but free to use. 
 ********/

#ifndef __NLG_H__
#define __NLG_H__

#include <stdio.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

typedef struct {
    FILE *file;
    char title[80];
    int  baseclk;
    int  tick;
    int  length;
    int  loop_ptr;
    int  version;
    
    int  ctc0;
    int  ctc3;
    int  irq_count;
    
    int  tick_count;
    
    int mode;
} NLGCTX;

NLGCTX *OpenNLG(const char *file);
void CloseNLG(NLGCTX *ctx);

int  ReadNLG(NLGCTX *ctx);
long  TellNLG(NLGCTX *ctx);
void SeekNLG(NLGCTX *ctx, long pos);


#ifndef NLG_READONLY

void WriteHeaderNLG(NLGCTX *ctx);
NLGCTX *CreateNLG(const char *file);
void WriteNLG_IRQ(NLGCTX *ctx);
void WriteNLG_CMD(NLGCTX *ctx, int cmd);
void WriteNLG_CTC(NLGCTX *ctx, int cmd,int ctc);
void WriteNLG_Data(NLGCTX *ctx,int cmd,int addr,int data);

void SetTitleNLG(NLGCTX *ctx, const char *title);
void SetBaseClkNLG(NLGCTX *ctx, int clock);

#endif

char *GetTitleNLG(NLGCTX *ctx);
int GetTickNLG(NLGCTX *ctx);
int GetLengthNLG(NLGCTX *ctx);
int GetLoopPtrNLG(NLGCTX *ctx);
int  GetBaseClkNLG(NLGCTX *ctx);

void SetCTC0_NLG(NLGCTX *ctx, int value);
void SetCTC3_NLG(NLGCTX *ctx, int value);


#define NLG_READ  0x00
#define NLG_WRITE 0x01

#define CMD_PSG  0x00
#define CMD_OPM  0x01
#define CMD_OPM2 0x02
#define CMD_IRQ  0x80

#define CMD_CTC0 0x81
#define CMD_CTC3 0x82


#endif
