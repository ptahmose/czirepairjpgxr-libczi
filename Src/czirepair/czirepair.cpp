#include "commandlineoptions.h"
#include "platform_defines.h"

#include <iostream>

#include "repairutilities.h"
#include "utilities.h"

#include "../libCZI/libCZI.h"

using namespace std;
using namespace libCZI;

namespace
{
    void DryRun(const CommandLineOptions& options)
    {
        const shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
        const auto reader = libCZI::CreateCZIReader();
        reader->Open(stream);

        vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> repair_info = RepairUtilities::GetRepairInfo(reader.get());

        if (repair_info.empty())
        {
            cout << "No repair needed." << std::endl;
        }
        else
        {
            cout << "Found discrepancies with " << repair_info.size() << " sub-block(s)." << std::endl;

            if (options.IsVerbosityGreaterOrEqual(Verbosity::Verbose))
            {
                for (const auto& info : repair_info)
                {
                    SubBlockInfo sub_block_info;
                    reader->TryGetSubBlockInfo(info.sub_block_index, &sub_block_info);
                    cout << "SubBlockIndex: " << info.sub_block_index << " -> size in 'dimension_info': " <<
                        sub_block_info.physicalSize.w << "x" << sub_block_info.physicalSize.h << ", JPGXR: " <<
                        (info.IsFixedSizeXValid() ? info.fixed_size_x : sub_block_info.physicalSize.w) << "x" <<
                        (info.IsFixedSizeYValid() ? info.fixed_size_y : sub_block_info.physicalSize.h) << std::endl;
                }
            }
        }
    }

    void Patch(const CommandLineOptions& options)
    {
        vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> repair_info;

        {
            const shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
            const auto reader = libCZI::CreateCZIReader();
            reader->Open(stream);

            repair_info = RepairUtilities::GetRepairInfo(reader.get());

            if (repair_info.empty())
            {
                cout << "No repair needed." << endl;
                return;
            }
            else
            {
                cout << "Found discrepancies with " << repair_info.size() << " sub-block(s)." << std::endl;

                if (options.IsVerbosityGreaterOrEqual(Verbosity::Verbose))
                {
                    for (const auto& info : repair_info)
                    {
                        cout << "SubBlockIndex: " << info.sub_block_index << " -> size in 'dimension_info': " <<
                            info.fixed_size_x << "x" << info.fixed_size_y << std::endl;
                        cout << std::endl;
                    }
                }
            }
        }

        cout << "Now opening the file in read-write-mode and will patch the file." << std::endl;
        shared_ptr<IInputOutputStream> input_output_stream = libCZI::CreateInputOutputStreamForFile(options.GetCZIFilename().c_str());

        RepairUtilities::PatchSubBlockDimensionInfoInSubBlockDirectory(input_output_stream.get(), repair_info);
        cout << "Patched the subblock-directory." << std::endl;

        RepairUtilities::PatchSubBlocks(input_output_stream.get(), repair_info);
        cout << "Patched the subblocks." << std::endl;

        input_output_stream.reset();
    }
}

int main(int argc, char** argv)
{
    CommandLineOptions options;
#if defined(WIN32ENV)
    CommandlineArgsWindowsHelper args_helper;
    auto commandline_parsing_result = options.Parse(args_helper.GetArgc(), args_helper.GetArgv());
#else
    auto commandline_parsing_result = options.Parse(argc, argv);
#endif

    if (commandline_parsing_result == CommandLineOptions::ParseResult::Error)
    {
        std::cerr << "Error parsing command line arguments" << std::endl;
        return 1;
    }
    else if (commandline_parsing_result == CommandLineOptions::ParseResult::Exit)
    {
        return EXIT_SUCCESS;
    }

    if (options.IsVerbosityGreaterOrEqual(Verbosity::Verbose))
    {
        const char* command_text;
        switch (options.GetCommand())
        {
        case Command::DryRun:
            command_text = "DryRun";
            break;
        case Command::Patch:
            command_text = "Patch";
            break;
        default:
            command_text = "Invalid";
            break;
        }

        std::cout << "Command: " << command_text << std::endl;
        std::cout << "Filename: " << Utilities::convertToUtf8(options.GetCZIFilename()) << std::endl << std::endl;
    }

    if (options.GetCommand() == Command::DryRun)
    {
        DryRun(options);
    }
    else if (options.GetCommand() == Command::Patch)
    {
        Patch(options);
    }

    return EXIT_SUCCESS;
}
