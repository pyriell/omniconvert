/*
 * omniconvert.c -- Main project file
 *
 * Copyright (C) 2003-2008 Pyriel
 * Copyright (C) 2006-2007 misfire
 * Copyright (C) 2003-2005 Parasyte
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
 *  This file does all the fun, user-facing tasks, like parsing input text
 *  and deciding how to deal with it.
 */

#include <windows.h>
#include <winreg.h>
#include <stdio.h>
#include <malloc.h>
#include "abbtypes.h"
#include "common.h"
#include "cheat.h"
#include "ar2.h"
#include "armax.h"
#include "cb2_crypto.h"
#include "gs3.h"
#include "translate.h"
#include "armlist.h"
#include "cbc.h"
#include "p2m.h"
#include "scf.h"
#include "resource.h"

//WINDOWS CRAP
HINSTANCE g_hInstance;
//Registry crap
#define REGSUBKEY	"Software\\CMP\\Omniconvert"
#define REGINPUT	"input"
#define REGOUTPUT	"output"
#define REGPARSE	"parser"
#define REGVERIFIER	"AR Verifiers"
#define REGLOC		"AR Region"
#define REGAR2		"AR2 Key"
#define REGGAMEID	"Game ID"
#define REGGAMENAME	"Game Name"
#define REGGS3KEY	"GS3 Key"
#define REGFOLDERS	"Make Folders"

//input selection (for saving options)
int g_input;
//input encryption format
int g_incrypt;
//input device
int g_indevice;

//output selection (for saving options);
int g_output;
//output encryption format
int g_outcrypt;
//output device
int g_outdevice;

#define DEF_INPUT ID_INPUT_UNENC_COMMON
#define DEF_OUTPUT ID_OUTPUT_UNENC_COMMON

//AR MAX verifier mode (AUTO MANUAL);
int g_verifiermode;
//AR MAX game id
u32 g_gameid;
//region
u32 g_region;
//folders (organizers)
u32 g_makefolders;
//drive letter to retrieve hash from
char g_hashdrive;

//Gameshark 3 encryption (output) key
u32 g_gs3key;
#define DEF_GS3_KEY 4

//auto verifier options
enum {
	MANUAL,
	AUTO
};

enum {
	REGION_USA,
	REGION_PAL,
	REGION_JAPAN,
	REGION_NULL
};

#define ARM_DEF_GAMEID 0x1B00
#define ARM_DEF_REGION REGION_USA

// Code parser defines
#define NUM_DIGITS_OCTET	8
#define NUM_DIGITS_CODE		(NUM_DIGITS_OCTET*2)
#define NUM_CHARS_ARM_CODE	15

typedef struct {
	char	*str;
	int	type;
} token_t;

//global for cheats
cheat_t *g_firstcheat;
game_t g_game;

//text parsing modes
enum {
	MODE_TEXT,
	MODE_CODE
};

enum {
	INCRYPT,
	OUTCRYPT
};

//text parsing options
enum {
	PARSE_OPT_SIMPLE,
	PARSE_OPT_REFORMAT,
	PARSE_OPT_STRICT
};

#define NUM_CRYPT_OPTS 19

//Encryptions
enum {
	CRYPT_AR1,
	CRYPT_AR2,
	CRYPT_ARMAX,
	CRYPT_CB,  		//includes user-provided BEEFC0DE CB7 encryptions
	CRYPT_CB7_COMMON,	//Common BEEFC0DE only.
	CRYPT_GS3,
	CRYPT_GS5,
	CRYPT_MAXRAW,
	CRYPT_RAW
};

typedef struct {
	u32	menuid;
	u32	crypt;
	u32	device;
	char	*desc;
} wincryptopt_t;

wincryptopt_t g_incryptopts[NUM_CRYPT_OPTS] = {
		{ID_INPUT_UNENC_COMMON, CRYPT_RAW, DEV_STD, "Raw/Unencrypted"},
		{ID_INPUT_UNENC_ARMAX, CRYPT_MAXRAW, DEV_ARMAX, "MAXRaw/Unencrypted"},
		{ID_INPUT_UNENC_AR1_2, CRYPT_RAW, DEV_AR2, "Raw for Action Replay V1/V2"},
		{ID_INPUT_UNENC_CB, CRYPT_RAW, DEV_CB, "Raw for CodeBreaker"},
		{ID_INPUT_UNENC_GS1_2, CRYPT_RAW, DEV_AR2, "Raw for GameShark V1/V2"},
		{ID_INPUT_UNENC_XP, CRYPT_RAW, DEV_GS3, "Raw for Xploder/GameShark V3"},
		{ID_INPUT_AR2_V1, CRYPT_AR1, DEV_AR1, "Action Replay V1"},
		{ID_INPUT_AR2_V2, CRYPT_AR2, DEV_AR2, "Action Replay V2"},
		{ID_INPUT_ARMAX, CRYPT_ARMAX, DEV_ARMAX, "Action Replay MAX"},
		{ID_INPUT_CB, CRYPT_CB, DEV_CB, "CodeBreaker V1+ (All)"},
		{ID_INPUT_CB7_COMMON, CRYPT_CB7_COMMON, DEV_CB, "CodeBreaker V7 Common"},
		{ID_INPUT_IA_V1, CRYPT_AR1, DEV_AR1, "Interact GameShark V1"},
		{ID_INPUT_IA_V2, CRYPT_AR2, DEV_AR2, "Interact GameShark V2"},
		{ID_INPUT_MC_V3, CRYPT_GS3, DEV_GS3, "MadCatz GameShark V3+"},
		{ID_INPUT_MC_V5, CRYPT_GS5, DEV_GS3, "MadCatz GameShark V5+ (w/Verifier)"},
		{ID_INPUT_XP_V1, CRYPT_CB, DEV_CB, "Xploder V1-V3"},
		{ID_INPUT_XP_V4, CRYPT_GS3, DEV_GS3, "Xploder V4"},
		{ID_INPUT_XP_V5, CRYPT_GS5, DEV_GS3, "Xploder V5"},
		{ID_INPUT_SMC, CRYPT_AR1, DEV_AR1, "Swap Magic Coder"}
};

