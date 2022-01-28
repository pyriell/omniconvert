/*
 * translate.c -- Translate/convert codes from one device to another.
 *
 * Copyright (C) 2008 Pyriel
 * All rights reserved.
 *
 * This file is part of Omniconvert.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * I never much cared for MAXConvert's conversion routines.  I wrote them
 * hastily as a lark and wound up married to them after that.  This file
 * completely replaces rawconvert.c and better handles conversions.
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "abbtypes.h"
#include "translate.h"

//Command types for devices using the "standard" code format.
//A couple of special case types are mixed in.
enum {
	STD_WRITE_BYTE,				//Command 0, 8-bit write
	STD_WRITE_HALF,				//Command 1, 16-bit write
	STD_WRITE_WORD,				//Command 2, 32-bit write
	STD_INCREMENT_WRITE,			//Command 3, variable-size read-increment-write
	STD_WRITE_WORD_MULTI,			//Command 4, 32-bit, multi-address write
	STD_COPY_BYTES,				//Command 5, read bytes from source, write to dest (copy bytes)
	STD_POINTER_WRITE,			//Command 6, variable-size pointer based write
	STD_BITWISE_OPERATE,			//Command 7, read-bitwise operate-write (CB7+ Only)
	GS5_VERIFIER = STD_BITWISE_OPERATE,	//On GS5+, the seven command identifies verifier data lines and disc checksums
	STD_ML_TEST,				//Command 8, master level test.  Comparison performed during enable code processing only.
	AR2_COND_HOOK = STD_ML_TEST,		//The AR2 uses command 8 as an enable hook command.  Similar to 9 on other devices.
	STD_COND_HOOK,				//Command 9, conditional hook
	STD_ML_WRITE_WORD,			//Command A, master level write.  Write word only during enable processing. (write once)
	STD_TIMER,				//Command B, execute X times before doing following codes
	STD_TEST_ALL,				//Command C, 32-bit execute all remaining codes when equal
	STD_TEST_SINGLE,			//Command D, variable-size, execute next code on true
	STD_TEST_MULTI,				//Command E, variable-size, execute next n code on true
	STD_UNCOND_HOOK				//Command F, unconditional hook
};

//Write/Test size definitions.
enum {
	SIZE_BYTE,
	SIZE_HALF,
	SIZE_WORD,
	SIZE_SPECIAL
};

//Action Replay MAX code types
enum {
	ARM_WRITE,				//With the AR MAX we can consider any command that results in the modification of RAM a "write"
	ARM_EQUAL,				//Equality tests with various skips
	ARM_NOT_EQUAL,				//Inequality tests with various skips
	ARM_LESS_SIGNED,			//Signed less than tests with various skips
	ARM_GREATER_SIGNED,			//Signed greater than tests with various skips
	ARM_LESS_UNSIGNED,	
	ARM_GREATER_UNSIGNED,
	ARM_AND,				//And mask tests.
};

//Action Replay MAX code sub-types (writes)
enum {
	ARM_DIRECT,				//Write to address in code.
	ARM_POINTER,				//Load pointer from address in code, add offset and write to resultant address.
	ARM_INCREMENT,				//Load data from address in code, increment by n, write back.
	ARM_HOOK				//Use address and data in code to establish branch to cheat engine.
};

//Action Replay MAX code sub-types (tests)
enum {
	ARM_SKIP_1,				//Only execute next line if condition is true.
	ARM_SKIP_2,				//Only execute next two lines if condition is true.
	ARM_SKIP_N,				//Only execute next n lines if condition is true (n is indicated by line 00000000 40000000.
	ARM_SKIP_ALL_INV			//Execute next n lines unless condition is true.
};

#define ARM_CMD_SPECIAL		0
#define ARM_CMD_RESUME 		0x40000000
#define ARM_MAX_INCREMENT	127
#define ARM_ENABLE_STD		0x0003FF00
#define ARM_ENABLE_INT		0x0002FF01
//this probably shouldn't be used
#define ARM_ENABLE_THREAD	0x0001FF01

#define MASK_ADDR 0x01FFFFFF
#define MASK_LOHALF 0x0000FFFF
#define MASK_HIHALF 0xFFFF0000
#define MASK_LOBYTE32 0x000000FF
#define MASK_HIBYTE32 0xFF000000
#define ADDR(a) (a & MASK_ADDR)
#define LOHALF(a) (a & MASK_LOHALF)
#define HIHALF(a) ((a >> 16) & MASK_LOHALF)
#define LOBYTE32(a) (a & MASK_LOBYTE32)
#define HIBYTE32(a) ((a >> 24) & MASK_LOBYTE32)
#define GET_STD_CMD(a) ((a >> 28) & 0xF)
#define GET_ARM_CMD(a) ((a >> 24) & 0xFE)
#define GET_ARM_TYPE(a) ((a >> 27) & 7)
#define GET_ARM_SUBTYPE(a) ((a >> 30) & 3)
#define GET_ARM_SIZE(a) ((a >> 25) & 3)
#define MAKE_ARM_CMD(type, subtype, size) ((subtype << 30) | (type << 27) | (size << 25))
#define MAKE_STD_CMD(cmd) (cmd << 28 | 0)
#define MAKEWORD32(val, size) (size == SIZE_BYTE ? (val << 24) | (val << 16) | (val << 8) | val : (val << 16) | val)
#define MAKEHALF(val, size) (size == SIZE_BYTE ? (val << 8) | val : val)
static u8 ARSize[7] = {SIZE_BYTE,SIZE_BYTE,SIZE_BYTE,SIZE_HALF,SIZE_HALF,SIZE_WORD,SIZE_WORD};
static u8 STDSize[7] = {SIZE_BYTE,SIZE_BYTE,SIZE_HALF,SIZE_HALF,SIZE_WORD,SIZE_WORD,SIZE_WORD};
static u8 STDCompToArm[6] = {ARM_EQUAL, ARM_NOT_EQUAL, ARM_LESS_UNSIGNED, ARM_GREATER_UNSIGNED, ARM_AND, ARM_AND};
static u32 valmask[3] = {0xFF, 0xFFFF, 0xFFFFFFFF};

#define NUM_ERROR_TEXT 15

enum {
	ERR_NONE,
	ERR_VALUE_INCREMENT,
	ERR_COPYBYTES,
	ERR_EXCESS_OFFSETS,
	ERR_OFFSET_VALUE_32,
	ERR_OFFSET_VALUE_16,
	ERR_OFFSET_VALUE_8,
	ERR_BITWISE,
	ERR_TIMER,
	ERR_ALL_OFF,
	ERR_TEST_TYPE,
	ERR_SINGLE_LINE_FILL,
	ERR_PTR_SIZE,
	ERR_TEST_TYPE_2,
	ERR_TEST_SIZE,
	ERR_MA_WRITE_SIZE,
	ERR_INVALID_CODE = 0xFFFF
};

char *transErrorText[NUM_ERROR_TEXT] = {
	"Code contains a value increment\nlarger than allowed by target device.",	//1
	"Target device does not support\nthe copy bytes command.",			//2
	"Target device does not support\npointer writes with more than one offset.",	//3
	"Target device does not allow\n32-bit pointer writes with offsets.",		//4
	"Target device does not allow\noffsets > 0xFFFF on 16-bit pointers.",		//5
	"Target device does not allow\noffsets > 0xFFFFFF on 8-bit pointers.",		//6
	"Target device does not support\nbitwise operations (type 7).",			//7
	"Target device does not support\ntimer codes.",					//8
	"Target device does not support\nthe all-off test command.",			//9
	"Target device does not support\nthe comparison specified by test command.",	//10
	"Single line fill codes not yet\nhandled by Omniconvert.", 			//11
	"Target device does not support\nsize indicated in pointer write.",		//12
	"Target device does not support\nthe comparison specified by test command.",	//13
	"Target device does not support\nthe size specified by test command.",		//14
	"Target device does not support\nthe size specified by multi-address write."	//15
};

char *transErrorOctets = "Code contains an invalid\nnumber of 8-digit values,\nor general crypt error.  Check your input.";

u8 err_suppress = 0;

void transSetErrorSuppress(u8 val) {
	err_suppress = val;
}

void transToggleErrorSuppress() {
	err_suppress ^= 1;
}

char *transGetErrorText(int idx) {
	char **tmp = transErrorText;
	idx--;
	if(idx >= NUM_ERROR_TEXT || idx < 0) return transErrorOctets;
	return *(tmp+idx);
}

int transOther(cheat_t *dest, cheat_t *src, int *idx) {
    u8 cmd = src->code[*idx] >> 28;
    u8 size;
    u32 addr, val;

    switch(cmd) {
        case STD_WRITE_BYTE:
        case STD_WRITE_HALF:
        case STD_WRITE_WORD:
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_INCREMENT_WRITE:
            addr = src->code[*idx];
            if(g_indevice == DEV_AR2 || g_indevice == DEV_AR1) {
                if(g_outdevice == DEV_CB || g_outdevice == DEV_GS3) addr -= 0x10000;  //CodeBreaker/GS use zero-based width indicators
                size = ((addr >> 20) & 7) - 1;
            }
            else {
                size = (addr >> 20) & 7;
                if(g_outdevice == DEV_AR2 || g_outdevice == DEV_AR1) addr += 0x10000; //AR1/AR2 use one-based width indicators
            }
            cheatAppendOctet(dest, addr);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            if(size > 3) {
                if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
                cheatAppendOctet(dest, src->code[*idx + 2]);
                cheatAppendOctet(dest, src->code[*idx + 3]);
                *idx+=2;
            }
            *idx+=2;
            break;
        case STD_WRITE_WORD_MULTI:
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
            if((g_outdevice == DEV_AR2 || g_outdevice == DEV_AR1) && src->code[*idx + 3] > 0) return ERR_VALUE_INCREMENT;
            cheatAppendOctet(dest, src->code[*idx + 2]);
            cheatAppendOctet(dest, src->code[*idx + 3]);
            *idx+=4;
            break;
        case STD_COPY_BYTES:
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
            cheatAppendOctet(dest, src->code[*idx + 2]);
            cheatAppendOctet(dest, src->code[*idx + 3]);
            *idx+=4;
            break;
        case STD_POINTER_WRITE:
            if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
            /* Attempt to translate pointer writes.
            so far as I know, GS3 stole (and subsequently wrecked) their format from the old AR
            The second nibble is a size parameter, and the second word is a "load offset" that is added
            to the address in the code (to accomodate addresses > 0xFFFFFF).  However, on the GS3, this infringes
            upon their use of 3 uppermost bits of that nibble as a decryption key*/
            if(g_outdevice == DEV_STD || g_indevice == DEV_STD || (g_outdevice != DEV_CB && g_indevice != DEV_CB)) {
                cheatAppendOctet(dest, src->code[*idx]);
                cheatAppendOctet(dest, src->code[*idx + 1]);
                cheatAppendOctet(dest, src->code[*idx + 2]);
                cheatAppendOctet(dest, src->code[*idx + 3]);
                *idx += 4;
                return 0;
            }
            if(g_indevice == DEV_CB) {
                if(LOHALF(src->code[*idx + 2]) > 1) return ERR_EXCESS_OFFSETS;
                addr = ADDR(src->code[*idx]);
                u32 offset = addr & 0x1000000;
                size = HIHALF(src->code[*idx + 2]);
                if(size == 0) return ERR_PTR_SIZE;
                cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | (size << 24) | (addr & 0xFFFFFF));
                cheatAppendOctet(dest, offset);
                cheatAppendOctet(dest, src->code[*idx + 3]);
                cheatAppendOctet(dest, src->code[*idx + 1]);
            } else {
                size = (src->code[*idx] >> 24) & 0x3;
                if(size == 0 || size == 3) size = 1;
                cheatAppendOctet(dest, (src->code[*idx] & 0xF0FFFFFF) + src->code[*idx + 1]); //build the proper address
                cheatAppendOctet(dest, src->code[*idx + 3]);				   //move value to third word
                cheatAppendOctet(dest, 1 | (size << 16));				   //ensure count is one and or in the size
                cheatAppendOctet(dest, src->code[*idx + 2]);				   //final word is offset
            }
            *idx+=4;
            break;
        case STD_BITWISE_OPERATE:		//GS5_VERIFIER | Neither GS nor AR do the bitwise ops, and GS5 uses them as verifiers.
            if(g_indevice == DEV_CB || g_indevice == DEV_STD) {
                if(g_outdevice == DEV_CB || g_outdevice == DEV_STD) {
                    cheatAppendOctet(dest, src->code[*idx]);
                    cheatAppendOctet(dest, src->code[*idx + 1]);
                } else {
                    return ERR_BITWISE;
                }
            }
            *idx+=2;
            break;
        case STD_ML_TEST:			//Also AR2_COND_HOOK
            if(g_indevice == DEV_AR2 || g_indevice == DEV_AR1) {
                if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
                if(g_outdevice == DEV_STD || g_outdevice == DEV_AR1 || g_outdevice == DEV_AR2) {
                    cheatAppendOctet(dest, src->code[*idx]);
                    cheatAppendOctet(dest, src->code[*idx + 1]);
                    cheatAppendOctet(dest, src->code[*idx + 2]);
                    cheatAppendOctet(dest, src->code[*idx + 3]);
                    *idx+=4;
                } else {
                    cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(src->code[*idx + 1]));
                    cheatAppendOctet(dest, src->code[*idx + 2]);
                    *idx+=4;
                }
            } else if(g_indevice == DEV_CB || g_indevice == DEV_GS3) {	//STD_ML_TEST
                if(g_outdevice == DEV_STD) {
                    cheatAppendOctet(dest, src->code[*idx]);
                    cheatAppendOctet(dest, src->code[*idx + 1]);
                    *idx+=2;
                } else {
                    return ERR_TEST_TYPE;
                }
            } else { //if (g_indevice == DEV_STD)
                if(*idx + 2 >= src->codecnt) return ERR_INVALID_CODE;
                if((src->code[*idx + 2] >> 28) == 0) {		//this is either an AR hook code, or an error
                    // TODO: [oddity] Should the following actually be checking g_outdevice & g_indevice?
                    if(g_outdevice == DEV_AR2 || g_outdevice == DEV_AR2) {
                        cheatAppendOctet(dest, src->code[*idx]);
                        cheatAppendOctet(dest, src->code[*idx + 1]);
                        cheatAppendOctet(dest, src->code[*idx + 2]);
                        cheatAppendOctet(dest, src->code[*idx + 3]);
                        *idx+=4;
                    } else {
                        cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(src->code[*idx + 1]));
                        cheatAppendOctet(dest, src->code[*idx + 2]);
                        *idx+=4;
                    }
                } else {
                    // TODO: [oddity] Should the following actually be checking g_outdevice & g_indevice?
                    if(g_outdevice == DEV_AR2 || g_outdevice == DEV_AR2) {
                        return ERR_TEST_TYPE;
                    } else {
                        cheatAppendOctet(dest, src->code[*idx]);
                        cheatAppendOctet(dest, src->code[*idx + 1]);
                        *idx+=2;
                    }
                }
            }
            break;
        case STD_COND_HOOK:
            if(g_outdevice == DEV_AR1 || g_outdevice == DEV_AR2) {
                cheatAppendOctet(dest, MAKE_STD_CMD(AR2_COND_HOOK) | (ADDR(src->code[*idx]) + 1));
                cheatAppendOctet(dest, ADDR(src->code[*idx]));
                cheatAppendOctet(dest, src->code[*idx + 1]);
                cheatAppendOctet(dest, 0);
            } else {
                cheatAppendOctet(dest, src->code[*idx]);
                cheatAppendOctet(dest, src->code[*idx + 1]);
            }
            *idx+=2;
            break;
        case STD_ML_WRITE_WORD:
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_TIMER:					//I've seen this code never.  Just assume the value is a standard count.
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_TEST_ALL:
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_TEST_SINGLE:
            if(g_indevice == DEV_CB && (g_outdevice != DEV_STD) && (((src->code[*idx + 1] >> 16) & 0xF)  > 0)) return ERR_TEST_SIZE;
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_TEST_MULTI:
            if(g_indevice == DEV_CB && (g_outdevice != DEV_STD) && (((src->code[*idx] >> 24) & 0xF) > 0)) return ERR_TEST_SIZE;
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
        case STD_UNCOND_HOOK:				//difficult to make determinations here.  Assume it's correct.
            cheatAppendOctet(dest, src->code[*idx]);
            cheatAppendOctet(dest, src->code[*idx + 1]);
            *idx+=2;
            break;
    }
    return 0;
}

