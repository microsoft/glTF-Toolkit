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
const wchar_t * PARAM_GENERATE_TANGENTS = L"-generate-tangents";
const wchar_t * PARAM_OPTIMIZE_MESHES = L"-optimize-meshes";
const wchar_t * PARAM_MIN_VERSION = L"-min-version";
const wchar_t * PARAM_PLATFORM = L"-platform";
const wchar_t * PARAM_REPLACE_TEXTURES = L"-replace-textures";
const wchar_t * PARAM_COMPRESS_MESHES = L"-compress-meshes";
const wchar_t * PARAM_VALUE_VERSION_1709 = L"1709";
const wchar_t * PARAM_VALUE_VERSION_1803 = L"1803";
const wchar_t * PARAM_VALUE_VERSION_1809 = L"1809";
const wchar_t * PARAM_VALUE_VERSION_RS3 = L"rs3";
const wchar_t * PARAM_VALUE_VERSION_RS4 = L"rs4";
const wchar_t * PARAM_VALUE_VERSION_RS5 = L"rs5";
const wchar_t * PARAM_VALUE_VERSION_LATEST = L"latest";
const wchar_t * PARAM_VALUE_HOLOGRAPHIC = L"holographic";
const wchar_t * PARAM_VALUE_HOLOLENS= L"hololens";
const wchar_t * PARAM_VALUE_DESKTOP = L"desktop";
const wchar_t * PARAM_VALUE_PC = L"pc";
const wchar_t * PARAM_VALUE_ALL = L"all";
const wchar_t * SUFFIX_CONVERTED = L"_converted";
const wchar_t * CLI_INDENT = L"    ";
const size_t MAXTEXTURESIZE_DEFAULT = 512;
const size_t MAXTEXTURESIZE_MAX = 4096;
const CommandLine::Version MIN_VERSION_DEFAULT = CommandLine::Version::Version1709;
const CommandLine::Platform PLATFORM_DEFAULT = CommandLine::Platform::Desktop;

enum class CommandLineParsingState
{
    Initial,
    InputRead,
    ReadOutFile,
    ReadTmpDir,
    ReadLods,
    ReadScreenCoverage,
    ReadMaxTextureSize,
    ReadMeshOptimizeOption,
    ReadMinVersion,
    ReadPlatform
};

