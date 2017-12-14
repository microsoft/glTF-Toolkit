// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>
#include <DirectXMesh.h>
#include <rapidjson/writer.h>

#include "DirectXMathUtils.h"
#include "GLTFMeshUtils.h"
#include "GLTFMeshSerializationHelpers.h"

#define EPSILON 1e-6f

// Attribute loop helpers
#define FOREACH_ATTRIBUTE_SUBRANGE(start, stop, op) for (size_t i = start; i < stop; ++i) { op((Attribute)i); }
#define FOREACH_ATTRIBUTE_SETSTART(start, op) FOREACH_ATTRIBUTE_SUBRANGE(start, Count, op)
#define FOREACH_ATTRIBUTE(op) FOREACH_ATTRIBUTE_SUBRANGE(0, Count, op)

using namespace DirectX;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
	AttributeList a ={ 0 };
	a.SetAttribute(Indices, p.indicesAccessorId.empty());
	a.SetAttribute(Positions, p.positionsAccessorId.empty());
	a.SetAttribute(Normals, p.normalsAccessorId.empty());
	a.SetAttribute(Tangents, p.tangentsAccessorId.empty());
	a.SetAttribute(UV0, p.uv0AccessorId.empty());
	a.SetAttribute(UV1, p.uv1AccessorId.empty());
	a.SetAttribute(Color0, p.color0AccessorId.empty());
	a.SetAttribute(Joints0, p.joints0AccessorId.empty());
	a.SetAttribute(Weights0, p.weights0AccessorId.empty());
	return a;
}

bool AccessorInfo::IsValid(void) const
{
	const uint8_t Zeros[sizeof(AccessorInfo)] ={ 0 };
	return std::memcmp(this, Zeros, sizeof(AccessorInfo)) != 0;
}

AccessorInfo AccessorInfo::Invalid(void)
{
	AccessorInfo Info;
	Info.Type		= COMPONENT_UNKNOWN;
	Info.Dimension	= TYPE_UNKNOWN;
	Info.Target		= UNKNOWN_BUFFER;
	return Info;
}

AccessorInfo AccessorInfo::Create(ComponentType CType, AccessorType AType, BufferViewTarget Target)
{
	AccessorInfo Info;
	Info.Type		= CType;
	Info.Dimension	= AType;
	Info.Target		= Target;
	return Info;
}

AccessorInfo AccessorInfo::Max(const AccessorInfo& a0, const AccessorInfo& a1)
{
	AccessorInfo MaxInfo;
	MaxInfo.Target		= a0.Target;
	MaxInfo.Type		= std::max(a0.Type, a1.Type);
	MaxInfo.Dimension	= std::max(a0.Dimension, a1.Dimension);
	return MaxInfo;
}

size_t PrimitiveInfo::GetVertexSize(void) const
{
	size_t Stride = 0;
	size_t Offsets[Count];
	GetVertexInfo(Stride, Offsets);
	return Stride;
}

void PrimitiveInfo::GetVertexInfo(size_t& Stride, size_t(&Offsets)[Count]) const
{
	Stride = 0;
	std::fill(Offsets, Offsets + Count, -1);

	// Iterate through attributes
	for (size_t i = Positions; i < Count; ++i)
	{
		// Only include valid attributes.
		if (Metadata[i].IsValid())
		{
			const size_t CompSize = Accessor::GetComponentTypeSize(Metadata[i].Type);

			// Pad to the component type's size.
			// TODO: Implement more complicated packing mechanism.
			if (Stride % CompSize != 0)
			{
				Stride += CompSize - Stride % CompSize;
			}

			Offsets[i] = Stride;
			Stride += CompSize * Accessor::GetTypeCount(Metadata[i].Dimension);
		}
	}
}

PrimitiveInfo PrimitiveInfo::Create(size_t IndexCount, size_t VertexCount, AttributeList Attributes, std::pair<ComponentType, AccessorType>(&Types)[Count])
{
	PrimitiveInfo Info ={ 0 };
	Info.IndexCount		= IndexCount;
	Info.VertexCount	= VertexCount;
	FOREACH_ATTRIBUTE([&](auto i)
	{
		if (Attributes.HasAttribute(i))
		{
			Info[i] = AccessorInfo::Create(Types[i].first, Types[i].second, i == Indices ? ELEMENT_ARRAY_BUFFER : ARRAY_BUFFER);
		}
	});
	return Info;
}

