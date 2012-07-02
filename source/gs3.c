/*
 * gs3.c -- GameShark Version 3+ and Version 5+ encryption routines.
 *
 * GameShark (MadCatz) Version 3+ Crypt, CRC, and Utility routines
 * Also for Xploder 4+ and Xterminator (ver?) devices.
 *
 * Copyright (C) 2004-2008 Pyriel
 * All rights reserved.
 *
 * This file is part of Omniconvert/P2ME.
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

#include <malloc.h>
#include <time.h>
#include "abbtypes.h"
#include "gs3.h"

//mask for the msb/sign bit
#define SMASK 0x80000000

//mask for the lower bits of a word.
#define LMASK 0x7FFFFFFF

//Size of the state vector
#define MT_STATE_SIZE	624

//Magical Mersenne masks
#define MT_MASK_B		0x9D2C5680
#define MT_MASK_C		0xEFC60000

//Offset for recurrence relationship
#define MT_REC_OFFSET	397
#define MT_BREAK_OFFSET	227

#define MT_MULTIPLIER	69069

enum {
	GS3_CRYPT_X1,
	GS3_CRYPT_2,
	GS3_CRYPT_1,
	GS3_CRYPT_3,
	GS3_CRYPT_4 = GS3_CRYPT_3,
	GS3_CRYPT_X2 = GS3_CRYPT_3
};

//maximum number of crypt tables required for encryption
#define GS3_NUM_TABLES 4

#define BYTES_TO_WORD(a,b,c,d) ((a << 24) | (b << 16) | (c << 8) | d)
#define BYTES_TO_HALF(a,b) ((a << 8) | b)
#define GETBYTE(word, num) ((word >> ((num-1) << 3)) & 0xFF)
#define DECRYPT_MASK 0xF1FFFFFF

typedef struct {
	int size;
	unsigned char table[0x100];	//Lock in to the maximum table size rather than malloc.
} t_crypt_table;

static int passcount = MT_STATE_SIZE;
static u32 gs3_mtStateTbl_code[MT_STATE_SIZE] = { 0 };
static u32 gs3_mtStateTbl_p2m[MT_STATE_SIZE] = { 0 };
static u32 gs3_decision_matrix[2] = { 0, 0x9908B0DF };
static u16 gs3_hseeds[3] = {0x6C27, 0x1D38, 0x7FE1};
//static int gs3_tbl_size[GS3_NUM_TABLES] = { 0x40, 0x39, 0x19, 0x100 };
static t_crypt_table gs3_encrypt_seeds[GS3_NUM_TABLES] = { { 0x40, {0} }, { 0x39, {0} },  { 0x19, {0} }, { 0x100, {0} } };
static t_crypt_table gs3_decrypt_seeds[GS3_NUM_TABLES] = { { 0x40, {0} }, { 0x39, {0} },  { 0x19, {0} }, { 0x100, {0} } };
static u8 gs3_init = 0;
static u8 gs3_linetwo = 0;
static u8 gs3_xkey;		//key for second line of two line code types

/***************************************************************
Gameshark Version 5+ Routines

While Gameshark versions 3 and up all use the same encryption,
Version 5 introduced a verification system for the codes.  The
verification consists of a prefixed code line, beginning with
"76" that contains the CCITT 16-bit CRC of all the lines in the
code.  The lines are summed as they appear, to the outside world
i.e. in a big endian byte order.

The verification lines are unnecessary (so far as I know).  You
can just as easily use raw codes or encrypted codes without
them.

There is another verifier that begins with a "77".  This is
for the automatic mode, and helps identify the game.  It is
probably a CRC of the ELF Header, or as quickly as detection
attempts are made, the SYSTEM.CNF file.  I attempted to use
the feature with only the code file included on the disc
(official codes), and detection always failed with version 5
software.  These verifiers are wholly unnecessary, and
building them may be a complete waste of time, if the feature
doesn't even work properly.

The routines have the "gs3" prefix only for simplicity's sake.
***************************************************************/

u16 ccitt_table[256];
u16 ccitt_poly = 0x1021;
u8 gs3Version5 = 0;