wincryptopt_t g_outcryptopts[NUM_CRYPT_OPTS] = {
		{ID_OUTPUT_UNENC_COMMON, CRYPT_RAW, DEV_STD, "Raw/Unencrypted"},
		{ID_OUTPUT_UNENC_ARMAX, CRYPT_MAXRAW, DEV_ARMAX, "MAXRaw/Unencrypted"},
		{ID_OUTPUT_UNENC_AR1_2, CRYPT_RAW, DEV_AR2, "Raw for Action Replay V1/V2"},
		{ID_OUTPUT_UNENC_CB, CRYPT_RAW, DEV_CB, "Raw for CodeBreaker"},
		{ID_OUTPUT_UNENC_GS1_2, CRYPT_RAW, DEV_AR2, "Raw for GameShark V1/V2"},
		{ID_OUTPUT_UNENC_XP, CRYPT_RAW, DEV_GS3, "Raw for Xploder/GameShark V3"},
		{ID_OUTPUT_AR2_V1, CRYPT_AR1, DEV_AR1, "Action Replay V1"},
		{ID_OUTPUT_AR2_V2, CRYPT_AR2, DEV_AR2, "Action Replay V2"},
		{ID_OUTPUT_ARMAX, CRYPT_ARMAX, DEV_ARMAX, "Action Replay MAX"},
		{ID_OUTPUT_CB, CRYPT_CB, DEV_CB, "CodeBreaker V1+ (All)"},
		{ID_OUTPUT_CB7_COMMON, CRYPT_CB7_COMMON, DEV_CB, "CodeBreaker V7 Common"},
		{ID_OUTPUT_IA_V1, CRYPT_AR1, DEV_AR1, "Interact GameShark V1"},
		{ID_OUTPUT_IA_V2, CRYPT_AR2, DEV_AR2, "Interact GameShark V2"},
		{ID_OUTPUT_MC_V3, CRYPT_GS3, DEV_GS3, "MadCatz GameShark V3+"},
		{ID_OUTPUT_MC_V5, CRYPT_GS5, DEV_GS3, "MadCatz GameShark V5+"},
		{ID_OUTPUT_XP_V1, CRYPT_CB, DEV_CB, "Xploder V1-V3"},
		{ID_OUTPUT_XP_V4, CRYPT_GS3, DEV_GS3, "Xploder V4"},
		{ID_OUTPUT_XP_V5, CRYPT_GS5, DEV_GS3, "Xploder V5"},
		{ID_OUTPUT_SMC, CRYPT_AR1, DEV_AR1, "Swap Magic Coder"}
};

int parseopt = PARSE_OPT_REFORMAT;

// Token types
#define TOK_WILDCARD		(1 << 0)
#define TOK_STRING		(1 << 1)
#define TOK_HEXOCTET		(1 << 2)
#define TOK_CODE		(1 << 3) // Address and value, w/o space
#define TOK_CODEADDR		(1 << 4)
#define TOK_CODEVAL		(1 << 5)
#define TOK_ARMCODE		(1 << 6)
#define TOK_ENDCODE		(1 << 7)
#define TOK_ENDLINE		(1 << 8)
#define TOK_NEWLINE		(1 << 9)

#define TOK_MAX_STEP		256

//maximum text sizes;
#define TEXT_SIZE	1024*1024*1
int g_textmax;

#define BUFFER_SIZE 1024

u32 ar2seeds, armseeds = 0x04030209;


int GetTokenType(const char *str) {
	int len = strlen(str);
	if(len == NUM_DIGITS_OCTET || len == NUM_DIGITS_CODE) {
		if(IsHexStr(str)) {
			return (len == NUM_DIGITS_OCTET) ? TOK_HEXOCTET : TOK_CODE;
		}
	}
	else if (len == NUM_CHARS_ARM_CODE) {
		if(IsArMaxStr(str)) {
			return TOK_ARMCODE;
		}
	}
	return TOK_STRING;	
}

void ResetDevices(u8 mode) {
	if(!mode) {
		if(g_incrypt == CRYPT_CB7_COMMON)
			CBSetCommonV7();
		else
			CBReset();
	} else {
		if(g_outcrypt == CRYPT_CB7_COMMON)
			CBSetCommonV7();
		else
			CBReset();
	}
}
int DecryptCode(cheat_t *cheat) {
	int i, ret = 0, err = 0;

	if(cheat->codecnt == 0) return 0;
	switch(g_incrypt) {
		case CRYPT_AR1:
			ar1BatchDecrypt(cheat);
			break;
		case CRYPT_AR2:
			ar2BatchDecrypt(cheat);
			break;
		case CRYPT_ARMAX: 
			ret = armBatchDecryptFull(cheat, armseeds);
			break;
		case CRYPT_CB:
		case CRYPT_CB7_COMMON:
			CBBatchDecrypt(cheat);
			break;
		case CRYPT_GS3:
		case CRYPT_GS5:
			gs3BatchDecrypt(cheat);
			break;
		default:
			break;
	}
	return ret;
}

int TranslateCode(cheat_t *cheat) {
	int i, ret = 0, err = 0;

	if(cheat->codecnt == 0) return 0;

	if(g_indevice == DEV_ARMAX && g_outdevice != DEV_ARMAX) {
		if(g_incrypt == CRYPT_ARMAX || g_verifiermode == MANUAL) {
			s16 octets;
			octets = 2 * armReadVerifier(cheat->code, cheat->codecnt);  //get the number of verifier lines
			cheatRemoveOctets(cheat, 1, octets);
		}
	}

	if(cheat->codecnt > 1) err = transBatchTranslate(cheat);
	if(err) return err;

	if(g_outdevice == DEV_ARMAX) {
		u32 verifier[2];
		if(g_indevice != DEV_ARMAX && cheat->codecnt > 1) {
			armMakeVerifier(cheat->code, cheat->codecnt, verifier, g_gameid, g_region);
			cheatPrependOctet(cheat, verifier[1]);
			cheatPrependOctet(cheat, verifier[0]);
		} else if(g_verifiermode == AUTO && g_incrypt != CRYPT_ARMAX) {
			armMakeVerifier(cheat->code, cheat->codecnt, verifier, g_gameid, g_region);
			cheatPrependOctet(cheat, verifier[1]);
			cheatPrependOctet(cheat, verifier[0]);
		}
	} 
	return 0;
}

int EncryptCode(cheat_t *cheat) {
	int i, ret = 0, err = 0;
	
	if(cheat->codecnt == 0) return 0;
	switch(g_outcrypt) {
		case CRYPT_AR1:
			ar1BatchEncrypt(cheat);
			break;
		case CRYPT_AR2:
			ar2AddKeyCode(cheat);
			ar2BatchEncrypt(cheat);
			break;
		case CRYPT_ARMAX: 
			ret = armBatchEncryptFull(cheat, armseeds);
			break;
		case CRYPT_CB:
		case CRYPT_CB7_COMMON:
			CBBatchEncrypt(cheat);
			break;
		case CRYPT_GS3:
			gs3BatchEncrypt(cheat, g_gs3key);
			break;
		case CRYPT_GS5:
			gs3BatchEncrypt(cheat, g_gs3key);
			gs3AddVerifier(cheat);
			break;
		default:
			break;
	}
	return ret;
}

int ConvertCode(cheat_t *cheat) {
	int ret;
	if((ret = DecryptCode(cheat)) 	!= 0) return ret;
	if((ret = TranslateCode(cheat))	!= 0) return ret;
	if((ret = EncryptCode(cheat))	!= 0) return ret;
	return 0;	
}

void CleanupCheats(cheat_t *cheat) {
	cheat_t *prev;
	while(cheat) {
		prev = cheat;
		cheat = cheat->nxt;
		cheatDestroy(prev);
	}
}

