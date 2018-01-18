// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTFDocument.h>

namespace Microsoft::glTF::Toolkit
{
    extern const char* EXTENSION_MSFT_LOD;
    extern const char* MSFT_LOD_IDS_KEY;
    typedef std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>> LODMap;

    /// <summary>
    /// Utilities to load and merge levels of detail (LOD) in glTF assets using the MSFT_lod extension.
    /// </summary>
    class GLTFLODUtils
    {
    public:
        /// <summary>
        /// Parses the node LODs in a GLTF asset as a map that can be used to read LOD values for each node.
        /// </summary>
        /// <returns>A map that relates each node ID to a vector of its levels of detail node IDs.</returns>
        /// <param name="doc">The glTF document containing LODs to be parsed.</param>
        static LODMap ParseDocumentNodeLODs(const GLTFDocument& doc);

        /// <summary>
        /// Inserts each LOD GLTFDocument as a node LOD (at the root level) of the specified primary GLTF asset.
        /// Note: Animation is not currently supported.
        /// </summary>
        /// <returns>The primary GLTF Document with the inserted LOD node.</returns>
        /// <param name="docs">A vector of glTF documents to merge as LODs. The first element of the vector is assumed to be the primary LOD.</param>
        /// <param name="relativePaths">A vector of relative path prefixes to the non-LOD0 LOD gltf documents. Used for finding resources in those LODs.
        /// If not specified, all resources are assumed to be in the same directory.</param>
        static GLTFDocument MergeDocumentsAsLODs(const std::vector<GLTFDocument>& docs, const std::vector<std::wstring>& relativePaths = std::vector<std::wstring>(), const bool& shared_materials = false);

        /// <summary>
        /// Inserts each LOD GLTFDocument as a node LOD (at the root level) of the specified primary GLTF asset.
        /// Note: Animation is not currently supported.
        /// </summary>
        /// <returns>The primary GLTF Document with the inserted LOD node.</returns>
        /// <param name="docs">A vector of glTF documents to merge as LODs. The first element of the vector is assumed to be the primary LOD.</param>
        /// <param name="screenCoveragePercentages">A vector with the screen coverage percentages corresponding to each LOD. If the size of this 
        /// vector is larger than the size of <see name="docs" />, lower coverage values will cause the asset to be invisible.</param>
        /// <param name="relativePaths">A vector of relative path prefixes to the non-LOD0 LOD gltf documents. Used for finding resources in those LODs.
        /// If not specified, all resources are assumed to be in the same directory.</param>
        static GLTFDocument MergeDocumentsAsLODs(const std::vector<GLTFDocument>& docs, const std::vector<double>& screenCoveragePercentages, const std::vector<std::wstring>& relativePaths = std::vector<std::wstring>(), const bool& shared_materials = false);

        /// <summary>
        /// Determines the highest number of Node LODs for a given glTF asset.
        /// </summary>
        /// <param name="doc">The glTF asset for which to count the max number of node LODs.</param>
        /// <param name="lods">A map containing the parsed node LODs in the document.</param>
        /// <returns>The highest number of Node LODs in the asset.</returns>
        static uint32_t NumberOfNodeLODLevels(const GLTFDocument& doc, const LODMap& lods);
    };
}

