// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "GLTFSDK.h"
#include "GLTFSDK/BufferBuilder.h"

namespace Microsoft::glTF::Toolkit
{
    /// <summary>Draco compression options.</summary>
    struct CompressionOptions
    {
        int PositionQuantizationBits = 14;
        int TexCoordQuantizationBits = 12;
        int NormalQuantizationBits = 10;
        int ColorQuantizationBits = 8;
        int GenericQuantizationBits = 12;
        int Speed = 3;
    };

    /// <summary>
    /// Utilities to compress textures in a glTF asset.
    /// </summary>
    class GLTFMeshCompressionUtils
    {
    public:
        /// <summary>
        /// Applies <see cref="CompressMesh" /> to every mesh in the document, following the same parameter structure as that function.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the mesh will be loaded.</param>
        /// <param name="options">The compression options that will be used.</param>
        /// <param name="outputDirectory">The output directory to which compressed data should be saved.</param>
        /// <returns>
        /// A new glTF manifest that uses the KHR_draco_mesh_compression extension to point to the compressed meshes.
        /// </returns>
        static Document CompressMeshes(
            std::shared_ptr<IStreamReader> streamReader,
            const Document & doc,
            CompressionOptions options,
            const std::string& outputDirectory);

        /// <summary>
        /// Applies Draco mesh compression to the supplied mesh and creates a new set of vertex buffers for all the primitive attributes.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the mesh will be loaded.</param>
        /// <param name="mesh">The mesh which the mesh will be compressed.</param>
        /// <param name="options">The compression options that will be used.</param>
        /// <param name="builder">The output buffer builder that handles bufferId generation for the return document.</param>
        /// <param name="bufferViewsToRemove">Out parameter of BufferView Ids that are no longer in use and should be removed.</param>
        /// <returns>
        /// A new glTF manifest that uses the KHR_draco_mesh_compression extension to point to the compressed meshes.
        /// </returns>
        static Document CompressMesh(
            std::shared_ptr<IStreamReader> streamReader,
            const Document & doc,
            CompressionOptions options,
            const Mesh & mesh,
            BufferBuilder* builder,
            std::unordered_set<std::string>& bufferViewsToRemove);
    };
}