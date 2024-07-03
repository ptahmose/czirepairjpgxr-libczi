#pragma once

#if defined(_WIN32)
// this means that the Win32-API is available, and the Windows-SDK is available
#define WIN32ENV (1)
#else
// otherwise, we assume that we are on Linux
#define LINUXENV (1)
#endif
