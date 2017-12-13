// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceWriter2.h>
#include <GLTFSDK/IStreamFactory.h>

#include "GLTFMeshSerializationHelpers.h"
#include "GLTFMeshUtils.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_MESH_OPTIMIZER = "MSFT_mesh_optimizer";


//-----------------------------------------
// Main Entrypoint

GLTFDocument GLTFMeshUtils::OptimizeAllMeshes(	const IStreamReader& StreamReader, 
												std::unique_ptr<const IStreamFactory>&& StreamFactory,
												const GLTFDocument& Doc, 
												const MeshOptions& Options,
												const std::string& OutputDirectory)
{
	(OutputDirectory);

	// Make sure there's meshes to optimize before performing a bunch of work. 
	if (Doc.meshes.Size() == 0)
	{
		return Doc;
	}

	// Make sure at least one mesh can be operated on.
	if (!std::any_of(Doc.meshes.Elements().begin(), Doc.meshes.Elements().end(), [](const auto& x) { return MeshInfo::CanParse(x); }))
	{
		return Doc;
	}

	GLTFDocument OutputDoc(Doc);

	auto GenBufferId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.buffers.Size() + b.GetBufferCount()); };
	auto GenBufferViewId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.bufferViews.Size() + b.GetBufferViewCount()); };
	auto GenAccessorId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.accessors.Size() + b.GetAccessorCount()); };

	BufferBuilder2 Builder = BufferBuilder2(std::make_unique<GLTFResourceWriter2>(std::move(StreamFactory), "mesh_optimized"), GenBufferId, GenBufferViewId, GenAccessorId);
	Builder.AddBuffer();

	MeshInfo MeshData;
	for (size_t i = 0; i < OutputDoc.meshes.Size(); ++i)
	{
		Mesh m = OutputDoc.meshes[i];

		if (!MeshData.Initialize(StreamReader, OutputDoc, m))
		{
			continue;
		}

		if (Options.Clean)
		{
			MeshData.Optimize();
		}

		MeshData.GenerateAttributes(Options.GenerateTangentSpace);
		MeshData.Export(Builder, m, Options.OutputFormat);

		OutputDoc.meshes.Replace(m);
	}

	Builder.Output(OutputDoc);

	OutputDoc.extensionsUsed.insert(EXTENSION_MSFT_MESH_OPTIMIZER);

	return OutputDoc;
}
