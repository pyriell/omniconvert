/*
 * common.h -- common, useful functions.
 *
 * Copyright (C) 2008 Pyriel
 * Copyright (C) 2006-2008 misfire
 * Copyright (C) 2003-2008 Parasyte
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

#ifndef _COMMON_H_
#define _COMMON_H_
#include "abbtypes.h"
#include <windows.h>

#define NEWLINE "\r\n"
#define APPNAME "Omniconvert"
#define VERSION "1.1.1"
#define WINDOWTITLE APPNAME " Version " VERSION

//useful routines
int IsHexStr(const char *s);
int IsNumStr(const char *s);
int IsEmptyStr(const char *s);
u32 swapbytes(unsigned int val);

//Text crap
int AppendText(char **dest, const char *src, u32 *textmax);
int PrependText(char **dest, const char *src, u32 *textmax);
int AppendNewLine(char **dest, int num, u32 *textmax);

//MsgBox
int MsgBox(HWND hwnd, UINT uType, const char *szFormat, ...);

#endif //_COMMON_H_
