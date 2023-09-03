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

    enum class Version
    {
        Version1709, // Fall Creators Update (RS3)
        Version1803,  // Spring Creators Update (RS4)
        Version1809,  // Fall 2018 Update (RS5)
        Latest = Version1809
    };

    void PrintHelp();

    void ParseCommandLineArguments(
        int argc, wchar_t *argv[],
        std::wstring& inputFilePath, AssetType& inputAssetType, std::wstring& outFilePath, std::wstring& tempDirectory,
        std::vector<std::wstring>& lodFilePaths, std::vector<double>& screenCoveragePercentages, size_t& maxTextureSize,
        bool& sharedMaterials, Version& minVersion, Platform& targetPlatforms, bool& replaceTextures, bool& compressMeshes, 
        bool& generateTangents, bool& optimizeMeshes);
};

