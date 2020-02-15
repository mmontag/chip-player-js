#include <stdio.h>
#include <stdlib.h>

#include "stdtype.h"
#include "emu/EmuStructs.h"
#include "emu/SoundEmu.h"
#include "emu/SoundDevs.h"
#include "emu/EmuCores.h"
#include "emu/cores/sn764intf.h"

int main(int argc, char* argv[])
{
	DEV_GEN_CFG devCfg;
	SN76496_CFG snCfg;
	DEV_INFO snDefInf;
	DEVFUNC_WRITE_A8D8 snWrite;
	UINT8 retVal;
	UINT8 curReg;
	
	devCfg.emuCore = 0;	// default core
	devCfg.srMode = DEVRI_SRMODE_NATIVE;
	devCfg.flags = 0x00;
	devCfg.clock = 3579545;
	devCfg.smplRate = 44100;
	snCfg._genCfg = devCfg;
	snCfg.shiftRegWidth = 0x10;
	snCfg.noiseTaps = 0x09;
	snCfg.negate = 1;
	snCfg.stereo = 1;
	snCfg.clkDiv = 8;
	snCfg.segaPSG = 0;
	snCfg.t6w28_tone = NULL;
	
	retVal = SndEmu_Start(DEVID_SN76496, (DEV_GEN_CFG*)&snCfg, &snDefInf);
	if (retVal)
		return 1;
	
	snDefInf.devDef->Reset(snDefInf.dataPtr);
	SndEmu_GetDeviceFunc(snDefInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&snWrite);
	snWrite(snDefInf.dataPtr, 1, 0xDE);
	for (curReg = 0x8; curReg < 0xE; curReg += 0x02)
	{
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x0F);
		snWrite(snDefInf.dataPtr, 0, 0x3F);
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x10);
	}
	snWrite(snDefInf.dataPtr, 0, 0xE0 | 0x00);
	snWrite(snDefInf.dataPtr, 0, 0xF0 | 0x00);
	
	{
		UINT32 smplCount;
		DEV_SMPL* smplData[2];
		FILE* hFile;
		UINT32 curSmpl;
		
		smplCount = snDefInf.sampleRate;
		smplData[0] = (DEV_SMPL*)malloc(smplCount * sizeof(DEV_SMPL));
		smplData[1] = (DEV_SMPL*)malloc(smplCount * sizeof(DEV_SMPL));
		
		snDefInf.devDef->Update(snDefInf.dataPtr, smplCount, smplData);
		hFile = fopen("out.raw", "wb");
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
		{
			fwrite(&smplData[0][curSmpl], 0x02, 0x01, hFile);
			fwrite(&smplData[1][curSmpl], 0x02, 0x01, hFile);
		}
		fclose(hFile);
		
		free(smplData[0]);	smplData[0] = NULL;
		free(smplData[1]);	smplData[1] = NULL;
	}
	SndEmu_Stop(&snDefInf);
	
	return 0;
}
