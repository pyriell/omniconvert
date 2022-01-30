/*
 * common.c -- common, useful functions.
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

/*
 *  Various useful routines that serve multiple purposes.
 */

#include <malloc.h>
#include <stdio.h>
#include <windows.h>
#include "abbtypes.h"
#include "common.h"

int IsHexStr(const char *s) {
	while (*s) {
		if (!isxdigit(*s++)) return 0;
	}

	return 1;
}

int IsNumStr(const char *s) {
	while (*s) {
		if (!isdigit(*s++)) return 0;
	}

	return 1;
}

int IsEmptyStr(const char *s)
{
	while (*s) {
		if (isgraph(*s++)) return 0;
	}

	return 1;
}

int MsgBox(HWND hwnd, UINT uType, const char *szFormat, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, szFormat);
	vsprintf(buf, szFormat, arg);
	va_end(arg);

	return MessageBox(hwnd, buf, APPNAME, uType);
}

u32 swapbytes(unsigned int val) {
	return (val << 24) | ((val << 8) & 0xFF0000) | ((val >> 8) & 0xFF00) | (val >> 24);
}

int AppendText(char **dest, const char *src, u32 *textmax) {
	int slen = strlen(*dest);
	int len = strlen(src) + slen + 3;
	if(len >= *textmax) {
		*textmax+=len;
		*dest = realloc(*dest, *textmax);
		if(!*dest) {
			MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate %i bytes for text", *textmax);
			exit(1);
		}
		(*dest)[slen+1] = '\0'; //ensure null termination
	}
	strcat(*dest, src);
	return 0;
}

int PrependText(char **dest, const char *src, u32 *textmax) {
	char *tmp;
	int slen = strlen(*dest);
	int len = strlen(src) + slen + 3;
	if(len >= *textmax) {
		*textmax+=len;
		*dest = (char *)realloc(*dest, *textmax * sizeof(char));
		if(!*dest) {
			MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate %i bytes for text", *textmax);
			exit(1);
		}
		(*dest)[slen+1] = '\0'; //ensure null termination
	}
	tmp = (char *)malloc(sizeof(char) * (slen + 1));
	strcpy(tmp, *dest);
	strcpy(*dest, src);
	strcat(*dest, tmp);
	free(tmp);
}

int AppendNewLine(char **dest, int num, u32 *textmax) {
	int ret, i;
	for(i = 0; i < num; i++) {
		ret = AppendText(dest, NEWLINE, textmax);
		if(ret) break;
	}
	return ret;
}
