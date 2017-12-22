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
#define FOREACH_ATTRIBUTE_SUBRANGE(start, stop, op) for (size_t _i = start; _i < stop; ++_i) { op((Attribute)_i); }
#define FOREACH_ATTRIBUTE_SETSTART(start, op) FOREACH_ATTRIBUTE_SUBRANGE(start, Count, op)
#define FOREACH_ATTRIBUTE(op) FOREACH_ATTRIBUTE_SUBRANGE(0, Count, op)

using namespace DirectX;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

std::string(MeshPrimitive::*AccessorIds[Count]) =
{
	&MeshPrimitive::indicesAccessorId,		// 0 Indices
	&MeshPrimitive::positionsAccessorId,	// 1 Positions
	&MeshPrimitive::normalsAccessorId,		// 2 Normals
	&MeshPrimitive::tangentsAccessorId,		// 3 Tangents
	&MeshPrimitive::uv0AccessorId,			// 4 UV0
	&MeshPrimitive::uv1AccessorId,			// 5 UV1
	&MeshPrimitive::color0AccessorId,		// 6 Color0
	&MeshPrimitive::joints0AccessorId,		// 7 Joints0
	&MeshPrimitive::weights0AccessorId,		// 8 Weights0
};

AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
	AttributeList a ={ 0 };
	FOREACH_ATTRIBUTE([&](auto i) { a.SetAttribute(i, !(p.*AccessorIds[i]).empty()); });
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

void PrimitiveInfo::GetVertexInfo(size_t& Stride, size_t(&Offsets)[Count], size_t* pAlignment) const
{
	size_t MaxCompSize = 0;
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

			MaxCompSize = std::max(MaxCompSize, CompSize);
			Offsets[i] = Stride;
			Stride += CompSize * Accessor::GetTypeCount(Metadata[i].Dimension);
		}
	}

	if (pAlignment)
	{
		*pAlignment = MaxCompSize;
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

	return Create(IndexCount, VertexCount, Attributes, Types, Offset);
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

	return Create(IndexCount, VertexCount, Attributes, Types, Offset);
}

PrimitiveInfo PrimitiveInfo::Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1)
{
	PrimitiveInfo MaxInfo;
	MaxInfo.IndexCount	= p0.IndexCount;
	MaxInfo.VertexCount = p0.VertexCount;
	MaxInfo.Offset		= p0.Offset;

	FOREACH_ATTRIBUTE([&](auto i) { MaxInfo[i] = AccessorInfo::Max(p0[i], p1[i]); });

	return MaxInfo;
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

MeshInfo::MeshInfo(const MeshInfo& Parent, size_t PrimIndex)
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
	, m_Attributes(Parent.m_Attributes)
	, m_PrimFormat(Parent.m_PrimFormat)
{
	m_Attributes = Parent.m_Attributes;

	const auto& Prim = Parent.m_Primitives[PrimIndex];

	if (m_Attributes.HasAttribute(Attribute::Indices))
	{
		std::unordered_map<uint32_t, uint32_t> IndexRemap;
		RemapIndices(IndexRemap, m_Indices, &Parent.m_Indices[Prim.Offset], Prim.IndexCount);

		auto RemapFunc = [&](uint32_t i) { return IndexRemap[i]; };

		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Positions, m_Positions);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Normals, m_Normals);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Tangents, m_Tangents);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_UV0, m_UV0);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_UV1, m_UV1);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Color0, m_Color0);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Joints0, m_Joints0);
		LocalizeAttribute(Prim, RemapFunc, Parent.m_Indices, Parent.m_Weights0, m_Weights0);
	}
	else
	{
		m_Positions.assign(&Parent.m_Positions[Prim.Offset], &Parent.m_Positions[Prim.Offset + Prim.VertexCount]);
		m_Normals.assign(&Parent.m_Normals[Prim.Offset], &Parent.m_Normals[Prim.Offset + Prim.VertexCount]);
		m_Tangents.assign(&Parent.m_Tangents[Prim.Offset], &Parent.m_Tangents[Prim.Offset + Prim.VertexCount]);
		m_UV0.assign(&Parent.m_UV0[Prim.Offset], &Parent.m_UV0[Prim.Offset + Prim.VertexCount]);
		m_UV1.assign(&Parent.m_UV1[Prim.Offset], &Parent.m_UV1[Prim.Offset + Prim.VertexCount]);
		m_Color0.assign(&Parent.m_Color0[Prim.Offset], &Parent.m_Color0[Prim.Offset + Prim.VertexCount]);
		m_Joints0.assign(&Parent.m_Joints0[Prim.Offset], &Parent.m_Joints0[Prim.Offset + Prim.VertexCount]);
		m_Weights0.assign(&Parent.m_Weights0[Prim.Offset], &Parent.m_Weights0[Prim.Offset + Prim.VertexCount]);
	}
}