void gs3SetVersion5(u8 flag) {
	gs3Version5 = flag;
}

u8 gs3IsVersion5() {
	return gs3Version5;
}

void gs3CcittBuildSeeds (void){
	short tmp;
	int i, j;
	for(i = 0; i < 256; i++) {
		tmp = 0;
		tmp = i << 8;
		for(j = 0; j < 8; j++) {
			if((tmp & 0x8000) > 0) {
				tmp = ((tmp << 1) ^ ccitt_poly) & 0xFFFF;
			}
			else {
				tmp <<= 1;
			}

		}
		ccitt_table[i] = tmp&0xFFFF;
	}
	return;
}

u16 gs3CcittCrc(const u8 *bytes, const int bytecount) {
	if (bytecount == 0) return 0;
	unsigned short crc;
	int j;
	crc = 0;
	for(j = 0; j < bytecount; j++) {
		crc = ((crc << 8) ^ ccitt_table[((crc >> 8) ^ (0xff & bytes[j]))]);
	}
	return crc;
}

unsigned short gs3GenCrc (u32 const *code, const int count) {
	u32 *swap = (u32*)malloc(sizeof(u32) * count);
	u8 *bytes;
	u16 crc;
	int i;

	if(!swap) return 0;
	
	for(i = 0; i < count; i++) {
		swap[i] = swapbytes(code[i]);
	}
	bytes = (u8 *)swap;
	crc = gs3CcittCrc(bytes, count << 2);
	free(swap);
	return crc;
}

/***************************************************************
End Version 5+ Routines.
***************************************************************/

/**
  *  Fire also implements their own bogus version of a CRC32.  I've never seen this
  *  variation before.  This is used in cheat update files and save archives.
  *  Cheat files are GS5+, but this has been in the save files for a long time.
  *  
*/

#define GS3_POLY 	0x04C11DB7

u32 gs3_crc32tab[256];

void gs3GenCrc32Tab() {
	u32 crc, i, j;
	for(i = 0; i < 256; i++) {
		crc = i << 24;
		for(j = 0; j < 8; j++) {
			crc = (crc & SMASK) == 0 ? crc << 1 : (crc << 1) ^ GS3_POLY;
		}
		gs3_crc32tab[i] = crc;
	}
}

u32 gs3Crc32(u8 *data, u32 size) {
	u32 i, crc = 0;
	if(size == 0 || size == 0xFFFFFFFF) return 0;
	for(i = 0; i < size; i++) {
		crc = (crc << 8) ^ gs3_crc32tab[((crc >> 24) ^ data[i])];
	}
	return crc;
}

u32 gs3GenCrc32(u8 *data, u32 size) {
	gs3GenCrc32Tab();
	return gs3Crc32(data, size);
}

/***************************************************************
End CRC32
***************************************************************/

/***************************************************************
Encryption/Decryption Seed Table Routines.
The encryption is based on MTCrypt.
***************************************************************/

