// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <vector>
#include "AssetType.h"

namespace CommandLine
{
    void PrintHelp();

    void ParseCommandLineArguments(
        int argc, wchar_t *argv[],
        std::wstring& inputFilePath, AssetType& inputAssetType, std::wstring& outFilePath, std::wstring& tempDirectory,
        std::vector<std::wstring>& lodFilePaths, std::vector<double>& screenCoveragePercentages, size_t& maxTextureSize);
};