// Creates a descriptor containing the most compressed form of mesh given a vertex and index count.
PrimitiveInfo PrimitiveInfo::CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes)
{
	std::pair<ComponentType, AccessorType> Types[] ={
		{ GetIndexType(IndexCount),	TYPE_SCALAR },	// Indices
		{ COMPONENT_FLOAT,			TYPE_VEC3 },	// Position
		{ COMPONENT_FLOAT,			TYPE_VEC3 },	// Normals
		{ COMPONENT_FLOAT,			TYPE_VEC4 },	// Tangents
		{ COMPONENT_UNSIGNED_BYTE,	TYPE_VEC2 },	// UV0
		{ COMPONENT_UNSIGNED_BYTE,	TYPE_VEC2 },	// UV1
		{ COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Color0
		{ COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Joints0
		{ COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Weights0
	};

	return Create(IndexCount, VertexCount, Attributes, Types);
}

// Creates a descriptor containing the maximimum precision index and vertex type.
PrimitiveInfo PrimitiveInfo::CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes)
{
	std::pair<ComponentType, AccessorType> Types[] ={
		{ COMPONENT_UNSIGNED_INT,	TYPE_SCALAR },	// Indices
		{ COMPONENT_FLOAT,			TYPE_VEC3 },	// Position
		{ COMPONENT_FLOAT,			TYPE_VEC3 },	// Normals
		{ COMPONENT_FLOAT,			TYPE_VEC4 },	// Tangents
		{ COMPONENT_FLOAT,			TYPE_VEC2 },	// UV0
		{ COMPONENT_FLOAT,			TYPE_VEC2 },	// UV1
		{ COMPONENT_FLOAT,			TYPE_VEC4 },	// Color0
		{ COMPONENT_UNSIGNED_SHORT,	TYPE_VEC4 },	// Joints0
		{ COMPONENT_FLOAT,			TYPE_VEC4 },	// Weights0
	};

	return Create(IndexCount, VertexCount, Attributes, Types);
}

PrimitiveInfo PrimitiveInfo::Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1)
{
	PrimitiveInfo MaxInfo;
	FOREACH_ATTRIBUTE([&](auto i) { MaxInfo[i] = AccessorInfo::Max(p0[i], p1[i]); });
	return MaxInfo;
}

void Microsoft::glTF::Toolkit::FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max)
{
	switch (Info.Type)
	{
	case COMPONENT_UNSIGNED_BYTE:	FindMinMax<uint8_t>(Info, Src, Stride, Offset, Count, Min, Max); break;
	case COMPONENT_UNSIGNED_SHORT:	FindMinMax<uint16_t>(Info, Src, Stride, Offset, Count, Min, Max); break;
	case COMPONENT_UNSIGNED_INT:	FindMinMax<uint16_t>(Info, Src, Stride, Offset, Count, Min, Max); break;
	case COMPONENT_FLOAT:			FindMinMax<float>(Info, Src, Stride, Offset, Count, Min, Max); break;
	}
}


MeshInfo::MeshInfo(void)
	: m_Name()
	, m_Primitives()
	, m_FacePrims()
	, m_Indices()
	, m_Positions()
	, m_Normals()
	, m_Tangents()
	, m_UV0()
	, m_UV1()
	, m_Color0()
	, m_Joints0()
	, m_Weights0()
	, m_VertexBuffer()
	, m_PointReps()
	, m_Adjacency()
	, m_DupVerts()
	, m_FaceRemap()
	, m_VertRemap()
	, m_IndexCount(0u)
	, m_VertexCount(0u)
	, m_Attributes{ 0 }
	, m_PrimFormat{ 0 }
{ }