/*
Initializes the state table for the adapted Mersenne Twist.
Arguments:	seed - a seed value (either from the static table or from a code)
*/
void gs3InitMtStateTbl(u32 *mtStateTbl, u32 seed) {
	u32 wseed, ms, ls;
	u32 mask = 0xFFFF0000;
	int i;
	wseed = seed;
	for(i = 0; i < MT_STATE_SIZE; i++) {
		ms = (wseed & mask);
		wseed = wseed * MT_MULTIPLIER + 1;
		ls = wseed & mask;
		mtStateTbl[i] = (ls >> 16) | ms;
		wseed = wseed * MT_MULTIPLIER + 1;
	}
	passcount = MT_STATE_SIZE;
	return;
}
/*
The whole thing is basically an adapted Mersenne Twist algorithm.  This just shuffles around the
state table at a given point, and returns a non-random value for use in shuffling the byte
seed tables.  The byte seed tables are used in encryption/decryption.

Arguments:	none.

Returns:  Integer value.
*/
unsigned int gs3GetMtNum(u32 *mtStateTbl) {
	u32 *tbl = mtStateTbl;
	u32 tmp;
	int i, off = MT_REC_OFFSET;

	if(passcount >= MT_STATE_SIZE) {
		if(passcount == MT_STATE_SIZE + 1) { gs3InitMtStateTbl(mtStateTbl, 0x1105); }
		for(i = 0; i < MT_STATE_SIZE - 1; i++) {
			if(i == MT_BREAK_OFFSET) off = -1 * MT_BREAK_OFFSET;
			tmp = (*tbl & SMASK) | (*(tbl+1) & LMASK);
			*tbl = (tmp >> 1) ^ *(tbl+off) ^ gs3_decision_matrix[tmp & 1];
			tbl++;
		}
		tmp = (*mtStateTbl & LMASK) | ((*(mtStateTbl + MT_STATE_SIZE - 1)) & SMASK);
		tbl = mtStateTbl + MT_STATE_SIZE - 1;
		*tbl = (tmp >> 1) ^ *(mtStateTbl+MT_REC_OFFSET-1) ^ gs3_decision_matrix[tmp & 1];
		passcount = 0;
	}

	tmp = *(mtStateTbl + passcount);
	tmp ^= tmp >> 11;
	tmp ^= (tmp << 7) & MT_MASK_B;
	tmp ^= (tmp << 15) & MT_MASK_C;
	passcount++;
	return (tmp >> 18) ^ tmp;
}

/*
Builds byte seed tables.

Arguments:	tbl - pointer to the table to be built.
			seed - a halfword seed used to build the word table.
			size - size of the byte seed table being built.
*/
void gs3BuildByteSeedTbl(u8 *tbl, const u16 seed, const int size) {
	int i, idx1, idx2;
	u8 tmp;

	if(size > 0) {
		//initialize the table with a 0 to size sequence.
		for(i = 0; i < size; i++) {
			tbl[i] = i;
		}
		//Build the word seed table from seed.
		gs3InitMtStateTbl(gs3_mtStateTbl_code, seed);
		//essentially pass through the table twice, swapping based on returned word values.
		i = size << 1;
		while(i--) {
			idx1 = gs3GetMtNum(gs3_mtStateTbl_code) % size;
			idx2 = gs3GetMtNum(gs3_mtStateTbl_code) % size;
			tmp = tbl[idx1];
			tbl[idx1] = tbl[idx2];
			tbl[idx2] = tmp;
		}
	}
	else {
		//this should really never happen.
		gs3InitMtStateTbl(gs3_mtStateTbl_code, seed);
	}
	return;
}

/*
This reverses the byte seed tables for use in decryption.

Arguments:  dest - pointer to the destination crypt table.
			src - pointer to the source crypt table.
			size - size of both tables.
*/

void gs3ReverseSeeds(u8 *dest, const u8 * src, const int size) {
	int i;
	for(i = 0; i < size; i++) {
		dest[src[i]] = i;
	}
	return;
}
/*
Build the byte seed tables using the halfword values passed.  This will either be from
a code with key type = 7, or the default halfword seeds.  Only one of them will actually
be used in generating the byte tables, seed2 and 3 just build the return value.

Arguments:  seed1, seed2, seed3 - the three halfword seeds required.
Returns: a byte value (in an int) based on the seeds provided.
*/

unsigned int gs3BuildSeeds(const u16 seed1, const u16 seed2, const u16 seed3) {
	int i, shift;
	for(i = 0; i < GS3_NUM_TABLES; i++) {
		shift= 5 - i;
		shift = shift < 3 ? 0 : shift;
		gs3BuildByteSeedTbl(gs3_encrypt_seeds[i].table, seed1 >> shift, gs3_encrypt_seeds[i].size);
		gs3ReverseSeeds(gs3_decrypt_seeds[i].table,
			gs3_encrypt_seeds[i].table, gs3_decrypt_seeds[i].size);
	}
	return (((((seed1 >> 8) | 1) + seed2 - seed3) - (((seed2 >> 8) & 0xFF) >> 2))
				+ (seed1 & 0xFC) + (seed3 >> 8) + 0xFF9A) & 0xFF;
}

