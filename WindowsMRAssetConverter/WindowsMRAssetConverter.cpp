// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "stdafx.h"

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/IStreamFactory.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFTexturePackingUtils.h>
#include <GLTFTextureCompressionUtils.h>
#include <GLTFLODUtils.h>
#include <SerializeBinary.h>
#include <GLBtoGLTF.h>

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

// Constants
const wchar_t * EXTENSION_GLTF = L".gltf";
const wchar_t * EXTENSION_GLB = L".glb";
const wchar_t * PARAM_OUTFILE = L"-o";
const wchar_t * PARAM_TMPDIR = L"-temp-directory";
const wchar_t * PARAM_LOD = L"-lod";
const wchar_t * PARAM_SCREENCOVERAGE = L"-screen-coverage";
const wchar_t * SUFFIX_CONVERTED = L"_converted";
const wchar_t * CLI_INDENT = L"    ";

enum class AssetType
{
    GLTF,
    GLB
};

class GLTFStreamReader : public IStreamReader
{
public:
    GLTFStreamReader(std::wstring uriBase) : m_uriBase(uriBase) {}

    virtual ~GLTFStreamReader() override {}
    virtual std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
    {
        std::wstring filenameW = std::wstring(filename.begin(), filename.end());

        wchar_t uriAbsoluteRaw[MAX_PATH];
        // Note: PathCchCombine will return the last argument if it's an absolute path
        if (FAILED(::PathCchCombine(uriAbsoluteRaw, ARRAYSIZE(uriAbsoluteRaw), m_uriBase.c_str(), filenameW.c_str())))
        {
            throw std::invalid_argument("Could not get the base path for the GLTF resources. Try specifying the full path.");
        }

        return std::make_shared<std::ifstream>(uriAbsoluteRaw, std::ios::binary);
    }
private:
    const std::wstring m_uriBase;
};

class GLBStreamFactory : public Microsoft::glTF::IStreamFactory
{
public:
    GLBStreamFactory(const std::wstring& filename) :
        m_stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out)),
        m_tempStream(std::make_shared<std::stringstream>(std::ios_base::binary | std::ios_base::in | std::ios_base::out))
    { }

    std::shared_ptr<std::istream> GetInputStream(const std::string&) const override
    {
        throw std::logic_error("Not implemented");
    }

    std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override
    {
        return m_stream;
    }

    std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override
    {
        return m_tempStream;
    }
private:
    std::shared_ptr<std::ofstream> m_stream;
    std::shared_ptr<std::stringstream> m_tempStream;
};

void PrintHelp()
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

enum class CommandLineParsingState
{
    Initial,
    InputRead,
    ReadOutFile,
    ReadTmpDir,
    ReadLods,
    ReadScreenCoverage
};

std::wstring GetFullPath(const std::wstring& path)
{
    wchar_t fullPath[MAX_PATH];
    if (GetFullPathName(path.c_str(), ARRAYSIZE(fullPath), fullPath, NULL) == 0)
    {
        throw std::invalid_argument("Invalid input file path.");
    }
    return std::wstring(fullPath);
}

