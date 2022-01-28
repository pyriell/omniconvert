/* armax.h -- Action Replay Max code routines
 *
 * Copyright notice for this file:
 *  Copyright (C) 2003-2004 Parasyte
 *  Copyright (C) 2008 Pyriel
 *
 * This file is part of Omniconvert.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _ARMAX_H_
#define _ARMAX_H_

#include "abbtypes.h"
#include "cheat.h"
#include <windows.h>

#define EXPANSION_DATA_FOLDER	0x0800
#define FLAGS_FOLDER		0x5 << 20
#define FLAGS_FOLDER_MEMBER	0x4 << 20

extern u32 g_gameid;
extern u32 g_region;

//common routines
void generateseeds(u32 *seeds, const u8 *seedtable, u8 doreverse);
u32 rotate_left(u32 value, u8 rotate);
u32 rotate_right(u32 value, u8 rotate);
u32 byteswap(u32 val);
void getcode(u32 *src, u32 *addr, u32 *val);
void setcode(u32 *dst, u32 addr, u32 val);
u16 gencrc16(u32 *codes, u16 size);
u8 verifycode(u32 *codes, u16 size);
void buildseeds();
int MaxRemoveDashes(char *dest, const char *src);
int IsArMaxStr(const char *s);
void armMakeVerifier(u32 *code, u32 num, u32 ver[2], u32 gameid, u8 region);
void armMakeFolder(cheat_t *cheat, u32 gameid, u8 region);
void armEnableExpansion(cheat_t *cheat);
int armMakeDiscHash(u32 *hash, HWND hwnd, char drive);
s16 armReadVerifier(u32 *code, u32 size);

//decryption
void unscramble1(u32 *addr, u32 *val);
void unscramble2(u32 *addr, u32 *val);
void decryptcode(u32 *seeds, u32 *code);
u8 getbitstring(u32 *ctrl, u32 *out, u8 len);
u8 batchdecrypt(u32 *codes, u16 size);
u8 alphatobin(char alpha[][14], u32 *dst, int size);
u8 armBatchDecryptFull(cheat_t *cheat, u32 ar2key);

//encryption
void scramble1(u32 *addr, u32 *val);
void scramble2(u32 *addr, u32 *val);
void encryptcode(u32 *seeds, u32 *code);
u8 batchencrypt(u32 *codes, u16 size);
void bintoalpha(char *dst, u32 *src, int index);
u8 armBatchEncryptFull(cheat_t *cheat, u32 ar2key);

#endif //_ARMAX_H_