void CheatToText(char **textout, int *textmax, cheat_t *cheat) {
	int err, i;
	
	if(strlen(cheat->name)) {
		AppendText(textout, cheat->name, textmax);
		AppendNewLine(textout, 1, textmax);
	}
	if(strlen(cheat->comment)) {
		AppendText(textout, cheat->comment, textmax);
		if(!strchr(NEWLINE, (*textout)[strlen(*textout) - 1])) AppendNewLine(textout, 1, textmax);
	}

	if(g_outcrypt == CRYPT_ARMAX) {
		char alpha[16];
		for(i = 0; i < cheat->codecnt; i+=2) {
			bintoalpha(alpha, cheat->code, i);
			AppendText(textout, alpha, textmax);
			AppendNewLine(textout, 1, textmax);
		}
	}
	else {
		for(i = 0; i < cheat->codecnt; i+=2) {
			char tmp[20];
			sprintf(tmp, "%08X %08X", cheat->code[i], cheat->code[i+1]);
			AppendText(textout, tmp, textmax);
			AppendNewLine(textout, 1, textmax);
		}
	}
	if(cheat->state) {
		AppendText(textout, transGetErrorText(cheat->state), textmax);
		AppendNewLine(textout, 2, textmax);
		return;
	}
	AppendNewLine(textout, 1, textmax);
}

inline void ProcessCode(char **textout, int *textmax, char delim, cheat_t *cheat) {
	int err, i;

	if(strlen(*textout) && !strchr(NEWLINE, delim)) AppendNewLine(textout, 1, textmax);
	if((err = ConvertCode(cheat)) != 0) {
		AppendText(textout, transGetErrorText(err), textmax);
		AppendNewLine(textout, 2, textmax);
		return;
	}

	if(g_outcrypt == CRYPT_ARMAX) {
		char alpha[16];
		for(i = 0; i < cheat->codecnt; i+=2) {
			bintoalpha(alpha, cheat->code, i);
			AppendText(textout, alpha, textmax);
			AppendNewLine(textout, 1, textmax);
		}
		AppendNewLine(textout, 1, textmax);
		return;
	}

	for(i = 0; i < cheat->codecnt; i+=2) {
		char tmp[20];
		sprintf(tmp, "%08X %08X", cheat->code[i], cheat->code[i+1]);
		AppendText(textout, tmp, textmax);
		AppendNewLine(textout, 1, textmax);
	}
	AppendNewLine(textout, 1, textmax);
	return;
}

int ProcessText(HWND hwnd) {
	int namemax = BUFFER_SIZE;
	char buffer[BUFFER_SIZE+1], *end, delim[2];
	char format[BUFFER_SIZE+1], *name = malloc(namemax+1);
	char *textout, *textin, **s, *line;
	static char *defname = "Unnamed Game";
	int ret, chrs, mode, i, type, err, crlf, ctrl, toknum = 0, tokmax=TOK_MAX_STEP, linetok;
	u32 tmp, num = 0, *max, dischash = 0;
	cheat_t *cheat = NULL, *prevCheat = NULL, *firstCheat = NULL;
	u8 namedone = 0;
	token_t *tok;

	tok = (token_t*)malloc(tokmax * sizeof(token_t));
	if(!tok) {
		return 1;
	}
	g_textmax = GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_IN));
	if(g_textmax == 0) return 1;
	g_textmax+=5;
	textin = malloc(g_textmax);
	if(!textin) {return 1;}
	memset(textin, 0, g_textmax);
	GetDlgItemText(hwnd, IDC_EDIT_IN, textin, g_textmax);
	AppendNewLine(&textin, 2, &g_textmax);

	textout = malloc(g_textmax+1);

	if(!name) { return 1; }
	if(!textout) { return 1; }
	memset(name, 0, namemax + 1);
	memset(textout, 0, g_textmax + 1);
	
	//initialize basic data about the code list.
	g_game.cnt = 0;
	if(GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_GAMENAME)))
		GetDlgItemText(hwnd, IDC_EDIT_GAMENAME, g_game.name, NAME_MAX_SIZE);
	else
		strcpy(g_game.name, defname);
	g_game.id = g_gameid;

	//Get the gameid before proceeding.
	if(g_verifiermode == AUTO || g_indevice != DEV_ARMAX) {
		GetDlgItemText(hwnd, IDC_EDIT_GAMEID, buffer, 5);
		sscanf(buffer,"%x",&g_gameid);
		g_gameid&=0x1FFF;
	}

	//Setup some of the devices for input or output;
	gs3Init();
	ar2SetSeed(ar2seeds);
	
	sprintf(format, "%%%d[^ \t\r]%%n", BUFFER_SIZE);
	line = textin;
	while(*line) {
		end = strchr(line, '\n');
		*end = '\0';
		linetok = 0;
		if(!IsEmptyStr(line)) {
			while(line < end) {
				if((toknum + 2) >= tokmax) {
					tokmax += TOK_MAX_STEP;
					tok = (token_t*)realloc(tok, tokmax * sizeof(token_t));
					if(tok == NULL) {
						MsgBox(hwnd, MB_ICONERROR | MB_OK, "Unable to allocate %d bytes for tokens", tokmax);
						return 1;
					}
				}
				chrs = 0;
				ret = sscanf(line, format, buffer, &chrs);
				if(ret > 0) {
					if((linetok == 0 && line[chrs] == '\t' && IsNumStr(buffer)) ||
					    (strlen(line) == 1 && *line == '#')) {
						line += chrs + 1;
						continue;
					}
					tok[toknum].str		= line;
					tok[toknum].type	= GetTokenType(buffer);
					if(g_incrypt == CRYPT_ARMAX) {
						if(tok[toknum].type & (TOK_CODE | TOK_HEXOCTET)) tok[toknum].type = TOK_STRING;
					} else if(tok[toknum].type & TOK_ARMCODE) tok[toknum].type = TOK_STRING;
					if(tok[toknum].type & TOK_CODE) {
						tok[toknum++].type	= TOK_CODEADDR;
						tok[toknum].str		= line + NUM_DIGITS_OCTET;
						tok[toknum].type	= TOK_CODEVAL;
					}
					if(line[chrs] == '\0' || line[chrs] == '\r') tok[toknum].type |= TOK_ENDLINE;
					line[chrs] = '\0';
					toknum++;
					linetok++;
				}
				line += chrs + 1;
			}
		} else if(*line = '\r' && toknum > 0) {  //save lines that are NEWLINE only so cheats can be distinguished.
			if((toknum + 1) >= tokmax) {
				tokmax += TOK_MAX_STEP;
				tok = (token_t*)realloc(tok, tokmax * sizeof(token_t));
				if(tok == NULL) {
					MsgBox(hwnd, MB_ICONERROR | MB_OK, "Unable to allocate %d bytes for tokens", tokmax);
					return 1;
				}
			}
			tok[toknum].str = NEWLINE;
			tok[toknum++].type = TOK_STRING | TOK_NEWLINE | TOK_ENDLINE;
			linetok++;
		}
		ctrl = 0;
		if(toknum > 0 && linetok > 0) {
			for(i = toknum - 1; i >= toknum - linetok; i--) {
				if(tok[i].type & TOK_HEXOCTET && ctrl & TOK_HEXOCTET) {
					tok[i].type = TOK_CODEADDR;
					tok[i+1].type = TOK_CODEVAL;
				}
				ctrl = tok[i].type;
			}
		}
		line = end+1;
	}
	for(i = 0; i < toknum; i++) {
		if(tok[i].type & TOK_HEXOCTET) tok[i].type = TOK_STRING;
		if(i > 1 && (tok[i].type & TOK_STRING) && ((tok[i-1].type & TOK_CODEVAL) || tok[i-1].type & TOK_ARMCODE)) {
			tok[i-1].type |= TOK_ENDCODE;
			g_game.cnt++;
		}
	}

	g_firstcheat	= cheatInit();
	cheat = prevCheat = g_firstcheat;
	s		= &cheat->name;
	max 		= &cheat->namemax;
	namedone 	= 0;
	for(i = 0; i < toknum; i++) {
		type = tok[i].type;
		if(type & TOK_STRING) {
			if(!(type & TOK_NEWLINE)) {
				AppendText(s, tok[i].str, max);
				if(tok[i].type & TOK_ENDLINE) {
					if(namedone) {
						AppendNewLine(s, 1, max);
					} else {
						s	= &cheat->comment;
						max	= &cheat->commentmax;
						namedone = 1;
					}
				}
				else AppendText(s, " ", max);
			} else if(namedone) {
				prevCheat	= cheat;
				cheat		= cheatInit();
				prevCheat->nxt	= cheat;
				s 		= &cheat->name;
				max		= &cheat->namemax;
				namedone = 0;
			}
		} else if(type & TOK_CODEADDR) {
			cheatAppendCodeFromText(cheat, tok[i].str, tok[i+1].str);
			i++;
			if(tok[i].type & TOK_ENDCODE && ((i+1) < toknum)) {
				prevCheat	= cheat;
				cheat		= cheatInit();
				prevCheat->nxt	= cheat;
				s 		= &cheat->name;
				max		= &cheat->namemax;
				namedone = 0;
			}
		} else if(type & TOK_ARMCODE) {
			char alpha[1][14];
			if(MaxRemoveDashes(alpha[0], tok[i].str)) {
				u32 bin[2];
				alphatobin(alpha, bin, 1);
				cheatAppendOctet(cheat, bin[0]);
				cheatAppendOctet(cheat, bin[1]);
			}
			if(tok[i].type & TOK_ENDCODE && ((i+1) < toknum)) {
				prevCheat	= cheat;
				cheat		= cheatInit();
				prevCheat->nxt	= cheat;
				s 		= &cheat->name;
				max		= &cheat->namemax;
				namedone = 0;
			}
		}
		//printf("Token: %s\tType: %08X\n", tok[i].str, tok[i].type);
	}
	if(cheat->codecnt == 0 && strlen(cheat->name) == 0) {
		if(cheat == g_firstcheat) g_firstcheat = NULL;
		cheatDestroy(cheat);
		cheat = prevCheat;
		prevCheat->nxt = NULL;
	}

	cheat = g_firstcheat;
	cheatClearFolderId();

	if(cheat && g_outdevice == DEV_ARMAX && g_verifiermode == AUTO && g_hashdrive) {
		armMakeDiscHash(&dischash, hwnd, g_hashdrive);
	} 
	ResetDevices(INCRYPT);
	while(cheat) {
		int err;
		if(DecryptCode(cheat)) cheat->state = 0xFF;
		err = TranslateCode(cheat);
		if(!cheat->state) cheat->state = err;
		if(cheat->codecnt == 0 && g_outdevice == DEV_ARMAX && g_makefolders) {
			armMakeFolder(cheat, g_gameid, g_region);
		}
		cheatFinalizeData(cheat, g_outdevice, dischash, g_makefolders);
		cheat = cheat->nxt;
	}

	cheat = g_firstcheat;
	ResetDevices(OUTCRYPT);
	while(cheat) {
		err = EncryptCode(cheat);
		if(!cheat->state && err) cheat->state = 0xFF;
		CheatToText(&textout, &g_textmax, cheat);
		cheat = cheat->nxt;
	}
	
	//display output text.
	SetDlgItemText(hwnd, IDC_EDIT_OUT, textout);
	sprintf(buffer, "%04X", g_gameid);
	SetDlgItemText(hwnd, IDC_EDIT_GAMEID, buffer);
	
	//cleanup
	free(tok);
	free(name);
	free(textout);
	free(textin);
}

