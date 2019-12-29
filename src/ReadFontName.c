/**
 * Font Name Parsing/Extraction Library
 * Last modified: 2009/03/04
 *
 * Font parsing functions loosely based on Microsoft's fontinst source code,
 * available at ftp://ftp.microsoft.com/Softlib/MSLFILES/FONTINST.EXE
 **/

#include "ReadFontName.h"
#include "ReadFontNameInternal.h"

FONTTYPE RFNAPI ReadFontName( PCTSTR pszFontPath, PTSTR pszFontName )
{
	PCTSTR pszFontExt = pszFontPath + SSLen(pszFontPath) - 4;

	if (pszFontExt > pszFontPath && *pszFontExt == TEXT('.'))
	{
		++pszFontExt;

		if (lstrcmpi(pszFontExt, TEXT("fon")) == 0)
		{
			return(ReadFontNameFON(pszFontPath, pszFontName));
		}
		else if ( lstrcmpi(pszFontExt, TEXT("ttf")) == 0 ||
		          lstrcmpi(pszFontExt, TEXT("ttc")) == 0 ||
		          lstrcmpi(pszFontExt, TEXT("otf")) == 0 )
		{
			return(ReadFontNameTTF(pszFontPath, pszFontName));
		}
	}

	return(TYPE_NULL);
}

FONTTYPE RFNAPI ReadFontNameFON( PCTSTR pszFontPath, PTSTR pszFontName )
{
	HANDLE hFile;
	IMAGE_DOS_HEADER doshdr;
	IMAGE_OS2_HEADER os2hdr;
	UINT8 cbName;

	CHAR szBuffer[MAX_FONTNAME];
	LPSTR pszFontNameA = szBuffer;

	if ((hFile = RFNOpen(pszFontPath)) == INVALID_HANDLE_VALUE)
		return(TYPE_NULL);

	RFNRead(hFile, &doshdr, sizeof(doshdr));

	RFNSeek(hFile, doshdr.e_lfanew, FILE_BEGIN);
	RFNRead(hFile, &os2hdr, sizeof(os2hdr));

	RFNSeek(hFile, os2hdr.ne_nrestab, FILE_BEGIN);
	RFNRead(hFile, &cbName, sizeof(cbName));
	RFNRead(hFile, pszFontNameA, cbName);

	CloseHandle(hFile);

	// Make sure we are null-terminated!
	pszFontNameA[cbName] = 0;

	// We should strip the FONTRES data from the name.
	// First, skip everything up to the colon...
	while (*pszFontNameA && *pszFontNameA != ':') ++pszFontNameA;

	// Then skip any whitespace following the colon...
	if (*pszFontNameA)
		while (*++pszFontNameA == ' ') { }

	if (!(*pszFontNameA)) pszFontNameA = szBuffer;

	// Copy (and thunk, if necessary) to the result buffer.
	AStrToTStr(pszFontNameA, pszFontName, MAX_FONTNAME);

	return((*pszFontNameA) ? TYPE_FON : TYPE_NULL);
}

