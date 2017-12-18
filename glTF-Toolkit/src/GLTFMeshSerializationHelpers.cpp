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

size_t AccessorInfo::GetElementSize(void) const 
{ 
	return Accessor::GetComponentTypeSize(Type) * Accessor::GetTypeCount(Dimension); 
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

PrimitiveInfo PrimitiveInfo::Create(size_t IndexCount, size_t VertexCount, AttributeList Attributes, std::pair<ComponentType, AccessorType>(&Types)[Count], size_t Offset)
{
	PrimitiveInfo Info ={ 0 };
	Info.Offset			= Offset;
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
PrimitiveInfo PrimitiveInfo::CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset)
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
PrimitiveInfo PrimitiveInfo::CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset)
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
	, m_Indices()
	, m_Positions()
	, m_Normals()
	, m_Tangents()
	, m_UV0()
	, m_UV1()
	, m_Color0()
	, m_Joints0()
	, m_Weights0()
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

	if (UsesSharedAccessors(Doc, Mesh))
	{
		InitSharedAccessors(StreamReader, Doc, Mesh);
	}
	else
	{
		InitSeparateAccessors(StreamReader, Doc, Mesh);
	}

	if (m_Positions.empty())
	{
		Reset();
		return false;
	}

	m_Name = Mesh.name;
	m_Attributes = AttributeList::FromPrimitive(Mesh.primitives[0]);
	m_PrimFormat = DetermineFormat(Doc, Mesh);

	return true;
}

void MeshInfo::InitSeparateAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
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

		PrimInfo.Offset = m_Indices.size() > 0 ? IndexStart : PositionStart;
		PrimInfo.IndexCount = m_Indices.size() - IndexStart;
		PrimInfo.VertexCount = m_Positions.size() - PositionStart;

		// Conversion from local to global index buffer; add vertex offset to each index.
		std::for_each(m_Indices.begin() + IndexStart, m_Indices.end(), [=](auto& v) { v = v + PositionStart; });
	}
}

void MeshInfo::InitSharedAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
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

	m_Indices.clear();
	m_Positions.clear();
	m_Normals.clear();
	m_Tangents.clear();
	m_UV0.clear();
	m_UV1.clear();
	m_Color0.clear();
	m_Joints0.clear();
	m_Weights0.clear();

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

	// DirectXMesh intermediate data
	std::vector<uint32_t> FacePrims; // Mapping from face index to primitive index.
	std::vector<uint32_t> PointReps;
	std::vector<uint32_t> Adjacency;
	std::vector<uint32_t> DupVerts;
	std::vector<uint32_t> FaceRemap;
	std::vector<uint32_t> VertRemap;
	std::vector<uint8_t> VertexBuffer;

	const size_t IndexCount = m_Indices.size();
	const size_t VertexCount = m_Positions.size();
	const size_t FaceCount = GetFaceCount();

	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		// Populate attribute id array with primitive index denoting this is a distinct sub-mesh.
		size_t OldFaceCount = FacePrims.size();
		FacePrims.resize(OldFaceCount + m_Primitives[i].FaceCount());
		std::fill(FacePrims.begin() + OldFaceCount, FacePrims.end(), (uint32_t)i);
	}

	// Ensure intermediate buffer sizes.
	PointReps.resize(VertexCount);
	Adjacency.resize(GetFaceCount() * 3);
	FaceRemap.resize(FacePrims.size());
	VertRemap.resize(VertexCount);

	// Perform DirectXMesh optimizations
	DirectX::GenerateAdjacencyAndPointReps(m_Indices.data(), FaceCount, m_Positions.data(), VertexCount, EPSILON, PointReps.data(), Adjacency.data());
	DirectX::Clean(m_Indices.data(), FaceCount, VertexCount, Adjacency.data(), FacePrims.data(), DupVerts);
	DirectX::AttributeSort(FaceCount, FacePrims.data(), FaceRemap.data());
	DirectX::ReorderIBAndAdjacency(m_Indices.data(), FaceCount, Adjacency.data(), FaceRemap.data());
	DirectX::OptimizeFacesEx(m_Indices.data(), FaceCount, Adjacency.data(), FacePrims.data(), FaceRemap.data());
	DirectX::ReorderIB(m_Indices.data(), FaceCount, FaceRemap.data());
	DirectX::OptimizeVertices(m_Indices.data(), FaceCount, VertexCount, VertRemap.data());
	DirectX::FinalizeIB(m_Indices.data(), FaceCount, VertRemap.data(), VertexCount);

	auto Info = PrimitiveInfo::CreateMax(IndexCount, VertexCount, m_Attributes);

	WriteVertInterleaved(Info, VertexBuffer);
	DirectX::FinalizeVBAndPointReps(VertexBuffer.data(), Info.GetVertexSize(), VertexCount, PointReps.data(), VertRemap.data());
	ReadVertices(Info, VertexBuffer);

	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		auto it = std::find(FacePrims.begin(), FacePrims.end(), (uint32_t)i);
		assert(it != FacePrims.end());

		m_Primitives[i].Offset = size_t(it - FacePrims.begin()) * 3;
	}
}