int transStdToMax(cheat_t *dest, cheat_t *src, int *idx) {
    int i, cnt, trans, tmp, ret;
    u8 command, size;
    u32 address, increment, offset, value, count, skip;
    u32 *code = src->code;

    cnt = trans = ret = 0;
    command = GET_STD_CMD(code[*idx]);
    switch(command) {
        case STD_WRITE_BYTE:
        case STD_WRITE_HALF:
        case STD_WRITE_WORD:
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_DIRECT, command) | ADDR(code[*idx]));
            cheatAppendOctet(dest, code[*idx + 1]);
            *idx+=2;
            break;
        case STD_INCREMENT_WRITE: //Also decrement
            tmp		= (code[*idx] >> 20) & 0x7;
            if(g_indevice == DEV_AR1 || g_indevice == DEV_AR2) {
                size = ARSize[tmp];
                tmp = (tmp & 1) ? 0 : 1;		//flag increment/decrement
            }
            else {
                size = STDSize[tmp];
                tmp = (tmp & 1) ? 1 : 0;		//flag increment/decrement
            }
            if(size == SIZE_WORD) {
                trans=4;
                if(*idx + 4 >= src->codecnt) {
                    ret = ERR_INVALID_CODE;		//too few octets
                    break;
                }
                value = code[*idx + 2];
            }
            else {
                value = LOHALF(code[*idx]);
                trans=2;
            }
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_INCREMENT, size) | ADDR(code[*idx + 1]));
            cheatAppendOctet(dest, (tmp) ? (~value) + 1 : value);
            *idx+=trans;
            break;
        case STD_WRITE_WORD_MULTI:
            if(*idx+4 > src->codecnt) {
                ret = ERR_INVALID_CODE;		//too few octets
                break;
            }
            increment 	= code[*idx + 3];
            if(increment > ARM_MAX_INCREMENT) {
                ret = ERR_VALUE_INCREMENT;	//unsupported value increment size
                break;
            }
            cheatAppendOctet(dest, ARM_CMD_SPECIAL);
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, 2, SIZE_WORD) | ADDR(code[*idx + 0]));
            cheatAppendOctet(dest, code[*idx + 2]);
            cheatAppendOctet(dest, ((increment & MASK_LOBYTE32) << 24) | (HIHALF(code[*idx + 1]) << 16) | LOHALF(code[*idx + 1]));
            *idx+=4;
            break;
        case STD_COPY_BYTES:
            *idx+=4;
            ret = ERR_COPYBYTES;					//ARM does not support copy bytes.
            break;
        case STD_POINTER_WRITE:
            if(*idx + 4 > src->codecnt) {
                ret = ERR_INVALID_CODE;		//too few octets
                break;
            }
            if(g_indevice == DEV_CB) {
                size 	= HIHALF(code[*idx + 2]) & 0x3;
                address = ADDR(code[*idx]);
                value 	= code[*idx + 1] & valmask[size];
                count 	= LOHALF(code[*idx + 2]);
                offset	= code[*idx + 3];
            } else {
                size 	= HIBYTE32(code[*idx]) & 0xF;
                if(size < 1 && g_indevice == DEV_GS3) size = 1;  //GS3 does not do 8-bit writes (or 32 really).
                address = code[*idx] & 0xFFFFFF + code[*idx + 1];
                offset 	= code[*idx + 2];
                value 	= code[*idx + 3] & valmask[size];
                count 	= 0;
            }
            *idx+=4;
            if(count > 1) {						//no support for multiple levels of indirection
                *idx+=(count>>1<<1);
                ret = ERR_EXCESS_OFFSETS;
                break;
            }
            if(size == SIZE_WORD && offset > 0) {			//32-bit write does not allow offset
                ret = ERR_OFFSET_VALUE_32;
                break;
            }
            if(size == SIZE_HALF && offset > 0xFFFF) {	//16-bit write has maximum offset 65535
                ret = ERR_OFFSET_VALUE_16;
                break;
            }
            if(size == SIZE_BYTE && offset > 0xFFFFFF) {		//8 -bit write has maximum offset 16,777,215
                ret = ERR_OFFSET_VALUE_8;
                break;
            }
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_POINTER, size) | address);
            cheatAppendOctet(dest, (offset << 16) | value);
            break;
        case STD_BITWISE_OPERATE: 					//CB Only!
            *idx+=2;
            ret = ERR_BITWISE;
            break;
        case STD_ML_TEST: {
            if(g_indevice == DEV_AR1 || g_indevice == DEV_AR2) {
                if(*idx + 4 > src->codecnt) {
                    return ERR_INVALID_CODE;
                }
                cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_WORD) | ADDR(code[*idx + 1]));
                cheatAppendOctet(dest, code[*idx + 2]);
                cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | ADDR(code[*idx + 1]));
                cheatAppendOctet(dest, ARM_ENABLE_STD);
                *idx+=4;
            } else {
                cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_HALF) | ADDR(code[*idx]));
                cheatAppendOctet(dest, LOHALF(code[*idx + 1]));
                int hold = dest->codecnt - 2;
                skip		= HIHALF(code[*idx + 1]) << 1;
                *idx+=2;
                skip += *idx;
                int lines = 0;
                while(*idx < src->codecnt && *idx < skip) {
                    ret = transStdToMax(dest, src, idx);
                    if(ret) break;
                    lines++;
                }
                if(!ret && lines > 1) {
                    if(lines == 2)
                        dest->code[hold] = MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_2, SIZE_HALF) | ADDR(dest->code[hold]);
                    else {
                        dest->code[hold] = MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_N, SIZE_HALF) | ADDR(dest->code[hold]);
                        cheatAppendOctet(dest, ARM_CMD_SPECIAL);
                        cheatAppendOctet(dest, ARM_CMD_RESUME);
                    }
                }
            }
            break;
        }
        case STD_COND_HOOK:
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_WORD) | ADDR(code[*idx]));
            cheatAppendOctet(dest, code[*idx + 1]);
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | ADDR(code[*idx]));
            cheatAppendOctet(dest, ARM_ENABLE_STD);
            *idx+=2;
            break;
        case STD_ML_WRITE_WORD:
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_DIRECT, SIZE_WORD) | ADDR(code[*idx]));
            cheatAppendOctet(dest, code[*idx + 1]);
            *idx+=2;
            break;
        case STD_TIMER:				//unsupported
            *idx+=2;
            ret = ERR_TIMER;
            break;
        case STD_TEST_ALL:			//unsupported
            *idx+=2;
            ret = ERR_ALL_OFF;
            break;
        case STD_TEST_SINGLE:
            tmp = (code[*idx + 1]>>20) & 0x7;
            if(tmp > 5 || tmp == 4) {
                ret = ERR_TEST_TYPE;
                break;
            }
            size = (g_indevice == DEV_CB) ? (HIHALF(code[*idx + 1]) & 0x1) ^ 1 : SIZE_HALF;
            cheatAppendOctet(dest, MAKE_ARM_CMD(STDCompToArm[tmp], ARM_SKIP_1, size) | ADDR(code[*idx]));
            cheatAppendOctet(dest, LOHALF(code[*idx + 1]));
            *idx+=2;
            break;
        case STD_TEST_MULTI:
            tmp = (code[*idx + 1]>>28) & 0x7;
            if(tmp > 5 || tmp == 4) {
                ret = ERR_TEST_TYPE;
                break;
            }
            size = (g_indevice == DEV_CB) ? (HIBYTE32(code[*idx]) & 0x1) ^ 1 : SIZE_HALF;
            skip = (g_indevice == DEV_CB) ? HIHALF(code[*idx]) & 0xFF : HIHALF(code[*idx]) & 0xFFF;
            count = (skip <= 2) ? skip-1 : ARM_SKIP_N;
            cheatAppendOctet(dest, MAKE_ARM_CMD(STDCompToArm[tmp], count, size) | ADDR(code[*idx+1]));
            cheatAppendOctet(dest, LOHALF(code[*idx]));
            *idx+=2;
            if(skip > 2) {
                skip = (skip << 1) + *idx;
                while(*idx < src->codecnt && *idx < skip) {
                    ret = transStdToMax(dest, src, idx);
                    if(ret) break;
                }
                if(!ret) {
                    cheatAppendOctet(dest, ARM_CMD_SPECIAL);
                    cheatAppendOctet(dest, ARM_CMD_RESUME);
                }
            }
            break;
        case STD_UNCOND_HOOK:
            address = ADDR(code[*idx]);
            value = ARM_ENABLE_STD;
            if(address == 0x100008 || address == 0x100000 || address == 0x200000 || address == 0x200008) {  //likely entry point values
                if(code[*idx + 1] == 0x1FD || code[*idx + 1] == 0xE)
                    value = ARM_ENABLE_INT;
                else
                    address = code[*idx + 1] & 0x1FFFFFC;
            }
            cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | address);
            cheatAppendOctet(dest, value);
            *idx+=2;
            break;
    }
    return ret;
}

