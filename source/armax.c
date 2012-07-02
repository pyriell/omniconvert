/* armax.c -- Action Replay Max code routines
 *
 * Copyright notice for this file:
 *  Copyright (C) 2003-2004 Parasyte
 *  Copyright (C) 2008 Pyriel
 *
 * This file is part of Omniconvert.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Parasyte is the source for the bulk of the code in this file.
 *  I (Pyriel) added some wrapper routines for batch conversion, the
 *  logic for generating disc hashes, folder and additional expansion changes,
 *  and improved the reading of verifier bitstrings so that the metadata
 *  in codes is decrypted and encrypted properly.
 */

#include "armax.h"
#include <windows.h>
#include <stdio.h>

#define NUM_CHARS_ARM_CODE	15

//The disc hash uses only the first 256 KB of the game ELF
#define ELF_CRC_SIZE		0x40000

enum {
	ERR_NODRIVES = 1,
	ERR_NOCNF,
	ERR_NOPARM,
	ERR_NOGAMEID,
	ERR_INVGAMEID,
	ERR_NOELF
};

u32 genseeds[0x20]; //80236C58
//u8 globalvar=0; //8009FC50
//u8 globalvar2=0; //8009FC54


const u8 bitstringlen[0x08] = {
	0x06, 0x0A, 0x0C, 0x13, 0x13, 0x08, 0x07, 0x20,
};

const u8 gentable0[0x38] = { //80035598
	0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01,
	0x3A, 0x32, 0x2A, 0x22, 0x1A, 0x12, 0x0A, 0x02,
	0x3B, 0x33, 0x2B, 0x23, 0x1B, 0x13, 0x0B, 0x03,
	0x3C, 0x34, 0x2C, 0x24, 0x3F, 0x37, 0x2F, 0x27,
	0x1F, 0x17, 0x0F, 0x07, 0x3E, 0x36, 0x2E, 0x26,
	0x1E, 0x16, 0x0E, 0x06, 0x3D, 0x35, 0x2D, 0x25,
	0x1D, 0x15, 0x0D, 0x05, 0x1C, 0x14, 0x0C, 0x04,
};
const u8 gentable1[0x08] = { //80035610
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
};
const u8 gentable2[0x10] = { //800355D0
	0x01, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
	0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1C,
};
const u8 gentable3[0x30] = { //800355E0
	0x0E, 0x11, 0x0B, 0x18, 0x01, 0x05, 0x03, 0x1C,
	0x0F, 0x06, 0x15, 0x0A, 0x17, 0x13, 0x0C, 0x04,
	0x1A, 0x08, 0x10, 0x07, 0x1B, 0x14, 0x0D, 0x02,
	0x29, 0x34, 0x1F, 0x25, 0x2F, 0x37, 0x1E, 0x28,
	0x33, 0x2D, 0x21, 0x30, 0x2C, 0x31, 0x27, 0x38,
	0x22, 0x35, 0x2E, 0x2A, 0x32, 0x24, 0x1D, 0x20,
};

const u16 crctable0[0x10] = { //80035630
	0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xA50A, 0xB58B, 0xC60C, 0xD68D, 0xE70E, 0xF78F,
};
const u16 crctable1[0x10] = { //80035650
	0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
	0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
};

const u8 gensubtable[0x08] = { //00248668
	0x1D, 0x2E, 0x7A, 0x85, 0x3F, 0xAB, 0xD9, 0x46,
};