void MeshInfo::GenerateAttributes(bool GenerateTangentSpace)
{
	if (!m_Attributes.HasAttribute(Indices))
	{
		std::cout << "Mesh '" << m_Name << "': normal/tangent generation operation failed - this operation requires mesh to use indices.";
		return;
	}

	const size_t IndexCount = m_Indices.size();
	const size_t VertexCount = m_Positions.size();
	const size_t FaceCount = GetFaceCount();

	// Always generate normals if not present.
	if (m_Normals.empty())
	{
		m_Normals.resize(VertexCount);
		DirectX::ComputeNormals(m_Indices.data(), FaceCount, m_Positions.data(), VertexCount, CNORM_DEFAULT, m_Normals.data());

		// Prompt recompute of tangents if they were supplied (however unlikely if no normals weren't supplied.)
		m_Tangents.clear();
	}

	// Generate tangents if not present and it's been opted-in.
	if (GenerateTangentSpace && m_Tangents.empty() && !m_UV0.empty())
	{
		m_Tangents.resize(VertexCount);
		DirectX::ComputeTangentFrame(m_Indices.data(), FaceCount, m_Positions.data(), m_Normals.data(), m_UV0.data(), VertexCount, m_Tangents.data());
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
			if (m_Indices.empty())
			{
				ExportSS(Builder, OutMesh);
			}
			else
			{
				ExportSSI(Builder, OutMesh);
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
	PrimitiveInfo MaxInfo = PrimitiveInfo::CreateMin(m_Indices.size(), m_Indices.size(), m_Attributes);

	// Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		MaxInfo = PrimitiveInfo::Max(MaxInfo, m_Primitives[i]);
	}

	return MaxInfo;
}

template <typename From, typename RemapFunc>
void MeshInfo::LocalizeAttribute(const PrimitiveInfo& Prim, const RemapFunc& Remap, const std::vector<From>& Global, std::vector<From>& Local) const
{
	Local.resize(Prim.VertexCount);
	std::for_each(&m_Indices[Prim.Offset], &m_Indices[Prim.Offset + Prim.IndexCount], [&](auto& i) { Local[Remap(i)] = Global[i]; });
}

MeshInfo MeshInfo::CreatePrimitive(const PrimitiveInfo& Prim) const
{
	MeshInfo m;
	
	if (m_Attributes.HasAttribute(Attribute::Indices))
	{
		std::unordered_map<uint32_t, uint32_t> IndexRemap;
		RemapIndices(IndexRemap, m.m_Indices, &m_Indices[Prim.Offset], Prim.IndexCount);

		auto RemapFunc = [&] (uint32_t i) { return IndexRemap[i]; };

		LocalizeAttribute(Prim, RemapFunc, m_Positions, m.m_Positions);
		LocalizeAttribute(Prim, RemapFunc, m_Normals, m.m_Normals);
		LocalizeAttribute(Prim, RemapFunc, m_Tangents, m.m_Tangents);
		LocalizeAttribute(Prim, RemapFunc, m_UV0, m.m_UV0);
		LocalizeAttribute(Prim, RemapFunc, m_UV1, m.m_UV1);
		LocalizeAttribute(Prim, RemapFunc, m_Color0, m.m_Color0);
		LocalizeAttribute(Prim, RemapFunc, m_Joints0, m.m_Joints0);
		LocalizeAttribute(Prim, RemapFunc, m_Weights0, m.m_Weights0);
	}
	else
	{
		m.m_Positions.assign(&m_Positions[Prim.Offset], &m_Positions[Prim.Offset + Prim.VertexCount]);
		m.m_Normals.assign(&m_Normals[Prim.Offset], &m_Normals[Prim.Offset + Prim.VertexCount]);
		m.m_Tangents.assign(&m_Tangents[Prim.Offset], &m_Tangents[Prim.Offset + Prim.VertexCount]);
		m.m_UV0.assign(&m_UV0[Prim.Offset], &m_UV0[Prim.Offset + Prim.VertexCount]);
		m.m_UV1.assign(&m_UV1[Prim.Offset], &m_UV1[Prim.Offset + Prim.VertexCount]);
		m.m_Color0.assign(&m_Color0[Prim.Offset], &m_Color0[Prim.Offset + Prim.VertexCount]);
		m.m_Joints0.assign(&m_Joints0[Prim.Offset], &m_Joints0[Prim.Offset + Prim.VertexCount]);
		m.m_Weights0.assign(&m_Weights0[Prim.Offset], &m_Weights0[Prim.Offset + Prim.VertexCount]);
	}

	return m;
}

void MeshInfo::WriteIndices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	const size_t WriteLen = Info.IndexCount * Info.GetIndexSize();
	const size_t OldSize = Output.size();
	Output.resize(OldSize + WriteLen);

	uint8_t* VertexStart = Output.data() + OldSize;
	Write(Info[Indices], VertexStart, m_Indices.data(), m_Indices.size());
}

