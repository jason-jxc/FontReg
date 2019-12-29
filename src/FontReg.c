#include <windows.h>
#include <shlobj.h>
#include "ReadFontName.h"
#include "libs\GetArgv.h"
#include "libs\SimpleString.h"
#include "version.h"

// Stuff to exclude from directory scans:
#define FILE_ATTRIBUTE_EXCLUDE ( FILE_ATTRIBUTE_DIRECTORY | \
                                 FILE_ATTRIBUTE_OFFLINE   | \
                                 FILE_ATTRIBUTE_HIDDEN    | \
                                 FILE_ATTRIBUTE_SYSTEM )

#define countof(x) (sizeof(x)/sizeof(x[0]))

typedef enum {
	OP_NONE,
	OP_COPY,
	OP_MOVE
} FILEOP, *PFILEOP;

// Main operations; function that are called only once should be inlined
__forceinline
VOID WINAPI InstallFontFiles( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend );
__forceinline
VOID WINAPI RemoveOrphanRegs( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend );
__forceinline
VOID WINAPI RegisterAllFonts( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend );

// Helper functions
__forceinline
BOOL WINAPI CopyMoveFile( PCTSTR pszSource, PCTSTR pszDest, FILEOP op );
__forceinline
BOOL WINAPI IsInvalidType( FONTTYPE fonttype, BOOL fIgnoreFON );
BOOL WINAPI RegisterFont( HKEY hKeyFonts, PCTSTR pszFileName, PCTSTR pszFontName );

#pragma comment(linker, "/entry:FontReg")
VOID WINAPI FontReg( )
{
	UINT uExitCode = 1;
	HKEY hKeyFonts = NULL;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
	                 TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"),
	                 0,
	                 KEY_READ | KEY_WRITE,
	                 &hKeyFonts) == ERROR_SUCCESS)
	{
		TCHAR szFontsRoot[MAX_PATH];

		if (SHGetSpecialFolderPath(NULL, szFontsRoot, CSIDL_FONTS, FALSE))
		{
			PTSTR pszPathAppend = szFontsRoot + SSLen(szFontsRoot);
			SSCpy2Ch(pszPathAppend++, TEXT('\\'), 0);

			InstallFontFiles(hKeyFonts, szFontsRoot, pszPathAppend);
			RemoveOrphanRegs(hKeyFonts, szFontsRoot, pszPathAppend);
			RegisterAllFonts(hKeyFonts, szFontsRoot, pszPathAppend);

			uExitCode = 0;
		}

		RegCloseKey(hKeyFonts);
	}

	ExitProcess(uExitCode);
}

/**
 * Main operations
 **/

VOID WINAPI InstallFontFiles( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend )
{
	UINT argc = 0;
	PTSTR *argv = GetArgv(&argc);

	FILEOP op = OP_NONE;

	if (argc == 2)
	{
		if (lstrcmpi(argv[1], TEXT("/copy")) == 0)
			op = OP_COPY;
		else if (lstrcmpi(argv[1], TEXT("/move")) == 0)
			op = OP_MOVE;
	}

	LocalFree(argv);

	if (op != OP_NONE)
	{
		HANDLE hFind;
		WIN32_FIND_DATA finddata;

		if ((hFind = FindFirstFile(TEXT(".\\*"), &finddata)) != INVALID_HANDLE_VALUE)
		{
			TCHAR szFontName[MAX_FONTNAME];
			BOOL fChanged = FALSE;

			do
			{
				SSChainCpy(pszPathAppend, finddata.cFileName);

				if (!( finddata.dwFileAttributes & FILE_ATTRIBUTE_EXCLUDE ||
				       ReadFontName(finddata.cFileName, szFontName) == TYPE_NULL ||
				       CopyMoveFile(finddata.cFileName, pszPath, op) == FALSE ||
				       AddFontResource(pszPath) == 0 ))
				{
					if (RegisterFont(hKeyFonts, finddata.cFileName, szFontName))
						fChanged = TRUE;
				}

			} while (FindNextFile(hFind, &finddata));

			FindClose(hFind);

			if (fChanged)
				PostMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
		}
	}
}