std::wstring CreateSubFolder(const std::wstring& parentPath, const std::wstring& subFolderName)
{
    std::wstring errorMessageW = L"Could not create a sub-folder of " + parentPath + L". Try specifying a different temporary path with " + std::wstring(PARAM_TMPDIR) + L".";
    std::string errorMessage(errorMessageW.begin(), errorMessageW.end());

    wchar_t subFolderPath[MAX_PATH];
    if (FAILED(PathCchCombine(subFolderPath, ARRAYSIZE(subFolderPath), parentPath.c_str(), (subFolderName + L"\\").c_str())))
    {
        throw std::runtime_error(errorMessage);
    }

    if (CreateDirectory(subFolderPath, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        throw std::runtime_error(errorMessage);
    }

    return std::wstring(subFolderPath);
}

std::wstring CreateTempFolder()
{
    std::wstring errorMessageW = L"Could not get a temporary folder. Try specifying one with " + std::wstring(PARAM_TMPDIR) + L".";
    std::string errorMessage(errorMessageW.begin(), errorMessageW.end());

    wchar_t tmpDirRaw[MAX_PATH];
    auto returnValue = GetTempPath(MAX_PATH, tmpDirRaw);
    if (returnValue > MAX_PATH || (returnValue == 0))
    {
        throw std::runtime_error(errorMessage);
    }

    // Get a random folder to drop the files
    GUID guid = { 0 };
    if (FAILED(CoCreateGuid(&guid)))
    {
        throw std::runtime_error(errorMessage);
    }

    wchar_t guidRaw[MAX_PATH];
    if (StringFromGUID2(guid, guidRaw, ARRAYSIZE(guidRaw)) == 0)
    {
        throw std::runtime_error(errorMessage);
    }

    return CreateSubFolder(tmpDirRaw, guidRaw);
}

AssetType GetFileAssetType(const std::wstring& assetPath)
{
    const wchar_t *inputExtensionRaw = nullptr;
    if (FAILED(PathCchFindExtension(assetPath.c_str(), assetPath.length() + 1, &inputExtensionRaw)))
    {
        throw std::invalid_argument("Invalid input file extension.");
    }

    if (_wcsicmp(inputExtensionRaw, EXTENSION_GLTF) == 0)
    {
        return AssetType::GLTF;
    }
    else if (_wcsicmp(inputExtensionRaw, EXTENSION_GLB) == 0)
    {
        return AssetType::GLB;
    }
    else
    {
        throw std::invalid_argument("Invalid file, please provide a GLTF or GLB.");
    }
}

void ParseCommandLineArguments(
    int argc, wchar_t *argv[],
    std::wstring& inputFilePath, AssetType& inputAssetType, std::wstring& outFilePath, std::wstring& tempDirectory,
    std::vector<std::wstring>& lodFilePaths, std::vector<double>& screenCoveragePercentages)
{
    CommandLineParsingState state = CommandLineParsingState::Initial;

    inputFilePath = GetFullPath(std::wstring(argv[1]));

    inputAssetType = GetFileAssetType(inputFilePath);

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
        if (param == PARAM_TMPDIR)
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
        else
        {
            switch (state)
            {
            case CommandLineParsingState::ReadOutFile:
                outFile = GetFullPath(param);
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadTmpDir:
                tmpDir = GetFullPath(param);
                state = CommandLineParsingState::InputRead;
                break;
            case CommandLineParsingState::ReadLods:
                lodFilePaths.push_back(GetFullPath(param));
                break;
            case CommandLineParsingState::ReadScreenCoverage:
            {
                auto paramA = std::string(param.begin(), param.end());
                screenCoveragePercentages.push_back(std::atof(paramA.c_str()));
                break;
            }
            case CommandLineParsingState::Initial:
            case CommandLineParsingState::InputRead:
            default:
                // Invalid argument detected
                throw std::invalid_argument("Invalid usage. For help, try the command again without parameters.");
            }
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
        tmpDir = CreateTempFolder();
    }

    tempDirectory = tmpDir;
}

GLTFDocument LoadAndConvertDocumentForWindowsMR(
    const std::wstring& inputFilePath,
    AssetType inputAssetType,
    const std::wstring& tempDirectory)
{
    // Load the document
    wchar_t *inputFileName = PathFindFileName(inputFilePath.c_str());
    std::wcout << L"Loading input document: " << inputFileName << L"..." << std::endl;

    std::wstring inputGltf(inputFilePath);
    if (inputAssetType == AssetType::GLB)
    {
        // Convert the GLB to GLTF in the temp directory

        std::wstring inputGltfFullPath = tempDirectory + inputFileName;
        wchar_t *inputGltfPathRaw = &inputGltfFullPath[0];
        PathRemoveExtension(inputGltfPathRaw);

        // inputGltfName is the path to the converted GLTF without extension
        std::wstring inputGltfName = inputGltfPathRaw;
        std::string inputGltfNameA = std::string(inputGltfName.begin(), inputGltfName.end());
        std::string inputFilePathA(inputFilePath.begin(), inputFilePath.end());

        GLBToGLTF::UnpackGLB(inputGltfNameA, inputFilePathA);

        inputGltf = inputGltfName + EXTENSION_GLTF;
    }

    auto stream = std::make_shared<std::ifstream>(inputGltf, std::ios::binary);
    GLTFDocument document = DeserializeJson(*stream);

    // Get the base path from where to read all the assets

    std::wstring inputGltfCopy(inputGltf);
    wchar_t *basePath = &inputGltfCopy[0];
    if (FAILED(PathCchRemoveFileSpec(basePath, inputGltfCopy.length() + 1)))
    {
        throw std::invalid_argument("Invalid input path.");
    }

    GLTFStreamReader streamReader(basePath);

    std::cout << "Packing textures..." << std::endl;

    // 1. Texture Packing
    auto tempDirectoryA = std::string(tempDirectory.begin(), tempDirectory.end());
    document = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(streamReader, document, TexturePacking::RoughnessMetallicOcclusion, tempDirectoryA);

    std::cout << "Compressing textures - this can take a few minutes..." << std::endl;

    // 2. Texture Compression
    document = GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(streamReader, document, tempDirectoryA);

    // 3. Fix Accessors for older version of the GLTF spec
    // The Windows MR Fall Creators Update GLTF loader was built on 
    // a version of the GLTF spec that requires that all accessors have a min
    // and max values. Make sure this is the case.
    // TODO: calculate actual min and max values
    for (auto accessor : document.accessors.Elements())
    {
        bool needsReplace = false;

        if (accessor.min.size() == 0)
        {
            accessor.min.push_back(0.0f);
            needsReplace = true;
        }

        if (accessor.max.size() == 0)
        {
            accessor.max.push_back(0.0f);
            needsReplace = true;
        }

        if (needsReplace)
        {
            document.accessors.Replace(accessor);
        }
    }

    return document;
}

int wmain(int argc, wchar_t *argv[])
{
    if (argc < 2)
    {
        PrintHelp();
        return 0;
    }

    // Initialize COM
    CoInitialize(NULL);

    // Arguments
    std::wstring inputFilePath;
    AssetType inputAssetType;
    std::wstring outFilePath;
    std::wstring tempDirectory;
    std::vector<std::wstring> lodFilePaths;
    std::vector<double> screenCoveragePercentages;

    try
    {
        ParseCommandLineArguments(argc, argv, inputFilePath, inputAssetType, outFilePath, tempDirectory, lodFilePaths, screenCoveragePercentages);
    }
    catch (std::exception ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    // Load document, and perform steps:
    // 1. Texture Packing
    // 2. Texture Compression
    // 3. Fix Accessors for older version of the GLTF spec
    auto document = LoadAndConvertDocumentForWindowsMR(inputFilePath, inputAssetType, tempDirectory);

    // 4. LOD Merging
    if (lodFilePaths.size() > 0)
    {
        std::cout << "Merging LODs..." << std::endl;

        std::vector<GLTFDocument> lodDocuments;
        lodDocuments.push_back(document);

        for (size_t i = 0; i < lodFilePaths.size(); i++)
        {
            auto lod = lodFilePaths[i];
            auto subFolder = CreateSubFolder(tempDirectory, L"lod" + std::to_wstring(i + 1));
            lodDocuments.push_back(LoadAndConvertDocumentForWindowsMR(lod, GetFileAssetType(lod), subFolder));
        }

        document = GLTFLODUtils::MergeDocumentAsLODs(lodDocuments, screenCoveragePercentages);
    }

    // 5. GLB Export
    std::cout << "Exporting as GLB..." << std::endl;

    GLTFStreamReader streamReader(tempDirectory);
    std::unique_ptr<const IStreamFactory> streamFactory = std::make_unique<GLBStreamFactory>(outFilePath);
    SerializeBinary(document, streamReader, streamFactory);

    std::cout << "Done!" << std::endl;
    std::wcout << L"Output file: " << outFilePath << std::endl;

    return 0;
}