// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureUtils.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

DirectX::ScratchImage GLTFTextureUtils::LoadTexture(std::shared_ptr<const IStreamReader> streamReader, const Document& doc, const std::string& textureId, bool treatAsLinear)
{
    DirectX::ScratchImage output;

    const Texture& texture = doc.textures.Get(textureId);

    GLTFResourceReader gltfResourceReader(streamReader);

    const Image& image = doc.images.Get(texture.imageId);

    std::vector<uint8_t> imageData = gltfResourceReader.ReadBinaryData(doc, image);

    DirectX::TexMetadata info;
    if (FAILED(DirectX::LoadFromDDSMemory(imageData.data(), imageData.size(), DirectX::DDS_FLAGS_NONE, &info, output)))
    {
        // DDS failed, try WIC
        // Note: try DDS first since WIC can load some DDS (but not all), so we wouldn't want to get 
        // a partial or invalid DDS loaded from WIC.
        if (FAILED(DirectX::LoadFromWICMemory(imageData.data(), imageData.size(), treatAsLinear ? DirectX::WIC_FLAGS_IGNORE_SRGB : DirectX::WIC_FLAGS_NONE, &info, output)))
        {
            throw GLTFException("Failed to load image - Image could not be loaded as DDS or read by WIC.");
        }
    }

    if (info.format == DXGI_FORMAT_R32G32B32A32_FLOAT && treatAsLinear)
    {
        return output;
    }
    else 
    {
        DirectX::ScratchImage converted;
        if (FAILED(DirectX::Convert(*output.GetImage(0, 0, 0), DXGI_FORMAT_R32G32B32A32_FLOAT, treatAsLinear ? DirectX::TEX_FILTER_DEFAULT : DirectX::TEX_FILTER_SRGB_IN, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
        {
            throw GLTFException("Failed to convert texture to DXGI_FORMAT_R32G32B32A32_FLOAT for processing.");
        }

        return converted;
    }
}

// Constants for the format DXGI_FORMAT_R32G32B32A32_FLOAT
constexpr size_t DXGI_FORMAT_R32G32B32A32_FLOAT_STRIDE = 16;

float* GLTFTextureUtils::GetChannelValue(uint8_t * imageData, size_t offset, Channel channel)
{
    return reinterpret_cast<float*>(imageData + offset * DXGI_FORMAT_R32G32B32A32_FLOAT_STRIDE + channel);
}

std::string GLTFTextureUtils::SaveAsPng(DirectX::ScratchImage* image, const std::string& fileName, const std::string& directory, const GUID* targetFormat)
{
    wchar_t outputImageFullPath[MAX_PATH];
    auto fileNameW = std::wstring(fileName.begin(), fileName.end());
    auto directoryW = std::wstring(directory.begin(), directory.end());

    if (FAILED(::PathCchCombine(outputImageFullPath, ARRAYSIZE(outputImageFullPath), directoryW.c_str(), fileNameW.c_str())))
    {
        throw GLTFException("Failed to compose output file path.");
    }

    const DirectX::Image* img = image->GetImage(0, 0, 0);
    if (FAILED(SaveToWICFile(*img, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, GUID_ContainerFormatPng, outputImageFullPath, targetFormat)))
    {
        throw GLTFException("Failed to save file.");
    }

    std::wstring outputImageFullPathStr(outputImageFullPath);
    return std::string(outputImageFullPathStr.begin(), outputImageFullPathStr.end());
}

std::string GLTFTextureUtils::AddImageToDocument(Document& doc, const std::string& imageUri)
{
    Image image;
    auto imageId = std::to_string(doc.images.Size());
    image.id = imageId;
    image.uri = imageUri;
    doc.images.Append(std::move(image));

    return imageId;
}

void GLTFTextureUtils::ResizeIfNeeded(const std::unique_ptr<DirectX::ScratchImage>& image, size_t resizedWidth, size_t resizedHeight)
{
    auto metadata = image->GetMetadata();
    if (resizedWidth != metadata.width || resizedHeight != metadata.height)
    {
        DirectX::ScratchImage resized;
        if (FAILED(DirectX::Resize(image->GetImages(), image->GetImageCount(), metadata, resizedWidth, resizedHeight, DirectX::TEX_FILTER_DEFAULT, resized)))
        {
            throw GLTFException("Failed to resize image while packing.");
        }

        *image = std::move(resized);
    }
}

Document GLTFTextureUtils::RemoveRedundantTexturesAndImages(const Document& doc)
{
    Document resultDocument(doc);

    // 1. Find used textures
    std::vector<std::string> usedTextureIds;
    for (const auto& material : doc.materials.Elements())
    {
        std::vector<std::string> textureIds = {
            material.metallicRoughness.baseColorTexture.textureId,
            material.metallicRoughness.metallicRoughnessTexture.textureId,
            material.normalTexture.textureId,
            material.occlusionTexture.textureId,
            material.emissiveTexture.textureId
        };

        for (const auto& textureId : textureIds)
        {
            const auto textureIdHasBeenAdded = std::find(usedTextureIds.begin(), usedTextureIds.end(), textureId) != usedTextureIds.end();
            if (!textureId.empty() && !textureIdHasBeenAdded)
            {
                usedTextureIds.push_back(textureId);
            }
        }
    }

    // 2. Find used images and remove unused textures
    std::vector<std::string> usedImageIds;
    for (const auto& texture : doc.textures.Elements())
    {
        const auto textureIsUsed = std::find(usedTextureIds.begin(), usedTextureIds.end(), texture.id) != usedTextureIds.end();
        const auto imageIdHasBeenAdded = std::find(usedImageIds.begin(), usedImageIds.end(), texture.imageId) != usedImageIds.end();

        if (textureIsUsed && !imageIdHasBeenAdded)
        {
            usedImageIds.push_back(texture.imageId);
        }
        else
        {
            resultDocument.textures.Remove(texture.id);
        }
    }

    // 3. Remove unused images
    for (const auto& image : doc.images.Elements())
    {
        auto imageIsUsed = std::find(usedImageIds.begin(), usedImageIds.end(), image.id) != usedImageIds.end();

        if (!imageIsUsed)
        {
            resultDocument.images.Remove(image.id);
        }
    }

    return resultDocument;
}

void GLTFTextureUtils::ResizeToLargest(std::unique_ptr<DirectX::ScratchImage>& image1, std::unique_ptr<DirectX::ScratchImage>& image2)
{
    auto metadata1 = image1->GetMetadata();
    auto metadata2 = image2->GetMetadata();
    if (metadata1.height != metadata2.height || metadata1.width != metadata2.width)
    {
        auto resizedWidth = std::max(metadata1.width, metadata2.width);
        auto resizedHeight = std::max(metadata1.height, metadata2.height);

        ResizeIfNeeded(image1, resizedWidth, resizedHeight);
        ResizeIfNeeded(image2, resizedWidth, resizedHeight);
    }
}