const u32 table0[0x40] = { //80034D98
	0x01010400, 0x00000000, 0x00010000, 0x01010404, 0x01010004, 0x00010404, 0x00000004, 0x00010000,
	0x00000400, 0x01010400, 0x01010404, 0x00000400, 0x01000404, 0x01010004, 0x01000000, 0x00000004,
	0x00000404, 0x01000400, 0x01000400, 0x00010400, 0x00010400, 0x01010000, 0x01010000, 0x01000404,
	0x00010004, 0x01000004, 0x01000004, 0x00010004, 0x00000000, 0x00000404, 0x00010404, 0x01000000,
	0x00010000, 0x01010404, 0x00000004, 0x01010000, 0x01010400, 0x01000000, 0x01000000, 0x00000400,
	0x01010004, 0x00010000, 0x00010400, 0x01000004, 0x00000400, 0x00000004, 0x01000404, 0x00010404,
	0x01010404, 0x00010004, 0x01010000, 0x01000404, 0x01000004, 0x00000404, 0x00010404, 0x01010400,
	0x00000404, 0x01000400, 0x01000400, 0x00000000, 0x00010004, 0x00010400, 0x00000000, 0x01010004,
};
const u32 table1[0x40] = { //80034E98
	0x80108020, 0x80008000, 0x00008000, 0x00108020, 0x00100000, 0x00000020, 0x80100020, 0x80008020,
	0x80000020, 0x80108020, 0x80108000, 0x80000000, 0x80008000, 0x00100000, 0x00000020, 0x80100020,
	0x00108000, 0x00100020, 0x80008020, 0x00000000, 0x80000000, 0x00008000, 0x00108020, 0x80100000,
	0x00100020, 0x80000020, 0x00000000, 0x00108000, 0x00008020, 0x80108000, 0x80100000, 0x00008020,
	0x00000000, 0x00108020, 0x80100020, 0x00100000, 0x80008020, 0x80100000, 0x80108000, 0x00008000,
	0x80100000, 0x80008000, 0x00000020, 0x80108020, 0x00108020, 0x00000020, 0x00008000, 0x80000000,
	0x00008020, 0x80108000, 0x00100000, 0x80000020, 0x00100020, 0x80008020, 0x80000020, 0x00100020,
	0x00108000, 0x00000000, 0x80008000, 0x00008020, 0x80000000, 0x80100020, 0x80108020, 0x00108000,
};
const u32 table2[0x40] = { //80034F98
	0x00000208, 0x08020200, 0x00000000, 0x08020008, 0x08000200, 0x00000000, 0x00020208, 0x08000200,
	0x00020008, 0x08000008, 0x08000008, 0x00020000, 0x08020208, 0x00020008, 0x08020000, 0x00000208,
	0x08000000, 0x00000008, 0x08020200, 0x00000200, 0x00020200, 0x08020000, 0x08020008, 0x00020208,
	0x08000208, 0x00020200, 0x00020000, 0x08000208, 0x00000008, 0x08020208, 0x00000200, 0x08000000,
	0x08020200, 0x08000000, 0x00020008, 0x00000208, 0x00020000, 0x08020200, 0x08000200, 0x00000000,
	0x00000200, 0x00020008, 0x08020208, 0x08000200, 0x08000008, 0x00000200, 0x00000000, 0x08020008,
	0x08000208, 0x00020000, 0x08000000, 0x08020208, 0x00000008, 0x00020208, 0x00020200, 0x08000008,
	0x08020000, 0x08000208, 0x00000208, 0x08020000, 0x00020208, 0x00000008, 0x08020008, 0x00020200,
};
const u32 table3[0x40] = { //80035098
	0x00802001, 0x00002081, 0x00002081, 0x00000080, 0x00802080, 0x00800081, 0x00800001, 0x00002001,
	0x00000000, 0x00802000, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00800080, 0x00800001,
	0x00000001, 0x00002000, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002001, 0x00002080,
	0x00800081, 0x00000001, 0x00002080, 0x00800080, 0x00002000, 0x00802080, 0x00802081, 0x00000081,
	0x00800080, 0x00800001, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00000000, 0x00802000,
	0x00002080, 0x00800080, 0x00800081, 0x00000001, 0x00802001, 0x00002081, 0x00002081, 0x00000080,
	0x00802081, 0x00000081, 0x00000001, 0x00002000, 0x00800001, 0x00002001, 0x00802080, 0x00800081,
	0x00002001, 0x00002080, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002000, 0x00802080,
};
const u32 table4[0x40] = { //80035198
	0x00000100, 0x02080100, 0x02080000, 0x42000100, 0x00080000, 0x00000100, 0x40000000, 0x02080000,
	0x40080100, 0x00080000, 0x02000100, 0x40080100, 0x42000100, 0x42080000, 0x00080100, 0x40000000,
	0x02000000, 0x40080000, 0x40080000, 0x00000000, 0x40000100, 0x42080100, 0x42080100, 0x02000100,
	0x42080000, 0x40000100, 0x00000000, 0x42000000, 0x02080100, 0x02000000, 0x42000000, 0x00080100,
	0x00080000, 0x42000100, 0x00000100, 0x02000000, 0x40000000, 0x02080000, 0x42000100, 0x40080100,
	0x02000100, 0x40000000, 0x42080000, 0x02080100, 0x40080100, 0x00000100, 0x02000000, 0x42080000,
	0x42080100, 0x00080100, 0x42000000, 0x42080100, 0x02080000, 0x00000000, 0x40080000, 0x42000000,
	0x00080100, 0x02000100, 0x40000100, 0x00080000, 0x00000000, 0x40080000, 0x02080100, 0x40000100,
};
const u32 table5[0x40] = { //80035298
	0x20000010, 0x20400000, 0x00004000, 0x20404010, 0x20400000, 0x00000010, 0x20404010, 0x00400000,
	0x20004000, 0x00404010, 0x00400000, 0x20000010, 0x00400010, 0x20004000, 0x20000000, 0x00004010,
	0x00000000, 0x00400010, 0x20004010, 0x00004000, 0x00404000, 0x20004010, 0x00000010, 0x20400010,
	0x20400010, 0x00000000, 0x00404010, 0x20404000, 0x00004010, 0x00404000, 0x20404000, 0x20000000,
	0x20004000, 0x00000010, 0x20400010, 0x00404000, 0x20404010, 0x00400000, 0x00004010, 0x20000010,
	0x00400000, 0x20004000, 0x20000000, 0x00004010, 0x20000010, 0x20404010, 0x00404000, 0x20400000,
	0x00404010, 0x20404000, 0x00000000, 0x20400010, 0x00000010, 0x00004000, 0x20400000, 0x00404010,
	0x00004000, 0x00400010, 0x20004010, 0x00000000, 0x20404000, 0x20000000, 0x00400010, 0x20004010,
};
const u32 table6[0x40] = { //80035398
	0x00200000, 0x04200002, 0x04000802, 0x00000000, 0x00000800, 0x04000802, 0x00200802, 0x04200800,
	0x04200802, 0x00200000, 0x00000000, 0x04000002, 0x00000002, 0x04000000, 0x04200002, 0x00000802,
	0x04000800, 0x00200802, 0x00200002, 0x04000800, 0x04000002, 0x04200000, 0x04200800, 0x00200002,
	0x04200000, 0x00000800, 0x00000802, 0x04200802, 0x00200800, 0x00000002, 0x04000000, 0x00200800,
	0x04000000, 0x00200800, 0x00200000, 0x04000802, 0x04000802, 0x04200002, 0x04200002, 0x00000002,
	0x00200002, 0x04000000, 0x04000800, 0x00200000, 0x04200800, 0x00000802, 0x00200802, 0x04200800,
	0x00000802, 0x04000002, 0x04200802, 0x04200000, 0x00200800, 0x00000000, 0x00000002, 0x04200802,
	0x00000000, 0x00200802, 0x04200000, 0x00000800, 0x04000002, 0x04000800, 0x00000800, 0x00200002,
};
const u32 table7[0x40] = { //80035498
	0x10001040, 0x00001000, 0x00040000, 0x10041040, 0x10000000, 0x10001040, 0x00000040, 0x10000000,
	0x00040040, 0x10040000, 0x10041040, 0x00041000, 0x10041000, 0x00041040, 0x00001000, 0x00000040,
	0x10040000, 0x10000040, 0x10001000, 0x00001040, 0x00041000, 0x00040040, 0x10040040, 0x10041000,
	0x00001040, 0x00000000, 0x00000000, 0x10040040, 0x10000040, 0x10001000, 0x00041040, 0x00040000,
	0x00041040, 0x00040000, 0x10041000, 0x00001000, 0x00000040, 0x10040040, 0x00001000, 0x00041040,
	0x10001000, 0x00000040, 0x10000040, 0x10040000, 0x10040040, 0x10000000, 0x00040000, 0x10001040,
	0x00000000, 0x10041040, 0x00040040, 0x10000040, 0x10040000, 0x10001000, 0x10001040, 0x00000000,
	0x10041040, 0x00041000, 0x00041000, 0x00001040, 0x00001040, 0x00040040, 0x10000000, 0x10041000,
};

