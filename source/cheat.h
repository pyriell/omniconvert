/*
 * cheat.h -- container for cheat data.
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

#ifndef _CHEAT_H_
#define _CHEAT_H_

#include "abbtypes.h"

#define NAME_MAX_SIZE		50
//Status flags for codes
#define CODE_INVALID		(1 << 0) //something is wrong with the code (uneven number of octets)
#define CODE_NO_TARGET		(1 << 1) //The target device does not support this code type
//large enough for most codes
#define MAX_CODE_STEP		25 << 1

enum {
	FLAG_DEFAULT_ON,
	FLAG_MCODE,
	FLAG_COMMENTS
};

//some of what is in these structs is device specific
//but they make handy containers for all devices.
typedef struct {
	u32		id;
	char		*name;
	u32		namemax;
	char		*comment;
	u32		commentmax;
	u8		flag[3];
	u32		codecnt;
	u32		codemax;
	u32		*code;
	u8		state;
	void		*nxt;
} cheat_t;

typedef struct {
	u32		id;
	char		name[NAME_MAX_SIZE];
//	char		*comment
	u32		cnt;	//number of cheats;
} game_t;

cheat_t * cheatInit();
void cheatClearFolderId();
void cheatAppendOctet(cheat_t *cheat, u32 octet);
void cheatAppendCodeFromText(cheat_t *cheat, char *addr, char *val);
void cheatPrependOctet(cheat_t *cheat, u32 octet);
void cheatRemoveOctets(cheat_t *cheat, int start, int count);
void cheatDestroy(cheat_t *cheat);
void cheatFinalizeData(cheat_t *cheat, u32 device, u32 dischash, u32 makefolders);

#endif //_ARMLIST_H_
