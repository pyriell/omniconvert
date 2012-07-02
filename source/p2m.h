/*
 *  p2m.h -- GameShark Version 5 + code save file (.p2m) crap.
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

#ifndef _P2M_H_
#define _P2M_H_

#include "cheat.h"

//"P2MS"
#define P2M_FILE_ID 		0x534d3250
//"P2V2"
#define P2M_FILE_VERSION 	0x32563250

//The total size of the file in the header is less the size of the fileid and filesize
//variables.  The remaining 4 bytes difference is probably the null padding/terminator
//in the footer.
#define P2M_SIZE_EXCLUDE 	12

#define P2M_NAME_SIZE		0x38
#define P2M_FNAME_SIZE		0x20
#define P2M_NUM_FILES		2

#define NUM_REGIONS 3
enum {
	REG_USA,
	REG_EUR,
	REG_JAP
};

#define ATTR_DIR		0x00008427
#define ATTR_FILE		0x00008417

//Color switches for name strings
//There are probably more of these
enum {
	COLOR_DEFAULT,
	COLOR_BLUE,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_PURPLE,
	COLOR_SKY_BLUE,
	COLOR_WHITE,
	COLOR_GREY
};

#define GS3_COLOR_IND		'{'
#define NUM_COLORS		9
#define GS_NAME_MAX_SIZE 	99


//Struct used for holding the date (64-bits);
typedef struct {
	u8		reserved; //0
	u8		second;
	u8		minute;
	u8		hour;
	u8		day;
	u8		month;
	u16		year;
} p2mdate_t;

//P2M file header
typedef struct {
	u32		fileid;
	u32		filesize;
	char	 	name[P2M_NAME_SIZE];
	u32		fileversion;
	u32		unknown;	//always contains 0xA.  Might just be LF at the tail of the version str.
} p2mheader_t;

typedef struct {
	u32		numfiles;
	u32		size;
} p2marcstat_t;

//P2M file descriptor
typedef struct {
	p2mdate_t	created;
	p2mdate_t	modified;
	u32		unknown1;  	//0
	u32		attributes;	//0x8427
	u32		unknown2[2];  	//0	
	char		name[P2M_FNAME_SIZE];
	u32		offset;		//offset relative to end of file header;
	u32		size;		//size in bytes of file;
	u32		crc;		//May not be a crc, but it doesn't seem to be checked either way
	u32		unknown3;	//0;
	u8		desc[0x30];	//Probably a longer file name or a descripton.  Always 0;
} p2mfd_t;

//Header for user.dat
typedef struct {
	u32		numgames;
	u32		numcheats;
	u32		numlines;
} p2mudheader_t;

int p2mCreateFile(cheat_t* cheat, game_t *game, char *szFileName, u8 doheadings);

#endif // _P2M_H_
