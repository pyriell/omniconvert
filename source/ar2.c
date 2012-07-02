/*
 * ar2.c -- Action Replay 1 & 2 encryption routines
 *
 * Copyright (C) 2003-2008 misfire
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
 *  Much of this fire is misfire's implementation of the well-known
 *  and simple, old AR encryption.  I (Pyriel) added the batch
 *  decryption functions, and some things to deal with encryption
 *  seeds.
 */



/*

ActionReplay2/GameShark2 V1/V2 crypto routines
RE'd by misfire of XFX
http://www.xploderfreax.de/

Some interesting RAM addresses...

data:

00139388        seed table #0
001393A8        seed table #1
001393C8        seed table #2
001393E8        seed table #3

DEADFACE xxxxyyyy

00139408        xxxx - used to decrypt address
0013940C        yyyy - used to decrypt value

decryption functions:

00105A78        main
00105908        (dis)assemble address
001059C0        (dis)assemble value
00104FA8        set default seeds
00104FC8        initialize decryption depending on type
                (used for both address and value)
0010501C        type #0
00105744        type #1
00105104        type #2
001051EC        type #3
001052D4        type #4
0010540C        type #5
00105554        type #6
00105698        type #7

(PAL XP2V2 / SLES-50215 / CRC-32: 0xF444AFE6)

*/

#include "ar2.h"

u8 g_seed[4];

const u8 tbl_[4][32] = {
	{
		// seed table #0
		0x00, 0x1F, 0x9B, 0x69, 0xA5, 0x80, 0x90, 0xB2,
		0xD7, 0x44, 0xEC, 0x75, 0x3B, 0x62, 0x0C, 0xA3,
		0xA6, 0xE4, 0x1F, 0x4C, 0x05, 0xE4, 0x44, 0x6E,
		0xD9, 0x5B, 0x34, 0xE6, 0x08, 0x31, 0x91, 0x72,
	},
	{
		// seed table #1
		0x00, 0xAE, 0xF3, 0x7B, 0x12, 0xC9, 0x83, 0xF0,
		0xA9, 0x57, 0x50, 0x08, 0x04, 0x81, 0x02, 0x21,
		0x96, 0x09, 0x0F, 0x90, 0xC3, 0x62, 0x27, 0x21,
		0x3B, 0x22, 0x4E, 0x88, 0xF5, 0xC5, 0x75, 0x91,
	},
	{
		// seed table #2
		0x00, 0xE3, 0xA2, 0x45, 0x40, 0xE0, 0x09, 0xEA,
		0x42, 0x65, 0x1C, 0xC1, 0xEB, 0xB0, 0x69, 0x14,
		0x01, 0xD2, 0x8E, 0xFB, 0xFA, 0x86, 0x09, 0x95,
		0x1B, 0x61, 0x14, 0x0E, 0x99, 0x21, 0xEC, 0x40,
	},
	{
		// seed table #3
		0x00, 0x25, 0x6D, 0x4F, 0xC5, 0xCA, 0x04, 0x39,
		0x3A, 0x7D, 0x0D, 0xF1, 0x43, 0x05, 0x71, 0x66,
		0x82, 0x31, 0x21, 0xD8, 0xFE, 0x4D, 0xC2, 0xC8,
		0xCC, 0x09, 0xA0, 0x06, 0x49, 0xD5, 0xF1, 0x83,
	}
};

u8 nibble_flip(u8 byte) {
	return (byte << 4) | (byte >> 4);
}