void InitOfn(OPENFILENAME *ofn, HWND hwnd, char *szFileName, char *szTitleName)
{
	static const char szFilter[] = \
		"Text Files (*.txt)\0*.txt\0" \
		"All Files (*.*)\0*.*\0" \
		"\0";

	ofn->lStructSize	= sizeof(OPENFILENAME);
	ofn->hwndOwner		= hwnd;
	ofn->hInstance		= NULL;
	ofn->lpstrFilter	= szFilter;
	ofn->lpstrCustomFilter	= NULL;
	ofn->nMaxCustFilter	= 0;
	ofn->nFilterIndex	= 0;
	ofn->lpstrFile		= szFileName;
	ofn->nMaxFile		= MAX_PATH;
	ofn->lpstrFileTitle	= szTitleName;
	ofn->nMaxFileTitle	= MAX_PATH;
	ofn->lpstrInitialDir	= NULL;
	ofn->lpstrTitle		= NULL;
	ofn->Flags		= 0;
	ofn->nFileOffset	= 0;
	ofn->nFileExtension	= 0;
	ofn->lpstrDefExt	= "txt";
	ofn->lCustData		= 0L;
	ofn->lpfnHook		= NULL;
	ofn->lpTemplateName	= NULL;
}

/*
 * Creates a "Open" dialog box.
 */
BOOL FileOpenDlg(OPENFILENAME *ofn)
{
     ofn->Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

     return GetOpenFileName(ofn);
}

/*
 * Creates a "Save" dialog box.
 */
BOOL FileSaveDlg(OPENFILENAME *ofn)
{
     ofn->Flags = OFN_OVERWRITEPROMPT;

     return GetSaveFileName(ofn);
}

BOOL ArmSaveDlg(OPENFILENAME *ofn) {
	static const char szFilter[] = "ARMAX Codelists (*.bin)\0*.bin\0\0";
	ofn->Flags 		= OFN_OVERWRITEPROMPT;
	ofn->lpstrFilter	= szFilter;
	ofn->lpstrFile[0]	= '\0';
	ofn->lpstrDefExt	= "bin";

	return GetSaveFileName(ofn);
}

BOOL CBCSaveDlg(OPENFILENAME *ofn) {
	static const char szFilter[] = "CB Cheat Files (*.cbc)\0*.cbc\0\0";
	ofn->Flags 		= OFN_OVERWRITEPROMPT;
	ofn->lpstrFilter	= szFilter;
	ofn->lpstrFile[0]	= '\0';
	ofn->lpstrDefExt	= "cbc";

	return GetSaveFileName(ofn);
}

BOOL GSSaveDlg(OPENFILENAME *ofn) {
	static const char szFilter[] = "GameShark/Xploder Files (*.p2m)\0*.p2m\0\0";
	static char szP2mFileName[MAX_PATH];
	ofn->Flags 		= OFN_OVERWRITEPROMPT;
	ofn->lpstrFilter	= szFilter;
	ofn->lpstrFile[0]	= '\0';
	ofn->lpstrDefExt	= "p2m";

	return GetSaveFileName(ofn);
}

BOOL SmcSaveDlg(OPENFILENAME *ofn) {
	static const char szFilter[] = "Swap Magic Codelists (*.bin)\0*.bin\0\0";
	ofn->Flags 		= OFN_OVERWRITEPROMPT;
	ofn->lpstrFilter	= szFilter;
	ofn->lpstrFile[0]	= '\0';
	ofn->lpstrDefExt	= "bin";

	return GetSaveFileName(ofn);
}

