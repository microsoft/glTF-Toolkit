// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <DirectXTex.h>

#include "GLTFTextureUtils.h"
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
    void AddTextureToExtension(const std::string& imageId, TexturePacking packing, Document& doc, rapidjson::Value& packedExtensionJson, rapidjson::MemoryPoolAllocator<>& a)
    {
        Texture packedTexture;
        packedTexture.imageId = imageId;
        auto textureId = doc.textures.Append(std::move(packedTexture), AppendIdPolicy::GenerateOnEmpty).id;

        rapidjson::Value packedTextureJson(rapidjson::kObjectType);
        {
            packedTextureJson.AddMember(rapidjson::StringRef(MSFT_PACKING_INDEX_KEY), rapidjson::Value(doc.textures.GetIndex(textureId)), a);
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

std::unordered_set<int> GLTFTexturePackingUtils::GetTextureIndicesFromMsftExtensions(const Material& material)
{
    static const char* extensionKeys[] = {
        EXTENSION_MSFT_PACKING_ORM,
        EXTENSION_MSFT_PACKING_NRM
    };

    static const char* textureKeys[] = {
        MSFT_PACKING_ORM_ORMTEXTURE_KEY,
        MSFT_PACKING_ORM_RMOTEXTURE_KEY,
        MSFT_PACKING_ORM_NORMALTEXTURE_KEY,
        MSFT_PACKING_NRM_KEY
    };

    std::unordered_set<int> textureIndices;

    for (const auto& extensionKey : extensionKeys)
    {
        auto extensionIt = material.extensions.find(extensionKey);
        if (extensionIt != material.extensions.end() && !extensionIt->second.empty())
        {
            rapidjson::Document extJson = RapidJsonUtils::CreateDocumentFromString(extensionIt->second);

            for (const auto& textureKey : textureKeys)
            {
                if (extJson.HasMember(textureKey))
                {
                    const auto index = extJson[textureKey][MSFT_PACKING_INDEX_KEY].GetInt();
                    textureIndices.insert(index);
                }
            }
        }
    }

    return textureIndices;
}

Document GLTFTexturePackingUtils::PackMaterialForWindowsMR(std::shared_ptr<IStreamReader> streamReader, const Document& doc, const Material& material, TexturePacking packing, const std::string& outputDirectory)
{
    Document outputDoc(doc);

    // No packing requested, return copy of document
    if (packing == TexturePacking::None)
    {
        return outputDoc;
    }

    // Read images from material
    auto metallicRoughness = material.metallicRoughness.metallicRoughnessTexture.textureId;
    auto normal = material.normalTexture.textureId;
    auto occlusion = material.occlusionTexture.textureId;

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
    if (hasMR)
    {
        try
        {
            metallicRoughnessImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, metallicRoughness));
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load metallic roughness texture.");
        }
    }

    bool packingIncludesOrm = (packing & (TexturePacking::OcclusionRoughnessMetallic | TexturePacking::RoughnessMetallicOcclusion)) > 0;

    std::unique_ptr<DirectX::ScratchImage> occlusionImage = nullptr;
    if (hasOcclusion && packingIncludesOrm)
    {
        try
        {
            occlusionImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, occlusion));
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load occlusion texture.");
        }
    }

    if (hasMR && hasOcclusion && packingIncludesOrm)
    {
        GLTFTextureUtils::ResizeToLargest(metallicRoughnessImage, occlusionImage);
    }

    bool packingIncludesNrm = (packing & TexturePacking::NormalRoughnessMetallic) > 0;

    std::unique_ptr<DirectX::ScratchImage> normalImage = nullptr;
    if (hasNormal && packingIncludesNrm)
    {
        try
        {
            normalImage = std::make_unique<DirectX::ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, normal));
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load normal texture.");
        }
    }

    if (hasMR && hasNormal && packingIncludesNrm)
    {
        GLTFTextureUtils::ResizeToLargest(metallicRoughnessImage, normalImage);
    }

    uint8_t *mrPixels = metallicRoughnessImage != nullptr ? metallicRoughnessImage->GetPixels() : nullptr;
    uint8_t *occlusionPixels = occlusionImage != nullptr ? occlusionImage->GetPixels() : nullptr;
    uint8_t *normalPixels = normalImage != nullptr ? normalImage->GetPixels() : nullptr;

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
            DirectX::ScratchImage orm;

            auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *occlusionImage->GetImage(0, 0, 0);
            if (FAILED(orm.Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
            {
                throw GLTFException("Failed to initialize from texture.");
            }

            auto ormPixels = orm.GetPixels();
            auto metadata = orm.GetMetadata();

            for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
            {
                // Occlusion: Occ [R] -> ORM [R]
                *GLTFTextureUtils::GetChannelValue(ormPixels, i, Channel::Red) = hasOcclusion ? *GLTFTextureUtils::GetChannelValue(occlusionPixels, i, Channel::Red) : 255.0f;
                // Roughness: MR [G] -> ORM [G]
                *GLTFTextureUtils::GetChannelValue(ormPixels, i, Channel::Green) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
                // Metalness: MR [B] -> ORM [B]
                *GLTFTextureUtils::GetChannelValue(ormPixels, i, Channel::Blue) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
            }

            // Convert with assumed sRGB because PNG defaults to that color space.
            DirectX::ScratchImage converted;
            if (FAILED(DirectX::Convert(*orm.GetImage(0, 0, 0), DXGI_FORMAT_B8G8R8X8_UNORM, DirectX::TEX_FILTER_SRGB_IN, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
            {
                throw GLTFException("Failed to convert texture to DXGI_FORMAT_B8G8R8X8_UNORM for storage.");
            }

            auto imagePath = GLTFTextureUtils::SaveAsPng(&converted, "packing_orm_" + material.id + ".png", outputDirectory);

            ormImageId = GLTFTextureUtils::AddImageToDocument(outputDoc, imagePath);
        }

        AddTextureToExtension(ormImageId, TexturePacking::OcclusionRoughnessMetallic, outputDoc, ormExtensionJson, ormAllocator);
    }

    if (packing & TexturePacking::RoughnessMetallicOcclusion && (hasMR || hasOcclusion))
    {
        DirectX::ScratchImage rmo;

        auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *occlusionImage->GetImage(0, 0, 0);
        if (FAILED(rmo.Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
        {
            throw GLTFException("Failed to initialize from texture.");
        }

        auto rmoPixels = rmo.GetPixels();
        auto metadata = rmo.GetMetadata();

        for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
        {
            // Roughness: MR [G] -> RMO [R]
            *GLTFTextureUtils::GetChannelValue(rmoPixels, i, Channel::Red) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
            // Metalness: MR [B] -> RMO [G]
            *GLTFTextureUtils::GetChannelValue(rmoPixels, i, Channel::Green) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
            // Occlusion: Occ [R] -> RMO [B]
            *GLTFTextureUtils::GetChannelValue(rmoPixels, i, Channel::Blue) = hasOcclusion ? *GLTFTextureUtils::GetChannelValue(occlusionPixels, i, Channel::Red) : 255.0f;
        }

        // Convert with assumed sRGB because PNG defaults to that color space.
        DirectX::ScratchImage converted;
        if (FAILED(DirectX::Convert(*rmo.GetImage(0, 0, 0), DXGI_FORMAT_B8G8R8X8_UNORM, DirectX::TEX_FILTER_SRGB_IN, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
        {
            throw GLTFException("Failed to convert texture to DXGI_FORMAT_B8G8R8X8_UNORM for storage.");
        }

        auto imagePath = GLTFTextureUtils::SaveAsPng(&converted, "packing_rmo_" + material.id + ".png", outputDirectory);

        // Add back to GLTF
        auto rmoImageId = GLTFTextureUtils::AddImageToDocument(outputDoc, imagePath);

        AddTextureToExtension(rmoImageId, TexturePacking::RoughnessMetallicOcclusion, outputDoc, ormExtensionJson, ormAllocator);
    }

    if (packingIncludesNrm && (hasMR || hasNormal))
    {
        DirectX::ScratchImage nrm;

        auto sourceImage = hasMR ? *metallicRoughnessImage->GetImage(0, 0, 0) : *normalImage->GetImage(0, 0, 0);
        if (FAILED(nrm.Initialize2D(sourceImage.format, sourceImage.width, sourceImage.height, 1, 1)))
        {
            throw GLTFException("Failed to initialize from texture.");
        }

        auto nrmPixels = nrm.GetPixels();
        auto metadata = nrm.GetMetadata();

        for (size_t i = 0; i < metadata.width * metadata.height; i += 1)
        {
            // Normal: N [RG] -> NRM [RG]
            *GLTFTextureUtils::GetChannelValue(nrmPixels, i, Channel::Red) = hasNormal ? *GLTFTextureUtils::GetChannelValue(normalPixels, i, Channel::Red) : 255.0f;
            *GLTFTextureUtils::GetChannelValue(nrmPixels, i, Channel::Green) = hasNormal ? *GLTFTextureUtils::GetChannelValue(normalPixels, i, Channel::Green) : 255.0f;
            // Roughness: MR [G] -> NRM [B]
            *GLTFTextureUtils::GetChannelValue(nrmPixels, i, Channel::Blue) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Green) : 255.0f;
            // Metalness: MR [B] -> NRM [A]
            *GLTFTextureUtils::GetChannelValue(nrmPixels, i, Channel::Alpha) = hasMR ? *GLTFTextureUtils::GetChannelValue(mrPixels, i, Channel::Blue) : 255.0f;
        }

        // sRGB conversion not needed for PNG in BGRA
        auto imagePath = GLTFTextureUtils::SaveAsPng(&nrm, "packing_nrm_" + material.id + ".png", outputDirectory, &GUID_WICPixelFormat32bppBGRA);

        // Add back to GLTF
        auto nrmImageId = GLTFTextureUtils::AddImageToDocument(outputDoc, imagePath);

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

Document GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(std::shared_ptr<IStreamReader> streamReader, const Document & doc, TexturePacking packing, const std::string& outputDirectory)
{
    Document outputDoc(doc);

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