bool MeshInfo::Initialize(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh)
{
	// Ensure mesh has the correct properties for us to process.
	if (!IsSupported(Mesh))
	{
		return false;
	}

	// Clear the old state of the mesh (to allow warm starting of buffers.)
	Reset();

	// Pull in the mesh data and cache the metadata.
	m_Primitives.resize(Mesh.primitives.size());

	if (UsesSharedAccessors(Mesh))
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
		if (PositionStart > 0)
		{
			std::for_each(m_Indices.begin() + IndexStart, m_Indices.end(), [=](auto& v) { v = v + PositionStart; });
		}
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
		if (Mesh.primitives.size() == 1)
		{
			ReadAccessor(StreamReader, Doc, p0.indicesAccessorId, m_Indices, PrimInfo0[Indices]);

			PrimInfo0.IndexCount = m_Indices.size();
			PrimInfo0.VertexCount = m_Positions.size();
		}
		else
		{
			// Use the uniqueness count to determine number of vertices for each primitive.
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
				PrimInfo.VertexCount = UniqueVertices.size(); 
			}
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
	m_PrimFormat = PrimitiveFormat::Combine;
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

	WriteVertices(Info, m_Scratch);
	DirectX::FinalizeVBAndPointReps(m_Scratch.data(), Info.GetVertexSize(), VertexCount, PointReps.data(), VertRemap.data());
	ReadVertices(Info, m_Scratch);

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

void MeshInfo::Export(const MeshOptions& Options, BufferBuilder& Builder, Mesh& OutMesh) const
{
	auto PrimFormat = Options.PrimitiveFormat == PrimitiveFormat::Preserved ? m_PrimFormat : Options.PrimitiveFormat;

	if (PrimFormat == PrimitiveFormat::Combine)
	{
		if (Options.AttributeFormat == AttributeFormat::Interleave)
		{
			ExportCI(Builder, OutMesh);
		}
		else
		{
			if (m_Indices.empty())
			{
				ExportCS(Builder, OutMesh);
			}
			else
			{
				ExportCSI(Builder, OutMesh);
			}
		}
	}
	else
	{
		if (Options.AttributeFormat == AttributeFormat::Interleave)
		{
			ExportSI(Builder, OutMesh);
		}
		else
		{
			ExportSS(Builder, OutMesh);
		}
	}

	auto& Buffer = Builder.GetCurrentBuffer();

	rapidjson::Document meshExtJson;
	meshExtJson.SetObject();

	meshExtJson.AddMember("uri", rapidjson::Value(Buffer.uri.c_str(), Buffer.uri.size()), meshExtJson.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	meshExtJson.Accept(writer);

	OutMesh.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_MESH_OPTIMIZER, buffer.GetString()));
}

bool MeshInfo::IsSupported(const Mesh& m)
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

