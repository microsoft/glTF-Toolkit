// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <DirectXTex.h>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFConstants.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/RapidJsonUtils.h>

#include "GLTFTextureLoadingUtils.h"
#include "GLTFTexturePackingUtils.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_PACKING_ORM = "MSFT_packing_occlusionRoughnessMetallic";
const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_PACKING_NRM = "MSFT_packing_normalRoughnessMetallic";
const char* Microsoft::glTF::Toolkit::MSFT_PACKING_INDEX_KEY = "index";
const char* Microsoft::glTF::Toolkit::MSFT_PACKING_ORM_ORMTEXTURE_KEY = "occlusionRoughnessMetallicTexture";
const char* Microsoft::glTF::Toolkit::MSFT_PACKING_ORM_RMOTEXTURE_KEY = "roughnessMetallicOcclusionTexture";
const char* Microsoft::glTF::Toolkit::MSFT_PACKING_ORM_NORMALTEXTURE_KEY = "normalTexture";
const char* Microsoft::glTF::Toolkit::MSFT_PACKING_NRM_KEY = "normalRoughnessMetallicTexture";

namespace
{
    enum Channel
    {
        Red = 0,
        Green = 4,
        Blue = 8,
        Alpha = 12
    };

    // Constants for the format DXGI_FORMAT_R32G32B32A32_FLOAT
    const size_t DXGI_FORMAT_R32G32B32A32_FLOAT_STRIDE = 16;

    // Gets the value of channel `channel` in pixel index `offset` in image `imageData`
    // assumed to be formatted as DXGI_FORMAT_R32G32B32A32_FLOAT
    float* GetChannelValue(uint8_t * imageData, size_t offset, Channel channel)
    {
        return reinterpret_cast<float*>(imageData + offset * DXGI_FORMAT_R32G32B32A32_FLOAT_STRIDE + channel);
    }

    std::string SaveAsPng(std::unique_ptr<DirectX::ScratchImage>& image, const std::string& fileName, const std::string& directory)
    {
        wchar_t outputImageFullPath[MAX_PATH];
        auto fileNameW = std::wstring(fileName.begin(), fileName.end());
        auto directoryW = std::wstring(directory.begin(), directory.end());

        if (FAILED(::PathCchCombine(outputImageFullPath, ARRAYSIZE(outputImageFullPath), directoryW.c_str(), fileNameW.c_str())))
        {
            throw GLTFException("Failed to compose output file path.");
        }

        const DirectX::Image* img = image->GetImage(0, 0, 0);
        if (FAILED(SaveToWICFile(*img, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, GUID_ContainerFormatPng, outputImageFullPath, &GUID_WICPixelFormat24bppBGR)))
        {
            throw GLTFException("Failed to save file.");
        }

        std::wstring outputImageFullPathStr(outputImageFullPath);
        return std::string(outputImageFullPathStr.begin(), outputImageFullPathStr.end());
    }

    std::string AddImageToDocument(GLTFDocument& doc, const std::string& imageUri)
    { 
        Image image;
        auto imageId = std::to_string(doc.images.Size());
        image.id = imageId;
        image.uri = imageUri;
        doc.images.Append(std::move(image));

        return imageId;
    }

    void AddTextureToExtension(const std::string& imageId, TexturePacking packing, GLTFDocument& doc, rapidjson::Value& packedExtensionJson, rapidjson::MemoryPoolAllocator<>& a)
    {
        Texture packedTexture;
        auto textureId = std::to_string(doc.textures.Size());
        packedTexture.id = textureId;
        packedTexture.imageId = imageId;
        doc.textures.Append(std::move(packedTexture));

        rapidjson::Value packedTextureJson(rapidjson::kObjectType);
        {
            packedTextureJson.AddMember(rapidjson::StringRef(MSFT_PACKING_INDEX_KEY), rapidjson::Value(std::stoi(textureId)), a);
        }
        switch (packing)
        {
        case TexturePacking::OcclusionRoughnessMetallic:
            packedExtensionJson.AddMember(rapidjson::StringRef(MSFT_PACKING_ORM_ORMTEXTURE_KEY), packedTextureJson, a);
            break;
        case TexturePacking::RoughnessMetallicOcclusion:
            packedExtensionJson.AddMember(rapidjson::StringRef(MSFT_PACKING_ORM_RMOTEXTURE_KEY), packedTextureJson, a);
            break;
        case TexturePacking::NormalRoughnessMetallic:
            packedExtensionJson.AddMember(rapidjson::StringRef(MSFT_PACKING_NRM_KEY), packedTextureJson, a);
            break;
        default:
            throw GLTFException("Invalid packing.");
        }
    }
}

