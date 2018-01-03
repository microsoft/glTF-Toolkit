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

namespace
{
	class BasicStreamWriter : public IStreamWriter
	{
	public:
		BasicStreamWriter(const std::string& OutputDirectory)
			: m_OutputDir(OutputDirectory)
		{ }

		std::shared_ptr<std::ostream> GetOutputStream(const std::string& Filename) const override
		{
			return std::make_shared<std::ofstream>(m_OutputDir + Filename, std::ios::binary);
		}

	private:
		std::string m_OutputDir;
	};
}

//-----------------------------------------
// Main Entrypoint

GLTFDocument GLTFMeshUtils::ProcessMeshes(const std::string& Filename, const GLTFDocument& Doc, const IStreamReader& StreamReader, const MeshOptions& Options, const std::string& OutputDirectory)
{
	// Make sure there's meshes to optimize before performing a bunch of work. 
	if (Doc.meshes.Size() == 0 || Doc.buffers.Size() == 0)
	{
		return Doc;
	}

	// Make sure at least one mesh can be operated on.
	if (!std::any_of(Doc.meshes.Elements().begin(), Doc.meshes.Elements().end(), [](const auto& x) { return MeshInfo::IsSupported(x); }))
	{
		return Doc;
	}

	// Generate a buffer name, based on the old buffer .bin filename, in the output directory.
	std::string BufferName = Filename;
	size_t Pos = BufferName.find_last_of('.');
	if (Pos == std::string::npos)
	{
		return Doc;
	}
	BufferName.resize(Pos);
	BufferName.append("_op");

	// Create output directory for file output.
	create_directories(OutputDirectory);

	// Spin up a document copy to modify.
	GLTFDocument OutputDoc(Doc);

	auto StreamWriter = std::make_unique<BasicStreamWriter>(OutputDirectory);
	auto ResourceWriter = std::make_unique<GLTFResourceWriter2>(std::move(StreamWriter), BufferName);

	auto GenBufferId = [&](const BufferBuilder& b) { return std::to_string(OutputDoc.buffers.Size() + b.GetBufferCount()); };
	auto GenBufferViewId = [&](const BufferBuilder& b) { return std::to_string(OutputDoc.bufferViews.Size() + b.GetBufferViewCount()); };
	auto GenAccessorId = [&](const BufferBuilder& b) { return std::to_string(OutputDoc.accessors.Size() + b.GetAccessorCount()); };

	auto Builder = BufferBuilder(std::move(ResourceWriter), GenBufferId, GenBufferViewId, GenAccessorId);
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

		if (Options.GenerateTangentSpace)
		{
			MeshData.GenerateAttributes();
		}

		MeshData.Export(Options, Builder, m);
		OutputDoc.meshes.Replace(m);
	}

	//MeshInfo::CopyOtherData(StreamReader, Builder, Doc, OutputDoc);

	Builder.Output(OutputDoc);

	MeshInfo::Cleanup(Doc, OutputDoc);

	return OutputDoc;
}
