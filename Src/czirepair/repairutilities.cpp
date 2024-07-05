#include "repairutilities.h"

#include "../libCZI/CziParse.h"

using namespace std;
using namespace libCZI;

std::vector<RepairUtilities::SubBlockDimensionInfoRepairInfo> RepairUtilities::GetRepairInfo(libCZI::ICZIReader* reader)
{
    std::vector<SubBlockDimensionInfoRepairInfo> result;

    // go through all sub-blocks
    reader->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& subblock_info)->bool
        {
            // we are only interested in JPGXR-compressed sub-blocks
            if (subblock_info.GetCompressionMode() == CompressionMode::JpgXr)
            {
                // read the sub-block
                const auto sub_block = reader->ReadSubBlock(index);

                // determine the width and height of the JPGXR-compressed bitmap
                uint32_t width_from_jpgxr, height_from_jpgxr;
                if (!sub_block->TryGetWidthAndHeightOfJpgxrCompressedBitmap(width_from_jpgxr, height_from_jpgxr))
                {
                    throw runtime_error("Failed to determine the width and height of the JPGXR-compressed bitmap.");
                }

                // Now, compare the width and height of the JPGXR-compressed bitmap with the width and height in the dimension-info.
                // If they differ, we need to fix the dimension-info - add the sub-block to the list of sub-blocks that need to be fixed.
                SubBlockDimensionInfoRepairInfo repair_info;
                repair_info.sub_block_index = index;
                if (subblock_info.physicalSize.w != width_from_jpgxr)
                {
                    repair_info.fixed_size_x = width_from_jpgxr;
                }

                if (subblock_info.physicalSize.h != height_from_jpgxr)
                {
                    repair_info.fixed_size_y = height_from_jpgxr;
                }

                // If the width or height of the sub-block should be fixed, add the sub-block to the list of sub-blocks that need to be fixed.
                if (repair_info.IsFixedSizeXValid() || repair_info.IsFixedSizeYValid())
                {
                    result.push_back(repair_info);
                }
            }

            return true;
        });

    return result;
}

void RepairUtilities::PatchSubBlockDimensionInfoInSubBlockDirectory(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list)
{
    CFileHeaderSegmentData file_header_segment_data = CCZIParse::ReadFileHeaderSegmentData(io_stream);

    CCZIParse::InplacePatchSubBlockDirectory(
        io_stream,
        file_header_segment_data.GetSubBlockDirectoryPosition(),
        [&](int sub_block_index, char dimension_identifier, std::int32_t size, std::int32_t& new_coordinate)->bool
        {
            if (dimension_identifier != 'X' && dimension_identifier != 'Y')
            {
                return false;
            }

            for (const auto& repair_info : patch_list)
            {
                if (repair_info.sub_block_index == sub_block_index)
                {
                    if (repair_info.IsFixedSizeXValid() && dimension_identifier == 'X')
                    {
                        new_coordinate = repair_info.fixed_size_x;
                        return true;
                    }

                    if (repair_info.IsFixedSizeYValid() && dimension_identifier == 'Y')
                    {
                        new_coordinate = repair_info.fixed_size_y;
                        return true;
                    }
                }
            }

            return false;
        });
}

void RepairUtilities::PatchSubBlocks(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list)
{
    CFileHeaderSegmentData file_header_segment_data = CCZIParse::ReadFileHeaderSegmentData(io_stream);
    CCZIParse::SubblockDirectoryParseOptions subblock_directory_parse_options;
    subblock_directory_parse_options.SetLaxParsing();
    auto sub_block_directory = CCZIParse::ReadSubBlockDirectory(io_stream, file_header_segment_data.GetSubBlockDirectoryPosition(), subblock_directory_parse_options);

    for (const auto& repair_info : patch_list)
    {
        CCziSubBlockDirectoryBase::SubBlkEntry sub_block_entry;
        sub_block_directory.TryGetSubBlock(repair_info.sub_block_index, sub_block_entry);

        CCZIParse::InplacePatchSubblock(
                io_stream,
                sub_block_entry.FilePosition,
                [&](char dimension_identifier, std::int32_t size, std::int32_t& new_coordinate)->bool
                {
                    if (dimension_identifier != 'X' && dimension_identifier != 'Y')
                    {
                        return false;
                    }

                    if (repair_info.IsFixedSizeXValid() && dimension_identifier == 'X')
                    {
                        new_coordinate = repair_info.fixed_size_x;
                        return true;
                    }

                    if (repair_info.IsFixedSizeYValid() && dimension_identifier == 'Y')
                    {
                        new_coordinate = repair_info.fixed_size_y;
                        return true;
                    }

                    return false;
                });
    }
}
