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
    size_t maxTextureSize,
    TexturePacking packing,
    bool processTextures = true,
    bool retainOriginalImages = true)
{
    // Load the document
    std::experimental::filesystem::path inputFilePathFS(inputFilePath);
    std::wstring inputFileName = inputFilePathFS.filename();
    std::wcout << L"Loading input document: " << inputFileName << L"..." << std::endl;

    if (inputAssetType == AssetType::GLB)
    {
        // Convert the GLB to GLTF in the temp directory

        std::string inputFilePathA(inputFilePath.begin(), inputFilePath.end());
        std::string tempDirectoryA(tempDirectory.begin(), tempDirectory.end());

        // inputGltfName is the path to the converted GLTF without extension
        std::wstring inputGltfName = inputFilePathFS.stem();
        std::string inputGltfNameA = std::string(inputGltfName.begin(), inputGltfName.end());

        GLBToGLTF::UnpackGLB(inputFilePathA, tempDirectoryA, inputGltfNameA);

        inputFilePath = tempDirectory + inputGltfName + EXTENSION_GLTF;
    }

    auto stream = std::make_shared<std::ifstream>(inputFilePath, std::ios::in);
    GLTFDocument document = DeserializeJson(*stream);

    // Get the base path from where to read all the assets

    GLTFStreamReader streamReader(FileSystem::GetBasePath(inputFilePath));

    if (processTextures)
    {
        std::wcout << L"Packing textures..." << std::endl;

        // 1. Texture Packing
        auto tempDirectoryA = std::string(tempDirectory.begin(), tempDirectory.end());
        document = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(streamReader, document, packing, tempDirectoryA);

        std::wcout << L"Compressing textures - this can take a few minutes..." << std::endl;

        // 2. Texture Compression
        document = GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(streamReader, document, tempDirectoryA, maxTextureSize, retainOriginalImages);
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
        size_t maxTextureSize;
        bool shareMaterials;
        bool compatibilityMode;
        CommandLine::Platform targetPlatforms;
        bool replaceTextures;

        CommandLine::ParseCommandLineArguments(
            argc, argv, inputFilePath, inputAssetType, outFilePath, tempDirectory, lodFilePaths, screenCoveragePercentages, 
            maxTextureSize, shareMaterials, compatibilityMode, targetPlatforms, replaceTextures);

        TexturePacking packing = TexturePacking::None;

        std::wstring compatibleVersionsText;

        if ((targetPlatforms & CommandLine::Platform::Holographic) > 0)
        {
            compatibleVersionsText += L"HoloLens";

            // Holographic mode: NRM
            packing = (TexturePacking)(packing | TexturePacking::NormalRoughnessMetallic);
        }

        if ((targetPlatforms & CommandLine::Platform::Desktop) > 0)
        {
            if (!compatibleVersionsText.empty())
            {
                compatibleVersionsText += L" and ";
            }

            // Desktop 1803+ mode: ORM
            packing = (TexturePacking)(packing | TexturePacking::OcclusionRoughnessMetallic);

            if (compatibilityMode)
            {
                // Desktop 1709 mode: RMO
                packing = (TexturePacking)(packing | TexturePacking::RoughnessMetallicOcclusion);

                compatibleVersionsText += L"Desktop (all versions)";
            }
            else
            {
                compatibleVersionsText +=  L"Desktop (version 1803+)";
            }
        }

        std::wcout << L"\nThis will generate an asset compatible with " << compatibleVersionsText << L"\n" << std::endl;

        // Load document, and perform steps:
        // 1. Texture Packing
        // 2. Texture Compression
        auto document = LoadAndConvertDocumentForWindowsMR(inputFilePath, inputAssetType, tempDirectory, maxTextureSize, packing, true /* processTextures */, !replaceTextures);

        // 3. LOD Merging
        if (lodFilePaths.size() > 0)
        {
            std::wcout << L"Merging LODs..." << std::endl;

            std::vector<GLTFDocument> lodDocuments;
            std::vector<std::wstring> lodDocumentRelativePaths;
            lodDocuments.push_back(document);

            for (size_t i = 0; i < lodFilePaths.size(); i++)
            {
                // Apply the same optimizations for each LOD
                auto lod = lodFilePaths[i];
                auto subFolder = FileSystem::CreateSubFolder(tempDirectory, L"lod" + std::to_wstring(i + 1));

                lodDocuments.push_back(LoadAndConvertDocumentForWindowsMR(lod, AssetTypeUtils::AssetTypeFromFilePath(lod), subFolder, maxTextureSize, packing, !shareMaterials, !replaceTextures));
            
                lodDocumentRelativePaths.push_back(FileSystem::GetRelativePathWithTrailingSeparator(FileSystem::GetBasePath(inputFilePath), FileSystem::GetBasePath(lod)));
            }

            document = GLTFLODUtils::MergeDocumentsAsLODs(lodDocuments, screenCoveragePercentages, lodDocumentRelativePaths, shareMaterials);
        }

        // 4. Make sure there's a default scene
        if (!document.HasDefaultScene())
        {
            document.defaultSceneId = document.scenes.Elements()[0].id;
        }

        // 5. GLB Export
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