//filter for valid AR MAX characters.
const char filter[33] = "0123456789ABCDEFGHJKMNPQRTUVWXYZ";
#define GETVAL(a,b) (strchr(a,b) - a)

int MaxRemoveDashes(char *dest, const char *src) {
	int i, chr = 0;
	if(strlen(src) > NUM_CHARS_ARM_CODE) return 0;
	for(i = 0; i < NUM_CHARS_ARM_CODE; i++) {
		if(src[i] != '-') dest[chr++] = src[i];
	}
	return 1;
}

int IsArMaxStr(const char *s) {
	int len = strlen(s), i;
	if(len != NUM_CHARS_ARM_CODE) return 0;
	for(i = 0; i < NUM_CHARS_ARM_CODE; i++) {
		if(i == 4 || i == 9) {
			if(s[i] != '-') return 0; else i++;
		}
		if(!strchr(filter, s[i])) return 0;
	}
	return 1;
}

void armMakeVerifier(u32 *code, u32 num, u32 ver[2], u32 gameid, u8 region) {
	int j;
	u8 mcode = 0;
	for (j = 0; j < num; j+=2) if ((code[j] >> 25) == 0x62) mcode=1;
	ver[0] = ((gameid << 15)|(rand()%0x8000));
	ver[1] = (((rand()%0x10)<<28)|0x00800000|(mcode<<27)|(region<<24));
}

