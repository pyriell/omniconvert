/*
 *  p2m.c -- GameShark Version 5 + code save file (.p2m) crap.
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

/**
 *  Files are encrypted by seeding an MT State table with the size of the cheat data
 *  (user.dat).  Each byte of the cheat data is then XORed with a number generated
 *  by calling gs3GetMtNum().
 *
 *  "Official" P2M files consist of 5 archived "files".
 *  ROOT_ID	- Dummy entry that identifies the save.  Name = BLAZE_USA in NA region
 *  icon.sys	- PS2 Icon/save descriptor data file
 *  blaze.ico	- The save icon
 *  user.dat	- The actual cheat data, including game name
 *  user.crc	- Presumably a crc
 *
 *  Only the user.dat file serves any purpose, and the others can be excluded, except
 *  possibly the ROOT_ID, which is static anyway.
 *
 *  All the "files" are unencrypted, except user.dat.
 *
 */

#include <windows.h>
#include <malloc.h>
#include <time.h>
#include "p2m.h"
#include "gs3.h"

//MT State Table for file encryption
const char *rootid[NUM_REGIONS] = {"BLAZE_USA", "BLAZE_UK", "BLAZE_JAPAN"};  //guessing on the last two.
const char *userfilename = "user.dat";
const char *filefooter = "Cheat File";

//Terminators used for some reason
static const u32 TERM_FILE	=	0x00000000;
static const u32 TERM_GAME_NAME =	0x20000000;
static const u32 TERM_CODE_NAME =	0x40000000;
static const u32 TERM_CODE_LINE	=	0x80000000;
const char *color_prefix[NUM_COLORS] = { "{0", "{1", "{2", "{3", "{4", "{5", "{6", "{7", "{8" };

void p2mInitHeader(p2mheader_t *header, char *name) {
	header->fileid			= P2M_FILE_ID;
	header->fileversion		= P2M_FILE_VERSION;
	strncpy(header->name, name, P2M_NAME_SIZE - 1);
	header->name[P2M_NAME_SIZE - 1] = '\0';
	header->unknown 		= 0xA;
}

void p2mInitFd(p2mfd_t *fd, struct tm *currtime, const char *name, const u32 attributes) {
	memset(fd, 0, sizeof(p2mfd_t));
	fd->offset = fd->size = -1;
	fd->created.second	= currtime->tm_sec;
	fd->created.minute	= currtime->tm_min;
	fd->created.hour	= currtime->tm_hour;
	fd->created.day		= currtime->tm_mday;
	fd->created.month	= currtime->tm_mon + 1;
	fd->created.year	= currtime->tm_year + 1900;
	strcpy(fd->name, name);
	fd->attributes		= attributes;
}

