// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "WindowsMRConversion.h"
#include "GLTFSerialization.h"
#include "GLTFStreams.h"

#include <ppltasks.h>
#include <GLTFTexturePackingUtils.h>
#include <GLTFTextureCompressionUtils.h>
#include <GLTFMeshCompressionUtils.h>
#include <GLTFSpecularGlossinessUtils.h>
#include <SerializeBinary.h>
#include <GLBtoGLTF.h>

#include <GLTFSDK/Document.h>
#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/IStreamWriter.h>
#include <GLTFSDK/ExtensionsKHR.h>

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Metadata;
using namespace Windows::Storage;
using namespace Windows::System::Profile;

using namespace Microsoft::glTF::Toolkit::UWP;

IAsyncOperation<StorageFile^>^ WindowsMRConversion::ConvertAssetForWindowsMR(StorageFile^ gltfOrGlbFile, StorageFolder^ outputFolder)
{
    return ConvertAssetForWindowsMR(gltfOrGlbFile, outputFolder, 512);
}

IAsyncOperation<StorageFile^>^ WindowsMRConversion::ConvertAssetForWindowsMR(StorageFile^ gltfOrGlbFile, StorageFolder^ outputFolder, size_t maxTextureSize)
{
    UWP::TexturePacking detectedPacking = UWP::TexturePacking::None;

    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Holographic")
    {
        detectedPacking = UWP::TexturePacking::NormalRoughnessMetallic;
    }
    else
    {
        bool isVersion1803OrNewer = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 6);
        detectedPacking = isVersion1803OrNewer ? UWP::TexturePacking::OcclusionRoughnessMetallic : UWP::TexturePacking::RoughnessMetallicOcclusion;
    }

    return ConvertAssetForWindowsMR(gltfOrGlbFile, outputFolder, maxTextureSize, detectedPacking);
}

IAsyncOperation<StorageFile^>^ WindowsMRConversion::ConvertAssetForWindowsMR(StorageFile ^ gltfOrGlbFile, StorageFolder ^ outputFolder, size_t maxTextureSize, TexturePacking packing)
{
    return ConvertAssetForWindowsMR(gltfOrGlbFile, outputFolder, maxTextureSize, packing, false);
}

IAsyncOperation<StorageFile^>^ WindowsMRConversion::ConvertAssetForWindowsMR(StorageFile ^ gltfOrGlbFile, StorageFolder ^ outputFolder, size_t maxTextureSize, TexturePacking packing, bool meshCompression)
{
    auto isGlb = gltfOrGlbFile->FileType == L".glb";

    return create_async([gltfOrGlbFile, maxTextureSize, outputFolder, isGlb, packing, meshCompression]()
    {
        return create_task([gltfOrGlbFile, isGlb]()
        {
            if (isGlb)
            {
                return create_task(GLTFSerialization::UnpackGLBAsync(gltfOrGlbFile, ApplicationData::Current->TemporaryFolder));
            }
            else
            {
                return task_from_result<StorageFile^>(gltfOrGlbFile);
            }
        })
        .then([maxTextureSize, outputFolder, isGlb, packing, meshCompression](StorageFile^ gltfFile)
        {
            auto stream = std::make_shared<std::ifstream>(gltfFile->Path->Data(), std::ios::in);
            Document document = Deserialize(*stream, KHR::GetKHRExtensionDeserializer());

            return create_task(gltfFile->GetParentAsync())
            .then([document, maxTextureSize, outputFolder, gltfFile, isGlb, packing, meshCompression](StorageFolder^ baseFolder)
            {
                auto streamReader = std::make_shared<GLTFStreamReader>(baseFolder);
                auto tempDirectory = std::wstring(ApplicationData::Current->TemporaryFolder->Path->Data());
                auto tempDirectoryA = std::string(tempDirectory.begin(), tempDirectory.end());

                // 0. Specular Glossiness conversion
                auto convertedDoc = GLTFSpecularGlossinessUtils::ConvertMaterials(streamReader, document, tempDirectoryA);

                // 1. Texture Packing
                convertedDoc = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(streamReader, convertedDoc, static_cast<Toolkit::TexturePacking>(packing), tempDirectoryA);

                // 2. Texture Compression
                convertedDoc = GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(streamReader, convertedDoc, tempDirectoryA, maxTextureSize, false /* retainOriginalImages */);

                // 3. Make sure there's a default scene set
                if (!convertedDoc.HasDefaultScene())
                {
                    convertedDoc.defaultSceneId = convertedDoc.scenes.Elements()[0].id;
                }

                // 4. Compress the meshes
                if (meshCompression)
                {
                    convertedDoc = GLTFMeshCompressionUtils::CompressMeshes(streamReader, convertedDoc, {}, tempDirectoryA);
                }

                // 5. GLB Export

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

                auto glbName = std::wstring(gltfFile->Name->Data());
                glbName = glbName.substr(0, glbName.rfind(gltfFile->FileType->Data()));

                if (isGlb)
                {
                    glbName += L"_converted";
                }

                glbName += L".glb";

                std::wstring outputGlbPathW = std::wstring(outputFolder->Path->Data()) + L"\\" + glbName;
                SerializeBinary(convertedDoc, streamReader, std::make_shared<GLBStreamWriter>(outputGlbPathW), accessorConversion);

                return create_task(outputFolder->GetFileAsync(ref new String(glbName.c_str())));
            });
        });
    });
}
