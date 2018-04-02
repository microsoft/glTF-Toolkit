// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <vector>
#include "AssetType.h"

namespace CommandLine
{
    enum Platform
    {
        None = 0x0,
        Holographic = 0x1,
        Desktop = 0x2
    };

    void PrintHelp();

    void ParseCommandLineArguments(
        int argc, wchar_t *argv[],
        std::wstring& inputFilePath, AssetType& inputAssetType, std::wstring& outFilePath, std::wstring& tempDirectory,
        std::vector<std::wstring>& lodFilePaths, std::vector<double>& screenCoveragePercentages, size_t& maxTextureSize,
        bool& sharedMaterials, bool& compatibilityMode, Platform& targetPlatforms, bool& replaceTextures);
};

