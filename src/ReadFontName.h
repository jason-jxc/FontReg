/**
 * Font Name Parsing/Extraction Library
 * Last modified: 2009/03/04
 *
 * Font parsing functions loosely based on Microsoft's fontinst source code,
 * available at ftp://ftp.microsoft.com/Softlib/MSLFILES/FONTINST.EXE
 **/

#ifndef __READFONTNAME_H__
#define __READFONTNAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

/**
 * The font name's length must be able to fit within an 8-bit unsigned integer
 **/

#define MAX_FONTNAME 0x100

/**
 * Possible return codes for the ReadFontName functions
 **/

typedef enum {
	TYPE_NULL,
	TYPE_FON,
	TYPE_TTF,
	TYPE_OTF
} FONTTYPE, *PFONTTYPE;

/**
 * ReadFontName
 * [in ] pszFontPath: path to the font file (may be relative or absolute)
 * [out] pszFontName: buffer of at least MAX_FONTNAME TCHARs in size
 **/

#define RFNAPI __fastcall

FONTTYPE RFNAPI ReadFontName   ( PCTSTR pszFontPath, PTSTR pszFontName );
FONTTYPE RFNAPI ReadFontNameFON( PCTSTR pszFontPath, PTSTR pszFontName );
FONTTYPE RFNAPI ReadFontNameTTF( PCTSTR pszFontPath, PTSTR pszFontName );

#ifdef __cplusplus
}
#endif

#endif
