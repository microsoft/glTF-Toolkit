// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK.h>

namespace Microsoft::glTF::Toolkit
{
    // Specifies the format of how each primitive is stored.
    enum class PrimitiveFormat : uint8_t
    {
        // Detect and preserve the existing structure of each underlying format.
        Preserved = 0,

        // Primitive index & vertex data are combined into a globalized set over the entire mesh. 
        // Note: Creates the least number of API objects when rendering.
        Combine = 1,

        // Primitives are partitioned into their own localized segments.
        // Note: Allows for additional per-primitive compression on index & vertex data.
        Separate = 2,
    };


    // Specifies the format of each primitive's vertex attributes.
    enum class AttributeFormat : uint8_t
    {
        // Vertex attributes are integrated into a single, interleaved buffer (array of structs).
        // Note: Fastest performance during draw calls with all attributes bound.
        Interleave = 0,

        // Vertex attributes are split into separate buffers (struct of arrays). 
        // Note: Worse performance, but provides flexibility of minimizing attribute selection when specifying input layouts.
        Separate = 1,
    };


    // Specifies the parameters under which to perform mesh optimization, vertex attribute generation, and output format.
    struct MeshOptions
    {
        bool Optimize = true;                                           // Perform an optimization pass on the mesh data (requires indices.)
        bool GenerateTangentSpace = false;                              // Generate normals and/or tangents if non-existent (requires indices.)
        PrimitiveFormat PrimitiveFormat = PrimitiveFormat::Separate;    // Specifies the format in which to output the mesh primitives.
        AttributeFormat AttributeFormat = AttributeFormat::Interleave;  // Specifies the format in which to output the attributes.
    };


    //-----------------------------------------------
    // GLTFMeshUtils

    class GLTFMeshUtils
    {
    public:
        static Document Process(const Document& doc, const MeshOptions& options, const std::string& bufferPrefix, std::shared_ptr<IStreamReader> reader, std::unique_ptr<IStreamWriter> writer);
        static Document Process(const Document& doc, const MeshOptions& options, const std::string& bufferPrefix, std::shared_ptr<IStreamReader> reader, std::unique_ptr<IStreamWriterCache> writerCache);
    };
}