void armEnableExpansion(cheat_t *cheat) {
	if(cheat->codecnt < 2) return;
	if(cheat->code[1] & 0x00800000) cheat->code[1] ^= 0x00800000;
}

void armMakeFolder(cheat_t *cheat, u32 gameid, u8 region) {
	u32 tmp = ((rand() % 0x10) << 28) | (region << 24) | (FLAGS_FOLDER) | EXPANSION_DATA_FOLDER;
	cheatPrependOctet(cheat, tmp);
	tmp = (gameid << 15) | (rand() % 0x8000);
	cheatPrependOctet(cheat, tmp);
}

/*
 * Using the drive letter passed, retrieve the necessary files from the PS2 disc
 * and generate and auto-recognition hash for it.
 */
int armMakeDiscHash(u32 *hash, HWND hwnd, char drive) {
	int len, i;
	DWORD read;
	HANDLE hFile;
	LRESULT idx;
	char filename[MAX_PATH + 1];
	char tmp[MAX_PATH + 1];
	char *buffer, *elf, *end;
	const char *cdrom = "cdrom0:\\";
	u32 crc_sys = 0xFFFFFFFF, crc_elf = 0xFFFFFFFF;
	*hash = 0;

	//Find the files and CRC them.
	filename[0] = drive;
	filename[1] = '\0';
	strcat(filename, ":\\SYSTEM.CNF");
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, 0, NULL);
	if(hFile == INVALID_HANDLE_VALUE) {
		MessageBox(hwnd, "Could not open SYSTEM.CNF file.  Disc is probably not a PS2 game.",
				"No System File", MB_ICONERROR | MB_OK);
		return ERR_NOCNF;
	}
	
	//Could just be hard-coded length of 256
	len = GetFileSize(hFile, NULL);
	buffer = (char *)malloc(len);
	ReadFile(hFile, buffer, len, &read, NULL);
	CloseHandle(hFile);
	crc_sys = crc32((u8*)buffer, len, crc_sys);
	elf = strstr(buffer, cdrom);
	elf += strlen(cdrom);
	if(!elf) {
		MessageBox(hwnd, "Could not find boot parameter in SYSTEM.CNF file.",
				"No Boot Param", MB_ICONERROR | MB_OK);
		return ERR_NOPARM;
	}
	end = strstr(elf, ";1");
	if(!end) {
		MessageBox(hwnd, "Could not find end of boot parameter in SYSTEM.CNF file.",
				"Invalid Boot Param", MB_ICONERROR | MB_OK);
		return ERR_NOPARM;
	}
	end[0] = '\0';
	filename[0] = drive;
	filename[1] = '\0';
	strcat(filename, ":\\");
	strcat(filename, elf);
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, 0, NULL);
	if(hFile == INVALID_HANDLE_VALUE) {
		MessageBox(hwnd, "Could not open ELF file indicated by SYSTEM.CNF",
			"No System File", MB_ICONERROR | MB_OK);
		return ERR_NOELF;
	}
	buffer = (char *)realloc(buffer, ELF_CRC_SIZE);
	ReadFile(hFile, buffer, ELF_CRC_SIZE, &read, NULL);
	CloseHandle(hFile);
	crc_elf = crc32((u8*)buffer, read, crc_elf);
	*hash = crc_elf ^ crc_sys;

	free(buffer);
	return 0;
}

