#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "ReadFontName.h"
#include "libs\GetArgv.h"
#include "libs\SimpleString.h"

// Stuff to exclude from directory scans:
#define FILE_ATTRIBUTE_EXCLUDE ( FILE_ATTRIBUTE_DIRECTORY | \
                                 FILE_ATTRIBUTE_OFFLINE   | \
                                 FILE_ATTRIBUTE_HIDDEN    | \
                                 FILE_ATTRIBUTE_SYSTEM )

__forceinline
VOID WINAPI ReadFonts( PTSTR pszPath );
__forceinline
BOOL WINAPI IsInvalidType( FONTTYPE fonttype, BOOL fIgnoreFON );

UINT main( UINT argc, PTSTR *argv )
{
	if (argc == 2)
	{
		TCHAR szPath[MAX_PATH];
		SSChainCpy(szPath, argv[1]);

		if (szPath[SSLen(szPath) - 1] != TEXT('\\'))
			SSCat(szPath, TEXT("\\"));

		ReadFonts(szPath);

		return(0);
	}
	else
	{
		printf("Usage: TestFontRead.exe FontDirectory\n");
		return(1);
	}
}

VOID WINAPI ReadFonts( PTSTR pszPath )
{
	PTSTR pszPathAppend = pszPath + SSLen(pszPath);

	HANDLE hFind;
	WIN32_FIND_DATA finddata;

	// If this is run at an early stage of Windows setup (prior to the
	// cmdlines.txt stage), the hidden attributes have not yet been set on
	// system fonts, which means that we must play it safe and ignore FON files
	// if this is the case.  Our litmus test: is marlett.ttf hidden?
	BOOL fIgnoreFON;
	SSStaticCpy(pszPathAppend, TEXT("marlett.ttf"));
	fIgnoreFON = !(GetFileAttributes(pszPath) & FILE_ATTRIBUTE_HIDDEN);

	SSCpy2Ch(pszPathAppend, TEXT('*'), 0);

	if ((hFind = FindFirstFile(pszPath, &finddata)) != INVALID_HANDLE_VALUE)
	{
		TCHAR szFontName[MAX_FONTNAME];
		PTSTR pszFontName;

		do
		{
			if (!( finddata.dwFileAttributes & FILE_ATTRIBUTE_EXCLUDE ||
			       lstrcmpi(finddata.cFileName, TEXT("marlett.ttf")) == 0 ))
			{
				SSChainCpy(pszPathAppend, finddata.cFileName);

				pszFontName = (IsInvalidType(ReadFontName(pszPath, szFontName), fIgnoreFON)) ?
					TEXT("READ_ERROR") :
					szFontName;

				_tprintf(TEXT("%-12s : %s\n"), finddata.cFileName, pszFontName);
			}

		} while (FindNextFile(hFind, &finddata));

		FindClose(hFind);
	}
}

BOOL WINAPI IsInvalidType( FONTTYPE fonttype, BOOL fIgnoreFON )
{
	return(fonttype == TYPE_NULL || (fIgnoreFON && fonttype == TYPE_FON));
}

#pragma comment(linker, "/entry:TestFontRead")
void TestFontRead( )
{
	UINT argc = 0;
	PTSTR *argv = GetArgv(&argc);
	UINT uExitCode = main(argc, argv);
	LocalFree(argv);
	ExitProcess(uExitCode);
}
