// SPDX-FileCopyrightText: 2024 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include "../libCZI/libCZI.h"

class RepairUtilities
{
public:
    /// This structure gather information about fixes that need to be applied to a sub-block's dimension-info.
    struct SubBlockDimensionInfoRepairInfo
    {
        int sub_block_index{ -1 };  ///< This index of the sub block.

        std::uint32_t fixed_size_x{ std::numeric_limits<std::uint32_t>::max() };    ///< The width the sub-block should have (in the dimension-info).
        std::uint32_t fixed_size_y{ std::numeric_limits<std::uint32_t>::max() };    ///< The height the sub-block should have (in the dimension-info).

        /// Query if the width of the sub-block should be fixed (and the property fixed_size_x is valid).
        ///
        /// \returns    True if the property fixed_size_x is valid, false if not.
        bool IsFixedSizeXValid() const { return fixed_size_x != std::numeric_limits<std::uint32_t>::max(); }

        /// Query if the width of the sub-block should be fixed (and the property fixed_size_y is valid).
        ///
        /// \returns    True if the property fixed_size_y is valid, false if not.
        bool IsFixedSizeYValid() const { return fixed_size_y != std::numeric_limits<std::uint32_t>::max(); }
    };

    struct ProgressInfo
    {
        int current_sub_block_index{ -1 };
        int total_sub_block_count{ -1 };
    };

    static std::vector<SubBlockDimensionInfoRepairInfo> GetRepairInfo(libCZI::ICZIReader* reader, const std::function<void(const ProgressInfo&)>& progress_callback);

    static void PatchSubBlockDimensionInfoInSubBlockDirectory(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list);
    static void PatchSubBlocks(libCZI::IInputOutputStream* io_stream, const std::vector<SubBlockDimensionInfoRepairInfo>& patch_list);
};