/*
 * Loads the contents of a text file into the input dialog box.
 */
BOOL LoadTextFile(HWND hwnd, char *szFileName)
{
	char *buf;
	DWORD dwBytesRead;
	HANDLE hFile;
	int iFileLength, ret;

	// Open file for reading
	hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(hwnd, MB_ICONERROR | MB_OK, "Can't read from file \"%s\"", szFileName);
		return FALSE;
	}

	// Check file size
	iFileLength = GetFileSize(hFile, NULL);
	if (iFileLength > TEXT_SIZE) {
		ret = MsgBox(hwnd, MB_ICONQUESTION | MB_YESNO,
			"%s can only load the first %i bytes of this file (%i bytes total).\n\nOpen anyway?",
			APPNAME, TEXT_SIZE, iFileLength);
		if (ret == IDNO) {
			CloseHandle(hFile);
			return FALSE;
		}
		iFileLength = TEXT_SIZE;
	}

	// Allocate memory
	buf = (char*)malloc(iFileLength + 1);
	if (buf == NULL) {
		MsgBox(hwnd, MB_ICONERROR | MB_OK, "Unable to allocate %i bytes.", iFileLength + 1);
		CloseHandle(hFile);
		return FALSE;
	}

	// Read from file into buffer
	ReadFile(hFile, buf, iFileLength, &dwBytesRead, NULL);
	CloseHandle(hFile);

	// Set text in input dialog box
	buf[iFileLength] = '\0';
	SetDlgItemText(hwnd, IDC_EDIT_IN, buf);
	free(buf);

	return TRUE;
}

/*
 * Saves the text in the output dialog box to a text file.
 */
BOOL SaveTextFile(HWND hwnd, char *szFileName)
{
	char *buf;
	DWORD dwBytesWritten;
	HANDLE hFile;
	HWND hCtrl;
	int iLength;

	// Open file for writing
	hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgBox(hwnd, MB_ICONERROR | MB_OK, "Can't write to file \"%s\"", szFileName);
		return FALSE;
	}

	// Get size of output dialog box text
	hCtrl = GetDlgItem(hwnd, IDC_EDIT_OUT);
	iLength = GetWindowTextLength(hCtrl);

	// Allocate memory
	buf = (char*)malloc(iLength + 1);
	if (buf == NULL) {
		MsgBox(hwnd, MB_ICONERROR | MB_OK, "Unable to allocate %i bytes.", iLength + 1);
		CloseHandle(hFile);
		return FALSE;
	}

	// Write text to file
	GetWindowText(hCtrl, buf, iLength + 1);
	WriteFile(hFile, buf, iLength, &dwBytesWritten, NULL);
	CloseHandle(hFile);
	free(buf);

	return TRUE;
}

void SetEditFont(HWND hwnd, char *fontname) {
	LOGFONT lf;
	HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	GetObject(hFont, sizeof(LOGFONT), &lf);
	strcpy(lf.lfFaceName,fontname);
	hFont = CreateFontIndirect(&lf);

	SendDlgItemMessage(hwnd,IDC_EDIT_IN,WM_SETFONT,(WPARAM)hFont,FALSE);
	SendDlgItemMessage(hwnd,IDC_EDIT_OUT,WM_SETFONT,(WPARAM)hFont,FALSE);
	SendDlgItemMessage(hwnd,IDC_EDIT_GAMEID,WM_SETFONT,(WPARAM)hFont,FALSE);
}

void SetInputCrypt(HWND hwnd, u16 menuid) {
	int i;
	char header[200] = "";
	for(i = 0; i < NUM_CRYPT_OPTS; i++) {
		if(g_incryptopts[i].menuid == menuid) {
			g_input		= menuid;
			g_incrypt	= g_incryptopts[i].crypt;
			g_indevice	= g_incryptopts[i].device;
			CheckMenuItem(GetMenu(hwnd), g_incryptopts[i].menuid, MF_CHECKED);
			strcat(header, g_incryptopts[i].desc);
			strcat(header, ":");
			SetDlgItemText(hwnd, ID_STATIC_INPUT, header);
		} else	CheckMenuItem(GetMenu(hwnd), g_incryptopts[i].menuid, MF_UNCHECKED); //uncheck all else
	}
}

void SetOutputCrypt(HWND hwnd, u16 menuid) {
	int i;
	char header[200] = "";
	for(i = 0; i < NUM_CRYPT_OPTS; i++) {
		if(g_outcryptopts[i].menuid == menuid) {
			g_output	= menuid;
			g_outcrypt	= g_outcryptopts[i].crypt;
			g_outdevice	= g_outcryptopts[i].device;
			CheckMenuItem(GetMenu(hwnd), g_outcryptopts[i].menuid, MF_CHECKED);
			strcat(header, g_incryptopts[i].desc);
			strcat(header, ":");
			SetDlgItemText(hwnd, ID_STATIC_OUTPUT, header);
		} else	CheckMenuItem(GetMenu(hwnd), g_outcryptopts[i].menuid, MF_UNCHECKED); //uncheck all else
	}
}

void SetParseOpt(HWND hwnd, int opt) {
	parseopt = opt;
	CheckMenuItem(GetMenu(hwnd), ID_PARSER_REFORMAT, parseopt == PARSE_OPT_REFORMAT ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_PARSER_SIMPLE, parseopt == PARSE_OPT_SIMPLE ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_PARSER_STRICT, parseopt == PARSE_OPT_STRICT ? MF_CHECKED : MF_UNCHECKED);
}

void SetVerifierMode(HWND hwnd, int mode) {
	g_verifiermode = mode;
	CheckMenuItem(GetMenu(hwnd), ID_ARMAXVERIFIERS_MANUAL, mode == MANUAL ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_ARMAXVERIFIERS_AUTOMATIC, mode == AUTO ? MF_CHECKED : MF_UNCHECKED);
}

void SetRegion(HWND hwnd, int region) {
	g_region = region;
	CheckMenuItem(GetMenu(hwnd), ID_ARMAXREGION_USA, region == REGION_USA ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_ARMAXREGION_PAL, region == REGION_PAL ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_ARMAXREGION_JAPAN, region == REGION_JAPAN ? MF_CHECKED : MF_UNCHECKED);
}

void SetGs3Key(HWND hwnd, int key) {
	g_gs3key = key;
	CheckMenuItem(GetMenu(hwnd), ID_GS3_KEY_0, key == 0 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_GS3_KEY_1, key == 1 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_GS3_KEY_2, key == 2 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_GS3_KEY_3, key == 3 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hwnd), ID_GS3_KEY_4, key == 4 ? MF_CHECKED : MF_UNCHECKED);
}

void SetArmHashOption(HWND hwnd, DWORD item) {
	int i;
	MENUITEMINFO mii;
	mii.cbSize	= sizeof(MENUITEMINFO);
	mii.fMask	= MIIM_DATA;
	for(i = ID_ARMAXHASH_NOHASH; i <= ID_ARMAXHASH_DRIVE_Z; i++) {
		if(item == i) {
			CheckMenuItem(GetMenu(hwnd), i, MF_CHECKED);
			GetMenuItemInfo(GetMenu(hwnd), i, FALSE, &mii);
			g_hashdrive = mii.dwItemData;
			
		} else  CheckMenuItem(GetMenu(hwnd), i, MF_UNCHECKED);
	}
}

