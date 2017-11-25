// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.#pragma once

#include <GLTFSDK/GLTFDocument.h>
#include <GLTFSDK/IStreamReader.h>
#include <DirectXTex.h>

namespace Microsoft { namespace glTF { namespace Toolkit
{
    class GLTFTextureLoadingUtils
    {
    public:
        static DirectX::ScratchImage LoadTexture(const IStreamReader& streamReader, const GLTFDocument& doc, const std::string& textureId);
    };
}}}