GLTFDocument GLTFTexturePackingUtils::PackMaterialForWindowsMR(const IStreamReader& streamReader, const GLTFDocument& doc, const Material& material, TexturePacking packing, const std::string& outputDirectory)
{
    GLTFDocument outputDoc(doc);

    // No packing requested, return copy of document
    if (packing == TexturePacking::None)
    {
        return outputDoc;
    }

    // Read images from material
    auto metallicRoughness = material.metallicRoughness.metallicRoughnessTextureId;
    auto normal = material.normalTexture.id;
    auto occlusion = material.occlusionTexture.id;

    bool hasMR = !metallicRoughness.empty();
    bool hasNormal = !normal.empty();
    bool hasOcclusion = !occlusion.empty();

    // Early return if there's nothing to pack
    if (!hasMR && !hasOcclusion && !hasNormal)
    {
        // RM, O and Normal are empty, and the packing requires at least one of them
        return outputDoc;
    }

    // TODO: Optimization - If the texture pair (MR + O) has already been packed together with the 
    // current packing, point to that existing texture instead of creating a new one

    Material outputMaterial = outputDoc.materials.Get(material.id);

    // Create the JSON for the material extension element
    rapidjson::Document ormExtensionJson;
    ormExtensionJson.SetObject();
    rapidjson::MemoryPoolAllocator<>& ormAllocator = ormExtensionJson.GetAllocator();

    rapidjson::Document nrmExtensionJson;
    nrmExtensionJson.SetObject();
    rapidjson::MemoryPoolAllocator<>& nrmAllocator = nrmExtensionJson.GetAllocator();

    std::unique_ptr<DirectX::ScratchImage> metallicRoughnessImage = nullptr;
    uint8_t *mrPixels = nullptr;
    if (hasMR)
    {
        try
        {
            metallicRoughnessImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureLoadingUtils::LoadTexture(streamReader, doc, metallicRoughness));
            mrPixels = metallicRoughnessImage->GetPixels();
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load metallic roughness texture.");
        }
    }

    bool packingIncludesOrm = (packing & (TexturePacking::OcclusionRoughnessMetallic | TexturePacking::RoughnessMetallicOcclusion)) > 0;

    std::unique_ptr<DirectX::ScratchImage> occlusionImage = nullptr;
    uint8_t *occlusionPixels = nullptr;
    if (hasOcclusion && packingIncludesOrm)
    {
        try
        {
            occlusionImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureLoadingUtils::LoadTexture(streamReader, doc, occlusion));
            occlusionPixels = occlusionImage->GetPixels();
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load occlusion texture.");
        }
    }

    bool packingIncludesNrm = (packing & TexturePacking::NormalRoughnessMetallic) > 0;

    std::unique_ptr<DirectX::ScratchImage> normalImage = nullptr;
    uint8_t *normalPixels = nullptr;
    if (hasNormal && packingIncludesNrm)
    {
        try
        {
            normalImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureLoadingUtils::LoadTexture(streamReader, doc, normal));
            normalPixels = normalImage->GetPixels();
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load normal texture.");
        }
    }

    // Pack textures using DirectXTex

    if (packing & TexturePacking::OcclusionRoughnessMetallic && (hasMR || hasOcclusion))
    {
        std::string ormImageId;

        // If occlusion and metallic roughness are pointing to the same texture,
        // according to the GLTF spec, that texture is already packed as ORM
        // (occlusion = R, roughness = G, metalness = B)
        if (occlusion == metallicRoughness && hasOcclusion)
        {
            ormImageId = metallicRoughness;
        }
        else
        {
            auto orm = std::make_unique<DirectX::ScratchImage>();

            auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *occlusionImage->GetImage(0, 0, 0);
            if (FAILED(orm->Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
            {
                throw GLTFException("Failed to initialize from texture.");
            }

            auto ormPixels = orm->GetPixels();
            auto metadata = orm->GetMetadata();

            // TODO: resize?

            for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
            {
                // Occlusion: Occ [R] -> ORM [R]
                *GetChannelValue(ormPixels, i, Channel::Red) = hasOcclusion ? *GetChannelValue(occlusionPixels, i, Channel::Red) : 255.0f;
                // Roughness: MR [G] -> ORM [G]
                *GetChannelValue(ormPixels, i, Channel::Green) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
                // Metalness: MR [B] -> ORM [B]
                *GetChannelValue(ormPixels, i, Channel::Blue) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
            }

            auto imagePath = SaveAsPng(orm, "packing_orm_" + material.id + ".png", outputDirectory);

            ormImageId = AddImageToDocument(outputDoc, imagePath);
        }

        AddTextureToExtension(ormImageId, TexturePacking::OcclusionRoughnessMetallic, outputDoc, ormExtensionJson, ormAllocator);
    }

    if (packing & TexturePacking::RoughnessMetallicOcclusion && (hasMR || hasOcclusion))
    {
        auto rmo = std::make_unique<DirectX::ScratchImage>();

        // TODO: resize?

        auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *occlusionImage->GetImage(0, 0, 0);
        if (FAILED(rmo->Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
        {
            throw GLTFException("Failed to initialize from texture.");
        }

        auto rmoPixels = rmo->GetPixels();
        auto metadata = rmo->GetMetadata();

        for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
        {
            // Roughness: MR [G] -> RMO [R]
            *GetChannelValue(rmoPixels, i, Channel::Red) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
            // Metalness: MR [B] -> RMO [G]
            *GetChannelValue(rmoPixels, i, Channel::Green) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
            // Occlusion: Occ [R] -> RMO [B]
            *GetChannelValue(rmoPixels, i, Channel::Blue) = hasOcclusion ? *GetChannelValue(occlusionPixels, i, Channel::Red) : 255.0f;
        }

        auto imagePath = SaveAsPng(rmo, "packing_rmo_" + material.id + ".png", outputDirectory);

        // Add back to GLTF
        auto rmoImageId = AddImageToDocument(outputDoc, imagePath);

        AddTextureToExtension(rmoImageId, TexturePacking::RoughnessMetallicOcclusion, outputDoc, ormExtensionJson, ormAllocator);
    }

    if (packingIncludesNrm && (hasMR || hasNormal))
    {
        auto nrm = std::make_unique<DirectX::ScratchImage>();

        // TODO: resize?

        auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *normalImage->GetImage(0, 0, 0);
        if (FAILED(nrm->Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
        {
            throw GLTFException("Failed to initialize from texture.");
        }

        auto nrmPixels = nrm->GetPixels();
        auto metadata = nrm->GetMetadata();

        for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
        {
            // Normal: N [RG] -> NRM [RG]
            *GetChannelValue(nrmPixels, i, Channel::Red) = hasNormal ? *GetChannelValue(normalPixels, i, Channel::Red) : 255.0f;
            *GetChannelValue(nrmPixels, i, Channel::Green) = hasNormal ? *GetChannelValue(normalPixels, i, Channel::Green) : 255.0f;
            // Roughness: MR [G] -> NRM [B]
            *GetChannelValue(nrmPixels, i, Channel::Blue) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
            // Metalness: MR [B] -> NRM [A]
            *GetChannelValue(nrmPixels, i, Channel::Alpha) = hasMR ? *GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
        }

        auto imagePath = SaveAsPng(nrm, "packing_nrm_" + material.id + ".png", outputDirectory);

        // Add back to GLTF
        auto nrmImageId = AddImageToDocument(outputDoc, imagePath);

        AddTextureToExtension(nrmImageId, TexturePacking::NormalRoughnessMetallic, outputDoc, nrmExtensionJson, nrmAllocator);
    }

    if (packingIncludesOrm)
    {
        if (hasNormal)
        {
            rapidjson::Value ormNormalTextureJson(rapidjson::kObjectType);
            {
                ormNormalTextureJson.AddMember(rapidjson::StringRef(MSFT_PACKING_INDEX_KEY), rapidjson::Value(std::stoi(normal)), ormAllocator);
            }
            ormExtensionJson.AddMember(rapidjson::StringRef(MSFT_PACKING_ORM_NORMALTEXTURE_KEY), ormNormalTextureJson, ormAllocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        ormExtensionJson.Accept(writer);

        outputMaterial.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_PACKING_ORM, buffer.GetString()));

        outputDoc.extensionsUsed.insert(EXTENSION_MSFT_PACKING_ORM);
    }

    if (packingIncludesNrm)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        nrmExtensionJson.Accept(writer);

        outputMaterial.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_PACKING_NRM, buffer.GetString()));

        outputDoc.extensionsUsed.insert(EXTENSION_MSFT_PACKING_NRM);
    }

    outputDoc.materials.Replace(outputMaterial);

    return outputDoc;
}

GLTFDocument GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, TexturePacking packing, const std::string& outputDirectory)
{
    GLTFDocument outputDoc(doc);

    // No packing requested, return copy of document
    if (packing == TexturePacking::None)
    {
        return outputDoc;
    }

    for (auto material : doc.materials.Elements())
    {
        outputDoc = PackMaterialForWindowsMR(streamReader, outputDoc, material, packing, outputDirectory);
    }

    return outputDoc;
}