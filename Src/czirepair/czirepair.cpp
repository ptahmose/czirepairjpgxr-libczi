#include "commandlineoptions.h"
#include "platform_defines.h"

#include <iostream>

#include "repairutilities.h"
#include "utilities.h"

#include "../libCZI/libCZI.h"

using namespace std;
using namespace libCZI;

static void DryRun(const CommandLineOptions& options);
static void Patch(const CommandLineOptions& options);

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
        return 0;
    }

    std::cout << "Command: " << static_cast<int>(options.GetCommand()) << std::endl;
    //std::cout << "CZI filename: " << options.GetCZIFilename() << std::endl;

    if (options.GetCommand() == Command::DryRun)
    {
        DryRun(options);
    }
    else if (options.GetCommand() == Command::Patch)
    {
        Patch(options);
    }


    return 0;
}

void DryRun(const CommandLineOptions& options)
{
    shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
    auto reader = libCZI::CreateCZIReader();
    reader->Open(stream);

    vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> repair_info = RepairUtilities::GetRepairInfo(reader.get());

    if (repair_info.empty())
    {
        cout << "No repair needed." << endl;
    }
    else
    {
        cout << "Found discrepancies with " << repair_info.size() << " sub-block(s)." << endl;

        if (options.IsVerbosityGreaterOrEqual(Verbosity::Verbose))
        {
            for (const auto& info : repair_info)
            {
                SubBlockInfo sub_block_info;
                reader->TryGetSubBlockInfo(info.sub_block_index, &sub_block_info);
                cout << "SubBlockIndex: " << info.sub_block_index << " -> size in 'dimension_info': " <<
                    sub_block_info.physicalSize.w << "x" << sub_block_info.physicalSize.h << ", JPGXR: " <<
                    (info.IsFixedSizeXValid() ? info.fixed_size_x : sub_block_info.physicalSize.w) << "x" << 
                    (info.IsFixedSizeYValid() ? info.fixed_size_y : sub_block_info.physicalSize.h) << endl;
            }
        }
    }
}

void Patch(const CommandLineOptions& options)
{
    vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> repair_info;

    {
        shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
        auto spReader = libCZI::CreateCZIReader();
        spReader->Open(stream);

        repair_info = RepairUtilities::GetRepairInfo(spReader.get());
    }

    shared_ptr<IInputOutputStream> input_output_stream = libCZI::CreateInputOutputStreamForFile(options.GetCZIFilename().c_str());
    RepairUtilities::PatchSubBlockDimensionInfo(input_output_stream.get(), repair_info);
    RepairUtilities::PatchSubBlocks(input_output_stream.get(), repair_info);
    input_output_stream.reset();
}