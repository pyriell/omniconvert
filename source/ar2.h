/*
 * ar2.h -- Action Replay 1 & 2 encryption routines (prototypes)
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


#ifndef _AR2_H_
#define _AR2_H_

#define AR2_KEY_ADDR	0xDEADFACE
#define AR1_SEED	0x05100518

#include <stdio.h>
#include "abbtypes.h"
#include "common.h"
#include "cheat.h"

u8 nibble_flip(u8 byte);
u32 ar2decrypt(u32 code, u8 type, u8 seed);
u32 ar2encrypt(u32 code, u8 type, u8 seed);
void ar2SetSeed(u32 key);
u32 ar2GetSeed();
void ar2BatchDecrypt(cheat_t *cheat);
void ar2BatchDecryptArr(u32 *code, u32 *size);
void ar2BatchEncrypt(cheat_t *cheat);
void ar2BatchEncryptArr(u32 *code, u32 *size);
void ar1BatchDecrypt(cheat_t *cheat);
void ar1BatchEncrypt(cheat_t *cheat);
u8 ar2AddKeyCode(cheat_t *cheat);

#endif //_AR2_H_
