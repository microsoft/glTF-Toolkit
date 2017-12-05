// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.#pragma once

#include <GLTFSDK/GLTFDocument.h>
#include <GLTFSDK/IStreamReader.h>
#include <DirectXTex.h>

namespace Microsoft::glTF::Toolkit
{
    /// <summary>
    /// Utilities to load textures from glTF assets.
    /// </summary>
    class GLTFTextureLoadingUtils
    {
    public:
        /// <summary>
        /// Loads a texture into a scratch image in the DXGI_FORMAT_R32G32B32A32_FLOAT format for in-memory processing.
        /// </summary>
        /// <returns>A scratch image containing the loaded texture in the DXGI_FORMAT_R32G32B32A32_FLOAT format.</returns>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the texture will be loaded.</param>
        /// <param name="textureId">The identifier of the texture to be loaded.</param>
        static DirectX::ScratchImage LoadTexture(const IStreamReader& streamReader, const GLTFDocument& doc, const std::string& textureId);
    };
}

