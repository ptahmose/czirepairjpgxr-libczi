#include "commandlineoptions.h"
#include "utilities.h"

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

using namespace std;


CommandLineOptions::ParseResult CommandLineOptions::CommandLineOptions::Parse(int argc, char** argv)
{
    // specify the string-to-enum-mapping for "command"
    std::map<string, Command> map_string_to_command
    {
        { "DryRun",                   Command::DryRun },
        { "Patch",                    Command::Patch },
    };

    CLI::App cli_app{ };

    Command argument_command;
    string argument_source_filename;

    cli_app.add_option("-c,--command", argument_command,
        R"(COMMAND can be one of 'DryRun' or 'Patch')")
        ->default_val(Command::Patch)
        ->option_text("COMMAND")
        ->transform(CLI::CheckedTransformer(map_string_to_command, CLI::ignore_case));

    cli_app.add_option("-f,--file", argument_source_filename,
        "Specifies the source CZI-file.")
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
        this->czi_filename_= Utilities::convertUtf8ToUCS2(argument_source_filename);
    }

    this->command_ = argument_command;
}

