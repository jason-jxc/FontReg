// Full name; this will appear in the version info
#if defined(_M_IX86)
#define FONTREG_NAME_STR "Font Registration Utility (x86-32)"
#elif defined(_M_AMD64) || defined(_M_X64)
#define FONTREG_NAME_STR "Font Registration Utility (x86-64)"
#elif defined(_M_IA64)
#define FONTREG_NAME_STR "Font Registration Utility (IA-64)"
#else
#define FONTREG_NAME_STR "Font Registration Utility"
#endif

// Full version: MUST be in the form of major,minor,revision,build
#define FONTREG_VERSION_FULL 2,1,3,0

// String version: May be any suitable string
#define FONTREG_VERSION_STR "2.1.3.0"

// PE version: MUST be in the form of major.minor
#pragma comment(linker, "/version:2.1")