void generateseeds(u32 *seeds, const u8 *seedtable, u8 doreverse) { //800102E0
	int i,j;
	u32 tmp3;
	u8 array0[0x38],array1[0x38],array2[0x08];
	u8 tmp,tmp2;

	i = 0;
	while (i < 0x38) {
		tmp = (gentable0[i] - 1);
		array0[i++] = ((u32)(0-(seedtable[tmp>>3] & gentable1[tmp&7])) >> 31);
	}

	i = 0;
	while (i < 0x10) {
		memset(array2,0,8);
		tmp2 = gentable2[i];

		for (j = 0; j < 0x38; j++) {
			tmp = (tmp2+j);

			if (j > 0x1B) {
				if (tmp > 0x37) tmp-=0x1C;
			}
			else if (tmp > 0x1B) tmp-=0x1C;

			array1[j] = array0[tmp];
		}
		for (j = 0; j < 0x30; j++) {
			if (!array1[gentable3[j]-1]) continue;
			tmp = (((j*0x2AAB)>>16) - (j>>0x1F));
			array2[tmp] |= (gentable1[j-(tmp*6)]>>2);
		}
		seeds[i<<1] = ((array2[0]<<24)|(array2[2]<<16)|(array2[4]<<8)|array2[6]);
		seeds[(i<<1)+1] = ((array2[1]<<24)|(array2[3]<<16)|(array2[5]<<8)|array2[7]);
		i++;
	}

	if (!doreverse) {
		j = 0x1F;
		for (i = 0; i < 16; i+=2) {
			tmp3 = seeds[i];
			seeds[i] = seeds[j-1];
			seeds[j-1] = tmp3;

			tmp3 = seeds[i+1];
			seeds[i+1] = seeds[j];
			seeds[j] = tmp3;
			j-=2;
		}
	}
}

u32 rotate_left(u32 value, u8 rotate) { //8001054C
	return ((value << rotate) | (value >> (0x20 - rotate)));
}

u32 rotate_right(u32 value, u8 rotate) { //80010554
	return ((value >> rotate) | (value << (0x20 - rotate)));
}

u32 byteswap(u32 val) { //800107E0
	return ((val << 24) | ((val << 8) & 0x00FF0000) | ((val >> 8) & 0x0000FF00) | (val >> 24));
}

void getcode(u32 *src, u32 *addr, u32 *val) { //80010800
	*addr = byteswap(src[0]);
	*val = byteswap(src[1]);
}

void setcode(u32 *dst, u32 addr, u32 val) { //80010868
	dst[0] = byteswap(addr);
	dst[1] = byteswap(val);
}

u16 gencrc16(u32 *codes, u16 size) { //80010AE8
	u16 ret=0;
	u8 tmp=0,tmp2;
	int i;

	if (size > 0) {
		while (tmp < size) {
			for (i = 0; i < 4; i++) {
				tmp2 = ((codes[tmp] >> (i<<3))^ret);
				ret = ((crctable0[(tmp2>>4)&0x0F]^crctable1[tmp2&0x0F])^(ret>>8));
			}
			tmp++;
		}
	}
	return ret;
}

u8 verifycode(u32 *codes, u16 size) { //80010B70
	u16 tmp;

	tmp = gencrc16(codes,size);
	return (((tmp>>12)^(tmp>>8)^(tmp>>4)^tmp)&0x0F);
}

void buildseeds() { //80010BAC
	generateseeds(genseeds,gensubtable,0);
}

void unscramble1(u32 *addr, u32 *val) {
	u32 tmp;

	*val = rotate_left(*val,4);

	tmp = ((*addr^*val)&0xF0F0F0F0);
	*addr ^= tmp;
	*val = rotate_right((*val^tmp),0x14);

	tmp = ((*addr^*val)&0xFFFF0000);
	*addr ^= tmp;
	*val = rotate_right((*val^tmp),0x12);

	tmp = ((*addr^*val)&0x33333333);
	*addr ^= tmp;
	*val = rotate_right((*val^tmp),6);

	tmp = ((*addr^*val)&0x00FF00FF);
	*addr ^= tmp;
	*val = rotate_left((*val^tmp),9);

	tmp = ((*addr^*val)&0xAAAAAAAA);
	*addr = rotate_left((*addr^tmp),1);
	*val ^= tmp;
}

