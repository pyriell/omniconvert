#include <malloc.h>
#include "abbtypes.h"
#include "gs3.h"
#include <stdio.h>

//maximum size of the name in a Swap Magic Coder File
#define SCF_NAME_MAX_SIZE	100
//The buttheads "encrypt" the CRC
#define SCF_CRC_MASK		0x5FA27ED6

typedef struct {
	u32	crc32;			//CRC32 of the file.  Uses GS/AR algorithm (Adler variant?)
	u32	datasize;		//Size of the file data (excluding header)
	u32	num_games;		//Number of games in the file
	u32	num_cheats;		//Number of cheat codes in the file
	u32	num_lines;		//Number of code lines in the file
} t_scf_header;

typedef struct {
	u16	num_cheats;		//Number of cheats available for the game
	u8	name_len;		//Length of the game name
	u8	*name;			//u8 name[name_len]
	//cheats
} t_scf_game;


typedef struct {
	u16	num_lines;		//Number of lines in the cheat (64-bits each)
	u8	name_len;		//Length of the cheat name
	u8	*name;			//u8 name[name_len]
	u32	*code;			//The cheats themselves.  u32 code[num_lines * 2]
} t_scf_cheat;

inline void scfInitHeader(t_scf_header *header) {
	memset((u8 *)header, 0, sizeof(t_scf_header));
	return;
}

int scfCreateFile(cheat_t* cheat, game_t *game, char *szFileName) {
	u8		*buffer, *userdata, mcode = 0;
	u32		size, filesize;
	u16		numlines;
	int		i;
	t_scf_header	scfHeader;
	t_scf_game	scfGame;
	cheat_t		*first		= cheat;
	HANDLE		hFile;
	DWORD		dwBytesOut;

	scfInitHeader(&scfHeader);
	scfHeader.num_games++;

	while(cheat) {
		if(cheat->flag[FLAG_MCODE]){
			mcode++;
		}
		if(!cheat->state && cheat->codecnt > 0) {
			size = strlen(cheat->name);
			if(size > SCF_NAME_MAX_SIZE) {
				size = SCF_NAME_MAX_SIZE;
				cheat->name[SCF_NAME_MAX_SIZE] = '\0';
			}
			scfHeader.datasize += (size + 2) * sizeof(char) + sizeof(u16);  // strlen + term null + name length + line count
			scfHeader.num_lines += (cheat->codecnt >> 1);
			scfHeader.datasize += cheat->codecnt * sizeof(u32);
			scfHeader.num_cheats++;
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

	if(scfHeader.datasize == 0 || scfHeader.num_cheats < 1) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "After careful analysis, it seems no cheats are valid for Swap Magic Coder.");
		return 1;
	}

	//setup the game data
	size = strlen(game->name);
	if(size > SCF_NAME_MAX_SIZE) {
		size = SCF_NAME_MAX_SIZE;
		game->name[SCF_NAME_MAX_SIZE] = '\0';
	}
	scfGame.name_len	= (size + 1) * sizeof(char);
	scfGame.num_cheats	= scfHeader.num_cheats;
	//add in the game data sizes
	scfHeader.datasize 	+= (size + 2) * sizeof(char) + sizeof(u16);

	filesize = scfHeader.datasize + sizeof(t_scf_header);
	buffer = (u8 *)malloc(filesize);
	if(!buffer) {
		MsgBox(NULL, MB_OK | MB_ICONERROR, "Unable to allocate output buffer for Swap Magic Coder file");
		return 1;
	}
	memset(buffer, 0, filesize);
	
	userdata = buffer + sizeof(t_scf_header);
	memcpy(userdata, &scfGame.num_cheats, sizeof(u16));
	userdata += sizeof(u16);
	userdata[0] = scfGame.name_len;
	userdata++;
	memcpy(userdata, game->name, scfGame.name_len);
	userdata += scfGame.name_len;
	
	cheat = first;
	while(cheat) {
		if(!cheat->state && cheat->codecnt > 0) {
			numlines = cheat->codecnt >> 1;
			memcpy(userdata, &numlines, sizeof(u16));
			userdata+=sizeof(u16);
			size = strlen(cheat->name) + 1;
			userdata[0] = (u8)size;
			userdata++;
			memcpy(userdata, cheat->name, size);
			userdata += size * sizeof(char);
			for(i = 0; i < cheat->codecnt; i+=2) {
				memcpy(userdata, &cheat->code[i], sizeof(u32));
				userdata += sizeof(u32);
				memcpy(userdata, &cheat->code[i + 1], sizeof(u32));
				userdata += sizeof(u32);
			}
		}
		cheat = cheat->nxt;
	}
	
	scfHeader.crc32 = SCF_CRC_MASK ^ gs3GenCrc32(buffer + sizeof(t_scf_header), scfHeader.datasize);
	memcpy(buffer, &scfHeader, sizeof(t_scf_header));

	hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(NULL, MB_ICONERROR | MB_OK, "Could not open bin file \"%s\"", szFileName);
		return FALSE;
	}


	WriteFile(hFile, buffer, filesize, &dwBytesOut, NULL);
	CloseHandle(hFile);
	free(buffer);
}
