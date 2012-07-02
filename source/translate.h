/*
 * translate.j -- Translate/convert codes from one device to another.
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

#ifndef _TRANSLATE_H_
#define _TRANSLATE_H_

#include "abbtypes.h"
#include "armlist.h"
#include "cheat.h"

enum {
	DEV_AR1,
	DEV_AR2,
	DEV_ARMAX,
	DEV_CB,
	DEV_GS3,
	DEV_STD
};

extern int g_indevice;
extern int g_outdevice;

void transSetErrorSuppress(u8 val);
void transToggleErrorSuppress();
int transBatchTranslate(cheat_t *src);
char *transGetErrorText(int idx);

#endif //_TRANSLATE_H_
