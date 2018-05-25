// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureLoadingUtils.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
}

DirectX::ScratchImage GLTFTextureLoadingUtils::LoadTexture(const IStreamReader& streamReader, const GLTFDocument& doc, const std::string& textureId)
{
    DirectX::ScratchImage output;

    const Texture& texture = doc.textures.Get(textureId);

    GLTFResourceReader gltfResourceReader(streamReader);

    const Image& image = doc.images.Get(texture.imageId);

    std::vector<uint8_t> imageData = gltfResourceReader.ReadBinaryData(doc, image);

    auto data = std::make_unique<uint8_t[]>(imageData.size());
    memcpy_s(data.get(), imageData.size(), imageData.data(), imageData.size());

    DirectX::TexMetadata info;
    if (FAILED(DirectX::LoadFromDDSMemory(data.get(), imageData.size(), DirectX::DDS_FLAGS_NONE, &info, output)))
    {
        // DDS failed, try WIC
        // Note: try DDS first since WIC can load some DDS (but not all), so we wouldn't want to get 
        // a partial or invalid DDS loaded from WIC.
        if (FAILED(DirectX::LoadFromWICMemory(data.get(), imageData.size(), DirectX::WIC_FLAGS_IGNORE_SRGB, &info, output)))
        {
            throw GLTFException("Failed to load image - Image could not be loaded as DDS or read by WIC.");
        }
    }

    if (info.format == DXGI_FORMAT_R32G32B32A32_FLOAT)
    {
        return output;
    }
    else 
    {
        DirectX::ScratchImage converted;
        if (FAILED(DirectX::Convert(*output.GetImage(0, 0, 0), DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
        {
            throw GLTFException("Failed to convert texture to DXGI_FORMAT_R32G32B32A32_FLOAT for processing.");
        }

        return converted;
    }
}