void unscramble2(u32 *addr, u32 *val) {
	u32 tmp;

	*val = rotate_right(*val,1);

	tmp = ((*addr^*val)&0xAAAAAAAA);
	*val ^= tmp;
	*addr = rotate_right((*addr^tmp),9);

	tmp = ((*addr^*val)&0x00FF00FF);
	*val ^= tmp;
	*addr = rotate_left((*addr^tmp),6);

	tmp = ((*addr^*val)&0x33333333);
	*val ^= tmp;
	*addr = rotate_left((*addr^tmp),0x12);

	tmp = ((*addr^*val)&0xFFFF0000);
	*val ^= tmp;
	*addr = rotate_left((*addr^tmp),0x14);

	tmp = ((*addr^*val)&0xF0F0F0F0);
	*val ^= tmp;
	*addr = rotate_right((*addr^tmp),4);
}

void decryptcode(u32 *seeds, u32 *code) {
	u32 addr,val;
	u32 tmp,tmp2;
	int i=0;

	getcode(code,&addr,&val);
	unscramble1(&addr,&val);
	while (i < 32) {
		tmp = (rotate_right(val,4)^seeds[i++]);
		tmp2 = (val^seeds[i++]);
		addr ^= (table6[tmp&0x3F]^table4[(tmp>>8)&0x3F]^table2[(tmp>>16)&0x3F]^table0[(tmp>>24)&0x3F]^table7[tmp2&0x3F]^table5[(tmp2>>8)&0x3F]^table3[(tmp2>>16)&0x3F]^table1[(tmp2>>24)&0x3F]);

		tmp = (rotate_right(addr,4)^seeds[i++]);
		tmp2 = (addr^seeds[i++]);
		val ^= (table6[tmp&0x3F]^table4[(tmp>>8)&0x3F]^table2[(tmp>>16)&0x3F]^table0[(tmp>>24)&0x3F]^table7[tmp2&0x3F]^table5[(tmp2>>8)&0x3F]^table3[(tmp2>>16)&0x3F]^table1[(tmp2>>24)&0x3F]);
	}
	unscramble2(&addr,&val);
	setcode(code,val,addr);
}

u8 getbitstring(u32 *ctrl, u32 *out, u8 len) {
	u32 *ptr=(u32*)(ctrl[0]+(ctrl[1]<<2));

	*out = 0;
	while (len--) {
		if (ctrl[2] > 0x1F) {
			ctrl[2] = 0;
			ctrl[1]++;
			ptr = (u32*)(ctrl[0]+(ctrl[1]<<2));
		}
		if (ctrl[1] >= ctrl[3]) return 0;
		*out = ((*out<<1) | ((*ptr >> (0x1F-ctrl[2])) & 1));
		ctrl[2]++;
	}
	return 1;
}

u8 batchdecrypt(u32 *codes, u16 size) {
	u32 tmp,tmp2,*ptr=codes;
	u32 tmparray[4] = { 0 },tmparray2[8] = { 0 };

	tmp = (size >> 1);
	while (tmp--) {
		decryptcode(genseeds,ptr);
		ptr+=2;
	}

	tmparray[0] = (u32)codes;
	tmparray[1] = 0;
	tmparray[2] = 4; //skip crc
	tmparray[3] = size;
	getbitstring(tmparray,tmparray2+1,13); //game id
	getbitstring(tmparray,tmparray2+2,19); //code id
	getbitstring(tmparray,tmparray2+3,1); //master code
	getbitstring(tmparray,tmparray2+4,1); //unknown
	getbitstring(tmparray,tmparray2+5,2); //region

	//grab gameid and region from the last decrypted code
	g_gameid = tmparray2[1];
	g_region = tmparray2[5];

	tmp = codes[0];
	codes[0] &= 0x0FFFFFFF;
	if ((tmp>>28) != verifycode(codes,size)) return 0;

	return 1;
}

