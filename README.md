# CZIrepair
[![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
[![REUSE status](https://api.reuse.software/badge/github.com/ZEISS/libczi)](https://api.reuse.software/info/github.com/ZEISS/libczi)
[![CMake](https://github.com/ZEISS/libczi/actions/workflows/cmake.yml/badge.svg?branch=main&event=push)](https://github.com/ptahmose/czirepairjpgxr-libczi/actions/workflows/cmake.yml)
[![MegaLinter](https://github.com/ZEISS/libczi/actions/workflows/mega-linter.yml/badge.svg?branch=main&event=push)](https://github.com/ptahmose/czirepairjpgxr-libczi/actions/workflows/mega-linter.yml)

## What
This is repo contains a console application **CZIrepair** which can be used to fix a certain type of malformed CZIs. It is only one type of corruption that the
tool can fix - when the width/height of a sub-block is reported differently in the CZI-file than it actually is, and only in the case of JPGXR-compressed data.
The tool will write the width/height determined from the JPGXR-compressed data to corresponding data-structures at CZI-level.
The tool offers two modes of operation:
* There is a dry-run mode, where the tool will only read the CZI-file and report the issues it found.
* There is a patch mode, where the corruption is fixed in-place (i.e. the original file is modified).

This tool is based on the [libCZI library](https://github.com/ZEISS/libczi). This repo contains a modified version of the libCZI library, which is used by the CZIrepair tool.

# Licensing
* libCZI (and the modifications made to it here) is licensed under the terms of LGPL3.0 (c.f. [here](https://github.com/ZEISS/libczi?tab=readme-ov-file#licensing)).
* CZIrepair is licensed under the terms of LGPL3.0.