void MeshInfo::Cleanup(const GLTFDocument& OldDoc, GLTFDocument& NewDoc)
{
	// Remove references to old accessors.
	for (const auto& m : OldDoc.meshes.Elements())
	{
		if (!NewDoc.meshes.Has(m.id))
		{
			// Generally shouldn't occur assuming NewDoc is a copy of the OldDoc.
			continue;
		}

		// Only perform cleanup on meshes that we successfully processed.
		auto& NewMesh = NewDoc.meshes.Get(m.id);
		if (NewMesh.extensions.count(EXTENSION_MSFT_MESH_OPTIMIZER) == 0)
		{
			continue;
		}

		for (const auto& p : m.primitives)
		{
			FOREACH_ATTRIBUTE([&](auto i)
			{
				if (!(p.*AccessorIds[i]).empty())
				{
					NewDoc.accessors.Remove(p.*AccessorIds[i]);
				}
			});
		}
	}

	// Iterate through the still existing accessors and accumulate all referenced buffer view ids.
	auto BufferViews = std::unordered_set<std::string>(OldDoc.bufferViews.Size());
	for (const auto& a : NewDoc.accessors.Elements())
	{
		BufferViews.insert(a.bufferViewId);
	}

	// Determine the buffer views which are no longer referenced; accumulate all referenced buffer ids.
	auto Buffers = std::unordered_set<std::string>(OldDoc.buffers.Size());
	for (const auto& bv : OldDoc.bufferViews.Elements())
	{
		if (BufferViews.count(bv.id) == 0)
		{
			NewDoc.bufferViews.Remove(bv.id);
		}
		else
		{
			Buffers.insert(bv.bufferId);
		}
	}

	// Determine the buffers which are no longer referenced.
	for (const auto& b : OldDoc.buffers.Elements())
	{
		if (Buffers.count(b.id) == 0)
		{
			NewDoc.buffers.Remove(b.id);
		}
	}
}

PrimitiveInfo MeshInfo::DetermineMeshFormat(void) const
{
	if (m_Primitives.empty())
	{
		return PrimitiveInfo{};
	}

	// Start at most compressed vertex attribute formats.
	PrimitiveInfo MaxInfo = PrimitiveInfo::CreateMin(m_Indices.size(), m_Positions.size(), m_Attributes);

	// Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		MaxInfo = PrimitiveInfo::Max(MaxInfo, m_Primitives[i]);
	}

	return MaxInfo;
}

void MeshInfo::WriteVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const
{
	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets);

	Output.resize(Info.VertexCount * Stride);

	Write(Info[Positions], Output.data(), Stride, Offsets[Positions], m_Positions.data(), m_Positions.size());
	Write(Info[Normals], Output.data(), Stride, Offsets[Normals], m_Normals.data(), m_Normals.size());
	Write(Info[Tangents], Output.data(), Stride, Offsets[Tangents], m_Tangents.data(), m_Tangents.size());
	Write(Info[UV0], Output.data(), Stride, Offsets[UV0], m_UV0.data(), m_UV0.size());
	Write(Info[UV1], Output.data(), Stride, Offsets[UV1], m_UV1.data(), m_UV1.size());
	Write(Info[Color0], Output.data(), Stride, Offsets[Color0], m_Color0.data(), m_Color0.size());
	Write(Info[Joints0], Output.data(), Stride, Offsets[Joints0], m_Joints0.data(), m_Joints0.size());
	Write(Info[Weights0], Output.data(), Stride, Offsets[Weights0], m_Weights0.data(), m_Weights0.size());
}

void MeshInfo::ReadVertices(const PrimitiveInfo& Info, const std::vector<uint8_t>& Input)
{
	m_Positions.resize(Info.VertexCount);
	m_Normals.resize(Info.VertexCount);
	m_Tangents.resize(Info.VertexCount);
	m_UV0.resize(Info.VertexCount);
	m_UV1.resize(Info.VertexCount);
	m_Color0.resize(Info.VertexCount);
	m_Joints0.resize(Info.VertexCount);
	m_Weights0.resize(Info.VertexCount);

	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets);

	Read(Info[Positions], m_Positions.data(), Input.data(), Stride, Offsets[Positions], Info.VertexCount);
	Read(Info[Normals], m_Normals.data(), Input.data(), Stride, Offsets[Normals], Info.VertexCount);
	Read(Info[Tangents], m_Tangents.data(), Input.data(), Stride, Offsets[Tangents], Info.VertexCount);
	Read(Info[UV0], m_UV0.data(), Input.data(), Stride, Offsets[UV0], Info.VertexCount);
	Read(Info[UV1], m_UV1.data(), Input.data(), Stride, Offsets[UV1], Info.VertexCount);
	Read(Info[Color0], m_Color0.data(), Input.data(), Stride, Offsets[Color0], Info.VertexCount);
	Read(Info[Joints0], m_Joints0.data(), Input.data(), Stride, Offsets[Joints0], Info.VertexCount);
	Read(Info[Weights0], m_Weights0.data(), Input.data(), Stride, Offsets[Weights0], Info.VertexCount);
}

