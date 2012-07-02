/*
 * cbc.c -- Create CodeBreaker .cbc (Day 1) files.
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
 *  Take in a list of cheats and some data about the game, and serialize
 *  it into a Day 1 (.cbc) file.  This is pretty simple.  Most of the hard
 *  work is in misfire's crypt routines.
 */


#include <windows.h>
#include "armlist.h"
#include "cb2_crypto.h"
#include "common.h"
#include "cbc.h"

#define CB7_HDR_SIZE 64

int cbcCreateFile(cheat_t *cheat, game_t *game, char *szFileName, u8 doheadings) {
	u32 filesize = CB7_HDR_SIZE, datasize = 0, size, i;
	u8 namemax = CB7_HDR_SIZE - 1;		//assume the name can only be 63 chars
	u8 name[CB7_HDR_SIZE], *buffer, *cbcdata, mcode = 0;
	u16 numcheats = 0, numlines;
	cheat_t *first = cheat;
	HANDLE hFile;
	DWORD dwBytesOut;
	
	//first compute the size of the file
	//the header is only the game name for CB7
	memset(name, 0, CB7_HDR_SIZE);
	strncpy(name, game->name, namemax);
	name[namemax] = '\0';

	//Size of the game name for the list itself (unpadded)
	datasize += strlen(name) + 1;
	//Number of cheats in file
	datasize += sizeof(u16);
	
	//total up size of data and check that the codes will make a valid file.
	while(cheat) {
		if(cheat == first) {
			if(!cheat->flag[FLAG_MCODE])
				break;
			else {
				mcode++;
			}
		} else if(cheat->flag[FLAG_MCODE]){
			mcode++;
			break;
		}
		if(!cheat->state && (doheadings || cheat->codecnt > 0)) {
			size = strlen(cheat->name);
			if(size > namemax) size = namemax;
			datasize += size + 2 + sizeof(u16);  // strlen + term null + switch byte + line count
			datasize += sizeof(u32) * cheat->codecnt;
			numcheats++;
		}
		cheat = cheat->nxt;
	}

	/**
	  ERROR BLOCK
	*/
	if(!mcode) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "A .cbc file cannot be created without a master code as the first code in the list.");
		return 1;
	}

	if(mcode > 1) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "A .cbc file cannot be created without more than one master code in the list.");
		return 1;
	}

	if(datasize == 0 || numcheats < 1) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "After careful analysis, it seems no cheats are valid for CodeBreaker.");
		return 1;
	}

	//Add the header size to the data size, and allocate a buffer.
	filesize += datasize;
	buffer = (u8 *)malloc(filesize);
	if(!buffer) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate output buffer for CBC file");
		return 1;
	}
	
	cbcdata = buffer;
	memset(buffer, 0, filesize);
	//copy the whole thing as the header
	memcpy(buffer, name, CB7_HDR_SIZE);
	buffer += CB7_HDR_SIZE;
	//copy just the valid characters as the game name
	strcpy(buffer, name);
	buffer += strlen(name) + 1;
	memcpy(buffer, &numcheats, sizeof(u16));
	buffer += sizeof(u16);

	cheat = first;
	while(cheat) {
		if(!cheat->state && (doheadings || cheat->codecnt > 0)) {
			size = strlen(cheat->name);
			if(size > namemax) size = namemax;
			strncpy(buffer, cheat->name, size);
			buffer[size] = '\0';
			buffer += size + 1;			//skip switch
			if(cheat->codecnt == 0) *buffer = 4;
			buffer += 1;
			numlines = cheat->codecnt >> 1;
			memcpy(buffer, &numlines, sizeof(u16));
			buffer += sizeof(u16);
			for(i = 0; i < cheat->codecnt; i++) {
				memcpy(buffer, &cheat->code[i], sizeof(u32));
				buffer += sizeof(u32);
			}
		}
		cheat = cheat->nxt;
	}

	hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(NULL, MB_ICONERROR | MB_OK, "Could not open cbc file \"%s\"", szFileName);
		return FALSE;
	}

	buffer = cbcdata + CB7_HDR_SIZE;
	CBCryptFileData(buffer, datasize);

	WriteFile(hFile, cbcdata, filesize, &dwBytesOut, NULL);
	CloseHandle(hFile);
	free(cbcdata);
}