int p2mCreateFile(cheat_t* cheat, game_t *game, char *szFileName, u8 doheadings) {
	u8 *buffer, *userdata, mcode = 0;
	u32 numcheats = 0, numlines = 0, size, filesize, i;
	cheat_t *first = cheat;
	p2mheader_t header;
	p2marcstat_t arcStat;
	p2mfd_t fdRoot, fdUserData;
	p2mudheader_t udHeader;
	time_t timestamp;
	struct tm *currtime;
	HANDLE hFile;
	DWORD dwBytesOut;
	
	timestamp = time(NULL);
	currtime = localtime(&timestamp);
	//initialize data structures
	p2mInitHeader(&header, game->name);
	memset(&arcStat, 0, sizeof(p2marcstat_t));
	memset(&udHeader, 0, sizeof(p2mudheader_t));
	p2mInitFd(&fdRoot, currtime, rootid[REG_USA], ATTR_DIR);
	p2mInitFd(&fdUserData, currtime, userfilename, ATTR_FILE);

	//we need two FDs
	arcStat.numfiles = P2M_NUM_FILES;
	arcStat.size = sizeof(p2mfd_t) * P2M_NUM_FILES;
	
	//total up size of data
	fdUserData.size = 0;
	while(cheat) {
		if(cheat == first) {
			if(!cheat->flag[FLAG_MCODE])
				break;
			else {
				mcode++;
				if(cheat->name[0] != GS3_COLOR_IND) PrependText(&cheat->name, color_prefix[COLOR_PURPLE], &cheat->namemax);
			}
		} else if(cheat->flag[FLAG_MCODE]){
			mcode++;
			break;
		}
		if(!cheat->state && (doheadings || cheat->codecnt > 0)) {
			if(cheat->codecnt == 0) PrependText(&cheat->name, color_prefix[COLOR_GREEN], &cheat->namemax);
			size = strlen(cheat->name);
			if(size > GS_NAME_MAX_SIZE) size = GS_NAME_MAX_SIZE;
			fdUserData.size += size * sizeof(wchar_t) + sizeof(wchar_t) + sizeof(u32) + sizeof(u16);  // strlen + term null + term + line count
			udHeader.numlines += (cheat->codecnt >> 1);
			fdUserData.size += sizeof(u32) * (cheat->codecnt + (cheat->codecnt >> 1));  		//include terminator
			udHeader.numcheats++;
		}
		cheat = cheat->nxt;
	}

	if(!mcode) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "File cannot be created without a master code in the list.");
		return 1;
	}

	if(mcode > 1) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "File cannot be created with more than one enable code");
		return 1;
	}

	if(fdUserData.size == 0 || udHeader.numcheats < 1) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "After careful analysis, it seems no cheats are valid for GameShark.");
		return 1;
	}

	//add in the game
	size = strlen(game->name);
	if(size > GS_NAME_MAX_SIZE) size = GS_NAME_MAX_SIZE;
	fdUserData.size += (size + 1) * sizeof(wchar_t) + sizeof(u16) + sizeof(u32) * 5;

	filesize = arcStat.size + fdUserData.size + sizeof(p2marcstat_t) + sizeof(p2mheader_t) + strlen(filefooter) + sizeof(u32);
	buffer = (u8 *)malloc(filesize);
	if(!buffer) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate output buffer for P2M file");
		return 1;
	}
	memset(buffer, 0, filesize);

	userdata = buffer + arcStat.size + sizeof(p2marcstat_t) + sizeof(p2mheader_t);
	udHeader.numgames	= 1;
	memcpy(userdata, &udHeader, sizeof(p2mudheader_t));
	userdata+=sizeof(p2mudheader_t);
	memcpy(userdata, &udHeader.numcheats, sizeof(u16));
	userdata+=sizeof(u16);
	mbstowcs((wchar_t*)userdata, game->name, size);
	userdata += size * sizeof(wchar_t);
	memset(userdata, 0, sizeof(wchar_t));
	userdata+=sizeof(wchar_t);
	memcpy(userdata, &TERM_GAME_NAME, sizeof(u32));
	userdata+=sizeof(u32);
	
	cheat = first;
	while(cheat) {
		if(!cheat->state && (doheadings || cheat->codecnt > 0)) {
			numlines = cheat->codecnt >> 1;
			memcpy(userdata, &numlines, sizeof(u16));
			userdata+=sizeof(u16);
			size = strlen(cheat->name);
			if(size > GS_NAME_MAX_SIZE) size = GS_NAME_MAX_SIZE;
			mbstowcs((wchar_t*)userdata, cheat->name, size);
			userdata += size * sizeof(wchar_t);
			memset(userdata, 0, sizeof(wchar_t));
			userdata+=sizeof(wchar_t);
			memcpy(userdata, &TERM_CODE_NAME, sizeof(u32));
			userdata += sizeof(u32);
			for(i = 0; i < cheat->codecnt; i+=2) {
				memcpy(userdata, &cheat->code[i], sizeof(u32));
				userdata += sizeof(u32);
				memcpy(userdata, &cheat->code[i + 1], sizeof(u32));
				userdata += sizeof(u32);
				memcpy(userdata, &TERM_CODE_LINE, sizeof(u32));
				userdata += sizeof(u32);
			}
		}
		cheat = cheat->nxt;
	}
	memcpy(userdata, &TERM_FILE, sizeof(u32));
	userdata += sizeof(u32);
	memcpy(userdata, filefooter, strlen(filefooter));
	userdata += strlen(filefooter);
	userdata = buffer + arcStat.size + sizeof(p2marcstat_t) + sizeof(p2mheader_t);
	gs3CryptFileData(userdata, fdUserData.size);
	fdUserData.crc = gs3GenCrc32(userdata, fdUserData.size);
	
	fdUserData.offset = arcStat.size + sizeof(p2marcstat_t);
	header.filesize = filesize - P2M_SIZE_EXCLUDE;
	arcStat.size = fdUserData.size;

	memcpy(buffer, &header, sizeof(p2mheader_t));
	i=sizeof(p2mheader_t);
	memcpy(&buffer[i], &arcStat, sizeof(p2marcstat_t));
	i+=sizeof(p2marcstat_t);
	memcpy(&buffer[i], &fdRoot, sizeof(p2mfd_t));
	i+=sizeof(p2mfd_t);
	memcpy(&buffer[i], &fdUserData, sizeof(p2mfd_t));
	i+=sizeof(p2mfd_t);

	hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(NULL, MB_ICONERROR | MB_OK, "Could not open p2m file \"%s\"", szFileName);
		return FALSE;
	}


	WriteFile(hFile, buffer, filesize, &dwBytesOut, NULL);
	CloseHandle(hFile);
	free(buffer);
}

/**
USER.DAT FILE STRUCTURE
0x00000000	0x04	0x000		COUNT OF GAMES IN FILE (1)
0x00000004	0x04	0x004		COUNT OF CODES IN FILE
0x00000008	0x04	0x008		COUNT OF CODE LINES IN FILE  (APPEARS UNUSED.  IT'S WRONG IN SOME OFFICIAL FILES)

GAME
0x0000000C	0x02	0x00C		COUNT OF CODES IN GAME (matches file)
0x0000000E	0x0?	0x00E		GAME NAME (UNICODE SZ) (CAN HAVE PREFIX)
		0x04			TERMINATOR FOR GAME NAME (0x20000000)
CHEAT
		0x02			COUNT OF LINES IN CODE
		0x0?			CODE NAME (UNICODE SZ) (CAN HAVE PREFIX)
		0x04			TERMINATOR FOR CODE NAME (0x40000000)
		0x04			CODE ADDR
		0x04			CODE VALUE
		0x04			TERMINATOR FOR CODE LINE (0x80000000)
		REPEAT FOR EACH LINE
		REPEAT CHEATS
		0x4			TERMINATOR FOR FILE (0)

PREFIX 0x0035007B "{5" TURNS NAME PURPLE	(Enable codes)
PREFIX 0x0033007B "{3" TURNS NAME GREEN	(Generally dividers)
PREFIX 0x0032007B "{2" TURNS NAME RED		(Notes)
*/
