// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "GLTFMeshUtils.h"

#include "GLTFMeshSerializationHelpers.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
    std::string FindFirstNotIn(size_t& index, const std::unordered_set<std::string>& set)
    {
        std::string curr;
        do
        {
            curr = std::to_string(index++);
        } while (set.count(curr) > 0);

        return curr;
    }
}

//-----------------------------------------
// Main Entrypoint

Document GLTFMeshUtils::Process(const Document& doc, const MeshOptions& options, const std::string& bufferPrefix, std::shared_ptr<IStreamReader> reader, std::unique_ptr<IStreamWriter> writer)
{
    return Process(doc, options, bufferPrefix, reader, MakeStreamWriterCache<StreamWriterCache>(std::move(writer)));
}

Document GLTFMeshUtils::Process(const Document& doc, const MeshOptions& options, const std::string& bufferPrefix, std::shared_ptr<IStreamReader> reader, std::unique_ptr<IStreamWriterCache> writerCache)
{
    // Determine if there's any work to do here.
    if (doc.meshes.Size() == 0 || doc.buffers.Size() == 0)
    {
        return doc;
    }

    // Make sure at least one mesh can be operated on.
    if (!std::any_of(doc.meshes.Elements().begin(), doc.meshes.Elements().end(), [](const auto& x) { return MeshOptimizer::IsSupported(x); }))
    {
        return doc;
    }

    const char* prefix = !bufferPrefix.empty() ? bufferPrefix.c_str() : "buffer";

    auto resourceWriter = std::make_unique<GLTFResourceWriter>(std::move(writerCache));
    resourceWriter->SetUriPrefix(prefix);

    std::unordered_set<std::string> accessorIds, bufferViewIds, bufferIds;
    MeshOptimizer::FindRestrictedIds(doc, accessorIds, bufferViewIds, bufferIds);

    size_t accessorIndex = 0, bufferViewIndex = 0, bufferIndex = 0;

    auto builder = BufferBuilder(std::move(resourceWriter),
        [&](auto& b) { _Unreferenced_parameter_(b); return FindFirstNotIn(bufferIndex, bufferIds); },
        [&](auto& b) { _Unreferenced_parameter_(b); return s_insertionId.empty() ? FindFirstNotIn(bufferViewIndex, bufferViewIds) : std::move(s_insertionId); },
        [&](auto& b) { _Unreferenced_parameter_(b); return FindFirstNotIn(accessorIndex, accessorIds); }
    );
    builder.AddBuffer();

    Document outputDoc(doc); // Start with a copy of the unmodified document.

    MeshOptimizer meshData;
    for (auto& mesh : outputDoc.meshes.Elements())
    {
        if (!meshData.Initialize(reader, outputDoc, mesh))
        {
            continue;
        }

        if (options.Optimize)
        {
            meshData.Optimize();
        }

        if (options.GenerateTangentSpace)
        {
            meshData.GenerateAttributes();
        }

        Mesh copy = mesh;
        meshData.Export(options, builder, copy);

        outputDoc.meshes.Replace(copy);
    }

    MeshOptimizer::Finalize(reader, builder, doc, outputDoc);

    return outputDoc;
}
