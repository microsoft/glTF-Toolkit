// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "GLTFSDK.h"

namespace DirectX
{
    class ScratchImage;
}

namespace Microsoft::glTF::Toolkit
{
    extern const char* EXTENSION_MSFT_TEXTURE_DDS;

    /// <summary>Supported compression algorithms for textures.</summary>
    enum class TextureCompression
    {
        None,
        BC3,
        BC5,
        BC7,
        BC7_SRGB
    };

    /// <summary>
    /// Utilities to compress textures in a glTF asset.
    /// </summary>
    class GLTFTextureCompressionUtils
    {
    public:
        /// <summary>Compresses a texture in a glTF from a WIC-readable format (PNG, JPEG, BMP, GIF, TIFF, HD Photo, ICO) 
        /// into a DDS with the appropriate compression.
        /// <para>If a dds extension already exists for this texture, do nothing.</para>
        /// <param name="streamReader">The stream reader that will be used to get streams to each image from its URI.</param>
        /// <param name="doc">Input glTF document.</param>
        /// <param name="texture">Texture object that is contained in input document. If texture does not exist in document, 
        /// throws exception.</param>
        /// <param name="compression">The desired block compression method (e.g. BC5, BC7).</param>
        /// <param name="outputDirectory">The output directory to which compressed files should be saved.</param>
        /// <param name="maxTextureSize">The maximum texture size to which textures should be resized before compression, in pixels.</param>
        /// <param name="generateMipMaps">If true, also generates mip maps when compressing.</param>
        /// <param name="retainOriginalImage">If true, retains the original image on the resulting glTF. If false, 
        /// replaces that image (making the glTF incompatible with most core glTF 2.0 viewers).</param>
        /// <returns>Returns a new GLTFDocument that contains a new reference to the compressed dds file added as part 
        /// of the MSFT_texture_dds extension.</returns>
        /// <example>
        /// Example Input:
        /// <code>
        /// "textures": [
        ///    {
        ///        "source": 0,
        ///    }
        /// ],
        /// "images": [
        /// {
        ///    "uri": "defaultTexture.png"
        /// }
        /// ]
        /// </code>
        ///
        /// Example Output (BC7 Compression, with retainOriginalImage == true):
        /// <code>
        /// "textures": 
        /// [
        ///    {
        ///        "source": 0,
        ///        "extensions": {
        ///            "MSFT_texture_dds": {
        ///                "source": 1
        ///            }
        ///        }
        ///    }
        /// ],
        /// "images": [
        /// {
        ///    "uri": "defaultTexture.png"
        /// },
        /// {
        ///    "uri": "defaultTexture-BC7.DDS"
        /// }
        /// ]
        /// </code>
        /// </example>
        /// </summary>
        static GLTFDocument CompressTextureAsDDS(const IStreamReader& streamReader, const GLTFDocument & doc, const Texture & texture, TextureCompression compression, const std::string& outputDirectory, size_t maxTextureSize = std::numeric_limits<size_t>::max(), bool generateMipMaps = true, bool retainOriginalImage = true);

        /// <summary>
        /// Applies <see cref="CompressTextureAsDDS" /> to all textures in the document that are accessible via materials according to the 
        /// requirements of the Windows Mixed Reality home.
        /// <para>Normal textures get compressed with BC5, while baseColorTexture, occlusion, metallicRoughness and emissive textures get compressed with BC7.</para>
        /// <param name="streamReader">The stream reader that will be used to get streams to each image from its URI.</param>
        /// <param name="doc">Input glTF document.</param>
        /// <param name="outputDirectory">The output directory to which compressed files should be saved.</param>
        /// <param name="maxTextureSize">The maximum texture size to which textures should be resized before compression, in pixels.</param>
        /// <param name="generateMipMaps">If true, also generates mip maps when compressing.</param>
        /// <param name="retainOriginalImage">If true, retains the original image on the resulting glTF. If false, 
        /// replaces that image (making the glTF incompatible with most core glTF 2.0 viewers).</param>
        /// <returns>Returns a new GLTFDocument that contains alternate textures for all applicable materials following the requirements of the Windows
        /// Mixed Reality home using the MSFT_texture_dds extension.</returns>
        /// </summary>
        static GLTFDocument CompressAllTexturesForWindowsMR(const IStreamReader& streamReader, const GLTFDocument & doc, const std::string& outputDirectory, size_t maxTextureSize = std::numeric_limits<size_t>::max(), bool retainOriginalImages = true);

        /// <summary>
        /// Compresses a DirectX::ScratchImage in place using the specified compression.
        /// </summary>
        /// <param name="image">The image to compress.</param>
        /// <param name="compression">The desired compression algorithm.</param>
        static void CompressImage(DirectX::ScratchImage& image, TextureCompression compression);
    };
}