/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - ops.h                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef M64P_R4300_OPS_H
#define M64P_R4300_OPS_H

#include "osal/preproc.h"

typedef struct _cpu_instruction_table
{
	/* All jump/branch instructions (except JR and JALR) have three versions:
	 * - JUMPNAME() which for jumps inside the current block.
	 * - JUMPNAME_OUT() which jumps outside the current block.
	 * - JUMPNAME_IDLE() which does busy wait optimization.
	 *
	 * Busy wait optimization is used when a jump jumps to itself,
	 * and the instruction on the delay slot is a NOP.
	 * The program is waiting for the next interrupt, so we can just
	 * increase Count until the point where the next interrupt happens. */

	// Load and store instructions
	void (*LB)(usf_state_t *);
	void (*LBU)(usf_state_t *);
	void (*LH)(usf_state_t *);
	void (*LHU)(usf_state_t *);
	void (*LW)(usf_state_t *);
	void (*LWL)(usf_state_t *);
	void (*LWR)(usf_state_t *);
	void (*SB)(usf_state_t *);
	void (*SH)(usf_state_t *);
	void (*SW)(usf_state_t *);
	void (*SWL)(usf_state_t *);
	void (*SWR)(usf_state_t *);

	void (*LD)(usf_state_t *);
	void (*LDL)(usf_state_t *);
	void (*LDR)(usf_state_t *);
	void (*LL)(usf_state_t *);
	void (*LWU)(usf_state_t *);
	void (*SC)(usf_state_t *);
	void (*SD)(usf_state_t *);
	void (*SDL)(usf_state_t *);
	void (*SDR)(usf_state_t *);
	void (*SYNC)(usf_state_t *);

	// Arithmetic instructions (ALU immediate)
	void (*ADDI)(usf_state_t *);
	void (*ADDIU)(usf_state_t *);
	void (*SLTI)(usf_state_t *);
	void (*SLTIU)(usf_state_t *);
	void (*ANDI)(usf_state_t *);
	void (*ORI)(usf_state_t *);
	void (*XORI)(usf_state_t *);
	void (*LUI)(usf_state_t *);

	void (*DADDI)(usf_state_t *);
	void (*DADDIU)(usf_state_t *);

	// Arithmetic instructions (3-operand)
	void (*ADD)(usf_state_t *);
	void (*ADDU)(usf_state_t *);
	void (*SUB)(usf_state_t *);
	void (*SUBU)(usf_state_t *);
	void (*SLT)(usf_state_t *);
	void (*SLTU)(usf_state_t *);
	void (*AND)(usf_state_t *);
	void (*OR)(usf_state_t *);
	void (*XOR)(usf_state_t *);
	void (*NOR)(usf_state_t *);

	void (*DADD)(usf_state_t *);
	void (*DADDU)(usf_state_t *);
	void (*DSUB)(usf_state_t *);
	void (*DSUBU)(usf_state_t *);

	// Multiply and divide instructions
	void (*MULT)(usf_state_t *);
	void (*MULTU)(usf_state_t *);
	void (*DIV)(usf_state_t *);
	void (*DIVU)(usf_state_t *);
	void (*MFHI)(usf_state_t *);
	void (*MTHI)(usf_state_t *);
	void (*MFLO)(usf_state_t *);
	void (*MTLO)(usf_state_t *);

	void (*DMULT)(usf_state_t *);
	void (*DMULTU)(usf_state_t *);
	void (*DDIV)(usf_state_t *);
	void (*DDIVU)(usf_state_t *);

	// Jump and branch instructions
	void (*J)(usf_state_t *);
	void (*J_OUT)(usf_state_t *);
	void (*J_IDLE)(usf_state_t *);
	void (*JAL)(usf_state_t *);
	void (*JAL_OUT)(usf_state_t *);
	void (*JAL_IDLE)(usf_state_t *);
	void (*JR)(usf_state_t *);
	void (*JALR)(usf_state_t *);
	void (*BEQ)(usf_state_t *);
	void (*BEQ_OUT)(usf_state_t *);
	void (*BEQ_IDLE)(usf_state_t *);
	void (*BNE)(usf_state_t *);
	void (*BNE_OUT)(usf_state_t *);
	void (*BNE_IDLE)(usf_state_t *);
	void (*BLEZ)(usf_state_t *);
	void (*BLEZ_OUT)(usf_state_t *);
	void (*BLEZ_IDLE)(usf_state_t *);
	void (*BGTZ)(usf_state_t *);
	void (*BGTZ_OUT)(usf_state_t *);
	void (*BGTZ_IDLE)(usf_state_t *);
	void (*BLTZ)(usf_state_t *);
	void (*BLTZ_OUT)(usf_state_t *);
	void (*BLTZ_IDLE)(usf_state_t *);
	void (*BGEZ)(usf_state_t *);
	void (*BGEZ_OUT)(usf_state_t *);
	void (*BGEZ_IDLE)(usf_state_t *);
	void (*BLTZAL)(usf_state_t *);
	void (*BLTZAL_OUT)(usf_state_t *);
	void (*BLTZAL_IDLE)(usf_state_t *);
	void (*BGEZAL)(usf_state_t *);
	void (*BGEZAL_OUT)(usf_state_t *);
	void (*BGEZAL_IDLE)(usf_state_t *);

	void (*BEQL)(usf_state_t *);
	void (*BEQL_OUT)(usf_state_t *);
	void (*BEQL_IDLE)(usf_state_t *);
	void (*BNEL)(usf_state_t *);
	void (*BNEL_OUT)(usf_state_t *);
	void (*BNEL_IDLE)(usf_state_t *);
	void (*BLEZL)(usf_state_t *);
	void (*BLEZL_OUT)(usf_state_t *);
	void (*BLEZL_IDLE)(usf_state_t *);
	void (*BGTZL)(usf_state_t *);
	void (*BGTZL_OUT)(usf_state_t *);
	void (*BGTZL_IDLE)(usf_state_t *);
	void (*BLTZL)(usf_state_t *);
	void (*BLTZL_OUT)(usf_state_t *);
	void (*BLTZL_IDLE)(usf_state_t *);
	void (*BGEZL)(usf_state_t *);
	void (*BGEZL_OUT)(usf_state_t *);
	void (*BGEZL_IDLE)(usf_state_t *);
	void (*BLTZALL)(usf_state_t *);
	void (*BLTZALL_OUT)(usf_state_t *);
	void (*BLTZALL_IDLE)(usf_state_t *);
	void (*BGEZALL)(usf_state_t *);
	void (*BGEZALL_OUT)(usf_state_t *);
	void (*BGEZALL_IDLE)(usf_state_t *);
	void (*BC1TL)(usf_state_t *);
	void (*BC1TL_OUT)(usf_state_t *);
	void (*BC1TL_IDLE)(usf_state_t *);
	void (*BC1FL)(usf_state_t *);
	void (*BC1FL_OUT)(usf_state_t *);
	void (*BC1FL_IDLE)(usf_state_t *);

	// Shift instructions
	void (*SLL)(usf_state_t *);
	void (*SRL)(usf_state_t *);
	void (*SRA)(usf_state_t *);
	void (*SLLV)(usf_state_t *);
	void (*SRLV)(usf_state_t *);
	void (*SRAV)(usf_state_t *);

	void (*DSLL)(usf_state_t *);
	void (*DSRL)(usf_state_t *);
	void (*DSRA)(usf_state_t *);
	void (*DSLLV)(usf_state_t *);
	void (*DSRLV)(usf_state_t *);
	void (*DSRAV)(usf_state_t *);
	void (*DSLL32)(usf_state_t *);
	void (*DSRL32)(usf_state_t *);
	void (*DSRA32)(usf_state_t *);

	// COP0 instructions
	void (*MTC0)(usf_state_t *);
	void (*MFC0)(usf_state_t *);

	void (*TLBR)(usf_state_t *);
	void (*TLBWI)(usf_state_t *);
	void (*TLBWR)(usf_state_t *);
	void (*TLBP)(usf_state_t *);
	void (*CACHE)(usf_state_t *);
	void (*ERET)(usf_state_t *);

	// COP1 instructions
	void (*LWC1)(usf_state_t *);
	void (*SWC1)(usf_state_t *);
	void (*MTC1)(usf_state_t *);
	void (*MFC1)(usf_state_t *);
	void (*CTC1)(usf_state_t *);
	void (*CFC1)(usf_state_t *);
	void (*BC1T)(usf_state_t *);
	void (*BC1T_OUT)(usf_state_t *);
	void (*BC1T_IDLE)(usf_state_t *);
	void (*BC1F)(usf_state_t *);
	void (*BC1F_OUT)(usf_state_t *);
	void (*BC1F_IDLE)(usf_state_t *);

	void (*DMFC1)(usf_state_t *);
	void (*DMTC1)(usf_state_t *);
	void (*LDC1)(usf_state_t *);
	void (*SDC1)(usf_state_t *);

	void (*CVT_S_D)(usf_state_t *);
	void (*CVT_S_W)(usf_state_t *);
	void (*CVT_S_L)(usf_state_t *);
	void (*CVT_D_S)(usf_state_t *);
	void (*CVT_D_W)(usf_state_t *);
	void (*CVT_D_L)(usf_state_t *);
	void (*CVT_W_S)(usf_state_t *);
	void (*CVT_W_D)(usf_state_t *);
	void (*CVT_L_S)(usf_state_t *);
	void (*CVT_L_D)(usf_state_t *);

	void (*ROUND_W_S)(usf_state_t *);
	void (*ROUND_W_D)(usf_state_t *);
	void (*ROUND_L_S)(usf_state_t *);
	void (*ROUND_L_D)(usf_state_t *);

	void (*TRUNC_W_S)(usf_state_t *);
	void (*TRUNC_W_D)(usf_state_t *);
	void (*TRUNC_L_S)(usf_state_t *);
	void (*TRUNC_L_D)(usf_state_t *);

	void (*CEIL_W_S)(usf_state_t *);
	void (*CEIL_W_D)(usf_state_t *);
	void (*CEIL_L_S)(usf_state_t *);
	void (*CEIL_L_D)(usf_state_t *);

	void (*FLOOR_W_S)(usf_state_t *);
	void (*FLOOR_W_D)(usf_state_t *);
	void (*FLOOR_L_S)(usf_state_t *);
	void (*FLOOR_L_D)(usf_state_t *);

	void (*ADD_S)(usf_state_t *);
	void (*ADD_D)(usf_state_t *);

	void (*SUB_S)(usf_state_t *);
	void (*SUB_D)(usf_state_t *);

	void (*MUL_S)(usf_state_t *);
	void (*MUL_D)(usf_state_t *);

	void (*DIV_S)(usf_state_t *);
	void (*DIV_D)(usf_state_t *);
	
	void (*ABS_S)(usf_state_t *);
	void (*ABS_D)(usf_state_t *);

	void (*MOV_S)(usf_state_t *);
	void (*MOV_D)(usf_state_t *);

	void (*NEG_S)(usf_state_t *);
	void (*NEG_D)(usf_state_t *);

	void (*SQRT_S)(usf_state_t *);
	void (*SQRT_D)(usf_state_t *);

	void (*C_F_S)(usf_state_t *);
	void (*C_F_D)(usf_state_t *);
	void (*C_UN_S)(usf_state_t *);
	void (*C_UN_D)(usf_state_t *);
	void (*C_EQ_S)(usf_state_t *);
	void (*C_EQ_D)(usf_state_t *);
	void (*C_UEQ_S)(usf_state_t *);
	void (*C_UEQ_D)(usf_state_t *);
	void (*C_OLT_S)(usf_state_t *);
	void (*C_OLT_D)(usf_state_t *);
	void (*C_ULT_S)(usf_state_t *);
	void (*C_ULT_D)(usf_state_t *);
	void (*C_OLE_S)(usf_state_t *);
	void (*C_OLE_D)(usf_state_t *);
	void (*C_ULE_S)(usf_state_t *);
	void (*C_ULE_D)(usf_state_t *);
	void (*C_SF_S)(usf_state_t *);
	void (*C_SF_D)(usf_state_t *);
	void (*C_NGLE_S)(usf_state_t *);
	void (*C_NGLE_D)(usf_state_t *);
	void (*C_SEQ_S)(usf_state_t *);
	void (*C_SEQ_D)(usf_state_t *);
	void (*C_NGL_S)(usf_state_t *);
	void (*C_NGL_D)(usf_state_t *);
	void (*C_LT_S)(usf_state_t *);
	void (*C_LT_D)(usf_state_t *);
	void (*C_NGE_S)(usf_state_t *);
	void (*C_NGE_D)(usf_state_t *);
	void (*C_LE_S)(usf_state_t *);
	void (*C_LE_D)(usf_state_t *);
	void (*C_NGT_S)(usf_state_t *);
	void (*C_NGT_D)(usf_state_t *);

	// Special instructions
	void (*SYSCALL)(usf_state_t *);
	void (*BREAK)(usf_state_t *);

	// Exception instructions
	void (*TEQ)(usf_state_t *);

	// Emulator helper functions
	void (*NOP)(usf_state_t *);          // No operation (used to nullify R0 writes)
	void (*RESERVED)(usf_state_t *);     // Reserved instruction handler
	void (*NI)(usf_state_t *);	        // Not implemented instruction handler

	void (*FIN_BLOCK)(usf_state_t *);    // Handler for the end of a block
	void (*NOTCOMPILED)(usf_state_t *);  // Handler for not yet compiled code
	void (*NOTCOMPILED2)(usf_state_t *); // TODOXXX
} cpu_instruction_table;

#endif /* M64P_R4300_OPS_H_*/