bool MeshInfo::Initialize(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
{
	// Ensure mesh has the correct properties for us to process.
	if (!CanParse(Mesh))
	{
		return false;
	}

	// Clear the old state of the mesh (to allow warm starting of buffers.)
	Reset();

	// Pull in the mesh data and cache the metadata.
	m_Primitives.resize(Mesh.primitives.size());

	m_PrimFormat = DetermineFormat(Doc, Mesh);

	if (m_PrimFormat == PrimitiveFormat::Separated)
	{
		InitSeparate(StreamReader, Doc, Mesh);
	}
	else
	{
		InitCombined(StreamReader, Doc, Mesh);
	}

	if (m_Positions.empty())
	{
		Reset();
		return false;
	}

	m_Name = Mesh.name;
	m_IndexCount = m_Indices.size();
	m_VertexCount = m_Positions.size();
	m_Attributes = AttributeList::FromPrimitive(Mesh.primitives[0]);

	return true;
}

void MeshInfo::InitSeparate(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
{
	for (size_t i = 0; i < Mesh.primitives.size(); ++i)
	{
		const auto& p = Mesh.primitives[i];
		auto& PrimInfo = m_Primitives[i];

		uint32_t IndexStart = (uint32_t)m_Indices.size();
		uint32_t PositionStart = (uint32_t)m_Positions.size();

		ReadAccessor(StreamReader, Doc, p.indicesAccessorId, m_Indices, PrimInfo[Indices]);
		ReadAccessor(StreamReader, Doc, p.positionsAccessorId, m_Positions, PrimInfo[Positions]);
		ReadAccessor(StreamReader, Doc, p.normalsAccessorId, m_Normals, PrimInfo[Normals]);
		ReadAccessor(StreamReader, Doc, p.tangentsAccessorId, m_Tangents, PrimInfo[Tangents]);
		ReadAccessor(StreamReader, Doc, p.uv0AccessorId, m_UV0, PrimInfo[UV0]);
		ReadAccessor(StreamReader, Doc, p.uv1AccessorId, m_UV1, PrimInfo[UV1]);
		ReadAccessor(StreamReader, Doc, p.color0AccessorId, m_Color0, PrimInfo[Color0]);
		ReadAccessor(StreamReader, Doc, p.joints0AccessorId, m_Joints0, PrimInfo[Joints0]);
		ReadAccessor(StreamReader, Doc, p.weights0AccessorId, m_Weights0, PrimInfo[Weights0]);

		PrimInfo.IndexCount = m_Indices.size() - IndexStart;
		PrimInfo.VertexCount = m_Positions.size() - PositionStart;

		// Conversion from local to global index buffer; add vertex offset to each index.
		std::for_each(m_Indices.begin() + IndexStart, m_Indices.end(), [=](auto& v) { v = v + PositionStart; });

		// Populate attribute id array with primitive index denoting this is a distinct sub-mesh.
		size_t OldFaceCount = m_FacePrims.size();
		m_FacePrims.resize(OldFaceCount + PrimInfo.FaceCount());
		std::fill(m_FacePrims.begin() + OldFaceCount, m_FacePrims.end(), (uint32_t)i);
	}
}

void MeshInfo::InitCombined(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
{
	const auto& p0 = Mesh.primitives[0];
	auto& PrimInfo0 = m_Primitives[0];

	// Combined meshes can only be segmented into primitives (sub-meshes) by index offsets + counts; otherwise it better be only one primitive.
	assert(Mesh.primitives.size() > 1 || !p0.indicesAccessorId.empty());

	ReadAccessor(StreamReader, Doc, p0.positionsAccessorId, m_Positions, PrimInfo0[Positions]);
	ReadAccessor(StreamReader, Doc, p0.normalsAccessorId, m_Normals, PrimInfo0[Normals]);
	ReadAccessor(StreamReader, Doc, p0.tangentsAccessorId, m_Tangents, PrimInfo0[Tangents]);
	ReadAccessor(StreamReader, Doc, p0.uv0AccessorId, m_UV0, PrimInfo0[UV0]);
	ReadAccessor(StreamReader, Doc, p0.uv1AccessorId, m_UV1, PrimInfo0[UV1]);
	ReadAccessor(StreamReader, Doc, p0.color0AccessorId, m_Color0, PrimInfo0[Color0]);
	ReadAccessor(StreamReader, Doc, p0.joints0AccessorId, m_Joints0, PrimInfo0[Joints0]);
	ReadAccessor(StreamReader, Doc, p0.weights0AccessorId, m_Weights0, PrimInfo0[Weights0]);

	// If there are indices, grab the vertex count for each primitive by determining the number of unique indices in its index set.
	if (!p0.indicesAccessorId.empty())
	{
		std::unordered_set<uint32_t> UniqueVertices;
		for (size_t i = 0; i < Mesh.primitives.size(); ++i)
		{
			const auto& p = Mesh.primitives[i];
			auto& PrimInfo = m_Primitives[i];

			uint32_t IndexStart = (uint32_t)m_Indices.size();
			ReadAccessor(StreamReader, Doc, p.indicesAccessorId, m_Indices, PrimInfo[Indices]);

			// Generate the unique vertex set.
			UniqueVertices.clear();
			UniqueVertices.insert(m_Indices.begin() + IndexStart, m_Indices.end());

			PrimInfo.IndexCount = m_Indices.size() - IndexStart;
			PrimInfo.VertexCount = UniqueVertices.size(); // Use the uniqueness count to determine number of vertices for this primitive.
		}
	}
	else
	{
		PrimInfo0.IndexCount = 0;
		PrimInfo0.VertexCount = m_Positions.size();
	}
}

void MeshInfo::Reset(void)
{
	m_Name.clear();
	m_Primitives.clear();

	m_FacePrims.clear();
	m_Indices.clear();

	m_Positions.clear();
	m_Normals.clear();
	m_Tangents.clear();
	m_UV0.clear();
	m_UV1.clear();
	m_Color0.clear();
	m_Joints0.clear();
	m_Weights0.clear();

	m_PointReps.clear();
	m_Adjacency.clear();
	m_DupVerts.clear();
	m_FaceRemap.clear();
	m_VertRemap.clear();

	m_IndexCount = 0;
	m_VertexCount = 0;
	m_Attributes ={ 0 };
	m_PrimFormat = PrimitiveFormat::Combined;
}

void MeshInfo::Optimize(void)
{
	if (!m_Attributes.HasAttribute(Indices))
	{
		std::cout << "Mesh '" << m_Name << "': optimize operation failed - this operation requires mesh to use indices.";
		return;
	}

	size_t FaceCount = GetFaceCount();

	// Ensure intermediate buffer sizes.
	m_PointReps.resize(m_VertexCount);
	m_Adjacency.resize(GetFaceCount() * 3);
	m_FaceRemap.resize(m_FacePrims.size());
	m_VertRemap.resize(m_VertexCount);

	// Perform DirectXMesh optimizations
	DirectX::GenerateAdjacencyAndPointReps(m_Indices.data(), FaceCount, m_Positions.data(), m_VertexCount, EPSILON, m_PointReps.data(), m_Adjacency.data());
	DirectX::Clean(m_Indices.data(), FaceCount, m_VertexCount, m_Adjacency.data(), m_FacePrims.data(), m_DupVerts);
	DirectX::AttributeSort(FaceCount, m_FacePrims.data(), m_FaceRemap.data());
	DirectX::ReorderIBAndAdjacency(m_Indices.data(), FaceCount, m_Adjacency.data(), m_FaceRemap.data());
	DirectX::OptimizeFacesEx(m_Indices.data(), FaceCount, m_Adjacency.data(), m_FacePrims.data(), m_FaceRemap.data());
	DirectX::ReorderIB(m_Indices.data(), FaceCount, m_FaceRemap.data());
	DirectX::OptimizeVertices(m_Indices.data(), FaceCount, m_VertexCount, m_VertRemap.data());
	DirectX::FinalizeIB(m_Indices.data(), FaceCount, m_VertRemap.data(), m_VertexCount);

	size_t Stride = GenerateInterleaved();
	DirectX::FinalizeVBAndPointReps(m_VertexBuffer.data(), Stride, m_VertexCount, m_PointReps.data(), m_VertRemap.data());
	RegenerateSeparate();
}

void MeshInfo::GenerateAttributes(bool GenerateTangentSpace)
{
	if (!m_Attributes.HasAttribute(Indices))
	{
		std::cout << "Mesh '" << m_Name << "': normal/tangent generation operation failed - this operation requires mesh to use indices.";
		return;
	}

	// Always generate normals if not present.
	if (m_Normals.empty())
	{
		m_Normals.resize(m_VertexCount);
		DirectX::ComputeNormals(m_Indices.data(), GetFaceCount(), m_Positions.data(), m_VertexCount, CNORM_DEFAULT, m_Normals.data());

		// Prompt recompute of tangents if they were supplied (however unlikely if no normals weren't supplied.)
		m_Tangents.clear();
	}

	// Generate tangents if not present and it's been opted-in.
	if (GenerateTangentSpace && m_Tangents.empty() && !m_UV0.empty())
	{
		m_Tangents.resize(m_VertexCount);
		DirectX::ComputeTangentFrame(m_Indices.data(), GetFaceCount(), m_Positions.data(), m_Normals.data(), m_UV0.data(), m_VertexCount, m_Tangents.data());
	}
}

void MeshInfo::Export(const MeshOptions& Options, BufferBuilder2& Builder, Mesh& OutMesh) const
{
	auto PrimFormat = Options.PrimitiveFormat == PrimitiveFormat::Preserved ? m_PrimFormat : Options.PrimitiveFormat;

	switch (PrimFormat)
	{
	case PrimitiveFormat::Combined:
		switch (Options.AttributeFormat)
		{
		case AttributeFormat::Interleaved: ExportCI(Builder, OutMesh); break;
		case AttributeFormat::Separated: ExportCSI(Builder, OutMesh); break;
		}
		break;

	case PrimitiveFormat::Separated:
		switch (Options.AttributeFormat)
		{
		case AttributeFormat::Interleaved: ExportSI(Builder, OutMesh); break;
		case AttributeFormat::Separated:
			if (m_IndexCount > 0)
			{
				ExportSSI(Builder, OutMesh);
			}
			else
			{
				ExportSS(Builder, OutMesh);
			}
			break;
		}
		break;
	}

	rapidjson::Document meshExtJson;
	meshExtJson.SetObject();

	meshExtJson.AddMember("clean", rapidjson::Value(Options.Optimize), meshExtJson.GetAllocator());
	meshExtJson.AddMember("tangents", rapidjson::Value(m_Tangents.size() > 0), meshExtJson.GetAllocator());
	meshExtJson.AddMember("primitive_format", rapidjson::Value((uint8_t)PrimFormat), meshExtJson.GetAllocator());
	meshExtJson.AddMember("attribute_format", rapidjson::Value((uint8_t)Options.AttributeFormat), meshExtJson.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	meshExtJson.Accept(writer);

	OutMesh.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_MESH_OPTIMIZER, buffer.GetString()));
}

bool MeshInfo::CanParse(const Mesh& m)
{
	if (m.primitives.empty())
	{
		return false;
	}

	AttributeList Attrs = AttributeList::FromPrimitive(m.primitives[0]);
	for (size_t i = 0; i < m.primitives.size(); ++i)
	{
		const auto& p = m.primitives[i];

		// DirectXMesh expects triangle lists - can't optimize other topologies.
		if (p.mode != MeshMode::MESH_TRIANGLES)
		{
			return false;
		}

		// Check for inconsistent index usage and/or vertex formats.
		if (Attrs != AttributeList::FromPrimitive(p))
		{
			return false;
		}
	}

	return true;
}

PrimitiveInfo MeshInfo::DetermineMeshFormat(void) const
{
	if (m_Primitives.empty())
	{
		return PrimitiveInfo{};
	}

	// Start at most compressed vertex attribute formats.
	PrimitiveInfo MaxInfo = PrimitiveInfo::CreateMin(m_IndexCount, m_VertexCount, m_Attributes);

	// Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		MaxInfo = PrimitiveInfo::Max(MaxInfo, m_Primitives[i]);
	}

	return MaxInfo;
}

void MeshInfo::WriteIndices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	size_t WriteLen = Info.IndexCount * Info.GetIndexSize();

	size_t OldSize = Output.size();
	Output.resize(OldSize + WriteLen);

	uint8_t* VertexStart = Output.data() + OldSize;
	Write(Info[Indices], VertexStart, m_Indices.data(), m_Indices.size());
}

