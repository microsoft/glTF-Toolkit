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

#include "CommandLine.h"
#include "FileSystem.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

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

GLTFDocument LoadAndConvertDocumentForWindowsMR(
    const std::wstring& inputFilePath,
    AssetType inputAssetType,
    const std::wstring& tempDirectory)
{
    // Load the document
    std::wstring inputFileName = PathFindFileName(inputFilePath.c_str());
    std::wcout << L"Loading input document: " << inputFileName << L"..." << std::endl;

    std::wstring inputGltf(inputFilePath);
    if (inputAssetType == AssetType::GLB)
    {
        // Convert the GLB to GLTF in the temp directory

        std::string inputFilePathA(inputFilePath.begin(), inputFilePath.end());
        std::string tempDirectoryA(tempDirectory.begin(), tempDirectory.end());

        wchar_t *inputFileNameRaw = &inputFileName[0];
        PathRemoveExtension(inputFileNameRaw);

        // inputGltfName is the path to the converted GLTF without extension
        std::wstring inputGltfName = inputFileNameRaw;
        std::string inputGltfNameA = std::string(inputGltfName.begin(), inputGltfName.end());

        GLBToGLTF::UnpackGLB(inputFilePathA, tempDirectoryA, inputGltfNameA);

        inputGltf = tempDirectory + inputGltfName + EXTENSION_GLTF;
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
        CommandLine::PrintHelp();
        return 0;
    }

    // Initialize COM
    CoInitialize(NULL);

    try
    {
        // Arguments
        std::wstring inputFilePath;
        AssetType inputAssetType;
        std::wstring outFilePath;
        std::wstring tempDirectory;
        std::vector<std::wstring> lodFilePaths;
        std::vector<double> screenCoveragePercentages;

        CommandLine::ParseCommandLineArguments(argc, argv, inputFilePath, inputAssetType, outFilePath, tempDirectory, lodFilePaths, screenCoveragePercentages);

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
                // Apply the same optimizations for each LOD
                auto lod = lodFilePaths[i];
                auto subFolder = FileSystem::CreateSubFolder(tempDirectory, L"lod" + std::to_wstring(i + 1));
                lodDocuments.push_back(LoadAndConvertDocumentForWindowsMR(lod, AssetTypeUtils::AssetTypeFromFilePath(lod), subFolder));
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
    }
    catch (std::exception ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}