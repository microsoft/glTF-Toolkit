// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "Validation.h"

#include <GLTFLODUtils.h>

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;
using namespace Validation;

// Windows MR asset limits
const unsigned int MAX_NODES_PER_LOD = 64;
const unsigned int MAX_TRIANGLES_PER_LOD = 10000;
const unsigned int MAX_SUBMESHES_PER_LOD = 32;
const unsigned int MAX_KEY_FRAMES_PER_CHANNEL = 20 * 60 * 30; // 20 mins at 30fps
const unsigned int MAX_MORPH_VERTEX_COUNT = 8192;
const unsigned int MAX_ANIMATION_DURATION_SECONDS = 20 * 60; // 20 mins

std::wstring Validation::ValidateWindowsMRAsset(const GLTFDocument& document)
{
    std::wstringstream errors;

    // Complexity validation
    
    auto lodMap = GLTFLODUtils::ParseDocumentNodeLODs(document);
    auto lodCount = GLTFLODUtils::NumberOfNodeLODLevels(document, lodMap);

    std::vector<std::pair<size_t, std::string>> nodesToRead;

    for (auto rootNode : document.scenes[document.defaultSceneId].nodes)
    {
        nodesToRead.push_back(std::make_pair(0, rootNode));
    }

    if (document.nodes.Size() > MAX_NODES_PER_LOD)
    {
        errors << L"Maximum node count (" << MAX_NODES_PER_LOD << L") per LOD exceeded." << std::endl;
        return errors.str();
    }


    for (const auto node : document.nodes.Elements())
    {
        if (!node.meshId.empty())
        {
            auto mesh = document.meshes[node.meshId];

            if (mesh.primitives.size() > MAX_SUBMESHES_PER_LOD)
            {
                errors << "Mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') exceeds max submesh count (allowed: " << MAX_SUBMESHES_PER_LOD << L", actual: " << mesh.primitives.size() << L")" << std::endl;
                return errors.str();
            }

            // TODO: add to LOD count

            std::size_t primitiveId = 0;
            for (const auto& primitive : mesh.primitives)
            {
                if (primitive.mode != MESH_TRIANGLES)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') is not a triangle mesh." << std::endl;
                    return errors.str();
                }

                const unsigned int maxVerticesIndices = MAX_TRIANGLES_PER_LOD * 3;

                const Accessor* indexAccessor = primitive.indicesAccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.indicesAccessorId];
                if (!indexAccessor || indexAccessor->count == 0 || (indexAccessor->count % 3) > 0)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') is missing indices, or index count is not a multiple of 3." << std::endl;
                    return errors.str();
                }

                if (indexAccessor->count > maxVerticesIndices)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') exceeds max polygon count (allowed: " << MAX_TRIANGLES_PER_LOD << L", actual: " << indexAccessor->count / 3 << L")." << std::endl;
                    return errors.str();
                }

                const Accessor* positionsAccessor = primitive.positionsAccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.positionsAccessorId];
                if (!positionsAccessor || positionsAccessor->count == 1)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') is missing position data." << std::endl;
                    return errors.str();
                }

                if (maxVerticesIndices > 0 && positionsAccessor->count > maxVerticesIndices)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') exceeds max vertex count (allowed: " << maxVerticesIndices << L", actual: " << positionsAccessor->count << L")." << std::endl;
                    return errors.str();
                }

                const Accessor* normalsAccessor = primitive.normalsAccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.normalsAccessorId];
                if (normalsAccessor && normalsAccessor->count != positionsAccessor->count)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') has differing normals and positions counts." << std::endl;
                    return errors.str();
                }

                const Accessor* tangentsAccessor = primitive.tangentsAccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.tangentsAccessorId];
                if (tangentsAccessor && tangentsAccessor->count != positionsAccessor->count)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') has differing tangents and positions counts." << std::endl;
                    return errors.str();
                }

                const Accessor* uv0Accessor = primitive.uv0AccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.uv0AccessorId];
                if (uv0Accessor && uv0Accessor->count != positionsAccessor->count)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') has differing uv0s and positions counts." << std::endl;
                    return errors.str();
                }

                const Accessor* color0Accessor = primitive.color0AccessorId.empty()
                    ? nullptr
                    : &document.accessors[primitive.color0AccessorId];
                if (color0Accessor && color0Accessor->count != positionsAccessor->count)
                {
                    errors << L"Primitive " << primitiveId << L" of mesh (id: " << mesh.id.c_str() << L", name: '" << mesh.name.c_str() << L"') has differing color0 and positions counts." << std::endl;
                    return errors.str();
                }

                ++primitiveId;
            }
        }
    }
    
        /*
    result.ValidMaxTrianglesPerLod = false; // TODO
    result.HasLods = false; // TODO

    // Animation validation
    result.HasAnimations = false;
    result.HasAnimationMap = false;
    result.ValidMaxKeyFramesPerChannel = false;
    result.ValidMaxMorphVertexCount = false;
    result.ValidMaxAnimationDurationSeconds = false;
    */

    return errors.str();
}