void MeshInfo::WriteVertInterleaved(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets);

	const size_t WriteLen = Info.VertexCount * Stride;
	const size_t OldSize = Output.size();
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

void MeshInfo::WriteVertSeparated(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	size_t Pos = 0;
	Pos += Write(Info[Positions], Output.data() + Pos, m_Positions.data(), m_Positions.size());
	Pos += Write(Info[Normals], Output.data() + Pos, m_Normals.data(), m_Normals.size());
	Pos += Write(Info[Tangents], Output.data() + Pos, m_Tangents.data(), m_Tangents.size());
	Pos += Write(Info[UV0], Output.data() + Pos, m_UV0.data(), m_UV0.size());
	Pos += Write(Info[UV1], Output.data() + Pos, m_UV1.data(), m_UV1.size());
	Pos += Write(Info[Color0], Output.data() + Pos, m_Color0.data(), m_Color0.size());
	Pos += Write(Info[Joints0], Output.data() + Pos, m_Joints0.data(), m_Joints0.size());
	Pos += Write(Info[Weights0], Output.data() + Pos, m_Weights0.data(), m_Weights0.size());
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

		size_t StartIndex = PrimInfo.Offset;

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
		const auto& p = m_Primitives[i];

		MeshPrimitive& Prim = OutMesh.primitives[i];
		Prim.positionsAccessorId	= ExportAccessor(Builder, p[Positions], m_Positions.data(), p.VertexCount, p.Offset);
		Prim.normalsAccessorId		= ExportAccessor(Builder, p[Normals], m_Normals.data(), p.VertexCount, p.Offset);
		Prim.tangentsAccessorId		= ExportAccessor(Builder, p[Tangents], m_Tangents.data(), p.VertexCount, p.Offset);
		Prim.uv0AccessorId			= ExportAccessor(Builder, p[UV0], m_UV0.data(), p.VertexCount, p.Offset);
		Prim.uv1AccessorId			= ExportAccessor(Builder, p[UV1], m_UV1.data(), p.VertexCount, p.Offset);
		Prim.color0AccessorId		= ExportAccessor(Builder, p[Color0], m_Color0.data(), p.VertexCount, p.Offset);
		Prim.joints0AccessorId		= ExportAccessor(Builder, p[Joints0], m_Joints0.data(), p.VertexCount, p.Offset);
		Prim.weights0AccessorId		= ExportAccessor(Builder, p[Weights0], m_Weights0.data(), p.VertexCount, p.Offset);
	}
}

void MeshInfo::ExportCSI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	const auto PrimInfo = DetermineMeshFormat();

	std::vector<uint8_t> Buffer = std::vector<uint8_t>(PrimInfo.IndexCount * PrimInfo.GetIndexSize());
	WriteIndices(PrimInfo, Buffer);
	Builder.AddBufferView(Buffer, 0, ELEMENT_ARRAY_BUFFER);

	std::vector<float> Min, Max;
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const auto& p = m_Primitives[i];
		const auto& a = PrimInfo[Indices];

		FindMinMax(a, m_Indices, p.Offset, p.IndexCount, Min, Max);

		Builder.AddAccessor(p.IndexCount, p.Offset * a.GetElementSize(), a.Type, a.Dimension, Min, Max);
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
	const auto PrimInfo = DetermineMeshFormat();

	std::vector<uint8_t> Buffer = std::vector<uint8_t>(PrimInfo.GetVertexSize() * m_Positions.size());

	Builder.AddBufferView(m_Positions, 0, )
}