DWORD RegReadDword(HKEY hKey, const char *name, DWORD def) {
	DWORD type, size = sizeof(DWORD), val;

	//if(RegGetValue(hKey, REGINPUT, 0x18, &type, (PVOID) &g_input, &size) != ERROR_SUCCESS damn it!
	if(RegQueryValueEx(hKey, name, 0, &type, (BYTE*)&val, &size) != ERROR_SUCCESS) return def;
	if(type != REG_DWORD || size != sizeof(DWORD)) return def;
	return val;
}

BOOL SaveOptions(void) {
	HKEY hKey;
	DWORD disp;
	if(RegCreateKeyEx(HKEY_CURRENT_USER, REGSUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS) {
		RegSetValueEx(hKey, REGINPUT, 0, REG_DWORD, (CONST BYTE*)&g_input, sizeof(DWORD));
		RegSetValueEx(hKey, REGOUTPUT, 0, REG_DWORD, (CONST BYTE*)&g_output, sizeof(DWORD));
		RegSetValueEx(hKey, REGPARSE, 0, REG_DWORD, (CONST BYTE*)&parseopt, sizeof(DWORD)); 
		RegSetValueEx(hKey, REGVERIFIER, 0, REG_DWORD, (CONST BYTE*)&g_verifiermode, sizeof(DWORD)); 
		RegSetValueEx(hKey, REGLOC, 0, REG_DWORD, (CONST BYTE*)&g_region, sizeof(DWORD)); 
		RegSetValueEx(hKey, REGGAMEID, 0, REG_DWORD, (CONST BYTE*)&g_gameid, sizeof(DWORD));
		RegSetValueEx(hKey, REGAR2, 0, REG_DWORD, (CONST BYTE*)&ar2seeds, sizeof(DWORD));
		RegSetValueEx(hKey, REGGS3KEY, 0, REG_DWORD, (CONST BYTE*)&g_gs3key, sizeof(DWORD));
		RegSetValueEx(hKey, REGFOLDERS, 0, REG_DWORD, (CONST BYTE*)&g_makefolders, sizeof(DWORD));
		return TRUE;
	}
	return FALSE;
}

BOOL LoadOptions (void) {
	HKEY hKey;
	DWORD disp;
	DWORD type, size;
	u32 key; //temp for ar2seeds;
	if(RegCreateKeyEx(HKEY_CURRENT_USER, REGSUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, &disp) == ERROR_SUCCESS) {
		if(disp == REG_OPENED_EXISTING_KEY) {
			g_input		= RegReadDword(hKey, REGINPUT, DEF_INPUT);
			g_output	= RegReadDword(hKey, REGOUTPUT, DEF_OUTPUT);
			parseopt	= RegReadDword(hKey, REGPARSE, PARSE_OPT_REFORMAT);
			g_verifiermode	= RegReadDword(hKey, REGVERIFIER, AUTO);
			g_region	= RegReadDword(hKey, REGLOC, ARM_DEF_REGION);
			g_gameid	= RegReadDword(hKey, REGGAMEID, ARM_DEF_GAMEID);
			ar2seeds	= RegReadDword(hKey, REGAR2, AR1_SEED);
			g_gs3key	= RegReadDword(hKey, REGGS3KEY, DEF_GS3_KEY);
			g_makefolders	= RegReadDword(hKey, REGFOLDERS, 1);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL EnumerateDrivesAddToMenu(HWND hwnd) {
	DWORD len, drives;
	int i;
	UINT type, count = 0;
	char pathname[MAX_PATH + 1], szMenu[MAX_PATH + 1];
	MENUITEMINFO mii;
	
	//initialize the menu item info
	mii.cbSize	= sizeof(MENUITEMINFO);
	mii.fMask	= MIIM_STATE | MIIM_ID | MIIM_TYPE | MIIM_DATA;
	mii.fType	= MFT_STRING;
	mii.fState	= MFS_UNCHECKED;
	mii.wID		= ID_ARMAXHASH_NOHASH + 1;
	mii.dwTypeData	= szMenu;
	mii.cch		= 0;
	
	drives = GetLogicalDrives();
	for(i = 0; i < 26; i++) {
		if((drives >> i) & 1) {
			pathname[0] = 0x41 + i;
			pathname[1] = '\0';
			strcat(pathname, ":\\");
			if(GetDriveType(pathname) == DRIVE_CDROM) {
				mii.cch = sprintf(szMenu, "Use Drive %s", pathname);
				mii.dwItemData = (DWORD)pathname[0];
				mii.wID += i;
				InsertMenuItem(GetMenu(hwnd), ID_ARMAXHASH_NOHASH, FALSE, &mii);
				count++;
			}
		}	
	}
	if(count > 0) {
		DrawMenuBar(hwnd);
		return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK ArDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	char buffer[NUM_DIGITS_OCTET + 1] = { 0 };
	u32 key;
	u32 ar1seed = AR1_SEED;
	u8 *tseed = (u8*)&ar1seed;
		
	switch(message) {
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg,IDC_EDIT_AR2,EM_SETLIMITTEXT,NUM_DIGITS_OCTET,0);
			key = ar2encrypt(ar2seeds, tseed[1], tseed[0]);
			sprintf(buffer, "%08X", key);
			SetDlgItemText(hwndDlg, IDC_EDIT_AR2, buffer);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK: {
					GetDlgItemText(hwndDlg, IDC_EDIT_AR2, buffer, NUM_DIGITS_OCTET + 1);
					if(strlen(buffer) == 8 && IsHexStr(buffer)) {
						sscanf(buffer, "%08x", &key);
						ar2seeds = ar2decrypt(key, tseed[1], tseed[0]);
						EndDialog(hwndDlg, 0);
					}
					return TRUE;
				}
				case ID_BTN_RESET: {
					ar2seeds = AR1_SEED;
					EndDialog(hwndDlg, 0);
					return TRUE;
				}
			}
	}
	return FALSE;
}

BOOL CALLBACK DlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HWND hCtrl;
	static char szFileName[MAX_PATH];
	static char szTitleName[MAX_PATH];
	static OPENFILENAME ofn;
	int iSelBeg, iSelEnd, iEnable, iLen;
	HICON hIcon_big, hIcon_sm;

	hCtrl = GetFocus();
	switch (message) {
		case WM_INITDIALOG:
			// Set window title
			SetWindowText(hwndDlg, WINDOWTITLE);
			// Set a fixed width font on the edit boxes
			SetEditFont(hwndDlg, "Courier New");
			SendDlgItemMessage(hwndDlg,IDC_EDIT_GAMEID,EM_SETLIMITTEXT,4,0);
			SendDlgItemMessage(hwndDlg,IDC_EDIT_GAMENAME,EM_SETLIMITTEXT,NAME_MAX_SIZE,0);
			SendDlgItemMessage(hwndDlg,IDC_EDIT_IN,EM_SETLIMITTEXT,TEXT_SIZE,0);
			SendDlgItemMessage(hwndDlg,IDC_EDIT_OUT,EM_SETLIMITTEXT,TEXT_SIZE<<1,0);

			//set icons
			hIcon_big = LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
			hIcon_sm = LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon_big);
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon_sm);

			//Get options from registry (if extant)
			LoadOptions();
			
			//Set the options based on reg values or defaults
			SetInputCrypt(hwndDlg, g_input);
			SetOutputCrypt(hwndDlg, g_output);
			SetParseOpt(hwndDlg, parseopt);
			SetVerifierMode(hwndDlg, g_verifiermode);
			SetRegion(hwndDlg, g_region);
			SetGs3Key(hwndDlg, g_gs3key);
			CheckMenuItem(GetMenu(hwndDlg), ID_OPTION_MAKEFOLDERS, g_makefolders ? MF_CHECKED : MF_UNCHECKED);
			EnumerateDrivesAddToMenu(hwndDlg);
			SetArmHashOption(hwndDlg, ID_ARMAXHASH_NOHASH);

			char tmp[5];
			sprintf(tmp, "%04X", g_gameid);
			SetDlgItemText(hwndDlg, IDC_EDIT_GAMEID, tmp);

			g_firstcheat = NULL;

			//build ARMAX seeds.  Luckily, we only have to do this once.
			buildseeds();
			return 0;
		case WM_INITMENUPOPUP:
			switch (LOWORD(lParam)) {
				case 0: // Submenu "File"
					iLen = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_EDIT_OUT));
					EnableMenuItem((HMENU)wParam, ID_FILE_SAVEASTEXT,
						iLen ? MF_ENABLED : MF_GRAYED);
					//Conversions are necessary for these anyway, so take the input text
					iLen = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_EDIT_IN));
					EnableMenuItem((HMENU)wParam, ID_FILE_SAVEASARMAX,
						iLen ? MF_ENABLED : MF_GRAYED);
					EnableMenuItem((HMENU)wParam, ID_FILE_SAVEASCBC,
						iLen ? MF_ENABLED : MF_GRAYED);
					EnableMenuItem((HMENU)wParam, ID_FILE_SAVEASP2M,
						iLen ? MF_ENABLED : MF_GRAYED);
					EnableMenuItem((HMENU)wParam, ID_FILE_SAVEASSMC,
						iLen ? MF_ENABLED : MF_GRAYED);
					break;

				case 1: // Submenu "Edit"
					if (hCtrl == GetDlgItem(hwndDlg, IDC_EDIT_IN) ||
					    hCtrl == GetDlgItem(hwndDlg, IDC_EDIT_GAMENAME) ||
					    hCtrl == GetDlgItem(hwndDlg, IDC_EDIT_GAMEID)) { 
						// Enable "Undo" if edit-control operation can be undone
						EnableMenuItem((HMENU)wParam, ID_EDIT_UNDO,
							SendMessage(hCtrl, EM_CANUNDO, 0, 0) ? MF_ENABLED : MF_GRAYED);

						// Enable "Paste" if clipboard contains text
						EnableMenuItem((HMENU)wParam, ID_EDIT_PASTE,
							IsClipboardFormatAvailable (CF_TEXT) ? MF_ENABLED : MF_GRAYED);

						// Enable "Cut", "Copy" and "Delete" if some text is selected
						SendMessage(hCtrl, EM_GETSEL, (WPARAM)&iSelBeg, (LPARAM)&iSelEnd);
						iEnable = (iSelBeg != iSelEnd) ? MF_ENABLED : MF_GRAYED;
						EnableMenuItem((HMENU)wParam, ID_EDIT_CUT, iEnable);
						EnableMenuItem((HMENU)wParam, ID_EDIT_COPY, iEnable);
						EnableMenuItem((HMENU)wParam, ID_EDIT_DELETE, iEnable);

						// Enable "Select All" if there is any text
						iEnable = GetWindowTextLength(hCtrl) ? MF_ENABLED : MF_GRAYED;
						EnableMenuItem((HMENU)wParam, ID_EDIT_SELECTALL, iEnable);

					} else if (hCtrl == GetDlgItem(hwndDlg, IDC_EDIT_OUT)) { // Output box has focus
						// Enable "Copy" if some text is selected
						SendMessage(hCtrl, EM_GETSEL, (WPARAM)&iSelBeg, (LPARAM)&iSelEnd);
						iEnable = (iSelBeg != iSelEnd) ? MF_ENABLED : MF_GRAYED;
						EnableMenuItem((HMENU)wParam, ID_EDIT_COPY, iEnable);

						// Enable "Select All" if there is any text
						iEnable = GetWindowTextLength(hCtrl) ? MF_ENABLED : MF_GRAYED;
						EnableMenuItem((HMENU)wParam, ID_EDIT_SELECTALL, iEnable);

						// Disable the other items
						EnableMenuItem((HMENU)wParam, ID_EDIT_UNDO, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_CUT, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_PASTE, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_DELETE, MF_GRAYED);
					} else {
						// Disable all menu items
						EnableMenuItem((HMENU)wParam, ID_EDIT_UNDO, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_CUT, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_COPY, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_PASTE, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_DELETE, MF_GRAYED);
						EnableMenuItem((HMENU)wParam, ID_EDIT_SELECTALL, MF_GRAYED);
					}
					break;

			}
			return 0;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case ID_FILE_SAVEASARMAX:
				case ID_FILE_SAVEASCBC:
				case ID_FILE_SAVEASP2M:
					if(GetWindowTextLength(GetDlgItem(hwndDlg, IDC_EDIT_GAMENAME)) == 0) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "You must enter a game name.");
						return 0;
					}
			}

			if(LOWORD(wParam) >= ID_ARMAXHASH_NOHASH && LOWORD(wParam) <= ID_ARMAXHASH_DRIVE_Z) {
				SetArmHashOption(hwndDlg, LOWORD(wParam));
				return 0;
			}
			
			switch (LOWORD(wParam)) { 
				case ID_BTN_CONVERT:
					ProcessText(hwndDlg);
					CleanupCheats(g_firstcheat);
					g_firstcheat = NULL;
					return 0;

				case ID_BTN_CLEAR_IN:
					SetDlgItemText(hwndDlg, IDC_EDIT_IN, "");
					return 0;

				case ID_BTN_CLEAR_OUT:
					SetDlgItemText(hwndDlg, IDC_EDIT_OUT, "");
					return 0;

				case ID_INPUT_UNENC_COMMON:
				case ID_INPUT_UNENC_ARMAX:
				case ID_INPUT_UNENC_AR1_2:
				case ID_INPUT_UNENC_CB:
				case ID_INPUT_UNENC_GS1_2:
				case ID_INPUT_UNENC_XP:
				case ID_INPUT_AR2_V1:
				case ID_INPUT_AR2_V2:
				case ID_INPUT_ARMAX:
				case ID_INPUT_CB:
				case ID_INPUT_CB7_COMMON:
				case ID_INPUT_IA_V1:
				case ID_INPUT_IA_V2:
				case ID_INPUT_MC_V3:
				case ID_INPUT_MC_V5:
				case ID_INPUT_XP_V1:
				case ID_INPUT_XP_V4:
				case ID_INPUT_XP_V5:
				case ID_INPUT_SMC:
					SetInputCrypt(hwndDlg, LOWORD(wParam));
					return 0;

				case ID_OUTPUT_UNENC_COMMON:
				case ID_OUTPUT_UNENC_ARMAX:
				case ID_OUTPUT_UNENC_AR1_2:
				case ID_OUTPUT_UNENC_CB:
				case ID_OUTPUT_UNENC_GS1_2:
				case ID_OUTPUT_UNENC_XP:
				case ID_OUTPUT_AR2_V1:
				case ID_OUTPUT_AR2_V2:
				case ID_OUTPUT_ARMAX:
				case ID_OUTPUT_CB:
				case ID_OUTPUT_CB7_COMMON:
				case ID_OUTPUT_IA_V1:
				case ID_OUTPUT_IA_V2:
				case ID_OUTPUT_MC_V3:
				case ID_OUTPUT_MC_V5:
				case ID_OUTPUT_XP_V1:
				case ID_OUTPUT_XP_V4:
				case ID_OUTPUT_XP_V5:
				case ID_OUTPUT_SMC:
					SetOutputCrypt(hwndDlg, LOWORD(wParam));
					return 0;

				case ID_ARMAXVERIFIERS_AUTOMATIC:
					SetVerifierMode(hwndDlg, AUTO);
					return 0;

				case ID_ARMAXVERIFIERS_MANUAL:
					SetVerifierMode(hwndDlg, MANUAL);
					return 0;

				case ID_ARMAXREGION_USA:
					SetRegion(hwndDlg, REGION_USA);
					return 0;

				case ID_ARMAXREGION_PAL:
					SetRegion(hwndDlg, REGION_PAL);
					return 0;

				case ID_ARMAXREGION_JAPAN:
					SetRegion(hwndDlg, REGION_JAPAN);
					return 0;

				case ID_OPTION_MAKEFOLDERS:
					g_makefolders ^= 1;
					CheckMenuItem(GetMenu(hwndDlg), ID_OPTION_MAKEFOLDERS, g_makefolders ? MF_CHECKED : MF_UNCHECKED);
					return 0;

				case ID_FILE_EXIT:
					SaveOptions();
					PostQuitMessage(0);
					return 0;

				case ID_FILE_LOADTEXT:
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if (FileOpenDlg(&ofn))
						LoadTextFile(hwndDlg, szFileName);
					return 0;

				case ID_FILE_SAVEASTEXT:
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if (FileSaveDlg(&ofn))
						SaveTextFile(hwndDlg, szFileName);
					return 0;
				
				case ID_FILE_SAVEASARMAX: {
					u32 tmp = g_outcrypt;
					u32 tmp2 = g_outdevice;
					g_outcrypt = CRYPT_ARMAX;
					g_outdevice = DEV_ARMAX;
					ProcessText(hwndDlg);
					if(!g_firstcheat) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "No saveable cheats exist.");
						return 0;
					}
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if(ArmSaveDlg(&ofn))
						alCreateList(g_firstcheat, &g_game, szFileName);
					g_outcrypt = tmp;
					g_outdevice = tmp2;
					CleanupCheats(g_firstcheat);
					g_firstcheat = NULL;
					return 0;
				}

				case ID_FILE_SAVEASCBC: {
					if(g_outcrypt != CRYPT_RAW && g_outdevice != DEV_CB) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "You must choose a compatible output encryption (unencrypted or CB variant)");
						return 0;
					}
					ProcessText(hwndDlg);
					if(!g_firstcheat) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "No saveable cheats exist.");
						return 0;
					}
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if(CBCSaveDlg(&ofn))
						cbcCreateFile(g_firstcheat, &g_game, szFileName, g_makefolders);
					CleanupCheats(g_firstcheat);
					g_firstcheat = NULL;
					return 0;
				}

				case ID_FILE_SAVEASP2M: {
					if(g_outcrypt != CRYPT_RAW && g_outdevice != DEV_GS3) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "You must choose a compatible output encryption (unencrypted or GS3/XP4 variant)");
						return 0;
					}
					ProcessText(hwndDlg);
					if(!g_firstcheat) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "No saveable cheats exist.");
						return 0;
					}
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if(GSSaveDlg(&ofn))
						p2mCreateFile(g_firstcheat, &g_game, szFileName, g_makefolders);
					CleanupCheats(g_firstcheat);
					g_firstcheat = NULL;
					return 0;
				}

				case ID_FILE_SAVEASSMC: {
					u32 tmp = g_outcrypt;
					u32 tmp2 = g_outdevice;
					g_outcrypt = CRYPT_AR1;
					g_outdevice = DEV_AR1;
					ProcessText(hwndDlg);
					if(!g_firstcheat) {
						MsgBox(hwndDlg, MB_ICONERROR | MB_OK, "No saveable cheats exist.");
						return 0;
					}
					InitOfn(&ofn, hwndDlg, szFileName, szTitleName);
					if(SmcSaveDlg(&ofn))
						scfCreateFile(g_firstcheat, &g_game, szFileName);
					g_outcrypt = tmp;
					g_outdevice = tmp2;
					CleanupCheats(g_firstcheat);
					g_firstcheat = NULL;
					return 0;
				}


				case ID_EDIT_UNDO:
					SendMessage(hCtrl, WM_UNDO, 0, 0);
					return 0;

				case ID_EDIT_CUT:
					SendMessage(hCtrl, WM_CUT, 0, 0);
					return 0;

				case ID_EDIT_COPY:
					SendMessage(hCtrl, WM_COPY, 0, 0);
					return 0;

				case ID_EDIT_PASTE:
					SendMessage(hCtrl, WM_PASTE, 0, 0);
					return 0;

				case ID_EDIT_DELETE:
					SendMessage(hCtrl, WM_CLEAR, 0, 0);
					return 0;

				case ID_EDIT_SELECTALL:
					SendMessage(hCtrl, EM_SETSEL, 0, -1);
					return 0;

				case ID_AR2_KEY:
					DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), hwndDlg, ArDlgProc);
					return 0;

				case ID_GS3_KEY_0:
					SetGs3Key(hwndDlg, 0);
					return 0;

				case ID_GS3_KEY_1:
					SetGs3Key(hwndDlg, 1);
					return 0;

				case ID_GS3_KEY_2:
					SetGs3Key(hwndDlg, 2);
					return 0;

				case ID_GS3_KEY_3:
					SetGs3Key(hwndDlg, 3);
					return 0;

				case ID_GS3_KEY_4:
					SetGs3Key(hwndDlg, 4);
					return 0;
			}
			break;

		case WM_CLOSE:
			SaveOptions();
			PostQuitMessage(0);
			break;
		case WM_QUIT:
			DestroyWindow(hwndDlg);
			return 0;
	}

	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
	HACCEL hAccel;
	HWND hwnd;
	MSG msg;

	g_hInstance = hInstance;
	// Create main dialog
	hwnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);
	if (hwnd == NULL) {
		MessageBox(NULL, "Could not create main dialog.", APPNAME, MB_ICONERROR | MB_OK);
		return 0;
	}
	ShowWindow(hwnd, iCmdShow);

	hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDA_ACCEL_TABLE));
	if(!hAccel) MessageBox(NULL, "Could not load accelerators", APPNAME, MB_ICONERROR | MB_OK);
	
	// Message loop
	while (GetMessage(&msg, NULL, 0, 0)) {
		if(TranslateAccelerator(hwnd, hAccel, &msg)) {
			DispatchMessage(&msg);
		} else if (!IsDialogMessage(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}