void transSmash(cheat_t *dest, u8 size, u32 address, u32 value, u32 fillcount) {
	u32 bytes = fillcount << size, cnt = 0;

	if(fillcount == 1) {
		cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
		cheatAppendOctet(dest, value);
		return;
	}
	while(bytes) {
		if(address % 4 == 0) {
			if( bytes > 12) {
				cheatAppendOctet(dest, MAKE_STD_CMD(STD_WRITE_WORD_MULTI) | address);
				u32 count = (bytes - bytes % 4) / 4;
				cheatAppendOctet(dest, (count << 16) | 1);
				cheatAppendOctet(dest, MAKEWORD32(value, size));
				cheatAppendOctet(dest, 0);
				address += count * 4;
				bytes -= count * 4;
			}
			else if(bytes >= 4) {
				cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_WORD) | address);
				cheatAppendOctet(dest, MAKEWORD32(value, size));
				address += 4;
				bytes -= 4;
			}
			else if(bytes >= 2) {
				cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_HALF) | address);
				cheatAppendOctet(dest, MAKEHALF(value, size));
				address += 2;
				bytes -= 2;
			}
			else {
				cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
				cheatAppendOctet(dest, value);
				address += 1;
				bytes -= 1;
			}
		} else if(address % 2 == 0) {
			if(bytes >= 2) {
				cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_HALF) | address);
				cheatAppendOctet(dest, MAKEHALF(value, size));
				address += 2;
				bytes -= 2;
			}
			else {
				cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
				cheatAppendOctet(dest, value);
				address += 1;
				bytes -= 1;
			}
		} else {
			cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
			cheatAppendOctet(dest, value);
			address += 1;
			bytes -= 1;
		}
		
	}
}

