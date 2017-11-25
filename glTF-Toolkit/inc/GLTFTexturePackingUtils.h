// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTFDocument.h>

namespace Microsoft { namespace glTF { namespace Toolkit
{
    extern const char* EXTENSION_MSFT_PACKING_ORM;

    // Texture packing flags. May be combined to pack multiple formats at once.
    enum TexturePacking
    {
        None = 0x0,
        OcclusionRoughnessMetallic = 0x1,
        RoughnessMetallicOcclusion = 0x2
    };

    class GLTFTexturePackingUtils
    {
    public:
        // Packs a single material's textures for Windows MR for all the packing schemes selected, and adds the resulting texture(s) back to the material in the document.
        static GLTFDocument PackMaterialForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, const Material & material, TexturePacking packing, const std::string& outputDirectory);

        // Applies PackMaterialForWindowsMR to every material in the document, following the same parameter structure as that function.
        static GLTFDocument PackAllMaterialsForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, TexturePacking packing, const std::string& outputDirectory);
    };
}}}