void MeshInfo::WriteVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets);

	size_t WriteLen = Info.VertexCount * Stride;

	size_t OldSize = Output.size();
	Output.resize(OldSize + WriteLen);

	uint8_t* VertexStart = Output.data() + OldSize;
	Write(Info[Positions], VertexStart, Stride, Offsets[Positions], m_Positions.data(), m_Positions.size());
	Write(Info[Normals], VertexStart, Stride, Offsets[Normals], m_Normals.data(), m_Normals.size());
	Write(Info[Tangents], VertexStart, Stride, Offsets[Tangents], m_Tangents.data(), m_Tangents.size());
	Write(Info[UV0], VertexStart, Stride, Offsets[UV0], m_UV0.data(), m_UV0.size());
	Write(Info[UV1], VertexStart, Stride, Offsets[UV1], m_UV1.data(), m_UV1.size());
	Write(Info[Color0], VertexStart, Stride, Offsets[Color0], m_Color0.data(), m_Color0.size());
	Write(Info[Joints0], VertexStart, Stride, Offsets[Joints0], m_Joints0.data(), m_Joints0.size());
	Write(Info[Weights0], VertexStart, Stride, Offsets[Weights0], m_Weights0.data(), m_Weights0.size());
}

void MeshInfo::ReadVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Input)
{
	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets);

	Read(Info[Positions], m_Positions.data(), Input.data(), Info.VertexCount, Stride, Offsets[Positions]);
	Read(Info[Normals], m_Normals.data(), Input.data(), Info.VertexCount, Stride, Offsets[Normals]);
	Read(Info[Tangents], m_Tangents.data(), Input.data(), Info.VertexCount, Stride, Offsets[Tangents]);
	Read(Info[UV0], m_UV0.data(), Input.data(), Info.VertexCount, Stride, Offsets[UV0]);
	Read(Info[UV1], m_UV1.data(), Input.data(), Info.VertexCount, Stride, Offsets[UV1]);
	Read(Info[Color0], m_Color0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Color0]);
	Read(Info[Joints0], m_Joints0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Joints0]);
	Read(Info[Weights0], m_Weights0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Weights0]);
}