void CommandLine::PrintHelp()
{
    auto indent = std::wstring(CLI_INDENT);
    std::wcerr << std::endl
        << L"Windows Mixed Reality Asset Converter" << std::endl
        << L"=====================================" << std::endl
        << std::endl
        << L"A command line tool to convert core GLTF 2.0 assets for use in "
        << L"the Windows Mixed Reality home, with the proper texture packing, "
        << L"compression, mesh optimization, and merged LODs." << std::endl << std::endl
        << L"Usage: WindowsMRAssetConverter <path to GLTF/GLB>" << std::endl
        << std::endl
        << L"Optional arguments:" << std::endl
        << indent << "[" << std::wstring(PARAM_OUTFILE) << L" <output file path>]" << std::endl
        << indent << "[" << std::wstring(PARAM_TMPDIR) << L" <temporary folder>] - default is the system temp folder for the user" << std::endl
        << indent << "[" << std::wstring(PARAM_PLATFORM) << " <" << PARAM_VALUE_ALL << " | " << PARAM_VALUE_HOLOGRAPHIC << " | " << PARAM_VALUE_DESKTOP << ">] - defaults to " << PARAM_VALUE_DESKTOP << std::endl
        << indent << "[" << std::wstring(PARAM_MIN_VERSION) << " <" << PARAM_VALUE_VERSION_1709 << " | " << PARAM_VALUE_VERSION_1803 << " | " << PARAM_VALUE_VERSION_1809 << " | " << PARAM_VALUE_VERSION_LATEST << ">] - defaults to " << PARAM_VALUE_VERSION_1709 << std::endl
        << indent << "[" << std::wstring(PARAM_LOD) << " <path to each lower LOD asset in descending order of quality>]" << std::endl
        << indent << "[" << std::wstring(PARAM_SCREENCOVERAGE) << " <LOD screen coverage values>]" << std::endl
        << indent << "[" << std::wstring(PARAM_SHARE_MATERIALS) << "] - disabled if not present" << std::endl
        << indent << "[" << std::wstring(PARAM_MAXTEXTURESIZE) << " <Max texture size in pixels>] - defaults to 512" << std::endl
        << indent << "[" << std::wstring(PARAM_REPLACE_TEXTURES) << "] - disabled if not present" << std::endl
        << indent << "[" << std::wstring(PARAM_COMPRESS_MESHES) << "] - compress meshes with Draco" << std::endl
        << indent << "[" << std::wstring(PARAM_OPTIMIZE_MESHES) << "] - DirectXMesh mesh optimization <on | off>" << std::endl
        << indent << "[" << std::wstring(PARAM_GENERATE_TANGENTS) << "]" << std::endl
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
    bool& shareMaterials, Version& minVersion, Platform& targetPlatforms, bool& replaceTextures, bool& compressMeshes, 
    bool& generateTangents, bool& optimizeMeshes)
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
    generateTangents = false;
    minVersion = MIN_VERSION_DEFAULT;
    targetPlatforms = PLATFORM_DEFAULT;
    replaceTextures = false;
    compressMeshes = false;
    optimizeMeshes = true;

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
            state = CommandLineParsingState::InputRead;
        }
        else if (param == PARAM_GENERATE_TANGENTS)
        {
            generateTangents = true;
        }
        else if (param == PARAM_OPTIMIZE_MESHES)
        {
            generateTangents = true;
        }
        else if (param == PARAM_MIN_VERSION)
        {
            minVersion = MIN_VERSION_DEFAULT;
            state = CommandLineParsingState::ReadMinVersion;
        }
        else if (param == PARAM_PLATFORM)
        {
            targetPlatforms = PLATFORM_DEFAULT;
            state = CommandLineParsingState::ReadPlatform;
        }
        else if (param == PARAM_REPLACE_TEXTURES)
        {
            replaceTextures = true;
            state = CommandLineParsingState::InputRead;
        }
        else if (param == PARAM_COMPRESS_MESHES)
        {
            if (minVersion >= CommandLine::Version::Version1809)
            {
                compressMeshes = true;
            }
            else
            {
                throw std::invalid_argument("Invalid min version specified with mesh compression; must be at least 1809.");
            }
            state = CommandLineParsingState::InputRead;
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
            case CommandLineParsingState::ReadMeshOptimizeOption:
                optimizeMeshes = _wcsicmp(param.c_str(), L"off") != 0; // Default to 'on' if the parameter is anything else but case-insensitive 'off'
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadMinVersion:
                if (_wcsicmp(param.c_str(), PARAM_VALUE_VERSION_1709) == 0 || _wcsicmp(param.c_str(), PARAM_VALUE_VERSION_RS3) == 0)
                {
                    minVersion = Version::Version1709;
                }
                else if (_wcsicmp(param.c_str(), PARAM_VALUE_VERSION_1803) == 0 || _wcsicmp(param.c_str(), PARAM_VALUE_VERSION_RS4) == 0)
                {
                    minVersion = Version::Version1803;
                }
                else if (_wcsicmp(param.c_str(), PARAM_VALUE_VERSION_1809) == 0 || _wcsicmp(param.c_str(), PARAM_VALUE_VERSION_RS5) == 0)
                {
                    minVersion = Version::Version1809;
                }
                else if (_wcsicmp(param.c_str(), PARAM_VALUE_VERSION_LATEST) == 0)
                {
                    minVersion = Version::Latest;
                }
                else
                {
                    throw std::invalid_argument("Invalid min version specified. For help, try the command again without parameters.");
                }
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadPlatform:
                if (_wcsicmp(param.c_str(), PARAM_VALUE_ALL) == 0)
                {
                    targetPlatforms = (Platform) (Platform::Desktop | Platform::Holographic);
                } 
                else if (_wcsicmp(param.c_str(), PARAM_VALUE_HOLOGRAPHIC) == 0 || _wcsicmp(param.c_str(), PARAM_VALUE_HOLOLENS) == 0)
                {
                    targetPlatforms = Platform::Holographic;
                }
                else if (_wcsicmp(param.c_str(), PARAM_VALUE_DESKTOP) == 0 || _wcsicmp(param.c_str(), PARAM_VALUE_PC) == 0)
                {
                    targetPlatforms = Platform::Desktop;
                }
                else
                {
                    throw std::invalid_argument("Invalid platform specified. For help, try the command again without parameters.");
                }
                state = CommandLineParsingState::InputRead;
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
