// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.#pragma once

#include "GLTFSDK.h"
#include <DirectXTex.h>
#include <GLTFSDK/Document.h>
#include <wincodec.h>

namespace Microsoft::glTF::Toolkit
{
    enum Channel
    {
        Red = 0,
        Green = 4,
        Blue = 8,
        Alpha = 12
    };

    /// <summary>
    /// Utilities to load textures from glTF assets.
    /// </summary>
    class GLTFTextureUtils
    {
    public:
        /// <summary>
        /// Loads a texture into a scratch image in the DXGI_FORMAT_R32G32B32A32_FLOAT format for in-memory processing.
        /// </summary>
        /// <returns>A scratch image containing the loaded texture in the DXGI_FORMAT_R32G32B32A32_FLOAT format.</returns>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the texture will be loaded.</param>
        /// <param name="textureId">The identifier of the texture to be loaded.</param>
        static DirectX::ScratchImage LoadTexture(std::shared_ptr<const IStreamReader> streamReader, const Document& doc, const std::string& textureId, bool treatAsLinear = true);

        /// <summary>
        /// Gets the value of channel `channel` in pixel index `offset` in image `imageData`
        /// assumed to be formatted as DXGI_FORMAT_R32G32B32A32_FLOAT
        /// </summary>
        static float* GetChannelValue(uint8_t * imageData, size_t offset, Channel channel);

        static std::string SaveAsPng(DirectX::ScratchImage* image, const std::string& fileName, const std::string& directory, const GUID* targetFormat = &GUID_WICPixelFormat24bppBGR);

        static std::string AddImageToDocument(Document& doc, const std::string& imageUri);
        
        static void ResizeToLargest(std::unique_ptr<DirectX::ScratchImage>& image1, std::unique_ptr<DirectX::ScratchImage>& image2);

        static void ResizeIfNeeded(const std::unique_ptr<DirectX::ScratchImage>& image, size_t resizedWidth, size_t resizedHeight);

        static Document RemoveRedundantTexturesAndImages(const Document& doc);
    };
}

