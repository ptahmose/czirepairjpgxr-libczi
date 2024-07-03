#pragma once

#include <string>
#include <vector>
#include "platform_defines.h"

class Utilities
{
public:
    static std::string convertToUtf8(const std::wstring& str);
    static std::wstring convertUtf8ToUCS2(const std::string& str);
};

#if defined(WIN32ENV)
/// A utility which is providing the command-line arguments (on Windows) as UTF8-encoded strings.
class CommandlineArgsWindowsHelper
{
private:
    std::vector<std::string> arguments_;
    std::vector<char*> pointers_to_arguments_;
public:
    /// Constructor.
    CommandlineArgsWindowsHelper();

    /// Gets an array of pointers to null-terminated, UTF8-encoded arguments. This size of this array is given
    /// by the "GetArgc"-method.
    /// Note that this pointer is only valid for the lifetime of this instance of the CommandlineArgsWindowsHelper-class.
    ///
    /// \returns    Pointer to an array of pointers to null-terminated, UTF8-encoded arguments.
    char** GetArgv();

    /// Gets the number of arguments.
    ///
    /// \returns    The number of arguments.
    int GetArgc();
};
#endif
