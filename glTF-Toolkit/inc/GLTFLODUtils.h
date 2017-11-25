// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTFDocument.h>

namespace Microsoft { namespace glTF { namespace Toolkit
{
    extern const char* EXTENSION_MSFT_LOD;
    extern const char* MSFT_LOD_IDS_KEY;
    typedef std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>> LODMap;
        
    class GLTFLODUtils
    {
    public:
        static LODMap ParseDocumentNodeLODs(const GLTFDocument& doc);

        /// <summary>
        /// Inserts the specified lod GLTFDocument as a node lod (at the root level) of the specified primary GLTF Document
        /// Note: Animation is not currently supported
        /// </summary>
        /// <returns>The primary GLTF Document with the inserted lod node</returns>
        static GLTFDocument MergeDocumentAsLODs(const std::vector<GLTFDocument>& docs);

        /// <summary>
        /// Inserts the specified lod GLTFDocument as a node lod (at the root level) of the specified primary GLTF Document
        /// Note: Animation is not currently supported
        /// </summary>
        /// <returns>The primary GLTF Document with the inserted lod node</returns>
        static GLTFDocument MergeDocumentAsLODs(const std::vector<GLTFDocument>& docs, const std::vector<double>& screenCoveragePercentages);

        /// <summary>
        /// Determines the highest number of Node LODs for the given gltf document
        /// </summary>
        /// <returns>The highest number of Material LODs</returns>
        static uint32_t NumberOfNodeLODLevels(const GLTFDocument& doc, const LODMap& lods);
    };
}}}

