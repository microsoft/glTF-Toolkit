// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "CommandLine.h"
#include "FileSystem.h"

// Constants
const wchar_t * PARAM_OUTFILE = L"-o";
const wchar_t * PARAM_TMPDIR = L"-temp-directory";
const wchar_t * PARAM_LOD = L"-lod";
const wchar_t * PARAM_SCREENCOVERAGE = L"-screen-coverage";
const wchar_t * PARAM_MAXTEXTURESIZE = L"-max-texture-size";
const wchar_t * PARAM_SHARE_MATERIALS = L"-share-materials";
const wchar_t * SUFFIX_CONVERTED = L"_converted";
const wchar_t * CLI_INDENT = L"    ";
const size_t MAXTEXTURESIZE_DEFAULT = 512;
const size_t MAXTEXTURESIZE_MAX = 4096;

enum class CommandLineParsingState
{
    Initial,
    InputRead,
    ReadOutFile,
    ReadTmpDir,
    ReadLods,
    ReadScreenCoverage,
    ReadMaxTextureSize
};

void CommandLine::PrintHelp()
{
    auto indent = std::wstring(CLI_INDENT);
    std::wcerr << std::endl
        << L"Windows Mixed Reality Asset Converter" << std::endl
        << L"=====================================" << std::endl
        << std::endl
        << L"A command line tool to convert core GLTF 2.0 assets for use in "
        << L"the Windows Mixed Reality home, with the proper texture packing, compression and merged LODs." << std::endl << std::endl
        << L"Usage: WindowsMRAssetConverter <path to GLTF/GLB>" << std::endl
        << std::endl
        << L"Optional arguments:" << std::endl
        << indent << "[" << std::wstring(PARAM_OUTFILE) << L" <output file path>]" << std::endl
        << indent << "[" << std::wstring(PARAM_TMPDIR) << L" <temporary folder, default is the system temp folder for the user>]" << std::endl
        << indent << "[" << std::wstring(PARAM_LOD) << " <path to each lower LOD asset in descending order of quality>]" << std::endl
        << indent << "[" << std::wstring(PARAM_SCREENCOVERAGE) << " <LOD screen coverage values>]" << std::endl
        << indent << "[" << std::wstring(PARAM_MAXTEXTURESIZE) << " <Max texture size in pixels, defaults to 512>]" << std::endl
        << indent << "[" << std::wstring(PARAM_SHARE_MATERIALS) << " defaults to false" << std::endl
        << std::endl
        << "Example:" << std::endl
        << indent << "WindowsMRAssetConverter FileToConvert.gltf "
        << std::wstring(PARAM_OUTFILE) << " ConvertedFile.glb "
        << std::wstring(PARAM_LOD) << " Lod1.gltf Lod2.gltf "
        << std::wstring(PARAM_SCREENCOVERAGE) << " 0.5 0.2 0.01" << std::endl
        << std::endl
        << "The above will convert \"FileToConvert.gltf\" into \"ConvertedFile.glb\" in the "
        << "current directory." << std::endl
        << std::endl
        << "If the file is a GLB and the output name is not specified, defaults to the same name as input "
        << "+ \"_converted.glb\"." << std::endl
        << std::endl;
}

void CommandLine::ParseCommandLineArguments(
    int argc, wchar_t *argv[],
    std::wstring& inputFilePath, AssetType& inputAssetType, std::wstring& outFilePath, std::wstring& tempDirectory,
    std::vector<std::wstring>& lodFilePaths, std::vector<double>& screenCoveragePercentages, size_t& maxTextureSize,
    bool& shareMaterials)
{
    CommandLineParsingState state = CommandLineParsingState::Initial;

    inputFilePath = FileSystem::GetFullPath(std::wstring(argv[1]));

    inputAssetType = AssetTypeUtils::AssetTypeFromFilePath(inputFilePath);

    // Reset input parameters
    outFilePath = L"";
    tempDirectory = L"";
    lodFilePaths.clear();
    screenCoveragePercentages.clear();
    maxTextureSize = MAXTEXTURESIZE_DEFAULT;
    shareMaterials = false;

    state = CommandLineParsingState::InputRead;

    std::wstring outFile;
    std::wstring tmpDir;
    for (int i = 2; i < argc; i++)
    {
        std::wstring param = argv[i];

        if (param == PARAM_OUTFILE)
        {
            outFile = L"";
            state = CommandLineParsingState::ReadOutFile;
        }
        else if (param == PARAM_TMPDIR)
        {
            tmpDir = L"";
            state = CommandLineParsingState::ReadTmpDir;
        }
        else if (param == PARAM_LOD)
        {
            lodFilePaths.clear();
            state = CommandLineParsingState::ReadLods;
        }
        else if (param == PARAM_SCREENCOVERAGE)
        {
            screenCoveragePercentages.clear();
            state = CommandLineParsingState::ReadScreenCoverage;
        }
        else if (param == PARAM_MAXTEXTURESIZE)
        {
            maxTextureSize = MAXTEXTURESIZE_DEFAULT;
            state = CommandLineParsingState::ReadMaxTextureSize;
        }
        else if (param == PARAM_SHARE_MATERIALS)
        {
            shareMaterials = true;
        }
        else
        {
            switch (state)
            {
            case CommandLineParsingState::ReadOutFile:
                outFile = FileSystem::GetFullPath(param);
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadTmpDir:
                tmpDir = FileSystem::GetFullPath(param);
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadLods:
                lodFilePaths.push_back(FileSystem::GetFullPath(param));
                break;
            case CommandLineParsingState::ReadScreenCoverage:
            {
                auto paramA = std::string(param.begin(), param.end());
                screenCoveragePercentages.push_back(std::atof(paramA.c_str()));
                break;
            }
            case CommandLineParsingState::ReadMaxTextureSize:
                maxTextureSize = std::min(static_cast<size_t>(std::stoul(param.c_str())), MAXTEXTURESIZE_MAX);
                break;
            case CommandLineParsingState::Initial:
            case CommandLineParsingState::InputRead:
            default:
                // Invalid argument detected
                throw std::invalid_argument("Invalid usage. For help, try the command again without parameters.");
            }
        }
    }

    if (!std::experimental::filesystem::exists(inputFilePath))
    {
        throw std::invalid_argument("Input file not found.");
    }
    for (auto& lodFilePath : lodFilePaths)
    {
        if (!std::experimental::filesystem::exists(lodFilePath))
        {
            throw  std::invalid_argument("Lod file not found.");
        }
    }

    if (outFile.empty())
    {
        std::wstring inputFilePathWithoutExtension = inputFilePath;
        if (FAILED(PathCchRemoveExtension(&inputFilePathWithoutExtension[0], inputFilePathWithoutExtension.length() + 1)))
        {
            throw std::invalid_argument("Invalid input file extension.");
        }

        outFile = std::wstring(&inputFilePathWithoutExtension[0]);

        if (inputAssetType == AssetType::GLB)
        {
            outFile += SUFFIX_CONVERTED;
        }

        outFile += EXTENSION_GLB;
    }

    outFilePath = outFile;

    if (tmpDir.empty())
    {
        tmpDir = FileSystem::CreateTempFolder();
    }

    tempDirectory = tmpDir;
}
