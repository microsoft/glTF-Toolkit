// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureLoadingUtils.h"
#include "GLTFTexturePackingUtils.h"
#include "GLTFTextureCompressionUtils.h"
#include "DeviceResources.h"

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFConstants.h>
#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/RapidJsonUtils.h>
#include <GLTFSDK/Schema.h>

// Usings for ComPtr
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;

// Usings for glTF
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

#include <DirectXTex.h>

const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_TEXTURE_DDS = "MSFT_texture_dds";

GLTFDocument GLTFTextureCompressionUtils::CompressTextureAsDDS(const IStreamReader& streamReader, const GLTFDocument & doc, const Texture & texture, TextureCompression compression, const std::string& outputDirectory, bool generateMipMaps, bool retainOriginalImage)
{
    GLTFDocument outputDoc(doc);

    // Early return cases:
    // - No compression requested
    // - This texture doesn't have an image associated
    // - The texture already has a DDS extension
    if (compression == TextureCompression::None ||
        texture.imageId.empty() ||
        texture.extensions.find(EXTENSION_MSFT_TEXTURE_DDS) != texture.extensions.end())
    {
        // Return copy of document
        return outputDoc;
    }

    auto image = std::make_unique<DirectX::ScratchImage>(GLTFTextureLoadingUtils::LoadTexture(streamReader, doc, texture.id));

    if (generateMipMaps)
    {
        auto mipChain = std::make_unique<DirectX::ScratchImage>();
        if (FAILED(DirectX::GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, *mipChain)))
        {
            throw GLTFException("Failed to generate mip maps.");
        }

        image = std::move(mipChain);
    }

    CompressImage(*image, compression);

    // Save image to file
    std::string outputImagePath = "texture_" + texture.id;

    if (!generateMipMaps)
    {
        // The default is to have mips, so note on the texture when it doesn't
        outputImagePath += "_nomips";
    }

    switch (compression)
    {
    case TextureCompression::BC3:
        outputImagePath += "_BC3";
        break;
    case TextureCompression::BC5:
        outputImagePath += "_BC5";
        break;
    case TextureCompression::BC7:
        outputImagePath += "_BC7";
        break;
    default:
        throw GLTFException("Invalid compression.");
        break;
    }

    outputImagePath += ".dds";
    std::wstring outputImagePathW(outputImagePath.begin(), outputImagePath.end());

    wchar_t outputImageFullPath[MAX_PATH];

    std::wstring outputDirectoryW(outputDirectory.begin(), outputDirectory.end());

    if (FAILED(::PathCchCombine(outputImageFullPath, ARRAYSIZE(outputImageFullPath), outputDirectoryW.c_str(), outputImagePathW.c_str())))
    {
        throw GLTFException("Failed to compose output file path.");
    }

    if (FAILED(SaveToDDSFile(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, outputImageFullPath)))
    {
        throw GLTFException("Failed to save image as DDS.");
    }

    std::wstring outputImageFullPathW(outputImageFullPath);
    std::string outputImageFullPathA(outputImageFullPathW.begin(), outputImageFullPathW.end());

    // Add back to GLTF
    std::string ddsImageId(texture.imageId);

    Image ddsImage(doc.images.Get(texture.imageId));
    ddsImage.mimeType = "image/vnd-ms.dds";
    ddsImage.uri = outputImageFullPathA;

    if (retainOriginalImage)
    {
        ddsImageId.assign(std::to_string(doc.images.Size()));
        ddsImage.id = ddsImageId;
        outputDoc.images.Append(std::move(ddsImage));
    }
    else
    {
        outputDoc.images.Replace(ddsImage);
    }

    Texture ddsTexture(texture);

    // Create the JSON for the DDS extension element
    rapidjson::Document ddsExtensionJson;
    ddsExtensionJson.SetObject();

    ddsExtensionJson.AddMember("source", rapidjson::Value(std::stoi(ddsImageId)), ddsExtensionJson.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    ddsExtensionJson.Accept(writer);

    ddsTexture.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_TEXTURE_DDS, buffer.GetString()));

    outputDoc.textures.Replace(ddsTexture);

    outputDoc.extensionsUsed.insert(EXTENSION_MSFT_TEXTURE_DDS);

    if (!retainOriginalImage)
    {
        outputDoc.extensionsRequired.insert(EXTENSION_MSFT_TEXTURE_DDS);
    }

    return outputDoc;
}

