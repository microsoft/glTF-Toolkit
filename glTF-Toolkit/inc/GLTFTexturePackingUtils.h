// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "GLTFSDK.h"

namespace Microsoft::glTF::Toolkit
{
    extern const char* EXTENSION_MSFT_PACKING_ORM;
    extern const char* EXTENSION_MSFT_PACKING_NRM;
    extern const char* MSFT_PACKING_INDEX_KEY;
    extern const char* MSFT_PACKING_ORM_ORMTEXTURE_KEY;
    extern const char* MSFT_PACKING_ORM_RMOTEXTURE_KEY;
    extern const char* MSFT_PACKING_ORM_NORMALTEXTURE_KEY;
    extern const char* MSFT_PACKING_NRM_KEY;

    /// <summary>Texture packing flags. May be combined to pack multiple formats at once.</summary>
    enum TexturePacking
    {
        None = 0x0,
        OcclusionRoughnessMetallic = 0x1,
        RoughnessMetallicOcclusion = 0x2,
        NormalRoughnessMetallic = 0x4
    };

    /// <summary>
    /// Utilities to pack textures from glTF assets and refer to them from an asset
    /// using the MSFT_packing_occlusionRoughnessMetallic extension.
    /// </summary>
    class GLTFTexturePackingUtils
    {
    public:
        /// <summary>
        /// Packs a single material's textures for Windows Mixed Reality for all the packing schemes selected, and adds the resulting texture(s) back to the material in the document.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the texture will be loaded.</param>
        /// <param name="material">The material to be packed.</param>
        /// <param name="packing">The packing scheme that will be used to pick the textures and choose their order.</param>
        /// <param name="outputDirectory">The output directory to which packed textures should be saved.</param>
        /// <returns>
        /// A new glTF manifest that uses the MSFT_packing_occlusionRoughnessMetallic extension to point to the packed textures.
        /// </returns>
        static GLTFDocument PackMaterialForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, const Material & material, TexturePacking packing, const std::string& outputDirectory);

        /// <summary>
        /// Applies <see cref="PackMaterialForWindowsMR" /> to every material in the document, following the same parameter structure as that function.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the texture will be loaded.</param>
        /// <param name="packing">The packing scheme that will be used to pick the textures and choose their order.</param>
        /// <param name="outputDirectory">The output directory to which packed textures should be saved.</param>
        /// <returns>
        /// A new glTF manifest that uses the MSFT_packing_occlusionRoughnessMetallic extension to point to the packed textures.
        /// </returns>
        static GLTFDocument PackAllMaterialsForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, TexturePacking packing, const std::string& outputDirectory);
    };
}

