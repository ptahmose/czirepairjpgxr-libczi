# SPDX-FileCopyrightText: 2017-2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set (CZIREPAIRFILES
        "commandlineoptions.h"
        "commandlineoptions.cpp"
        "czirepair.cpp"
        "utilities.h"
        "utilities.cpp"
        "platform_defines.h"
        "repairutilities.cpp"
        "repairutilities.h")

add_executable(CZIrepair ${CZIREPAIRFILES})

set_target_properties(CZIrepair PROPERTIES CXX_STANDARD 11)
target_compile_definitions(CZIrepair PRIVATE _LIBCZISTATICLIB)

target_link_libraries(CZIrepair PRIVATE CLI11::CLI11 libCZIStatic)