/*
Initialize the encryption tables using the default halfword seeds.
This should be called prior to using the encryption routines, to reset the tables' states.
*/
void gs3Init() {
	int i;
	if(!gs3_init) gs3CcittBuildSeeds();
	gs3BuildSeeds(gs3_hseeds[0], gs3_hseeds[1], gs3_hseeds[2]);
	gs3_init = 1;
	return;
}

/*
Update the encryption seeds.  For some reason, this doesn't replace the halfword seeds.
It just feeds the code [XXXX][YYYY] [ZZZZ][IGNORED] into the seed build routine.
Also for some reason, it stores the return value from BuildSeeds back in the 3rd byte of
the code address.

Arguments:  u32 seeds[2] - A pointer to a code.
Returns: 0 on success, 1 otherwise.
*/

int gs3Update(unsigned int seeds[2]) {
	unsigned short *seed = (unsigned short *)seeds;
	*seeds = (gs3BuildSeeds(seed[0], seed[1], seed[2]) << 16) | (*seeds & 0xFF00FFFF);
	return 0;
}
/***************************************************************
Encryption/Decryption Routines.
***************************************************************/

/*
Decrypt a single line.  Key indicators are built into the individual codes.
*/

u8 gs3Decrypt(u32* address, u32* value) {
	u32 command, tmp = 0, tmp2 = 0, mask = 0;
	u8 flag, *seedtbl, key;
	int size, i = 0;
	
	//First, check to see if this is the second line
	//  of a two-line code type.
	if(gs3_linetwo) {
	  switch(gs3_xkey) {
		case 1:
		case 2: {  //key x1 - second line for key 1 and 2.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_X1].table;
			size = gs3_decrypt_seeds[GS3_CRYPT_X1].size;
			for(i = 0; i < size; i++) {
				flag =  i < 32 ?
					((*value ^ (gs3_hseeds[1] << 13)) >> i) & 1 :
					(((*address ^ gs3_hseeds[1] << 2)) >> (i - 32)) & 1;
				if(flag > 0) {
					if(seedtbl[i] < 32) {
						tmp2 |= (1 << seedtbl[i]);
					}
					else {
						tmp |= (1 << (seedtbl[i] - 32));
					}
				}
			}
			*address = tmp ^ (gs3_hseeds[2] << 3);
			*value = tmp2 ^ gs3_hseeds[2];
			gs3_linetwo = 0;
			return 0;
		}
		case 3:
		case 4: {  //key x2 - second line for key 3 and 4.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_X2].table;
			tmp = *address ^ (gs3_hseeds[1] << 3);
			*address = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					^ (gs3_hseeds[2] << 16);
			tmp = *value ^ (gs3_hseeds[1] << 9);
			*value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					^ (gs3_hseeds[2] << 5);
			gs3_linetwo = 0;
			return 0;
		}
		default: {
			gs3_linetwo = 0;
			return 0;
		}
	  }
	}
	
	//Now do normal encryptions.
	command = *address & 0xFE000000;
	key = (*address >> 25) & 0x7;
	if(command >= 0x30000000 && command <= 0x6FFFFFFF) {
		gs3_linetwo = 1;
		gs3_xkey = key;
	}
	
	if((command >> 28) == 7) return 0;  //do nothing on verifier lines.

	switch(key) {
		case 0: //Key 0.  Raw.
			break;
		case 1: {  //Key 1.  This key is used for codes built into GS/XP cheat discs.
					//  You typically cannot see these codes.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_1].table;
			size = gs3_decrypt_seeds[GS3_CRYPT_1].size;
			tmp = (*address & 0x01FFFFFF) ^ (gs3_hseeds[1] << 8);
			mask = 0;
			for(i = 0; i < size; i++) {
				mask |= ((tmp & (1 << i)) >> i) << seedtbl[i];
			}
			*address = ((mask ^ gs3_hseeds[2]) | command) & DECRYPT_MASK;
			break;
		}
		case 2: {  //Key 2.  The original encryption used publicly.  Fell into disuse 
					//around August, 2004.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_2].table;
			size = gs3_decrypt_seeds[GS3_CRYPT_2].size;
			*address = (*address & 0x01FFFFFF) ^ (gs3_hseeds[1] << 1);
			tmp = tmp2 = 0;
			for(i = 0; i < size; i++) {
				flag = (i < 32) ? ((*value  ^ (gs3_hseeds[1] << 16)) >> i) & 1 :
					(*address >> (i - 32)) & 1;
				if(flag > 0) {
					if(seedtbl[i] < 32) {
						tmp2 |= (1 << seedtbl[i]);
					}
					else {
						tmp |= (1 << (seedtbl[i] - 32));
					}
				}
			}
			*address = ((tmp ^ (gs3_hseeds[2] << 8)) | command) & DECRYPT_MASK;
			*value = tmp2 ^ gs3_hseeds[2];
			break;
		}
		case 3: {  //Key 3.  Unused publicly.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_3].table;
			tmp = *address ^ (gs3_hseeds[1] << 8);
			*address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 1)], seedtbl[GETBYTE(tmp, 2)])
				| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[2] << 4)) & DECRYPT_MASK;
			break;
		}
		case 4: {  //Key 4.  Second encryption used publicly.
			seedtbl = gs3_decrypt_seeds[GS3_CRYPT_4].table;
			tmp = *address ^ (gs3_hseeds[1] << 8);
			*address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[2] << 4)) & DECRYPT_MASK;
			tmp = *value ^ (gs3_hseeds[1] << 9);
			*value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
						seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
						^ (gs3_hseeds[2] << 5);
			break;
		}
		case 5:
		case 6:	//Key 5 and Key 6 appear to be nonexistent routines.  Entering codes that use
			//  key 6 into GSv3 and XPv4 result in code entries that cannot be activated except
			//  with much effort and luck.
			break;
		case 7: { //seed refresh
			u32 code[2] = { *address, *value };
			gs3Update(code);
			break;
		}
		default:
			break;
	}
	return 0;
};
/*
Encrypt a single line.  The key indicator is passed, however an allowance
has been made for a per-code specification using bits 25-27 as in encrypted codes.
*/
u8 gs3Encrypt(u32* address, u32* value, u8 key)
{
	u32 command = *address & 0xFE000000;
	u32 tmp = 0, tmp2 = 0, mask = 0;
	u8 flag, *seedtbl;
	int size, i = 0;
	if (!(key > 0 && key < 8)) {
		key = ((*address) >> 25) & 0x7;
	}

	//First, check to see if this is the second line
	//  of a two-line code type.
	if(gs3_linetwo) {
	  switch(gs3_xkey) {
		case 1:
		case 2: { //key x1 - second line for key 1 and 2.
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_X1].table;
			size	= gs3_encrypt_seeds[GS3_CRYPT_X1].size;
			*address = *address ^ (gs3_hseeds[2] << 3);
			*value = *value ^ gs3_hseeds[2];
			for(i = 0; i < size; i++) {
				flag = (i < 32) ? (*value >> i) & 1 : (*address >> (i - 32)) & 1;
				if(flag > 0) {
					if(seedtbl[i] < 32) {
						tmp2 |= (1 << seedtbl[i]);
					}
					else {
						tmp |= (1 << (seedtbl[i] - 32));
					}
				}
			}
			*address = tmp ^ (gs3_hseeds[1] << 2);
			*value = tmp2 ^ (gs3_hseeds[1] << 13);
			gs3_linetwo = 0;
			return 0;
		}
		case 3:
		case 4: {  //key x2 - second line for key 3 and 4.
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_X2].table;
			tmp = *address ^ (gs3_hseeds[2] << 16);
			*address = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					^ (gs3_hseeds[1] << 3);
			tmp = *value ^ (gs3_hseeds[2] << 5);
			*value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					^ (gs3_hseeds[1] << 9);
			gs3_linetwo = 0;
			return 0;
		}
		default: {
			gs3_linetwo = 0;
			return 0;
		}
	  }
	}

	if(command >= 0x30000000 && command <= 0x6FFFFFFF) {
		gs3_linetwo = 1;
		gs3_xkey = key;
	}

	switch(key) {
		case 0: //Raw
			break;
		case 1: {//Key 1.  This key is used for codes built into GS/XP cheat discs.
					//  You typically cannot see these codes.
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_1].table;
			size = gs3_encrypt_seeds[GS3_CRYPT_1].size;
			tmp = (*address & 0x01FFFFFF) ^ gs3_hseeds[2];
			mask = 0;
			for(i = 0; i < size; i++) {
				mask |= ((tmp & (1 << i)) >> i) << seedtbl[i];
			}
			*address =  ((mask  ^ (gs3_hseeds[1] << 8)))  | command;
			break;
		}

		case 2: {  //Key 2.  The original encryption used publicly.  Fell into disuse
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_2].table;
			size	= gs3_encrypt_seeds[GS3_CRYPT_2].size;
			*address = (*address ^ (gs3_hseeds[2] << 8)) & 0x01FFFFFF;
			*value ^= gs3_hseeds[2];
			tmp = tmp2 = 0;
			for(i = 0; i < 0x39; i++) {
				flag = (i < 32) ? (*value >> i) & 1 : (*address >> (i - 32)) & 1;
				if(flag > 0) {
					if(seedtbl[i] < 32) {
						tmp2 |= (1 << seedtbl[i]);
					}
					else {
						tmp |= (1 << (seedtbl[i] - 32));
					}
				}
			}
			*address = (tmp ^ (gs3_hseeds[1] << 1)) | command;
			*value = tmp2 ^ (gs3_hseeds[1] << 16);
			break;
		}
		case 3: {  //Key 3.  Unused publicly.
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_3].table;
			tmp = (*address & 0xF1FFFFFF) ^ (gs3_hseeds[2] << 4);
			*address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 1)], seedtbl[GETBYTE(tmp, 2)])
					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[1] << 8));
			break;
		}
		case 4: {  //Key 4.  Second encryption used publicly.
			seedtbl	= gs3_encrypt_seeds[GS3_CRYPT_4].table;
			tmp = (*address & 0xF1FFFFFF) ^ (gs3_hseeds[2] << 4);
			*address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[1] << 8));
			tmp = *value ^ (gs3_hseeds[2] << 5);
			*value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
						seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)]) ^ (gs3_hseeds[1] << 9);
			break;
		}
		case 5:
		case 6:
			break;
		case 7: {//seed refresh.
			u32 code[2] = { *address, *value };
			gs3Update(code);
			break;
		}
		default:
			break;
	}
	//add key to the code
	*address |= (key << 25);
	return 0;
}