u32 ar2decrypt(u32 code, u8 type, u8 seed) {
	if (type == 7) {
		if (seed & 1) type = 1;
		else return ~code;
	}

	u8 tmp[4];
	*(u32*)tmp = code;

	switch (type) {
		case 0:
			tmp[3] ^= tbl_[0][seed];
			tmp[2] ^= tbl_[1][seed];
			tmp[1] ^= tbl_[2][seed];
			tmp[0] ^= tbl_[3][seed];
			break;
		case 1:
			tmp[3] = nibble_flip(tmp[3]) ^ tbl_[0][seed];
			tmp[2] = nibble_flip(tmp[2]) ^ tbl_[2][seed];
			tmp[1] = nibble_flip(tmp[1]) ^ tbl_[3][seed];
			tmp[0] = nibble_flip(tmp[0]) ^ tbl_[1][seed];
			break;
		case 2:
			tmp[3] += tbl_[0][seed];
			tmp[2] += tbl_[1][seed];
			tmp[1] += tbl_[2][seed];
			tmp[0] += tbl_[3][seed];
			break;
		case 3:
			tmp[3] -= tbl_[3][seed];
			tmp[2] -= tbl_[2][seed];
			tmp[1] -= tbl_[1][seed];
			tmp[0] -= tbl_[0][seed];
			break;
		case 4:
			tmp[3] = (tmp[3] ^ tbl_[0][seed]) + tbl_[0][seed];
			tmp[2] = (tmp[2] ^ tbl_[3][seed]) + tbl_[3][seed];
			tmp[1] = (tmp[1] ^ tbl_[1][seed]) + tbl_[1][seed];
			tmp[0] = (tmp[0] ^ tbl_[2][seed]) + tbl_[2][seed];
			break;
		case 5:
			tmp[3] = (tmp[3] - tbl_[1][seed]) ^ tbl_[0][seed];
			tmp[2] = (tmp[2] - tbl_[2][seed]) ^ tbl_[1][seed];
			tmp[1] = (tmp[1] - tbl_[3][seed]) ^ tbl_[2][seed];
			tmp[0] = (tmp[0] - tbl_[0][seed]) ^ tbl_[3][seed];
			break;
		case 6:
			tmp[3] += tbl_[0][seed];
			tmp[2] -= tbl_[1][(seed + 1) & 0x1F];
			tmp[1] += tbl_[2][(seed + 2) & 0x1F];
			tmp[0] -= tbl_[3][(seed + 3) & 0x1F];
			break;
	}

	return *(u32*)tmp;
}

u32 ar2encrypt(u32 code, u8 type, u8 seed) {
	if (type == 7) {
		if (seed & 1) type = 1;
		else return ~code;
	}

	u8 tmp[4];
	*(u32*)tmp = code;

	switch (type) {
		case 0:
			tmp[3] ^= tbl_[0][seed];
			tmp[2] ^= tbl_[1][seed];
			tmp[1] ^= tbl_[2][seed];
			tmp[0] ^= tbl_[3][seed];
			break;
		case 1:
			tmp[3] = nibble_flip(tmp[3] ^ tbl_[0][seed]);
			tmp[2] = nibble_flip(tmp[2] ^ tbl_[2][seed]);
			tmp[1] = nibble_flip(tmp[1] ^ tbl_[3][seed]);
			tmp[0] = nibble_flip(tmp[0] ^ tbl_[1][seed]);
			break;
		case 2:
			tmp[3] -= tbl_[0][seed];
			tmp[2] -= tbl_[1][seed];
			tmp[1] -= tbl_[2][seed];
			tmp[0] -= tbl_[3][seed];
			break;
		case 3:
			tmp[3] += tbl_[3][seed];
			tmp[2] += tbl_[2][seed];
			tmp[1] += tbl_[1][seed];
			tmp[0] += tbl_[0][seed];
			break;
		case 4:
			tmp[3] = (tmp[3] - tbl_[0][seed]) ^ tbl_[0][seed];
			tmp[2] = (tmp[2] - tbl_[3][seed]) ^ tbl_[3][seed];
			tmp[1] = (tmp[1] - tbl_[1][seed]) ^ tbl_[1][seed];
			tmp[0] = (tmp[0] - tbl_[2][seed]) ^ tbl_[2][seed];
			break;
		case 5:
			tmp[3] = (tmp[3] ^ tbl_[0][seed]) + tbl_[1][seed];
			tmp[2] = (tmp[2] ^ tbl_[1][seed]) + tbl_[2][seed];
			tmp[1] = (tmp[1] ^ tbl_[2][seed]) + tbl_[3][seed];
			tmp[0] = (tmp[0] ^ tbl_[3][seed]) + tbl_[0][seed];
			break;
		case 6:
			tmp[3] -= tbl_[0][seed];
			tmp[2] += tbl_[1][(seed + 1) & 0x1F];
			tmp[1] -= tbl_[2][(seed + 2) & 0x1F];
			tmp[0] += tbl_[3][(seed + 3) & 0x1F];
			break;
	}

	return *(u32*)tmp;
}

