#ifndef __OOTAKE_PSG_PRIVATE_H__
#define __OOTAKE_PSG_PRIVATE_H__

/*-----------------------------------------------------------------------------
	[PSG.h]
		ＰＳＧを記述するのに必要な定義および関数のプロトタイプ宣言を行ないます．

	Copyright (C) 2004 Ki

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**---------------------------------------------------------------------------*/
#include "../../stdtype.h"
#include "../snddef.h"

static UINT8 device_start_c6280_ootake(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);

/*-----------------------------------------------------------------------------
** 関数のプロトタイプ宣言を行ないます．
**---------------------------------------------------------------------------*/
static void*
OPSG_Init(
	UINT32		clock,
	UINT32		sampleRate);

static void
OPSG_Deinit(void* chip);

static void
OPSG_Mix(
	void*		chip,
	UINT32		nSample,			// 書き出すサンプル数
	DEV_SMPL**	pDst);				// 出力先バッファ

static UINT8
OPSG_Read(void* chip, UINT8 regNum);

static void
OPSG_Write(
	void*		chip,
	UINT8		regNum,
	UINT8		data);

//Kitao追加
static void
OPSG_ResetVolumeReg(void* chip);

//Kitao追加
static void
OPSG_SetMutePsgChannel(
	void*	chip,
	UINT8	num,
	UINT8	bMute);

static void OPSG_SetMuteMask(void* chip, UINT32 MuteMask);

//Kitao追加
static UINT8
OPSG_GetMutePsgChannel(
	void*	chip,
	UINT8	num);

//Kitao追加。v2.60
static void
OPSG_SetHoneyInTheSky(
	void*	chip,
	UINT8	bHoneyInTheSky);

#endif	// __OOTAKE_PSG_PRIVATE_H__