void transExplode(cheat_t *dest, u8 size, u32 address, u32 value, u32 fillcount, u8 skip, u8 increment) {
	u32 bytes = fillcount << size, cnt = 0;

	if(skip == 1 && increment == 0) {
		transSmash(dest, size, address, value, fillcount);
		return;
	}
	if(fillcount == 1) {
		cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
		cheatAppendOctet(dest, value);
		return;
	}
	while(fillcount--) {
		cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
		cheatAppendOctet(dest, value);
		address += (1 << size) * skip;
		value   += increment;
	}
	return;
}

int transMaxToStd(cheat_t *dest, cheat_t *src, int *idx) {
	int i, cnt, trans, tmp, ret;
	u8 command, size, type, subtype, type2, subtype2;
	u32 address, increment, offset, value, count, skip;
	u32 *code = src->code;

	trans = cnt = ret = 0;
	if(code[*idx] == ARM_CMD_SPECIAL) {
		if(code[*idx + 1] == ARM_CMD_RESUME) {
			*idx+=2;
			return 0;
		}
		else if(HIBYTE32(code[*idx + 1]) >= 0x84 && HIBYTE32(code[*idx + 1]) <= 0x85) {
			if(*idx+3 >= src->codecnt) {
				return ERR_INVALID_CODE;		//too few octets
			}
			cheatAppendOctet(dest, MAKE_STD_CMD(STD_WRITE_WORD_MULTI) | ADDR(code[*idx + 1]));
			cheatAppendOctet(dest, (LOBYTE32(HIHALF(code[*idx + 3])) << 16) | LOHALF(code[*idx + 3]));
			cheatAppendOctet(dest, code[*idx + 2]);
			cheatAppendOctet(dest, HIBYTE32(code[*idx + 3]));
			*idx+=4;
			return 0;
		} else {
			if(*idx + 3 >= src->codecnt) return ERR_INVALID_CODE;
			if(HIBYTE32(code[*idx + 1]) >= 0x80 && HIBYTE32(code[*idx + 1]) <= 0x83) {
				transExplode(dest, GET_ARM_SIZE(code[*idx + 1]), ADDR(code[*idx + 1]), code[*idx + 2],
					LOBYTE32(HIHALF(code[*idx + 3])), LOHALF(code[*idx + 3]), HIBYTE32(code[*idx + 3]));
				*idx += 4;
				return 0;
			}
			*idx += 2;
			return ERR_MA_WRITE_SIZE;
		}
	}

	command	= GET_ARM_CMD(code[*idx]);
	type	= GET_ARM_TYPE(code[*idx]);
	subtype = GET_ARM_SUBTYPE(code[*idx]);
	size	= GET_ARM_SIZE(code[*idx]);

	if(type == ARM_WRITE) {
		switch(subtype) {
			case ARM_DIRECT:
				if(size == SIZE_BYTE) {
					count = code[*idx + 1] >> 8;
					value = LOBYTE32(code[*idx + 1]);
				} else if(size == SIZE_HALF) {
					count = HIHALF(code[*idx + 1]);
					value = LOHALF(code[*idx + 1]);
				} else {
					count = 0;
					value = code[*idx + 1];
				}
				if(count > 0) {
					transSmash(dest, size, ADDR(code[*idx]), value, count);
				}
				else {
					cheatAppendOctet(dest, MAKE_STD_CMD(size) | ADDR(code[*idx]));
					cheatAppendOctet(dest, value);
				}
				*idx+=2;
				break;
			case ARM_POINTER:
				if(size == SIZE_BYTE) {
					if(g_outdevice == DEV_GS3) {
						ret = ERR_PTR_SIZE;			//unsupported size;
						break;
					}
					offset	= code[*idx + 1] >> 8;
					value	= LOBYTE32(code[*idx + 1]);
				} else if(size == SIZE_HALF) {
					offset	= HIHALF(code[*idx + 1]);
					value	= LOHALF(code[*idx + 1]);
				} else {
					offset	= 0;
					value	= code[*idx + 1];
				}
				if(g_outdevice == DEV_CB) {
					cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | ADDR(code[*idx]));
					cheatAppendOctet(dest, value);
					cheatAppendOctet(dest, (size << 16) | 1); //count;
					cheatAppendOctet(dest, offset);
				} else {
					cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | (code[*idx] & 0xFFFFFF));
					cheatAppendOctet(dest, code[*idx] & 0x1000000);
					cheatAppendOctet(dest, offset);
					cheatAppendOctet(dest, value);
				}
				*idx+=2;
				break;
			case ARM_INCREMENT:
				tmp = (g_outdevice == DEV_AR1 || g_outdevice == DEV_AR2) ? (size << 1) + 1 : (size << 1);
				cheatAppendOctet(dest, (size < SIZE_WORD) ?
							MAKE_STD_CMD(STD_INCREMENT_WRITE) | (tmp << 20) | (code[*idx + 1] & valmask[size]) :
							MAKE_STD_CMD(STD_INCREMENT_WRITE) | (tmp << 20));
				cheatAppendOctet(dest, ADDR(code[*idx]));
				*idx+=2;
				if(size > SIZE_WORD) {
					cheatAppendOctet(dest, code[*idx + 1]);
					*idx+=2;
				}
				break;
			case ARM_HOOK:
				if(*idx > 0 && GET_ARM_TYPE(code[*idx-2]) == ARM_EQUAL && GET_ARM_SUBTYPE(code[*idx-2]) == ARM_SKIP_1
					&& ADDR(code[*idx - 2]) == ADDR(code[*idx])) {
					dest->code[dest->codecnt - 2] = MAKE_STD_CMD(STD_COND_HOOK) | ADDR(code[*idx]);
				}
				else {
					cheatAppendOctet(dest, MAKE_STD_CMD(STD_UNCOND_HOOK) | ADDR(code[*idx]));
					cheatAppendOctet(dest, ADDR(code[*idx]) + 3);
				}
				*idx+=2;
				break;
			}
	}
	else {  //test commands
		if(type == ARM_LESS_SIGNED || type == ARM_GREATER_SIGNED || subtype == ARM_SKIP_ALL_INV || (subtype == ARM_AND && g_outdevice != DEV_CB)) {
			ret = ERR_TEST_TYPE;
		}
		if(size == SIZE_BYTE && g_outdevice != DEV_CB) ret = ERR_TEST_SIZE;
		trans = 0;
		if(size == SIZE_WORD) {
			if(type == ARM_EQUAL) {
				if((*idx + 2) < src->codecnt) {
					type2		= GET_ARM_TYPE(code[*idx + 2]);
					subtype2	= GET_ARM_SUBTYPE(code[*idx + 2]);
					if(type2 == ARM_WRITE && subtype2 == ARM_HOOK) {
						if(g_outdevice == DEV_AR2) {
							cheatAppendOctet(dest, MAKE_STD_CMD(AR2_COND_HOOK) | ADDR(code[*idx]) + 1);
							cheatAppendOctet(dest, ADDR(code[*idx]));
							cheatAppendOctet(dest, ADDR(code[*idx + 1]));
							cheatAppendOctet(dest, 0);
						} else {
							cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(code[*idx]));
							cheatAppendOctet(dest, code[*idx + 1]);
						}
						trans = 1;
						*idx+=4;
					} else ret = ERR_TEST_SIZE;
				} else ret = ERR_INVALID_CODE;
			} else ret = ERR_TEST_SIZE;
		}
		if(!ret && !trans) {
			address	= ADDR(code[*idx]);
			value	= code[*idx + 1] & valmask[size];

			if(type == ARM_AND) type-=2;
			else if(type <= ARM_NOT_EQUAL) type--;
			else type-=3;

			if(subtype == ARM_SKIP_1) skip = 2;
			else if(subtype == ARM_SKIP_2) skip = 4;
			else skip = src->codecnt;			//max

			//temporarily append the code
			int tmpout = dest->codecnt;
			size = size == SIZE_HALF ? 0 : 1;
			cheatAppendOctet(dest, 0);
			cheatAppendOctet(dest, address | (type << 28));
			*idx+=2;
			int tmpcnt = dest->codecnt;
			skip += *idx;
			while(*idx < src->codecnt && *idx < skip) {
				if(code[*idx] == ARM_CMD_SPECIAL && code[*idx] == ARM_CMD_RESUME) {
					*idx+=2;
					break;
				}
				ret = transMaxToStd(dest, src, idx);
				if(ret) break;
			}
			tmpcnt = (dest->codecnt - tmpcnt) >> 1;
			//fix the code
			if(tmpcnt == 1) {
				dest->code[tmpout]	= MAKE_STD_CMD(STD_TEST_SINGLE) | address;
				dest->code[tmpout+1]	= (type << 20) | (size << 16) | value;
			} else {
				dest->code[tmpout]	= MAKE_STD_CMD(STD_TEST_MULTI) | (size << 24) | (tmpcnt << 16) | value;
			}
		}
	}
	return ret;
}

int transBatchTranslate(cheat_t *src) {
    cheat_t *dest = cheatInit(MAX_CODE_STEP);
    int ret, i;

    if(g_indevice == g_outdevice) return 0;

    if(g_indevice != DEV_ARMAX) {
        if(g_outdevice == DEV_ARMAX) {
            for(i = 0; i < src->codecnt;) {
                ret = transStdToMax(dest, src, &i);
                if(!err_suppress && ret) break;
            }
        } else {
            for(i = 0; i < src->codecnt;) {
                ret = transOther(dest, src, &i);
                if(!err_suppress && ret) break;
            }
        }
    } else {
        for(i = 0; i < src->codecnt;) {
            ret = transMaxToStd(dest, src, &i);
            if(!err_suppress && ret) break;
        }
    }
    if(dest->codemax > src->codemax) src->code = realloc(src->code, sizeof(u32) * dest->codemax);
    memcpy(src->code, dest->code, sizeof(u32) * dest->codecnt);
    src->codecnt = dest->codecnt;

    return ret;
}