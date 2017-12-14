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

namespace
{
	class BasicStreamFactory : public IStreamFactory
	{
	public:
		BasicStreamFactory(const std::string& filename) 
			: m_Stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out))
			, m_TempStream(std::make_shared<std::stringstream>(std::ios_base::binary | std::ios_base::in | std::ios_base::out))
		{ }

		std::shared_ptr<std::istream> GetInputStream(const std::string&) const override { throw std::logic_error("Not implemented"); }
		std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override { return m_Stream; }
		std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override { return m_TempStream; }

	private:
		std::shared_ptr<std::ofstream> m_Stream;
		std::shared_ptr<std::stringstream> m_TempStream;
	};
}


//-----------------------------------------
// Main Entrypoint

GLTFDocument GLTFMeshUtils::ProcessMeshes(const IStreamReader& StreamReader, const GLTFDocument& Doc, const MeshOptions& Options, const std::string& OutputDirectory)
{
	// Make sure there's meshes to optimize before performing a bunch of work. 
	if (Doc.meshes.Size() == 0 || Doc.buffers.Size() == 0)
	{
		return Doc;
	}

	// Make sure at least one mesh can be operated on.
	if (!std::any_of(Doc.meshes.Elements().begin(), Doc.meshes.Elements().end(), [](const auto& x) { return MeshInfo::CanParse(x); }))
	{
		return Doc;
	}

	// Generate a buffer name based on the old buffer .bin file name in the output directory.
	std::string BufferName = OutputDirectory + Doc.buffers[0].name;
	size_t Pos = BufferName.find_last_of('.');
	if (Pos == std::string::npos)
	{
		return Doc;
	}
	BufferName.insert(Pos, "_optimized_mesh.bin");

	// Spin up a document copy to modify.
	GLTFDocument OutputDoc(Doc);

	auto GenBufferId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.buffers.Size() + b.GetBufferCount()); };
	auto GenBufferViewId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.bufferViews.Size() + b.GetBufferViewCount()); };
	auto GenAccessorId = [&](const BufferBuilder2& b) { return std::to_string(OutputDoc.accessors.Size() + b.GetAccessorCount()); };

	BufferBuilder2 Builder = BufferBuilder2(std::make_unique<GLTFResourceWriter2>(std::make_unique<BasicStreamFactory>(BufferName), GenBufferId, GenBufferViewId, GenAccessorId));
	Builder.AddBuffer();

	MeshInfo MeshData;
	for (size_t i = 0; i < OutputDoc.meshes.Size(); ++i)
	{
		Mesh m = OutputDoc.meshes[i];

		if (!MeshData.Initialize(StreamReader, OutputDoc, m))
		{
			continue;
		}

		if (Options.Optimize)
		{
			MeshData.Optimize();
		}

		MeshData.GenerateAttributes(Options.GenerateTangentSpace);
		MeshData.Export(Options, Builder, m);

		OutputDoc.meshes.Replace(m);
	}

	Builder.Output(OutputDoc);

	OutputDoc.extensionsUsed.insert(EXTENSION_MSFT_MESH_OPTIMIZER);

	return OutputDoc;
}