// Combine primitives, separate attributes, indexed
void MeshInfo::ExportCSI(BufferBuilder& Builder, Mesh& OutMesh) const
{
	const auto PrimInfo = DetermineMeshFormat();

	ExportSharedView(Builder, PrimInfo, Indices, &MeshInfo::m_Indices, OutMesh);

	std::string Ids[Count];
	Ids[Positions]	= ExportAccessor(Builder, PrimInfo, Positions, &MeshInfo::m_Positions);
	Ids[Normals]	= ExportAccessor(Builder, PrimInfo, Normals, &MeshInfo::m_Normals);
	Ids[Tangents]	= ExportAccessor(Builder, PrimInfo, Tangents, &MeshInfo::m_Tangents);
	Ids[UV0]		= ExportAccessor(Builder, PrimInfo, UV0, &MeshInfo::m_UV0);
	Ids[UV1]		= ExportAccessor(Builder, PrimInfo, UV1, &MeshInfo::m_UV1);
	Ids[Color0]		= ExportAccessor(Builder, PrimInfo, Color0, &MeshInfo::m_Color0);
	Ids[Joints0]	= ExportAccessor(Builder, PrimInfo, Joints0, &MeshInfo::m_Joints0);
	Ids[Weights0]	= ExportAccessor(Builder, PrimInfo, Weights0, &MeshInfo::m_Weights0);

	// Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
	for (auto& x : OutMesh.primitives)
	{
		FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { x.*AccessorIds[i] = Ids[i]; })
	}
}

// Combine primitives, separate attributes, non-indexed
void MeshInfo::ExportCS(BufferBuilder& Builder, Mesh& OutMesh) const
{
	const auto PrimInfo = DetermineMeshFormat();

	ExportSharedView(Builder, PrimInfo, Positions, &MeshInfo::m_Positions, OutMesh);
	ExportSharedView(Builder, PrimInfo, Normals, &MeshInfo::m_Normals, OutMesh);
	ExportSharedView(Builder, PrimInfo, Tangents, &MeshInfo::m_Tangents, OutMesh);
	ExportSharedView(Builder, PrimInfo, UV0, &MeshInfo::m_UV0, OutMesh);
	ExportSharedView(Builder, PrimInfo, UV1, &MeshInfo::m_UV1, OutMesh);
	ExportSharedView(Builder, PrimInfo, Color0, &MeshInfo::m_Color0, OutMesh);
	ExportSharedView(Builder, PrimInfo, Joints0, &MeshInfo::m_Joints0, OutMesh);
	ExportSharedView(Builder, PrimInfo, Weights0, &MeshInfo::m_Weights0, OutMesh);
}

// Combine primitives, interleave attributes
void MeshInfo::ExportCI(BufferBuilder& Builder, Mesh& OutMesh) const
{
	// Can't write a non-indexed combined mesh with multiple primitives.
	if (!m_Attributes.HasAttribute(Indices) && m_Primitives.size() > 1)
	{
		ExportSI(Builder, OutMesh);
	}

	auto PrimInfo = DetermineMeshFormat();
	ExportSharedView(Builder, PrimInfo, Indices, &MeshInfo::m_Indices, OutMesh);

	std::string Ids[Count];
	ExportInterleaved(Builder, PrimInfo, Ids);

	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
		{
			OutMesh.primitives[i].*AccessorIds[j] = Ids[j];
		});
	}
}

