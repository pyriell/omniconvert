/*
 * gs3.h -- GameShark Version 3+ and Version 5+ encryption routines.
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

#ifndef _GS3_H_
#define _GS3_H_

#include "abbtypes.h"
#include "cheat.h"
#include "common.h"

unsigned int gs3BuildSeeds(const u16 seed1, const u16 seed2, const u16 seed3);
void gs3Init();
//int gs3Destroy();
int gs3Update(unsigned int seeds[2]);
u8 gs3Decrypt(u32* address, u32* value);
u8 gs3Encrypt(u32* address, u32* value, u8 key);
void gs3BatchDecrypt(cheat_t *cheat);
void gs3BatchEncrypt(cheat_t *cheat, u8 key);
void gs3CreateVerifier(u32 *address, u32 *value, u32 *codes, int size);
void gs3AddVerifier(cheat_t *cheat);
void gs3GenCrc32Tab();
u32 gs3Crc32(u8 *data, u32 size);
u32 gs3GenCrc32(u8 *data, u32 size);
void gs3CryptFileData(u8 *data, u32 size);

#endif //_GS3_H_