//I keep the specific seed for this device separate from other seeds.
void ar2SetSeed(u32 key) {
	key = swapbytes(key);
	memcpy(g_seed, &key, 4);
}

u32 ar2GetSeed() {
	u32 key;
	memcpy(&key, g_seed, 4);
	key = swapbytes(key);
}

//Batch decrypt an array of u32s
void ar2BatchDecryptArr(u32 *code, u32 *size) {
	u32 i, j;
	
	for(i = 0; i < *size; i+=2) {
		code[i] = ar2decrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2decrypt(code[i+1], g_seed[2], g_seed[3]);
		if(code[i] == 0xDEADFACE) { 
			ar2SetSeed(code[i+1]);
			j = i + 2;
			if(j < *size) {
				for(;j < *size; j++) {
					code[j-2] = code[j];
				}
			}
			i-=2;
			*size -= 2;
		}
	}
}

//Batch decrypt a cheat struct
void ar2BatchDecrypt(cheat_t *cheat) {
	u32 i;
	u32 *code = cheat->code;
	
	for(i = 0; i < cheat->codecnt; i+=2) {
		code[i] = ar2decrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2decrypt(code[i+1], g_seed[2], g_seed[3]);
		if(code[i] == 0xDEADFACE) { 
			ar2SetSeed(code[i+1]);
			cheatRemoveOctets(cheat, i+1, 2);
			i-=2;
		}
	}
}

//Batch encrypt an array of u32s
void ar2BatchEncryptArr(u32 *code, u32 *size) {
	int i;
	u32 seed = 0xFFFFFFFF;

	for(i = 0; i < *size; i+=2) {
		if(code[i] == 0xDEADFACE) seed = code[i+1];
		code[i] = ar2encrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2encrypt(code[i+1], g_seed[2], g_seed[3]);
		if(seed != 0xFFFFFFFF) ar2SetSeed(seed);
	}
}

//Batch encrypt a cheat struct
void ar2BatchEncrypt(cheat_t *cheat) {
	int i, num = cheat->codecnt;
	u32 *code = cheat->code, seed = 0xFFFFFFFF;

	for(i = 0; i < num; i+=2) {
		if(code[i] == 0xDEADFACE) seed = code[i+1];
		code[i] = ar2encrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2encrypt(code[i+1], g_seed[2], g_seed[3]);
		if(seed != 0xFFFFFFFF) ar2SetSeed(seed);
	}
}

//Deal with AR1 encryption
void ar1BatchDecrypt(cheat_t *cheat) {
	int i;
	u32 hold	= ar2GetSeed();
	u32 num 	= cheat->codecnt;
	u32 *code 	= cheat->code;
	ar2SetSeed(AR1_SEED);
	for(i = 0; i < num; i+=2) {
		code[i] = ar2decrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2decrypt(code[i+1], g_seed[2], g_seed[3]);
	}
	ar2SetSeed(hold);
}

void ar1BatchEncrypt(cheat_t *cheat) {
	int i;
	u32 hold	= ar2GetSeed();
	u32 num 	= cheat->codecnt;
	u32 *code 	= cheat->code;
	ar2SetSeed(AR1_SEED);
	for(i = 0; i < num; i+=2) {
		code[i] = ar2encrypt(code[i], g_seed[0], g_seed[1]);
		code[i+1] = ar2encrypt(code[i+1], g_seed[2], g_seed[3]);
	}
	ar2SetSeed(hold);
}

//Prepend the key code (DEADFACE) to all enable codes in the list.
u8 ar2AddKeyCode(cheat_t *cheat) {
	int i;
	u32 tmp;
	u32 num		= cheat->codecnt;
	u32 *code	= cheat->code;
	u8 ret = 0;
	for(i = 0; i < num && ret == 0; i += 2) {
		if(code[i] == AR2_KEY_ADDR) return 1;
		tmp = code[i] >> 28;
		if(tmp == 0xF || tmp == 0x8) ret = 1;
	}
	if(ret) {
		tmp = ar2GetSeed();
		cheatPrependOctet(cheat, tmp);
		cheatPrependOctet(cheat, AR2_KEY_ADDR);
		ar2SetSeed(AR1_SEED);
	}

	return ret;
}