GLTFDocument GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, const std::string& outputDirectory, bool retainOriginalImages)
{
    GLTFDocument outputDoc(doc);

    for (auto material : doc.materials.Elements())
    {
        auto compressIfNotEmpty = [&outputDoc, &streamReader, &outputDirectory, retainOriginalImages](const std::string& textureId, TextureCompression compression)
        {
            if (!textureId.empty())
            {
                outputDoc = CompressTextureAsDDS(streamReader, outputDoc, outputDoc.textures.Get(textureId), compression, outputDirectory, true, retainOriginalImages);
            }
        };

        // Compress base and emissive texture as BC7
        compressIfNotEmpty(material.metallicRoughness.baseColorTextureId, TextureCompression::BC7);
        compressIfNotEmpty(material.emissiveTextureId, TextureCompression::BC7);

        // Get other textures from the MSFT_packing_occlusionRoughnessMetallic extension
        if (material.extensions.find(EXTENSION_MSFT_PACKING_ORM) != material.extensions.end())
        {
            rapidjson::Document packingOrmContents;
            packingOrmContents.Parse(material.extensions[EXTENSION_MSFT_PACKING_ORM].c_str());

            // Compress packed textures as BC7
            if (packingOrmContents.HasMember("roughnessMetallicOcclusionTexture"))
            {
                auto rmoTextureId = packingOrmContents["roughnessMetallicOcclusionTexture"]["index"].GetInt();
                compressIfNotEmpty(std::to_string(rmoTextureId), TextureCompression::BC7);
            }

            if (packingOrmContents.HasMember("occlusionRoughnessMetallicTexture"))
            {
                auto ormTextureId = packingOrmContents["occlusionRoughnessMetallicTexture"]["index"].GetInt();
                compressIfNotEmpty(std::to_string(ormTextureId), TextureCompression::BC7);
            }

            // Compress normal texture as BC5
            if (packingOrmContents.HasMember("normalTexture"))
            {
                auto normalTextureId = packingOrmContents["normalTexture"]["index"].GetInt();
                compressIfNotEmpty(std::to_string(normalTextureId), TextureCompression::BC5);
            }
        }
    }

    return outputDoc;
}

void GLTFTextureCompressionUtils::CompressImage(DirectX::ScratchImage& image, TextureCompression compression)
{
    if (compression == TextureCompression::None)
    {
        return;
    }

    DXGI_FORMAT compressionFormat = DXGI_FORMAT_BC7_UNORM;
    switch (compression)
    {
    case TextureCompression::BC3:
        compressionFormat = DXGI_FORMAT_BC3_UNORM;
        break;
    case TextureCompression::BC5:
        compressionFormat = DXGI_FORMAT_BC5_UNORM;
        break;
    case TextureCompression::BC7:
        compressionFormat = DXGI_FORMAT_BC7_UNORM;
        break;
    default:
        throw std::invalid_argument("Invalid compression specified.");
        break;
    }

    DX::DeviceResources deviceResources;
    deviceResources.CreateDeviceResources();
    ComPtr<ID3D11Device> device(deviceResources.GetD3DDevice());
    
    DirectX::ScratchImage compressedImage;

    bool gpuCompressionSuccessful = false;
    if (device != nullptr)
    {
        if (SUCCEEDED(DirectX::Compress(device.Get(), image.GetImages(), image.GetImageCount(), image.GetMetadata(), compressionFormat, DirectX::TEX_COMPRESS_DEFAULT, 0, compressedImage)))
        {
            gpuCompressionSuccessful = true;
        }
    }

    if (!gpuCompressionSuccessful)
    {
        // Try software compression
        if (FAILED(DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), compressionFormat, DirectX::TEX_COMPRESS_DEFAULT, 0, compressedImage)))
        {
            throw GLTFException("Failed to compress data using software compression");
        }
    }

    image = std::move(compressedImage);
}