FONTTYPE RFNAPI ReadFontNameTTF( PCTSTR pszFontPath, PTSTR pszFontName )
{
	HANDLE             hFile;
	FONTTYPE           ftResult = TYPE_TTF;
	UINT8              i, j;
	UINT16             cNames;
	UINT16             cTables;
	UINT               uFoundNames = 0;
	TCHAR              szNames[MAX_TTC][MAX_FONTNAME];
	WCHAR              szBuffer[MAX_FONTNAME];
	INT                iScore, iScorePrev;
	SFNTTTCHEADERSTUB  sfntTTCHeaderStub;
	SFNTOFFSETTABLE    sfntOffsetTable;
	SFNTDIRECTORYENTRY sfntTable;
	SFNTNAMINGTABLE    sfntNamingTable;
	SFNTNAMERECORD     sfntNameRecord;

	if ((hFile = RFNOpen(pszFontPath)) == INVALID_HANDLE_VALUE)
		return(TYPE_NULL);

	RFNRead(hFile, &sfntTTCHeaderStub, sizeof(sfntTTCHeaderStub));

	if (sfntTTCHeaderStub.tag == tag_TTCTag)
	{
		// This is a TTC font.
		sfntTTCHeaderStub.numFonts = SwapV32(sfntTTCHeaderStub.numFonts);
		if (sfntTTCHeaderStub.numFonts > MAX_TTC)
			sfntTTCHeaderStub.numFonts = MAX_TTC;
	}
	else
	{
		// If this is not a TTC, then pretend that we are working with a TTC
		// with only one font.
		sfntTTCHeaderStub.numFonts = 1;
		sfntTTCHeaderStub.offsets[0] = 0;
	}

	for (i = 0; i < sfntTTCHeaderStub.numFonts; ++i)
	{
		/* First off, read the initial directory header on the TTF.  We're only
		 * interested in the "numOffsets" variable to tell us how many tables
		 * are present in this file.
		 *
		 * Remember to always convert from Motorola format (Big Endian to
		 * Little Endian).
		 */
		RFNSeek(hFile, SwapV32(sfntTTCHeaderStub.offsets[i]), FILE_BEGIN);
		RFNRead(hFile, &sfntOffsetTable, sizeof(sfntOffsetTable));

		// Get (and cap, if necessary) the table count.
		cTables = SwapV16(sfntOffsetTable.numOffsets);
		if (cTables > MAX_TABLES)
			cTables = MAX_TABLES;

		// OTF check #1: Does it have an OTF signature?
		if (sfntOffsetTable.version == tag_OTSig)
			ftResult = TYPE_OTF;

		for (j = 0; j < cTables; ++j)
		{
			if (RFNRead(hFile, &sfntTable, sizeof(sfntTable)) != sizeof(sfntTable))
			{
				// Oops, looks like we just hit the end of the file!
				break;
			}
			else if (sfntTable.tag == tag_CFFTable)
			{
				// OTF check #2: Does it have an CFF table?
				ftResult = TYPE_OTF;
			}
			else if (sfntTable.tag == tag_NamingTable)
			{
				/* Now that we've found the entry for the name table, seek to that
				 * position in the file and read in the initial header for this
				 * particular table.  See "True Type Font Files" for information
				 * on this record layout.
				 */
				RFNSeek(hFile, SwapV32(sfntTable.offset), FILE_BEGIN);
				RFNRead(hFile, &sfntNamingTable, sizeof(sfntNamingTable));
				cNames = SwapV16(sfntNamingTable.count);

				/* For most fonts, there are many versions of the font name.
				 * We will look at every name, and select the one with the
				 * highest preference score.
				 */
				iScorePrev = 0;

				while (cNames--)
				{
					RFNRead(hFile, &sfntNameRecord, sizeof(sfntNameRecord));

					if (SwapV16(sfntNameRecord.nameID) == 4)
					{
						UINT16 pid = SwapV16(sfntNameRecord.platformID);
						LANGID lid = SwapV16(sfntNameRecord.languageID);

						LONG lPosition = RFNSeek(hFile, 0, FILE_CURRENT);

						LONG lOffset = SwapV32(sfntTable.offset) +
						               SwapV16(sfntNamingTable.stringOffset) +
						               SwapV16(sfntNameRecord.offset);

						ZeroMemory(szBuffer, sizeof(szBuffer));
						RFNSeek(hFile, lOffset, FILE_BEGIN);
						RFNRead(hFile, szBuffer, SwapV16(sfntNameRecord.length));
						RFNSeek(hFile, lPosition, FILE_BEGIN);

						/* Set preference score.
						 *
						 * 1: pid=3, Microsoft name (generic)
						 * 2: pid=3, Microsoft name with lang=en-US (0x0409)
						 * 3: pid=0, Unicode name
						 * 4: pid=3, Microsoft name with lang=neutral
						 * 5: pid=1, Legacy name (8-bit)
						 * 6: pid=3, Microsoft name with matching lang
						 * 7: pid=3, Microsoft name with matching lang & sublang
						 */
						switch (pid)
						{
							case 0:
								iScore = 3;
								break;

							case 1:
								iScore = 5;
								break;

							case 3:
								if (lid == GetSystemDefaultUILanguage())
									iScore = 7;
								else if (PRIMARYLANGID(lid) == PRIMARYLANGID(GetSystemDefaultUILanguage()))
									iScore = 6;
								else if (PRIMARYLANGID(lid) == LANG_NEUTRAL)
									iScore = 4;
								else if (lid == 0x0409)
									iScore = 2;
								else
									iScore = 1;
								break;

							default:
								iScore = 0;
						}

						if (pid != 1)
						{
							SwapA16I(szBuffer, countof(szBuffer));
							if (szBuffer[0] == 0) iScore = 0;
						}
						else if (*((LPSTR)szBuffer) == 0)
						{
							iScore = 0;
						}

						if (iScore > iScorePrev)
						{
							if (pid != 1)
								WStrToTStr(szBuffer, szNames[uFoundNames], countof(szNames[uFoundNames]));
							else
								AStrToTStr((LPSTR)szBuffer, szNames[uFoundNames], countof(szNames[uFoundNames]));

							iScorePrev = iScore;
						}
					}
				} // name table loop

				if (iScorePrev) ++uFoundNames;

				// Since we have found the naming table, we can stop looking
				break;
			}
		} // TTF table loop
	}

	CloseHandle(hFile);

	if (uFoundNames)
	{
		for (i = 0; i < uFoundNames; ++i)
		{
			if (i)
			{
				SSCpy4Ch(pszFontName, TEXT(' '), TEXT('&'), TEXT(' '), 0);
				pszFontName += 3;
			}

			pszFontName = SSChainCpy(pszFontName, szNames[i]);
		}

		if (ftResult == TYPE_TTF)
			SSStaticCpy(pszFontName, TEXT(" (TrueType)"));
		else
			SSStaticCpy(pszFontName, TEXT(" (OpenType)"));

		return(ftResult);
	}

	return(TYPE_NULL);
}

HANDLE RFNAPI RFNOpen( PCTSTR pszPath )
{
	return(CreateFile(
		pszPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	));
}

DWORD WINAPI RFNRead( HANDLE hFile, PVOID pvBuffer, DWORD cbBuffer )
{
	return((ReadFile(hFile, pvBuffer, cbBuffer, &cbBuffer, NULL)) ? cbBuffer : 0);
}

DWORD WINAPI RFNSeek( HANDLE hFile, LONG lOffset, DWORD dwOrigin )
{
	return(SetFilePointer(hFile, lOffset, NULL, dwOrigin));
}