u8 alphatobin(char alpha[][14], u32 *dst, int size) {
	int i,j=0,k;
	u32 bin[2];
	u8 ret=0,parity;

	while (size) {
		bin[0]=0;
		for (i = 0; i < 6; i++) {
			bin[0] |= (GETVAL(filter,alpha[j>>1][i]) << (((5-i)*5)+2));
		}
		bin[0] |= (GETVAL(filter,alpha[j>>1][6]) >> 3);
		dst[j++] = bin[0];

		bin[1]=0;
		for (i = 0; i < 6; i++) {
			bin[1] |= (GETVAL(filter,alpha[j>>1][i+6]) << (((5-i)*5)+4));
		}
		bin[1] |= (GETVAL(filter,alpha[j>>1][12]) >> 1);
		dst[j++] = bin[1];

		//verify parity bit
		k=0;
		parity=0;
		for (i = 0; i < 64; i++) {
			if (i == 32) k++;
			parity ^= (bin[k] >> (i-(k<<5)));
		}
		if ((parity&1) != (GETVAL(filter,alpha[(j-2)>>1][12])&1)) ret=1;

		size--;
	}

	return ret;
}

void bintoalpha(char *dst, u32 *src, int index) {
	int i,j=0,k;
	u8 parity=0;

	//convert hex directly to alpha-numeric equivelant
	for (i = 0; i < 6; i++) {
		if (i == 4) {
			dst[i] = '-';
			j++;
		}
		dst[i+j] = filter[(src[index] >> (((5-i)*5)+2)) & 0x1F];
	}
	dst[i+j] = filter[((src[index]<<3)|(src[index+1]>>29))&0x1F];
	j+=6;
	for (i = 1; i < 6; i++) {
		if (i == 2) {
			dst[i+j] = '-';
			j++;
		}
		dst[i+j] = filter[(src[index+1] >> (((5-i)*5)+4)) & 0x1F];
	}
	j+=i;

	//set parity bit
	k=0;
	for (i = 0; i < 64; i++) {
		if (i == 32) k++;
		parity ^= (src[index+k] >> (i-(k<<5)));
	}

	dst[j] = filter[((src[index+1]<<1)&0x1E)|(parity&1)];
	dst[j+1] = '\0';
}

void scramble1(u32 *addr, u32 *val) {
	u32 tmp;

	*addr = rotate_left(*addr,4);

	tmp = ((*addr^*val)&0xF0F0F0F0);
	*val ^= tmp;
	*addr = rotate_right((*addr^tmp),0x14);

	tmp = ((*addr^*val)&0xFFFF0000);
	*val ^= tmp;
	*addr = rotate_right((*addr^tmp),0x12);

	tmp = ((*addr^*val)&0x33333333);
	*val ^= tmp;
	*addr = rotate_right((*addr^tmp),6);

	tmp = ((*addr^*val)&0x00FF00FF);
	*val ^= tmp;
	*addr = rotate_left((*addr^tmp),9);

	tmp = ((*addr^*val)&0xAAAAAAAA);
	*val = rotate_left((*val^tmp),1);
	*addr ^= tmp;
}

void scramble2(u32 *addr, u32 *val) {
	u32 tmp;

	*addr = rotate_right(*addr,1);

	tmp = ((*addr^*val)&0xAAAAAAAA);
	*addr ^= tmp;
	*val = rotate_right((*val^tmp),9);

	tmp = ((*addr^*val)&0x00FF00FF);
	*addr ^= tmp;
	*val = rotate_left((*val^tmp),6);

	tmp = ((*addr^*val)&0x33333333);
	*addr ^= tmp;
	*val = rotate_left((*val^tmp),0x12);

	tmp = ((*addr^*val)&0xFFFF0000);
	*addr ^= tmp;
	*val = rotate_left((*val^tmp),0x14);

	tmp = ((*addr^*val)&0xF0F0F0F0);
	*addr ^= tmp;
	*val = rotate_right((*val^tmp),4);
}

