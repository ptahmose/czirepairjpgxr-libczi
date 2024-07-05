// SPDX-FileCopyrightText: 2024 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <string>

/// Values that represent the command - the operation this program should perform.
enum class Command
{
    Invalid = 0,

    DryRun, ///< An enum constant representing the "dry run" option - i.e. just determine what would be done.

    Patch,  ///< An enum constant representing the "patch option" - i.e. actually patch the file if necessary.
};

enum class Verbosity
{
    Invalid = 0,

    Quiet,

    Normal,

    Verbose,
};

class CommandLineOptions
{
public:
    /// Values that represent the result of the "Parse"-operation.
    enum class ParseResult
    {
        OK,     ///< An enum constant representing the result "arguments successfully parsed, operation can start".
        Exit,   ///< An enum constant representing the result "operation complete, the program should now be terminated, e.g. the synopsis was printed".
        Error   ///< An enum constant representing the result "there was an error parsing the command line arguments, program should terminate".
    };
private:
    std::wstring czi_filename_;
    Command command_{ Command::Invalid };
    Verbosity verbosity_{ Verbosity::Normal };

public:
    ParseResult Parse(int argc, char** argv);

    const std::wstring& GetCZIFilename() const { return this->czi_filename_; }
    Command GetCommand() const { return this->command_; }
    Verbosity GetVerbosity() const { return this->verbosity_; }

    bool IsVerbosityGreaterOrEqual(Verbosity verbosity) const;

    static bool TryParseVerbosityLevel(const std::string& s, Verbosity* verbosity);
};