// Separate primitives, separate attributes
void MeshInfo::ExportSS(BufferBuilder& Builder, Mesh& OutMesh) const
{
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		MeshInfo Prim = MeshInfo(*this, i);

		OutMesh.primitives[i].indicesAccessorId		= Prim.ExportAccessor(Builder, m_Primitives[i], Indices, &MeshInfo::m_Indices);
		OutMesh.primitives[i].positionsAccessorId	= Prim.ExportAccessor(Builder, m_Primitives[i], Positions, &MeshInfo::m_Positions);
		OutMesh.primitives[i].normalsAccessorId		= Prim.ExportAccessor(Builder, m_Primitives[i], Normals, &MeshInfo::m_Normals);
		OutMesh.primitives[i].tangentsAccessorId	= Prim.ExportAccessor(Builder, m_Primitives[i], Tangents, &MeshInfo::m_Tangents);
		OutMesh.primitives[i].uv0AccessorId			= Prim.ExportAccessor(Builder, m_Primitives[i], UV0, &MeshInfo::m_UV0);
		OutMesh.primitives[i].uv1AccessorId			= Prim.ExportAccessor(Builder, m_Primitives[i], UV1, &MeshInfo::m_UV1);
		OutMesh.primitives[i].color0AccessorId		= Prim.ExportAccessor(Builder, m_Primitives[i], Color0, &MeshInfo::m_Color0);
		OutMesh.primitives[i].joints0AccessorId		= Prim.ExportAccessor(Builder, m_Primitives[i], Joints0, &MeshInfo::m_Joints0);
		OutMesh.primitives[i].weights0AccessorId	= Prim.ExportAccessor(Builder, m_Primitives[i], Weights0, &MeshInfo::m_Weights0);
	}
}

// Separate primitives, interleave attributes
void MeshInfo::ExportSI(BufferBuilder& Builder, Mesh& OutMesh) const
{
	for (size_t i = 0; i < m_Primitives.size(); ++i)
	{
		MeshInfo Prim = MeshInfo(*this, i);

		OutMesh.primitives[i].indicesAccessorId = Prim.ExportAccessor(Builder, m_Primitives[i], Indices, &MeshInfo::m_Indices);

		std::string Ids[Count];
		Prim.ExportInterleaved(Builder, m_Primitives[i], Ids);

		FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
		{
			OutMesh.primitives[i].*AccessorIds[j] = Ids[j];
		});
	}
}

void MeshInfo::ExportInterleaved(BufferBuilder& Builder, const PrimitiveInfo& Info, std::string(&OutIds)[Count]) const
{
	WriteVertices(Info, m_Scratch);

	size_t Alignment = 1;
	size_t Stride;
	size_t Offsets[Count];
	Info.GetVertexInfo(Stride, Offsets, &Alignment);

	Builder.AddBufferView(m_Scratch, Stride, ARRAY_BUFFER, Alignment);
	FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i)
	{
		if (m_Attributes.HasAttribute(i))
		{
			FindMinMax(Info[i], m_Scratch.data(), Stride, Offsets[i], Info.VertexCount, m_Min, m_Max);
			Builder.AddAccessor(Info.VertexCount, Offsets[i], Info[i].Type, Info[i].Dimension, m_Min, m_Max);

			OutIds[i] = Builder.GetCurrentAccessor().id;
		}
	});
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

		if (aid >= 0 && (size_t)aid < Doc.accessors.Size()) return std::string();
		return Doc.accessors[aid].bufferViewId;
	};

	std::string ViewIds[Count];
	FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { ViewIds[i] = GetBufferViewId(m.primitives[0].*AccessorIds[i]); })

	// Combined vs. separate primitives is determined by whether the vertex data is combined into a single or separate accessors.
	for (size_t i = 1; i < m.primitives.size(); ++i)
	{
		for (size_t j = Positions; j < Count; ++j)
		{
			if (ViewIds[j] != GetBufferViewId(m.primitives[i].*AccessorIds[j]))
			{
				return PrimitiveFormat::Separate;
			}
		}
	}

	return PrimitiveFormat::Combine;
}

bool MeshInfo::UsesSharedAccessors(const Mesh& m)
{
	if (m.primitives[0].indicesAccessorId.empty())
	{
		return false;
	}

	for (size_t i = 1; i < m.primitives.size(); ++i)
	{
		for (size_t j = Positions; j < Count; ++j)
		{
			if (m.primitives[0].*AccessorIds[j] != m.primitives[i].*AccessorIds[j])
			{
				return false;
			}
		}
	}

	return true;
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
