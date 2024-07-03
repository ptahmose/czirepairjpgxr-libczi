#include "commandlineoptions.h"
#include "platform_defines.h"

#include <iostream>

#include "utilities.h"

#include "../libCZI/libCZI.h"

using namespace std;
using namespace libCZI;

static void DryRun(const CommandLineOptions& options);

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

    DryRun(options);

    return 0;
}

void DryRun(const CommandLineOptions& options)
{
    shared_ptr<IStream> stream = libCZI::CreateStreamFromFile(options.GetCZIFilename().c_str());
    auto spReader = libCZI::CreateCZIReader();
    spReader->Open(stream);

    for (int i = 0;; ++i)
    {
        auto sub_block = spReader->ReadSubBlock(i);

        if (!sub_block)
        {
            break;
        }

        uint32_t width, height;
        sub_block->TryGetWidthAndHeightOfJpgxrCompressedBitmap(width, height);

        auto subblock_info = sub_block->GetSubBlockInfo();

        if (subblock_info.physicalSize.w != width || subblock_info.physicalSize.h != height)
        {
            cout << "Subblock " << i << ": subblock_info: " << subblock_info.physicalSize.w << "x" << subblock_info.physicalSize.h << ", decompressed size: " << width << "x" << height << endl;
        }
    }
}
