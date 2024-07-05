#include "commandlineoptions.h"
#include "utilities.h"

#include <algorithm>
#include <cctype>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

using namespace std;

/// CLI11-validator for the option "--verbosity".
struct VerbosityValidator : public CLI::Validator
{
    VerbosityValidator()
    {
        this->name_ = "VerbosityValidator";
        this->func_ = [](const std::string& str) -> string
            {
                const bool parsed_ok = CommandLineOptions::TryParseVerbosityLevel(str, nullptr);
                if (!parsed_ok)
                {
                    ostringstream string_stream;
                    string_stream << "Invalid verbosity given \"" << str << "\".";
                    throw CLI::ValidationError(string_stream.str());
                }

                return {};
            };
    }
};


CommandLineOptions::ParseResult CommandLineOptions::CommandLineOptions::Parse(int argc, char** argv)
{
    // specify the string-to-enum-mapping for "command"
    std::map<string, Command> map_string_to_command
    {
        { "DryRun", Command::DryRun },
        { "Patch",  Command::Patch },
    };

    const static VerbosityValidator verbosity_validator;

    CLI::App cli_app{ };

    Command argument_command;
    string argument_source_filename;
    string argument_verbosity;

    cli_app.add_option("-c,--command", argument_command,
        R"(Can be one of 'DryRun' or 'Patch' - only with 'Patch' the CZI-file is modified. Default is 'DryRun'.)")
        ->default_val(Command::DryRun)
        ->option_text("COMMAND")
        ->transform(CLI::CheckedTransformer(map_string_to_command, CLI::ignore_case));

    cli_app.add_option("-v,--verbosity", argument_verbosity,
            "Set the verbosity of the output to stdout of the program. Can be either 'quiet', 'normal' or 'verbose'.")
        ->option_text("VERBOSITY")
        ->check(verbosity_validator);

    cli_app.add_option("-f,--file", argument_source_filename,
        "Specifies the CZI-file to operate on.")
        ->option_text("SOURCEFILE");

    try
    {
        cli_app.parse(argc, argv);
    }
    catch (const CLI::CallForHelp& e)
    {
        cli_app.exit(e);
        return ParseResult::Exit;
    }
    catch (const CLI::ParseError& e)
    {
        cli_app.exit(e);
        return ParseResult::Error;
    }

    if (!argument_source_filename.empty())
    {
        this->czi_filename_ = Utilities::convertUtf8ToUCS2(argument_source_filename);
    }

    if (!argument_verbosity.empty())
    {
        CommandLineOptions::TryParseVerbosityLevel(argument_verbosity, &this->verbosity_);
    }

    this->command_ = argument_command;

    return ParseResult::OK;
}

/*static*/bool CommandLineOptions::TryParseVerbosityLevel(const std::string& s, Verbosity* verbosity)
{
    auto lower_case = s;
    transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);
    if (lower_case == "quiet")
    {
        if (verbosity != nullptr)
        {
            *verbosity = Verbosity::Quiet;
        }

        return true;
    }
    else if (lower_case == "normal")
    {
        if (verbosity != nullptr)
        {
            *verbosity = Verbosity::Normal;
        }

        return true;
    }
    else if (lower_case == "verbose")
    {
        if (verbosity != nullptr)
        {
            *verbosity = Verbosity::Verbose;
        }

        return true;
    }

    return false;
}

bool CommandLineOptions::IsVerbosityGreaterOrEqual(Verbosity verbosity) const
{
    return static_cast<typename underlying_type<Verbosity>::type>(this->verbosity_) >= static_cast<typename underlying_type<Verbosity>::type>(verbosity);
}
