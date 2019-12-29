/**
 * Font Name Parsing/Extraction Library
 * Last modified: 2009/03/04
 *
 * Font parsing functions loosely based on Microsoft's fontinst source code,
 * available at ftp://ftp.microsoft.com/Softlib/MSLFILES/FONTINST.EXE
 **/

#ifndef __READFONTNAMEINTERNAL_H__
#define __READFONTNAMEINTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include "libs\SimpleString.h"
#include "libs\SwapIntrinsics.h"

// Helper for array size determination
#define countof(x)              (sizeof(x)/sizeof(x[0]))

// Macros for Unicode portability
#define WStrToAStr(w, a, c)     WideCharToMultiByte(CP_ACP, 0, w, -1, a, c, NULL, NULL)
#define AStrToWStr(a, w, c)     MultiByteToWideChar(CP_ACP, 0, a, -1, w, c)

#ifdef UNICODE
#define TStrToAStr              WStrToAStr
#define AStrToTStr              AStrToWStr
#define TStrToWStr(t, w, c)     SSChainCpyW(w, t)
#define WStrToTStr(w, t, c)     SSChainCpyW(t, w)
#else
#define TStrToAStr(t, a, c)     SSChainCpyA(a, t)
#define AStrToTStr(a, t, c)     SSChainCpyA(t, a)
#define TStrToWStr              AStrToWStr
#define WStrToTStr              WStrToAStr
#endif

// Various 4-byte ASCII strings expressed as little-endian uint32
#define tag_TTCTag              0x66637474
#define tag_OTSig               0x4F54544F
#define tag_CFFTable            0x20464643
#define tag_NamingTable         0x656D616E

// Some arbitrary limits on TTF/TTC tables to make life a bit easier
#define MAX_TABLES              40
#define MAX_TTC                 8

// TrueType structures
typedef struct {
	UINT16 platformID;
	UINT16 specificID;
	UINT16 languageID;
	UINT16 nameID;
	UINT16 length;
	UINT16 offset;
} SFNTNAMERECORD;

typedef struct {
	UINT16 format;
	UINT16 count;
	UINT16 stringOffset;
} SFNTNAMINGTABLE;

typedef struct {
	UINT32 tag;
	UINT32 checkSum;
	UINT32 offset;
	UINT32 length;
} SFNTDIRECTORYENTRY;

typedef struct {
	UINT32 version;       // 0x10000 (1.0)
	UINT16 numOffsets;    // number of tables
	UINT16 searchRange;   // (max2 <= numOffsets)*16
	UINT16 entrySelector; // log2 (max2 <= numOffsets)
	UINT16 rangeShift;    // numOffsets*16-searchRange
} SFNTOFFSETTABLE;

typedef struct {
	UINT32 tag;
	UINT32 version;
	UINT32 numFonts;
	UINT32 offsets[MAX_TTC];
} SFNTTTCHEADERSTUB;

// Wrappers for the overly complicated file functions provided by the API:
HANDLE RFNAPI RFNOpen( PCTSTR pszPath );
DWORD WINAPI RFNRead( HANDLE hFile, PVOID pvBuffer, DWORD cbBuffer );
DWORD WINAPI RFNSeek( HANDLE hFile, LONG lOffset, DWORD dwOrigin );

#ifdef __cplusplus
}
#endif

#endif
