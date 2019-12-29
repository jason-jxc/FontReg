#include <windows.h>
#include "SimpleString.h"

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(x[0]))
#endif

/**
 * The wide counterpart to GetArgvA is defined as a macro in the header file.
 **/

PSTR * __fastcall GetArgvA( INT *pcArgs )
{
	PWSTR *argv = CommandLineToArgvW(GetCommandLineW(), pcArgs);
	WCHAR szTempW[0x200];
	INT i;

	for (i = 0; i < *pcArgs; ++i)
	{
		UINT cchArg = (UINT)SSLenW(argv[i]) + 1;
		if (cchArg > countof(szTempW))
			cchArg = countof(szTempW);

		SSChainNCpyW(szTempW, argv[i], cchArg);
		szTempW[countof(szTempW) - 1] = 0;

		WideCharToMultiByte(
			CP_ACP,
			0,
			szTempW,
			cchArg,
			(PSTR)argv[i],
			cchArg * sizeof(WCHAR),
			NULL,
			NULL
		);
	}

	return((PSTR *)argv);
}