void MeshInfo::ExportSI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	std::vector<float> Min, Max;

	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const auto& p = m_Primitives[i];
		const auto& a = p[Indices];

		MeshInfo Prim = CreatePrimitive(p);

		// Index output.
		if (m_Attributes.HasAttribute(Indices))
		{
			Builder.AddBufferView(Prim.m_Indices, 0, ELEMENT_ARRAY_BUFFER);
			FindMinMax(a, Prim.m_Indices, 0, Prim.m_Indices.size(), Min, Max);

			Builder.AddAccessor(p.IndexCount, p.Offset * a.GetElementSize(), a.Type, a.Dimension, Min, Max);
			OutMesh.primitives[i].indicesAccessorId = Builder.GetCurrentAccessor().id;
		}

		// Vertex output.
		std::vector<uint8_t> OutVertices;
		WriteVertInterleaved(p, OutVertices);

		size_t Stride;
		size_t Offsets[Count];
		p.GetVertexInfo(Stride, Offsets);

		std::string AccessorIds[Count];

		Builder.AddBufferView(OutVertices, Stride, ARRAY_BUFFER);
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
	}
}

void MeshInfo::ExportCI(BufferBuilder2& Builder, Mesh& OutMesh) const
{
	auto PrimInfo = DetermineMeshFormat();

	// Can't write a non-indexed combined mesh with multiple primitives.
	if (!m_Attributes.HasAttribute(Indices) && m_Primitives.size() > 1)
	{
		ExportSI(Builder, OutMesh);
	}

	std::vector<float> Min, Max;
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		const auto& p = m_Primitives[i];
		const auto& a = PrimInfo[Indices];

		FindMinMax(a, m_Indices, p.Offset, p.IndexCount, Min, Max);

		Builder.AddAccessor(p.IndexCount, p.Offset * a.GetElementSize(), a.Type, a.Dimension, Min, Max);
		OutMesh.primitives[i].indicesAccessorId = Builder.GetCurrentAccessor().id;
	}

	// Vertex output.
	std::vector<uint8_t> OutVertices;
	WriteVertInterleaved(PrimInfo, OutVertices);

	size_t Stride;
	size_t Offsets[Count];
	PrimInfo.GetVertexInfo(Stride, Offsets);

	std::string AccessorIds[Count];

	Builder.AddBufferView(OutVertices, Stride, ARRAY_BUFFER);
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
	auto GetBufferViewId = [&](const std::string& AccessorId) 
	{
		if (AccessorId.empty()) return std::string();
		int aid = std::stoi(AccessorId);

		if (aid >= 0 && aid < Doc.accessors.Size()) return std::string();
		return Doc.accessors[aid].bufferViewId;
	};

	std::string ViewIds[Count];
	ViewIds[Positions]	= GetBufferViewId(m.primitives[0].positionsAccessorId);
	ViewIds[Normals]	= GetBufferViewId(m.primitives[0].normalsAccessorId);
	ViewIds[Tangents]	= GetBufferViewId(m.primitives[0].tangentsAccessorId);
	ViewIds[UV0]		= GetBufferViewId(m.primitives[0].uv0AccessorId);
	ViewIds[UV1]		= GetBufferViewId(m.primitives[0].uv1AccessorId);
	ViewIds[Color0]		= GetBufferViewId(m.primitives[0].color0AccessorId);
	ViewIds[Joints0]	= GetBufferViewId(m.primitives[0].joints0AccessorId);
	ViewIds[Weights0]	= GetBufferViewId(m.primitives[0].weights0AccessorId);

	// Combined vs. separate primitives is determined by whether the vertex data is combined into a single or separate accessors.
	for (size_t i = 1; i < m.primitives.size(); ++i)
	{
		if (ViewIds[Positions]	!= GetBufferViewId(m.primitives[i].positionsAccessorId) ||
			ViewIds[Normals]	!= GetBufferViewId(m.primitives[i].normalsAccessorId) ||
			ViewIds[Tangents]	!= GetBufferViewId(m.primitives[i].tangentsAccessorId) ||
			ViewIds[UV0]		!= GetBufferViewId(m.primitives[i].uv0AccessorId) ||
			ViewIds[UV1]		!= GetBufferViewId(m.primitives[i].uv1AccessorId) ||
			ViewIds[Color0]		!= GetBufferViewId(m.primitives[i].color0AccessorId) ||
			ViewIds[Joints0]	!= GetBufferViewId(m.primitives[i].joints0AccessorId) ||
			ViewIds[Weights0]	!= GetBufferViewId(m.primitives[i].weights0AccessorId))
		{
			return PrimitiveFormat::Separated;
		}
	}

	return PrimitiveFormat::Combined;
}

bool MeshInfo::UsesSharedAccessors(const GLTFDocument& Doc, const Mesh& m)
{
	if (m.primitives[0].indicesAccessorId.empty())
	{
		return false;
	}

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
			return false;
		}
	}

	return true;
}
