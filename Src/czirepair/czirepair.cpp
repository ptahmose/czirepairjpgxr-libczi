// SPDX-FileCopyrightText: 2024 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "commandlineoptions.h"
#include "platform_defines.h"

#include <iostream>
#include <functional>

#include "repairutilities.h"
#include "utilities.h"

#include "../libCZI/libCZI.h"

using namespace std;
using namespace libCZI;

namespace
{
    class ProgressReporter
    {
        int max_line_length_{ -1 };
    public:
        void operator()(const RepairUtilities::ProgressInfo& progress_info)
        {
            ostringstream oss;
            oss << "Processing sub-block " << progress_info.current_sub_block_index << " of " << progress_info.total_sub_block_count << '\r';
            const auto line = oss.str();
            this->max_line_length_ = max(this->max_line_length_, static_cast<int>(line.size()));
            cout << oss.str();
        }

        void ClearLine()
        {
            if (this->max_line_length_ > 0)
            {
                cout << string(this->max_line_length_, ' ') << '\r';
                this->max_line_length_ = -1;
            }
        }
    };

    void DryRun(const CommandLineOptions& options)
    {
        const shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
        const auto reader = libCZI::CreateCZIReader();
        reader->Open(stream);

        std::function<void(const RepairUtilities::ProgressInfo&)> progress_reporter_functor;
        ProgressReporter progress_reporter;
        if (Utilities::IsStdOutATerminal() && options.IsVerbosityGreaterOrEqual(Verbosity::Normal))
        {
            progress_reporter_functor = std::ref(progress_reporter);
        }

        vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> repair_info = RepairUtilities::GetRepairInfo(
            reader.get(),
            progress_reporter_functor);
        progress_reporter.ClearLine();

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

                cout << std::endl;
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

            std::function<void(const RepairUtilities::ProgressInfo&)> progress_reporter_functor;
            ProgressReporter progress_reporter;
            if (Utilities::IsStdOutATerminal() && options.IsVerbosityGreaterOrEqual(Verbosity::Normal))
            {
                progress_reporter_functor = std::ref(progress_reporter);
            }

            repair_info = RepairUtilities::GetRepairInfo(reader.get(), progress_reporter_functor);
            progress_reporter.ClearLine();

            if (repair_info.empty())
            {
                cout << "No repair needed." << endl;
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

                    cout << std::endl;
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

    int DoOperationAndHandleException(const CommandLineOptions& options, const function<void(const CommandLineOptions&)> operation)
    {
        try
        {
            operation(options);
            return EXIT_SUCCESS;
        }
        catch (const exception& e)
        {
            cerr << "An error occurred: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
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

    int return_code = EXIT_FAILURE;
    if (options.GetCommand() == Command::DryRun)
    {
        return_code = DoOperationAndHandleException(options, DryRun);
    }
    else if (options.GetCommand() == Command::Patch)
    {
        return_code = DoOperationAndHandleException(options, Patch);
    }

    return return_code;
}