void encryptcode(u32 *seeds, u32 *code) {
	u32 addr,val;
	u32 tmp,tmp2;
	int i=31;

	getcode(code,&val,&addr);
	scramble1(&addr,&val);
	while (i >= 0) {
		tmp2 = (addr^seeds[i--]);
		tmp = (rotate_right(addr,4)^seeds[i--]);
		val ^= (table6[tmp&0x3F]^table4[(tmp>>8)&0x3F]^table2[(tmp>>16)&0x3F]^table0[(tmp>>24)&0x3F]^table7[tmp2&0x3F]^table5[(tmp2>>8)&0x3F]^table3[(tmp2>>16)&0x3F]^table1[(tmp2>>24)&0x3F]);

		tmp2 = (val^seeds[i--]);
		tmp = (rotate_right(val,4)^seeds[i--]);
		addr ^= (table6[tmp&0x3F]^table4[(tmp>>8)&0x3F]^table2[(tmp>>16)&0x3F]^table0[(tmp>>24)&0x3F]^table7[tmp2&0x3F]^table5[(tmp2>>8)&0x3F]^table3[(tmp2>>16)&0x3F]^table1[(tmp2>>24)&0x3F]);
	}
	scramble2(&addr,&val);
	setcode(code,addr,val);
}

u8 batchencrypt(u32 *codes, u16 size) {
	u32 tmp,*ptr=codes;

	if (codes[0]>>28) return 0;

	codes[0] |= (verifycode(codes,size)<<28);

	tmp = (size >> 1);
	while (tmp--) {
		encryptcode(genseeds,ptr);
		ptr+=2;
	}

	return 1;
}

/*
 * Scan the verifier bit string and determine how many
 * code lines it occupies.
 */
s16 armReadVerifier(u32 *code, u32 size) {
	u32 ctrl[4] = { 0 }, term, exp_size_ind, lines = 1;
	u8 ret;
	int bits = 0;
	//Static array for expansion sizes
	//6:?, 10:?, 12:?, 19:folder content, 8:folder, 7:?, 32:disc hashes, maybe others
	static const u8 exp_size[8] = { 6, 10, 12, 19, 19, 8, 7, 32 };
	
	//setup initial controll array
	ctrl[0] = (u32)code;
	ctrl[1] = 1;			//skip first word
	ctrl[2] = 8;			//skip first 8 bits
	ctrl[3] = size;

	//see if any used bits remain
	ret = getbitstring(ctrl,&term,1);		 //get verifier terminator
	if(!ret) return -1;
	bits++;
	while(!term) {
		ret = getbitstring(ctrl, &exp_size_ind, 3);  //get expansion size
		if(!ret) return -1;
		bits += 3;
		//retrieve the expansion data, if possible, but we don't actually need it.
		if(ret = getbitstring(ctrl, &term, exp_size[exp_size_ind])) { 
			ret = getbitstring(ctrl, &term, 1);		//get next terminator value
		}
		if(!ret) return -1;
		bits += exp_size[exp_size_ind] + 1;
	}
	bits -= 24;			//there are 24 bits available on the first line for [term | size | expansion data]
	while(bits > 0) {
		lines++;
		bits -= 64;		//size of a line
	}
	return lines;
}

u8 armBatchDecryptFull(cheat_t *cheat, u32 ar2key) {
	u8 ret;
	u32 *code, size, lines;
	int i;

	ret = batchdecrypt(cheat->code, cheat->codecnt);
	if(!ret) return 1;
	
	if((lines = armReadVerifier(cheat->code, cheat->codecnt)) == -1) return 1;
	size = cheat->codecnt - (lines << 1);
	if(size > 0) {
		code = cheat->code + (lines << 1);
		for(i = 0; i < size; i++) {
			code[i] = byteswap(code[i]);
		}
		ar2SetSeed(ar2key);
		ar2BatchDecryptArr(code, &size);
	}
	return 0;
}

u8 armBatchEncryptFull(cheat_t *cheat, u32 ar2key) {
	u8 ret;
	u32 *code, size, lines;
	int i;
	
	if((lines = armReadVerifier(cheat->code, cheat->codecnt)) == -1) return 1;
	size = cheat->codecnt - (lines << 1);
	if(size > 0) {
		code = cheat->code + (lines << 1);
		ar2SetSeed(ar2key);
		ar2BatchEncryptArr(code, &size);
		for(i = 0; i < size; i++) {
			code[i] = byteswap(code[i]);
		}
	}
	ret = !(batchencrypt(cheat->code, cheat->codecnt));
	return ret;
}
