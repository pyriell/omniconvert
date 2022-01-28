/*
 * cheat.c -- container for cheat data.
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

/*
 *  This is just a simple structure that contains all the data I need about
 *  a cheat.  Most importantly, it permits the cheat to expanded and shrunk
 *  at will, as is often required during conversion operations.
 */

#include <windows.h>
#include <stdio.h>
#include "cheat.h"
#include "common.h"
#include "translate.h"
#include "armax.h"


#define EXPANSION_DATA_FOLDER	0x0800
#define FLAGS_FOLDER		0x5 << 20
#define FLAGS_FOLDER_MEMBER	0x4 << 20
#define FLAGS_DISC_HASH		0x7 << 20

static u32 g_folderId;

void cheatClearFolderId() {
	g_folderId = 0;
}

cheat_t * cheatInit() {
	cheat_t *cheat = malloc(sizeof(cheat_t));
	if(!cheat) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate memory for code container");
		exit(1);
	}

	cheat->name = malloc(sizeof(char) * (NAME_MAX_SIZE + 1));
	if(!cheat->name) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate memory for cheat name");
		exit(1);
	}
	memset(cheat->name, 0, NAME_MAX_SIZE + 1);

	cheat->comment = malloc(sizeof(char) * (NAME_MAX_SIZE + 1));
	if(!cheat->name) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate memory for cheat comments");
		exit(1);
	}
	memset(cheat->comment, 0, NAME_MAX_SIZE + 1);

	cheat->commentmax = cheat->namemax = NAME_MAX_SIZE;

	cheat->flag[0] = cheat->flag[1] = cheat->flag[2] = 0;

	cheat->code		= (u32 *)malloc(sizeof(u32) * MAX_CODE_STEP);
	if(!cheat->code) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate initial memory for code");
		exit(1);
	}
	cheat->state = cheat->codecnt = 0;
	cheat->codemax = MAX_CODE_STEP;
	cheat->nxt = NULL;
	return cheat;
}

void cheatAppendOctet(cheat_t *cheat, u32 octet) {
	if(cheat->codecnt + 2 >= cheat->codemax) {
		cheat->codemax	+= MAX_CODE_STEP;
		cheat->code	=  (u32 *)realloc(cheat->code, sizeof(u32) * cheat->codemax);
		if(cheat->code == NULL) {
			MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to expand code");
			exit(1);
		}
	}
	cheat->code[cheat->codecnt] = octet;
	cheat->codecnt++;
}

void cheatAppendCodeFromText(cheat_t *cheat, char *addr, char *val) {
	u32 code;
	sscanf(addr, "%08X", &code);
	cheatAppendOctet(cheat, code);
	sscanf(val, "%08X", &code);
	cheatAppendOctet(cheat, code);
}

void cheatPrependOctet(cheat_t *cheat, u32 octet) {
	int i;
	if(cheat->codecnt < 1) {
		cheatAppendOctet(cheat, octet);
		return;
	}
	cheatAppendOctet(cheat, cheat->code[cheat->codecnt - 1]);
	for(i = cheat->codecnt - 2; i > 0; i--) {
		cheat->code[i] = cheat->code[i-1];
	}
	cheat->code[0] = octet;
}

void cheatRemoveOctets(cheat_t *cheat, int start, int count) {
	if(start >= cheat->codecnt || start < 0 || count < 1) return;
	if(start + count > cheat->codecnt) {
		cheat->codecnt -= count;
		return;
	}
	int i = start + count - 1;
	int j = cheat->codecnt - i;
	start--;
	while(j--) {
		cheat->code[start++] = cheat->code[i++];
	}
	cheat->codecnt -= count;
}

void cheatDestroy(cheat_t *cheat) {
	if(cheat) { 
		if(cheat->name) free(cheat->name);
		if(cheat->comment) free(cheat->comment);
		if(cheat->code) free(cheat->code);
		free(cheat);
	}
}

void cheatFinalizeData(cheat_t *cheat, u32 device, u32 dischash, u32 makefolders) {
	int i;
	u32 tmp;
	if(cheat->state) return;
	if(cheat->codecnt == 0) return;
	if(device == DEV_ARMAX) {
		cheat->id = ((cheat->code[0] & 0x7FFF) << 4) | ((cheat->code[1] >> 28) & 0xF);
		if((cheat->code[1] & FLAGS_FOLDER) && makefolders) g_folderId = cheat->id;
		if(g_folderId > 0 && g_folderId != cheat->id) {
			armEnableExpansion(cheat);
			cheat->code[1] |= FLAGS_FOLDER_MEMBER | (g_folderId << 1) | 1;
		}
		for(i = 2; i < cheat->codecnt; i+=2) {
			if(cheat->code[i] >> 25 == 0x62) {
				cheat->flag[FLAG_MCODE] = 1;
				if(strlen(cheat->name) == 0 && cheat->codecnt > 0) AppendText(&cheat->name, "(M) Code", &cheat->namemax);
			}
			if(cheat->code[i] == 0 && cheat->code[i + 1] >= 0x80000000 && cheat->code[i + 1] <= 0x85FFFFFF) i+=2;
		}
		if(dischash > 0 && cheat->flag[FLAG_MCODE]) {
			armEnableExpansion(cheat);
			cheat->code[1] |= FLAGS_DISC_HASH | (dischash >> 12);
			cheatPrependOctet(cheat, cheat->code[1]);		//Need to add two octets after verifier
			cheatPrependOctet(cheat, cheat->code[1]);		//just prepend the whole verifier
			cheat->code[2] = 0x00080000 | (dischash << 20);		//complete the dischash
			cheat->code[3] = 0;					//whatever
		}
	} else {
		for(i = 0; i < cheat->codecnt; i+=2) {
			tmp = cheat->code[i] >> 28;
			if(tmp == 0xF || tmp == 0x9) {
				cheat->flag[FLAG_MCODE] = 1;
				if(strlen(cheat->name) == 0 && cheat->codecnt > 0) AppendText(&cheat->name, "Enable Code", &cheat->namemax);
			}
			if ((device == DEV_AR1 && tmp == 0x8) || (device == DEV_AR2 && tmp == 0x8)) {
				cheat->flag[FLAG_MCODE] = 1;
				if(strlen(cheat->name) == 0 && cheat->codecnt > 0) AppendText(&cheat->name, "Enable Code", &cheat->namemax);
				i+=2;
			}
			if(tmp >= 4 && tmp <= 6) {
				i+=2;
			} else if(tmp == 3) {
				tmp = (cheat->code[i] >> 20) & 0xF;
				if(device == DEV_CB) tmp++;
				if(tmp > 4) i+=2;
			}
		}
	}
	if(strlen(cheat->name) == 0) AppendText(&cheat->name, "Unnamed Cheat", &cheat->namemax);
}