void MeshInfo::ExportSSI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const auto& PrimInfo = m_Primitives[i];

		auto it = std::find(m_FacePrims.begin(), m_FacePrims.end(), (uint32_t)i);
		assert(it != m_FacePrims.end());

		uint32_t StartIndex = (uint32_t)(it - m_FacePrims.begin()) * 3;

		// Remap indices to a localized range.
		std::unordered_map<uint32_t, uint32_t> IndexRemap;
		std::vector<uint32_t> NewIndices;
		RemapIndices(IndexRemap, NewIndices, &m_Indices[StartIndex], PrimInfo.IndexCount);

		auto RemapFunc = [&](uint32_t i) { return IndexRemap.at(i); };

		MeshPrimitive& Prim = OutMesh.primitives[i];
		Prim.indicesAccessorId		= ExportAccessor(Builder, PrimInfo[Indices], NewIndices.data(), PrimInfo.IndexCount);
		Prim.positionsAccessorId	= ExportAccessorIndexed(Builder, PrimInfo[Positions], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Positions.data());
		Prim.normalsAccessorId		= ExportAccessorIndexed(Builder, PrimInfo[Normals], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Normals.data());
		Prim.tangentsAccessorId		= ExportAccessorIndexed(Builder, PrimInfo[Tangents], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Tangents.data());
		Prim.uv0AccessorId			= ExportAccessorIndexed(Builder, PrimInfo[UV0], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_UV0.data());
		Prim.uv1AccessorId			= ExportAccessorIndexed(Builder, PrimInfo[UV1], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_UV1.data());
		Prim.color0AccessorId		= ExportAccessorIndexed(Builder, PrimInfo[Color0], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Color0.data());
		Prim.joints0AccessorId		= ExportAccessorIndexed(Builder, PrimInfo[Joints0], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Joints0.data());
		Prim.weights0AccessorId		= ExportAccessorIndexed(Builder, PrimInfo[Weights0], PrimInfo.VertexCount, m_Indices.data(), PrimInfo.IndexCount, RemapFunc, m_Weights0.data());
	}
}

