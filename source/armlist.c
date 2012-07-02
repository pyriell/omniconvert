/*
 * armlist.c -- Create ARMAX .bin files (code lists)
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
 *  it into an AR MAX code list file (.bin)
 */

#include <stdlib.h>
#include <windows.h>
#include "common.h"
#include "crc32.h"
#include "armlist.h"
#include "armax.h"
#include "translate.h"

#define LIST_VERSION		0xD00DB00B
#define NUM_DIGITS_OCTET	8


int alCreateList(cheat_t *cheat, game_t *game, char *szFileName) {
	static char *listid = "PS2_CODELIST";
	u32 size=0, tsize, namesize, commentsize, crc;
	u8 *buffer, *fullbuf, *end, mcode = 0;
	list_t list;
	cheat_t *first = cheat;
	wchar_t uname[NAME_MAX_SIZE+2];
	char *defname = "Unnamed Game", comment[NAME_MAX_SIZE + 1];
	HANDLE hFile;
	DWORD dwBytesOut;

	if(strlen(game->name) < 1) strncpy(game->name, defname, NAME_MAX_SIZE);

	game->cnt = 0;
	while(cheat) {
		if(cheat->codecnt > 0 && !cheat->state){
//			printf("Cheat %04d: %s\n\%s\n%08X %08X\n", game->cnt + 1, cheat->name, cheat->comment, cheat->code[0], cheat->code[1]);
			tsize = 0;
			tsize += sizeof(u32) * 2;					//id and cnt
			if(strlen(cheat->name) < NAME_MAX_SIZE)
				namesize = strlen(cheat->name);
			else {
				namesize = NAME_MAX_SIZE;
				cheat->name[namesize] = '\0';
			}
			namesize++;
			end = strchr(cheat->comment, '\r');
			if(end > 0) {
				*end = '\0';
				commentsize = strlen(cheat->comment);
				if(commentsize > NAME_MAX_SIZE) {
					commentsize = NAME_MAX_SIZE;
					cheat->comment[commentsize] = '\0';
				}
			} else {
				if(strlen(cheat->comment) < NAME_MAX_SIZE)
					commentsize = (strlen(cheat->comment));
				else {
					commentsize = NAME_MAX_SIZE;
					cheat->comment[commentsize] = '\0';
				}
			}
			commentsize++;
			tsize += (commentsize + namesize) * sizeof(wchar_t);
			tsize += sizeof(u8) * 3;					//flags
			tsize += cheat->codecnt * sizeof(u32);				//code parts
			size += tsize;
			if(cheat->flag[FLAG_MCODE]) mcode = 1;
			if(commentsize) cheat->flag[FLAG_COMMENTS] = 1;
			game->cnt++;
		}
		cheat = cheat->nxt;
	}

	if(size == 0) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "After careful analysis, it seems no cheats are valid for AR MAX.");
		return 1;
	}
	if(!mcode) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "File cannot be created without a master code in the list.");
		return 1;
	}

	//add in the game header;
	size += sizeof(u32) * 2;					//id and cnt
	if(strlen(game->name) < NAME_MAX_SIZE)
		tsize = (strlen(game->name) + 1) * sizeof(wchar_t);	//name (UNICODE)
	else
		tsize = (NAME_MAX_SIZE + 1) * sizeof(wchar_t);
	size += tsize + sizeof(wchar_t);


	
	buffer = malloc(size + sizeof(list_t));
	if(!buffer) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate output buffer for AR MAX list");
		return 1;
	}
	memset(buffer, 0, size);
	end = buffer + size + sizeof(list_t);
	fullbuf = buffer;
	buffer += sizeof(list_t);
	//gamedata
	memcpy(buffer, &game->id, sizeof(u32));
	buffer+=sizeof(u32);
	mbstowcs(uname, game->name, NAME_MAX_SIZE);
	memcpy(buffer, uname, tsize);
	buffer+=tsize;
	buffer+=sizeof(wchar_t);
	memcpy(buffer, &game->cnt, sizeof(u32));
	buffer+=sizeof(u32);

	cheat = first;
	while(cheat && buffer < end) {
		if(cheat->codecnt > 0 && !cheat->state) {
			memcpy(buffer, &cheat->id, sizeof(u32));
			buffer+=sizeof(u32);
			tsize = (strlen(cheat->name) + 1) * sizeof(wchar_t);		//name (UNICODE)
			memset(uname, 0, sizeof(wchar_t) * (NAME_MAX_SIZE + 1));
			mbstowcs(uname, cheat->name, strlen(cheat->name));
			memcpy(buffer, uname, tsize);
			buffer+=tsize;
			tsize = (strlen(cheat->comment) + 1) * sizeof(wchar_t);		//comment (UNICODE)
			memset(uname, 0, sizeof(wchar_t) * (NAME_MAX_SIZE + 1));
			mbstowcs(uname, cheat->comment, strlen(cheat->comment));
			memcpy(buffer, uname, tsize);
			buffer+=tsize;
			buffer[0] = cheat->flag[0];
			buffer[1] = cheat->flag[1];
			buffer[2] = cheat->flag[2];
			buffer+=3;
			memcpy(buffer, &(cheat->codecnt), sizeof(u32));
			buffer+=sizeof(u32);
			memcpy(buffer, cheat->code, cheat->codecnt * sizeof(u32));
			buffer+=sizeof(u32) * cheat->codecnt;
		}
		cheat = cheat->nxt;
	}

	memset(&list, 0, sizeof(list_t));
	memcpy(&list.l_ident, listid, 12);
	list.version	= LIST_VERSION;
	list.size	= size + sizeof(u32) * 2;
	list.gamecnt	= 1;
	list.codecnt	= game->cnt;
	memcpy(fullbuf, &list, sizeof(list_t));
	buffer=fullbuf + (sizeof(list_t) - sizeof(u32) * 2);
	crc = 0xFFFFFFFF;
	int i;
	list.crc	= crc32(buffer, list.size, crc);
	memcpy(fullbuf, &list, sizeof(list_t));
	size+=sizeof(list_t);

	hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(NULL, MB_ICONERROR | MB_OK, "Could not open MAX file \"%s\"", szFileName);
		return FALSE;
	}

	WriteFile(hFile, fullbuf, size, &dwBytesOut, NULL);
	CloseHandle(hFile);
	free(fullbuf);
}