/*
Decrypt a batch of codes (lines).
*/
void gs3BatchDecrypt(cheat_t *cheat) {
	int i, j;
	for(i = 0; i < cheat->codecnt;) {
		if((cheat->code[i] >> 28) == 7 && !gs3_linetwo) {  //if the code starts with 7, it's a verifier, unless it's line two of two-line code type.
			cheatRemoveOctets(cheat, i+1, 2);
		} else { 
			gs3Decrypt(&cheat->code[i], &cheat->code[i+1]);
			i+=2;
		}
	}
}

/*
Encrypt a batch of codes (lines).
*/
void gs3BatchEncrypt(cheat_t *cheat, u8 key) {
	int i;
	for(i = 0; i < cheat->codecnt; i+=2) {
		gs3Encrypt(&cheat->code[i], &cheat->code[i+1], key);
	}
	return;
}

/*
Create a verifier for version 5 codes if necessary.
*/
void gs3CreateVerifier(u32 *address, u32 *value, u32 *codes, int size) {
	u16 crc = gs3GenCrc(codes, size);
	*value = 0;
	*address = 0x76000000 | crc << 4;
	return;
}

void gs3AddVerifier(cheat_t *cheat) {
	int i;
	u32 addr, val;
	gs3CreateVerifier(&addr, &val, cheat->code, cheat->codecnt);
	cheatPrependOctet(cheat, val);
	cheatPrependOctet(cheat, addr);
	return;
}

void gs3CryptFileData(u8 *data, u32 size) {
	u32 i;
	gs3InitMtStateTbl(gs3_mtStateTbl_p2m, size);
	for(i = 0; i < size; i++) {
		data[i] ^= gs3GetMtNum(gs3_mtStateTbl_p2m);
	}
}