void MeshInfo::ExportSS(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const auto& PrimInfo = m_Primitives[i];

		auto FaceIt = std::find(m_FacePrims.begin(), m_FacePrims.end(), (uint32_t)i);
		assert(FaceIt != m_FacePrims.end());

		size_t StartIndex = (FaceIt - m_FacePrims.begin()) * 3;

		MeshPrimitive& Prim = OutMesh.primitives[i];
		Prim.positionsAccessorId	= ExportAccessor(Builder, PrimInfo[Positions], m_Positions.data(), PrimInfo.VertexCount, StartIndex);
		Prim.normalsAccessorId		= ExportAccessor(Builder, PrimInfo[Normals], m_Normals.data(), PrimInfo.VertexCount, StartIndex);
		Prim.tangentsAccessorId		= ExportAccessor(Builder, PrimInfo[Tangents], m_Tangents.data(), PrimInfo.VertexCount, StartIndex);
		Prim.uv0AccessorId			= ExportAccessor(Builder, PrimInfo[UV0], m_UV0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.uv1AccessorId			= ExportAccessor(Builder, PrimInfo[UV1], m_UV1.data(), PrimInfo.VertexCount, StartIndex);
		Prim.color0AccessorId		= ExportAccessor(Builder, PrimInfo[Color0], m_Color0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.joints0AccessorId		= ExportAccessor(Builder, PrimInfo[Joints0], m_Joints0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.weights0AccessorId		= ExportAccessor(Builder, PrimInfo[Weights0], m_Weights0.data(), PrimInfo.VertexCount, StartIndex);
	}
}

void MeshInfo::ExportCSI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	std::vector<float> Min, Max;
	auto PrimInfo = DetermineMeshFormat();

	ExportBufferView(Builder, PrimInfo[Indices], m_Indices.data(), m_Indices.size(), 0);
	for (size_t i = 0; i < OutMesh.primitives.size(); ++i)
	{
		auto FaceIt = std::find(m_FacePrims.begin(), m_FacePrims.end(), (uint32_t)i);
		assert(FaceIt != m_FacePrims.end());

		size_t StartIndex = (FaceIt - m_FacePrims.begin()) * 3;
		size_t IndexCount = m_Primitives[i].IndexCount;
		size_t IndexSize = PrimInfo.GetIndexSize();

		FindMinMax(PrimInfo[Indices], m_Indices.data(), IndexCount, Min, Max);

		 Builder.AddAccessor(IndexCount, StartIndex * IndexSize, PrimInfo[Indices].Type, TYPE_SCALAR, Min, Max);
		 OutMesh.primitives[i].indicesAccessorId = Builder.GetCurrentAccessor().id;
	}

	std::string AccessorIds[Count];
	AccessorIds[Positions]	= ExportAccessor(Builder, PrimInfo[Positions], m_Positions.data(), m_Positions.size(), 0);
	AccessorIds[Normals]	= ExportAccessor(Builder, PrimInfo[Normals], m_Normals.data(), m_Normals.size(), 0);
	AccessorIds[Tangents]	= ExportAccessor(Builder, PrimInfo[Tangents], m_Tangents.data(), m_Tangents.size(), 0);
	AccessorIds[UV0]		= ExportAccessor(Builder, PrimInfo[UV0], m_UV0.data(), m_UV0.size(), 0);
	AccessorIds[UV1]		= ExportAccessor(Builder, PrimInfo[UV1], m_UV1.data(), m_UV1.size(), 0);
	AccessorIds[Color0]		= ExportAccessor(Builder, PrimInfo[Color0], m_Color0.data(), m_Color0.size(), 0);
	AccessorIds[Joints0]	= ExportAccessor(Builder, PrimInfo[Joints0], m_Joints0.data(), m_Joints0.size(), 0);
	AccessorIds[Weights0]	= ExportAccessor(Builder, PrimInfo[Weights0], m_Weights0.data(), m_Weights0.size(), 0);

	// Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
	for (auto& x : OutMesh.primitives)
	{
		x.positionsAccessorId	= AccessorIds[Positions];
		x.normalsAccessorId		= AccessorIds[Normals];
		x.tangentsAccessorId	= AccessorIds[Tangents];
		x.uv0AccessorId			= AccessorIds[UV0];
		x.uv1AccessorId			= AccessorIds[UV1];
		x.color0AccessorId		= AccessorIds[Color0];
		x.joints0AccessorId		= AccessorIds[Joints0];
		x.weights0AccessorId	= AccessorIds[Weights0];
	}
}

void MeshInfo::ExportCS(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	std::string BufferViewIds[Count];
	BufferViewIds[Positions]	= ExportBufferView(Builder, PrimInfo[Positions], m_Positions.data(), m_Positions.size(), 0);
	BufferViewIds[Normals]		= ExportBufferView(Builder, PrimInfo[Normals], m_Normals.data(), m_Normals.size(), 0);
	BufferViewIds[Tangents]		= ExportBufferView(Builder, PrimInfo[Tangents], m_Tangents.data(), m_Tangents.size(), 0);
	BufferViewIds[UV0]			= ExportBufferView(Builder, PrimInfo[UV0], m_UV0.data(), m_UV0.size(), 0);
	BufferViewIds[UV1]			= ExportBufferView(Builder, PrimInfo[UV1], m_UV1.data(), m_UV1.size(), 0);
	BufferViewIds[Color0]		= ExportBufferView(Builder, PrimInfo[Color0], m_Color0.data(), m_Color0.size(), 0);
	BufferViewIds[Joints0]		= ExportBufferView(Builder, PrimInfo[Joints0], m_Joints0.data(), m_Joints0.size(), 0);
	BufferViewIds[Weights0]		= ExportBufferView(Builder, PrimInfo[Weights0], m_Weights0.data(), m_Weights0.size(), 0);


	// Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
	for (auto& x : OutMesh.primitives)
	{
		x.positionsAccessorId	= BufferViewIds[Positions];
		x.normalsAccessorId		= BufferViewIds[Normals];
		x.tangentsAccessorId	= BufferViewIds[Tangents];
		x.uv0AccessorId			= BufferViewIds[UV0];
		x.uv1AccessorId			= BufferViewIds[UV1];
		x.color0AccessorId		= BufferViewIds[Color0];
		x.joints0AccessorId		= BufferViewIds[Joints0];
		x.weights0AccessorId	= BufferViewIds[Weights0];
	}
}

void MeshInfo::ExportSI(BufferBuilder2& Builder, Mesh& OutMesh) const
{

}

void MeshInfo::ExportCI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	// Can't write a non-indexed combined mesh with multiple primitives.
	if (!m_Attributes.HasAttribute(Indices) && m_Primitives.size() > 1)
	{
		ExportSI(Builder, OutMesh);
	}

	auto PrimInfo = DetermineMeshFormat();
	std::vector<float> Min, Max;

	// Index output.
	// Write indices.
	std::vector<uint8_t> OutIndices;
	WriteIndices(PrimInfo, OutIndices);

	// Add Buffer View for index list.
	size_t IndexSize = PrimInfo.GetIndexSize();
	Builder.AddBufferView(OutIndices, IndexSize, ELEMENT_ARRAY_BUFFER);

	// Create Index buffer accessor for each primitive at the appropriate byte offset.
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const AccessorInfo& AccInfo = PrimInfo[Indices];

		// Find the starting index of the buffer.
		auto FaceIt = std::find(m_FacePrims.begin(), m_FacePrims.end(), (uint32_t)i);
		size_t StartIndex = (FaceIt - m_FacePrims.begin()) * 3;

		// Find the min and max elements of the index list.
		FindMinMax(AccInfo, OutIndices.data(), IndexSize, StartIndex * IndexSize, m_Primitives[i].IndexCount, Min, Max);

		// Add the unique index accessor for this primitive.
		Builder.AddAccessor(m_Primitives[i].IndexCount, StartIndex * IndexSize, AccInfo.Type, AccInfo.Dimension, Min, Max);
		OutMesh.primitives[i].indicesAccessorId = Builder.GetCurrentAccessor().id;
	}

	// Vertex output.
	std::vector<uint8_t> OutVertices;
	WriteVertices(PrimInfo, OutVertices);

	size_t Stride;
	size_t Offsets[Count];
	PrimInfo.GetVertexInfo(Stride, Offsets);

	std::string AccessorIds[Count];

	Builder.AddBufferView(OutVertices, PrimInfo.GetVertexSize(), ARRAY_BUFFER);
	FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i)
	{
		if (m_Attributes.HasAttribute(i))
		{
			// Find the min and max elements of the index list.
			FindMinMax(PrimInfo[i], OutVertices.data(), Stride, Offsets[i], PrimInfo.VertexCount, Min, Max);

			// Add the interleaved vertex accessors.
			Builder.AddAccessor(PrimInfo.VertexCount, Offsets[i], PrimInfo[i].Type, PrimInfo[i].Dimension, Min, Max);
			AccessorIds[i] = Builder.GetCurrentAccessor().id;
		}
	});

	// Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
	for (auto& x : OutMesh.primitives)
	{
		x.positionsAccessorId	= AccessorIds[Positions];
		x.normalsAccessorId		= AccessorIds[Normals];
		x.tangentsAccessorId	= AccessorIds[Tangents];
		x.uv0AccessorId			= AccessorIds[UV0];
		x.uv1AccessorId			= AccessorIds[UV1];
		x.color0AccessorId		= AccessorIds[Color0];
		x.joints0AccessorId		= AccessorIds[Joints0];
		x.weights0AccessorId	= AccessorIds[Weights0];
	}
}