VOID WINAPI RemoveOrphanRegs( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend )
{
	TCHAR szName[0x400];
	TCHAR szFile[0x400];
	DWORD dwIndex = 0;
	DWORD dwType;
	DWORD cchName = countof(szName);
	DWORD cbFile = sizeof(szFile);

	while (RegEnumValue(hKeyFonts, dwIndex++, szName, &cchName, NULL, &dwType, (PBYTE)szFile, &cbFile) == ERROR_SUCCESS)
	{
		if (dwType == REG_SZ && cbFile > sizeof(TCHAR))
		{
			memcpy(pszPathAppend, szFile, cbFile);

			// In some cases, such as Equation Editor's "MT Extra" font, an
			// absolute path is given instead of a relative path, so we
			// need to check for that case as well.
			if ( GetFileAttributes(pszPath) == INVALID_FILE_ATTRIBUTES &&
			     GetFileAttributes(szFile) == INVALID_FILE_ATTRIBUTES )
			{
				// Font file does not exist, so delete it and reset the
				// index (modifying the registry can affect the indexing).
				if (RegDeleteValue(hKeyFonts, szName) == ERROR_SUCCESS)
					dwIndex = 0;
			}
		}

		cchName = countof(szName);
		cbFile = sizeof(szFile);
	}
}

VOID WINAPI RegisterAllFonts( HKEY hKeyFonts, PTSTR pszPath, PTSTR pszPathAppend )
{
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
		BOOL fChanged = FALSE;

		do
		{
			if (!( finddata.dwFileAttributes & FILE_ATTRIBUTE_EXCLUDE ||
			       lstrcmpi(finddata.cFileName, TEXT("marlett.ttf")) == 0 ))
			{
				SSChainCpy(pszPathAppend, finddata.cFileName);

				if (!( IsInvalidType(ReadFontName(pszPath, szFontName), fIgnoreFON) ||
				       AddFontResource(pszPath) == 0 ))
				{
					if (RegisterFont(hKeyFonts, finddata.cFileName, szFontName))
						fChanged = TRUE;
				}
			}

		} while (FindNextFile(hFind, &finddata));

		FindClose(hFind);

		if (fChanged)
			PostMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
	}
}

/**
 * Helper functions
 **/

BOOL WINAPI CopyMoveFile( PCTSTR pszSource, PCTSTR pszDest, FILEOP op )
{
	if (op == OP_COPY)
		return(CopyFile(pszSource, pszDest, FALSE));
	else
		return(MoveFileEx(pszSource, pszDest, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING));
}

BOOL WINAPI IsInvalidType( FONTTYPE fonttype, BOOL fIgnoreFON )
{
	return(fonttype == TYPE_NULL || (fIgnoreFON && fonttype == TYPE_FON));
}

BOOL WINAPI RegisterFont( HKEY hKeyFonts, PCTSTR pszFileName, PCTSTR pszFontName )
{
	BOOL fFoundDupe = FALSE;
	TCHAR szName[0x400];
	TCHAR szData[0x400];
	DWORD dwIndex = 0;
	DWORD dwType;
	DWORD cchName = countof(szName);
	DWORD cbData = sizeof(szData);

	while (!fFoundDupe && RegEnumValue(hKeyFonts, dwIndex++, szName, &cchName, NULL, &dwType, (PBYTE)szData, &cbData) == ERROR_SUCCESS)
	{
		if (dwType == REG_SZ && cbData > sizeof(TCHAR))
		{
			if (lstrcmpi(szName, pszFontName) == 0 ||
			    lstrcmpi(szData, pszFileName) == 0)
			{
				fFoundDupe = TRUE;
			}
		}

		cchName = countof(szName);
		cbData = sizeof(szData);
	}

	if (!fFoundDupe)
	{
		RegSetValueEx(
			hKeyFonts,
			pszFontName,
			0,
			REG_SZ,
			(PBYTE)pszFileName,
			((DWORD)SSLen(pszFileName) + 1) * sizeof(TCHAR)
		);

		return(TRUE);
	}

	return(FALSE);
}
