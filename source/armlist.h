/*
 * armlist.h -- Create ARMAX .bin files (code lists)
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

#ifndef _ARMLIST_H_
#define _ARMLIST_H_

#include "abbtypes.h"
#include "cheat.h"

typedef struct {
	char		l_ident[12];
	u32		version;
	u32		unknown;
	u32		size;
	u32		crc;
	u32		gamecnt;
	u32		codecnt;
} list_t;

int alCreateList(cheat_t *cheat, game_t *game, char *szFileName);

#endif //_ARMLIST_H_
