// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>
#include <regex>
#include <experimental\filesystem>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceWriter2.h>

#include "GLTFMeshSerializationHelpers.h"
#include "GLTFMeshUtils.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;
using namespace std::experimental::filesystem;

//-----------------------------------------
// Main Entrypoint

GLTFDocument GLTFMeshUtils::ProcessMeshes(const std::string& filename, const GLTFDocument& doc, const IStreamReader& reader, const MeshOptions& options, std::unique_ptr<const IStreamWriter>& streamWriter)
{
    // Make sure there's meshes to optimize before performing a bunch of work. 
    if (doc.meshes.Size() == 0 || doc.buffers.Size() == 0)
    {
        return doc;
    }

    // Make sure at least one mesh can be operated on.
    if (!std::any_of(doc.meshes.Elements().begin(), doc.meshes.Elements().end(), [](const auto& x) { return MeshInfo::IsSupported(x); }))
    {
        return doc;
    }

    // Generate a buffer name, based on the old buffer .bin filename, in the output directory.
    std::string bufferName = filename;
    size_t pos = bufferName.find_last_of('.');
    if (pos == std::string::npos)
    {
        return doc;
    }
    bufferName.resize(pos);

    // Spin up a document copy to modify.
    GLTFDocument outputDoc(doc);

    auto resourceWriter = std::make_unique<GLTFResourceWriter2>(std::move(streamWriter), bufferName);

    auto genBufferId = [&](const BufferBuilder& b) { return std::to_string(outputDoc.buffers.Size() + b.GetBufferCount()); };
    auto genBufferViewId = [&](const BufferBuilder& b) { return std::to_string(outputDoc.bufferViews.Size() + b.GetBufferViewCount()); };
    auto genAccessorId = [&](const BufferBuilder& b) { return std::to_string(outputDoc.accessors.Size() + b.GetAccessorCount()); };

    auto builder = BufferBuilder(std::move(resourceWriter), genBufferId, genBufferViewId, genAccessorId);
    builder.AddBuffer();

    MeshInfo meshData;
    for (size_t i = 0; i < outputDoc.meshes.Size(); ++i)
    {
        Mesh m = outputDoc.meshes[i];

        if (!meshData.Initialize(reader, outputDoc, m))
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

        meshData.Export(options, builder, m);
        outputDoc.meshes.Replace(m);
    }
    
    MeshInfo::CopyAndCleanup(reader, builder, doc, outputDoc);
    builder.Output(outputDoc);

    return outputDoc;
}
