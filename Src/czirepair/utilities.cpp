#include "utilities.h"

#include <locale>
#include <codecvt>
#include <memory>

#if defined(WIN32ENV)
#include <Windows.h>
#endif

using namespace std;

std::string Utilities::convertToUtf8(const std::wstring& wide_str)
{
#if defined(WIN32ENV)
    int byte_count = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, NULL, 0, NULL, NULL);
    if (byte_count == 0) 
    {
        // Handle error (e.g., invalid wstring)
        return "";  // Empty string on error
    }

    // Allocate memory for the UTF-8 string (including null terminator)
    std::string utf8_str(byte_count, '\0');

    // Perform the actual conversion
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, &utf8_str[0], byte_count, NULL, NULL);

    return utf8_str;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
    std::string conv = utf8_conv.to_bytes(str);
    return conv;
#endif
}

std::wstring Utilities::convertUtf8ToUCS2(const std::string& utf8_str)
{
#if defined(WIN32ENV)
    int wide_char_count = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, 0);
    if (wide_char_count == 0) 
    {
        // Handle error (e.g., invalid UTF-8)
        return L"";  // Empty wstring on error
    }

    // Allocate memory for the wide character string
    std::wstring wide_str(wide_char_count, L'\0');

    // Perform the actual conversion
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide_str[0], wide_char_count);

    return wide_str;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8conv;
    std::wstring conv = utf8conv.from_bytes(str);
    return conv;
#endif
}

#if defined(WIN32ENV)
CommandlineArgsWindowsHelper::CommandlineArgsWindowsHelper()
{
    int number_arguments;
    const unique_ptr<LPWSTR, decltype(LocalFree)*> wide_argv
    {
        CommandLineToArgvW(GetCommandLineW(), &number_arguments),
        &LocalFree
    };

    this->pointers_to_arguments_.reserve(number_arguments);
    this->arguments_.reserve(number_arguments);

    for (int i = 0; i < number_arguments; ++i)
    {
        this->arguments_.emplace_back(Utilities::convertToUtf8(wide_argv.get()[i]));
    }

    for (int i = 0; i < number_arguments; ++i)
    {
        this->pointers_to_arguments_.emplace_back(
            const_cast<char*>(this->arguments_[i].c_str()));
    }
}

char** CommandlineArgsWindowsHelper::GetArgv()
{
    return this->pointers_to_arguments_.data();
}

int CommandlineArgsWindowsHelper::GetArgc()
{
    return static_cast<int>(this->pointers_to_arguments_.size());
}
#endif
