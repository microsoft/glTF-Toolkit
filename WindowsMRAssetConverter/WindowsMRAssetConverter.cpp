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
    std::wstring& inputFilePath,
    AssetType inputAssetType,
    const std::wstring& tempDirectory,
    size_t maxTextureSize)
{
    // Load the document
    std::wstring inputFileName = PathFindFileName(inputFilePath.c_str());
    std::wcout << L"Loading input document: " << inputFileName << L"..." << std::endl;

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

        inputFilePath = tempDirectory + inputGltfName + EXTENSION_GLTF;
    }

    auto stream = std::make_shared<std::ifstream>(inputFilePath, std::ios::in);
    GLTFDocument document = DeserializeJson(*stream);

    // Get the base path from where to read all the assets

    GLTFStreamReader streamReader(FileSystem::GetBasePath(inputFilePath));

    std::wcout << L"Packing textures..." << std::endl;

    // 1. Texture Packing
    auto tempDirectoryA = std::string(tempDirectory.begin(), tempDirectory.end());
    document = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(streamReader, document, TexturePacking::RoughnessMetallicOcclusion, tempDirectoryA);

    std::wcout << L"Compressing textures - this can take a few minutes..." << std::endl;

    // 2. Texture Compression
    document = GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(streamReader, document, tempDirectoryA, maxTextureSize);

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
        size_t maxTextureSize;

        CommandLine::ParseCommandLineArguments(argc, argv, inputFilePath, inputAssetType, outFilePath, tempDirectory, lodFilePaths, screenCoveragePercentages, maxTextureSize);

        // Load document, and perform steps:
        // 1. Texture Packing
        // 2. Texture Compression
        auto document = LoadAndConvertDocumentForWindowsMR(inputFilePath, inputAssetType, tempDirectory, maxTextureSize);

        // 3. LOD Merging
        if (lodFilePaths.size() > 0)
        {
            std::wcout << L"Merging LODs..." << std::endl;

            std::vector<GLTFDocument> lodDocuments;
            lodDocuments.push_back(document);

            for (size_t i = 0; i < lodFilePaths.size(); i++)
            {
                // Apply the same optimizations for each LOD
                auto lod = lodFilePaths[i];
                auto subFolder = FileSystem::CreateSubFolder(tempDirectory, L"lod" + std::to_wstring(i + 1));

                lodDocuments.push_back(LoadAndConvertDocumentForWindowsMR(lod, AssetTypeUtils::AssetTypeFromFilePath(lod), subFolder, maxTextureSize));
            }

            // TODO: LOD assets can be in different places in disk, so the merged document will not have 
            // the right relative paths to resources. We must either compute the correct relative paths or embed
            // all resources as base64 in the source document, otherwise the export to GLB will fail.
            document = GLTFLODUtils::MergeDocumentsAsLODs(lodDocuments, screenCoveragePercentages);
        }

        // 4. GLB Export
        std::wcout << L"Exporting as GLB..." << std::endl;

        // The Windows MR Fall Creators update has restrictions on the supported
        // component types of accessors.
        AccessorConversionStrategy accessorConversion = [](const Accessor& accessor)
        {
            if (accessor.type == AccessorType::TYPE_SCALAR)
            {
                switch (accessor.componentType)
                {
                case ComponentType::COMPONENT_BYTE:
                case ComponentType::COMPONENT_UNSIGNED_BYTE:
                case ComponentType::COMPONENT_SHORT:
                    return ComponentType::COMPONENT_UNSIGNED_SHORT;
                default:
                    return accessor.componentType;
                }
            }
            else if (accessor.type == AccessorType::TYPE_VEC2 || accessor.type == AccessorType::TYPE_VEC3)
            {
                return ComponentType::COMPONENT_FLOAT;
            }

            return accessor.componentType;
        };

        GLTFStreamReader streamReader(FileSystem::GetBasePath(inputFilePath));
        std::unique_ptr<const IStreamFactory> streamFactory = std::make_unique<GLBStreamFactory>(outFilePath);
        SerializeBinary(document, streamReader, streamFactory, accessorConversion);

        std::wcout << L"Done!" << std::endl;
        std::wcout << L"Output file: " << outFilePath << std::endl;
    }
    catch (std::exception ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}