size_t MeshInfo::GenerateInterleaved(void)
{
	auto Info = PrimitiveInfo::CreateMax(m_IndexCount, m_VertexCount, m_Attributes);
	WriteVertices(Info, m_VertexBuffer);
	return Info.GetVertexSize();
}

void MeshInfo::RegenerateSeparate(void)
{
	auto Info = PrimitiveInfo::CreateMax(m_IndexCount, m_VertexCount, m_Attributes);
	ReadVertices(Info, m_VertexBuffer);
}

void MeshInfo::RemapIndices(std::unordered_map<uint32_t, uint32_t>& Map, std::vector<uint32_t>& NewIndices, const uint32_t* Indices, size_t Count)
{
	Map.clear();
	NewIndices.clear();

	uint32_t j = 0;
	for (size_t i = 0; i < Count; ++i)
	{
		uint32_t Index = Indices[i];

		auto it = Map.find(Index);
		if (it == Map.end())
		{
			Map.insert(std::make_pair(Index, j));
			NewIndices.push_back(j++);
		}
		else
		{
			NewIndices.push_back(it->second);
		}
	}
}
PrimitiveFormat MeshInfo::DetermineFormat(const GLTFDocument& Doc, const Mesh& m)
{
	auto GetBufferViewId = [&](const std::string& AccessorId) {
		if (AccessorId.empty())
		{
			return std::string();
		}

		int aid = std::stoi(AccessorId);
		if (aid >= 0 && aid < Doc.accessors.Size())
		{
			return std::string();
		}

		return Doc.accessors[aid].bufferViewId;
	};

	// Check if we have indices.
	if (!m.primitives[0].indicesAccessorId.empty())
	{
		// Combined vs. separate primitives is determined by whether the index data is combined into a single or separate buffer views.
		std::string ViewId = GetBufferViewId(m.primitives[0].indicesAccessorId);

		for (size_t i = 1; i < m.primitives.size(); ++i)
		{
			if (ViewId != GetBufferViewId(m.primitives[i].indicesAccessorId))
			{
				return PrimitiveFormat::Separated;
			}
		}
	}
	else
	{
		// Combined vs. separate primitives is determined by whether the vertex data is combined into a single or separate accessors.
		for (size_t i = 1; i < m.primitives.size(); ++i)
		{
			if (m.primitives[0].positionsAccessorId != m.primitives[i].positionsAccessorId ||
				m.primitives[0].normalsAccessorId != m.primitives[i].normalsAccessorId ||
				m.primitives[0].tangentsAccessorId != m.primitives[i].tangentsAccessorId ||
				m.primitives[0].uv0AccessorId != m.primitives[i].uv0AccessorId ||
				m.primitives[0].uv1AccessorId != m.primitives[i].uv1AccessorId ||
				m.primitives[0].color0AccessorId != m.primitives[i].color0AccessorId ||
				m.primitives[0].joints0AccessorId != m.primitives[i].joints0AccessorId ||
				m.primitives[0].weights0AccessorId != m.primitives[i].weights0AccessorId)
			{
				return PrimitiveFormat::Separated;
			}
		}
	}


	return PrimitiveFormat::Combined;
}
