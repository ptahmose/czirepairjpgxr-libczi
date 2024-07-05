#pragma once

#include <cstdint>
#include <vector>
#include "../libCZI/libCZI.h"

class RepairUtilities
{
public:
    struct SubBlockDimensionInfoRepairInfo
    {
        int	sub_block_index{ -1 };
        std::uint32_t fixed_size_x{ std::numeric_limits<std::uint32_t>::max() };
        std::uint32_t fixed_size_y{ std::numeric_limits<std::uint32_t>::max() };

        bool IsFixedSizeXValid() const { return fixed_size_x != std::numeric_limits<std::uint32_t>::max(); }
        bool IsFixedSizeYValid() const { return fixed_size_y != std::numeric_limits<std::uint32_t>::max(); }
    };

    static std::vector<SubBlockDimensionInfoRepairInfo> GetRepairInfo(libCZI::ICZIReader* reader);

    static void PatchSubBlockDimensionInfoInSubBlockDirectory(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list);
    static void PatchSubBlocks(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list);
};
