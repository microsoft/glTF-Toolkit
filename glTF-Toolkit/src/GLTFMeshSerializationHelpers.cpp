// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>
#include <DirectXMesh.h>
#include "DirectXMathUtils.h"
#include "GLTFMeshUtils.h"
#include "GLTFMeshSerializationHelpers.h"

#define EPSILON 1e-6f

using namespace DirectX;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
	AttributeList a ={ 0 };
	a.bIndices	= p.indicesAccessorId.empty();
	a.bPositions= p.positionsAccessorId.empty();
	a.bNormals	= p.normalsAccessorId.empty();
	a.bTangents	= p.tangentsAccessorId.empty();
	a.bUV0		= p.uv0AccessorId.empty();
	a.bUV1		= p.uv1AccessorId.empty();
	a.bColor0	= p.color0AccessorId.empty();
	a.bJoints0	= p.joints0AccessorId.empty();
	a.bWeights0	= p.weights0AccessorId.empty();
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
				Stride += (CompSize - Stride % CompSize);
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
			Info[i] = AccessorInfo::Create(Types[i].first, Types[i].second, i == Attribute::Indices ? ELEMENT_ARRAY_BUFFER : ARRAY_BUFFER);
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
	: Primitives()
	, FacePrims()
	, Indices()
	, Positions()
	, Normals()
	, Tangents()
	, UV0()
	, UV1()
	, Color0()
	, Joints0()
	, Weights0()
	, VertexBuffer()
	, PointReps()
	, Adjacency()
	, DupVerts()
	, FaceRemap()
	, VertRemap()
	, m_IndexCount(0u)
	, m_VertexCount(0u)
	, m_Attributes{ 0 }
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
	Primitives.resize(Mesh.primitives.size());

	for (size_t i = 0; i < Mesh.primitives.size(); ++i)
	{
		const auto& p = Mesh.primitives[i];

		uint32_t IndexStart = (uint32_t)Indices.size();
		uint32_t PositionStart = (uint32_t)Positions.size();

		auto& PrimInfo = Primitives[i];
		ReadAccessor(StreamReader, Doc, p.indicesAccessorId, Indices, PrimInfo[Attribute::Indices]);
		ReadAccessor(StreamReader, Doc, p.positionsAccessorId, Positions, PrimInfo[Attribute::Positions]);
		ReadAccessor(StreamReader, Doc, p.normalsAccessorId, Normals, PrimInfo[Attribute::Normals]);
		ReadAccessor(StreamReader, Doc, p.tangentsAccessorId, Tangents, PrimInfo[Attribute::Tangents]);
		ReadAccessor(StreamReader, Doc, p.uv0AccessorId, UV0, PrimInfo[Attribute::UV0]);
		ReadAccessor(StreamReader, Doc, p.uv1AccessorId, UV1, PrimInfo[Attribute::UV1]);
		ReadAccessor(StreamReader, Doc, p.color0AccessorId, Color0, PrimInfo[Attribute::Color0]);
		ReadAccessor(StreamReader, Doc, p.joints0AccessorId, Joints0, PrimInfo[Attribute::Joints0]);
		ReadAccessor(StreamReader, Doc, p.weights0AccessorId, Weights0, PrimInfo[Attribute::Weights0]);

		PrimInfo.IndexCount = Indices.size() - IndexStart;
		PrimInfo.VertexCount = Positions.size() - PositionStart;

		// Conversion from local to global index buffer; add vertex offset to each index.
		std::for_each(Indices.begin() + IndexStart, Indices.end(), [=](auto& v) { v = v + PositionStart; });

		// Populate attribute id array with primitive index denoting this is a distinct sub-mesh.
		size_t OldFaceCount = FacePrims.size();
		FacePrims.resize(OldFaceCount + PrimInfo.FaceCount());
		std::fill(FacePrims.begin() + OldFaceCount, FacePrims.end(), (uint32_t)i);
	}

	if (Positions.empty())
	{
		Reset();
		return false;
	}

	m_IndexCount; std::accumulate(Primitives.begin(), Primitives.end(), (size_t)0u, [](size_t Sum, const auto& p) { return Sum + p.IndexCount; });
	m_VertexCount = std::accumulate(Primitives.begin(), Primitives.end(), (size_t)0u, [](size_t Sum, const auto& p) { return Sum + p.VertexCount; });
	m_Attributes = AttributeList::FromPrimitive(Mesh.primitives[0]);

	return true;
}

void MeshInfo::Reset(void)
{
	Primitives.clear();

	FacePrims.clear();
	Indices.clear();

	Positions.clear();
	Normals.clear();
	Tangents.clear();
	UV0.clear();
	UV1.clear();
	Color0.clear();
	Joints0.clear();
	Weights0.clear();

	PointReps.clear();
	Adjacency.clear();
	DupVerts.clear();
	FaceRemap.clear();
	VertRemap.clear();

	m_IndexCount = 0;
	m_VertexCount = 0;
	m_Attributes ={ 0 };
}

void MeshInfo::Optimize(void)
{
	size_t FaceCount = GetFaceCount();

	// Ensure intermediate buffer sizes.
	PointReps.resize(m_VertexCount);
	Adjacency.resize(GetFaceCount() * 3);
	FaceRemap.resize(FacePrims.size());
	VertRemap.resize(m_VertexCount);

	// Perform DirectXMesh optimizations
	DirectX::GenerateAdjacencyAndPointReps(Indices.data(), FaceCount, Positions.data(), m_VertexCount, EPSILON, PointReps.data(), Adjacency.data());
	DirectX::Clean(Indices.data(), FaceCount, m_VertexCount, Adjacency.data(), FacePrims.data(), DupVerts);
	DirectX::AttributeSort(FaceCount, FacePrims.data(), FaceRemap.data());
	DirectX::ReorderIBAndAdjacency(Indices.data(), FaceCount, Adjacency.data(), FaceRemap.data());
	DirectX::OptimizeFacesEx(Indices.data(), FaceCount, Adjacency.data(), FacePrims.data(), FaceRemap.data());
	DirectX::ReorderIB(Indices.data(), FaceCount, FaceRemap.data());
	DirectX::OptimizeVertices(Indices.data(), FaceCount, m_VertexCount, VertRemap.data());
	DirectX::FinalizeIB(Indices.data(), FaceCount, VertRemap.data(), m_VertexCount);

	size_t Stride = GenerateInterleaved();
	DirectX::FinalizeVBAndPointReps(VertexBuffer.data(), Stride, m_VertexCount, PointReps.data(), VertRemap.data());
	RegenerateSeparate();
}

void MeshInfo::GenerateAttributes(bool GenerateTangentSpace)
{
	// Always generate normals if not present.
	if (Normals.empty())
	{
		Normals.resize(m_VertexCount);
		DirectX::ComputeNormals(Indices.data(), GetFaceCount(), Positions.data(), m_VertexCount, CNORM_DEFAULT, Normals.data());

		// Prompt recompute of tangents if they were supplied (however unlikely if no normals weren't supplied.)
		Tangents.clear();
	}

	// Generate tangents if not present and it's been opted-in.
	if (GenerateTangentSpace && Tangents.empty() && !UV0.empty())
	{
		Tangents.resize(m_VertexCount);
		DirectX::ComputeTangentFrame(Indices.data(), GetFaceCount(), Positions.data(), Normals.data(), UV0.data(), m_VertexCount, Tangents.data());
	}
}

void MeshInfo::Export(BufferBuilder2& Builder, Mesh& OutMesh, MeshOutputFormat StreamType)
{
	if (StreamType == MeshOutputFormat::Separate)
	{
		if (m_IndexCount > 0)
		{
			ExportIndexedSeparate(Builder, OutMesh);
		}
		else
		{
			WriteNonIndexedSeparate(Builder, OutMesh);
		}
	}
	else
	{
		ExportInterleaved(Builder, OutMesh);
	}
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

PrimitiveInfo MeshInfo::DetermineMeshFormat(void)
{
	if (Primitives.empty())
	{
		return PrimitiveInfo{};
	}

	// Start at most compressed vertex attribute formats.
	PrimitiveInfo MaxInfo = PrimitiveInfo::CreateMin(m_IndexCount, m_VertexCount, m_Attributes);

	// Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
	for (size_t i = 0; i < Primitives.size(); ++i)
	{
		MaxInfo = PrimitiveInfo::Max(MaxInfo, Primitives[i]);
	}

	return MaxInfo;
}

void MeshInfo::WriteIndices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output)
{
	size_t WriteLen = Info.IndexCount * Info.GetIndexSize();

	size_t OldSize = Output.size();
	Output.resize(OldSize + WriteLen);

	uint8_t* VertexStart = Output.data() + OldSize;
	Write(Info[Attribute::Indices], VertexStart, Indices.data(), Indices.size());
}

void MeshInfo::WriteVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output)
{
	size_t Stride;
	size_t Offsets[Attribute::Count];
	Info.GetVertexInfo(Stride, Offsets);

	size_t WriteLen = Info.VertexCount * Stride;

	size_t OldSize = Output.size();
	Output.resize(OldSize + WriteLen);

	uint8_t* VertexStart = Output.data() + OldSize;
	Write(Info[Attribute::Positions], VertexStart, Stride, Offsets[Attribute::Positions], Positions.data(), Positions.size());
	Write(Info[Attribute::Normals], VertexStart, Stride, Offsets[Attribute::Normals], Normals.data(), Normals.size());
	Write(Info[Attribute::Tangents], VertexStart, Stride, Offsets[Attribute::Tangents], Tangents.data(), Tangents.size());
	Write(Info[Attribute::UV0], VertexStart, Stride, Offsets[Attribute::UV0], UV0.data(), UV0.size());
	Write(Info[Attribute::UV1], VertexStart, Stride, Offsets[Attribute::UV1], UV1.data(), UV1.size());
	Write(Info[Attribute::Color0], VertexStart, Stride, Offsets[Attribute::Color0], Color0.data(), Color0.size());
	Write(Info[Attribute::Joints0], VertexStart, Stride, Offsets[Attribute::Joints0], Joints0.data(), Joints0.size());
	Write(Info[Attribute::Weights0], VertexStart, Stride, Offsets[Attribute::Weights0], Weights0.data(), Weights0.size());
}

void MeshInfo::ReadVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Input)
{
	size_t Stride;
	size_t Offsets[Attribute::Count];
	Info.GetVertexInfo(Stride, Offsets);

	Read(Info[Attribute::Positions], Positions.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Positions]);
	Read(Info[Attribute::Normals], Normals.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Normals]);
	Read(Info[Attribute::Tangents], Tangents.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Tangents]);
	Read(Info[Attribute::UV0], UV0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::UV0]);
	Read(Info[Attribute::UV1], UV1.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::UV1]);
	Read(Info[Attribute::Color0], Color0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Color0]);
	Read(Info[Attribute::Joints0], Joints0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Joints0]);
	Read(Info[Attribute::Weights0], Weights0.data(), Input.data(), Info.VertexCount, Stride, Offsets[Attribute::Weights0]);
}

void MeshInfo::ExportIndexedSeparate(BufferBuilder2& Builder, Mesh& OutMesh)
{
	for (size_t i = 0; i < Primitives.size(); ++i)
	{
		const auto& PrimInfo = Primitives[i];

		auto it = std::find(FacePrims.begin(), FacePrims.end(), (uint32_t)i);
		assert(it != FacePrims.end());

		uint32_t StartIndex = (uint32_t)(it - FacePrims.begin()) * 3;

		// Remap indices to a localized range.
		std::unordered_map<uint32_t, uint32_t> IndexRemap;
		std::vector<uint32_t> NewIndices;
		RemapIndices(IndexRemap, NewIndices, &Indices[StartIndex], PrimInfo.IndexCount);

		auto RemapFunc = [&](uint32_t i) { return IndexRemap.at(i); };

		MeshPrimitive& Prim = OutMesh.primitives[i];
		Prim.indicesAccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Indices], NewIndices.data(), PrimInfo.IndexCount);
		Prim.positionsAccessorId	= ExportBufferIndexed(Builder, PrimInfo[Attribute::Positions], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Positions.data());
		Prim.normalsAccessorId		= ExportBufferIndexed(Builder, PrimInfo[Attribute::Normals], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Normals.data());
		Prim.tangentsAccessorId		= ExportBufferIndexed(Builder, PrimInfo[Attribute::Tangents], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Tangents.data());
		Prim.uv0AccessorId			= ExportBufferIndexed(Builder, PrimInfo[Attribute::UV0], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, UV0.data());
		Prim.uv1AccessorId			= ExportBufferIndexed(Builder, PrimInfo[Attribute::UV1], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, UV1.data());
		Prim.color0AccessorId		= ExportBufferIndexed(Builder, PrimInfo[Attribute::Color0], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Color0.data());
		Prim.joints0AccessorId		= ExportBufferIndexed(Builder, PrimInfo[Attribute::Joints0], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Joints0.data());
		Prim.weights0AccessorId		= ExportBufferIndexed(Builder, PrimInfo[Attribute::Weights0], PrimInfo.VertexCount, Indices.data(), PrimInfo.IndexCount, RemapFunc, Weights0.data());
	}
}

void MeshInfo::WriteNonIndexedSeparate(BufferBuilder2& Builder, Mesh& OutMesh)
{
	for (size_t i = 0; i < Primitives.size(); ++i)
	{
		const auto& PrimInfo = Primitives[i];

		auto FaceIt = std::find(FacePrims.begin(), FacePrims.end(), (uint32_t)i);
		assert(FaceIt != FacePrims.end());

		size_t StartIndex = (FaceIt - FacePrims.begin()) * 3;

		MeshPrimitive& Prim = OutMesh.primitives[i];
		Prim.positionsAccessorId	= ExportBuffer(Builder, PrimInfo[Attribute::Positions], Positions.data(), PrimInfo.VertexCount, StartIndex);
		Prim.normalsAccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Normals], Normals.data(), PrimInfo.VertexCount, StartIndex);
		Prim.tangentsAccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Tangents], Tangents.data(), PrimInfo.VertexCount, StartIndex);
		Prim.uv0AccessorId			= ExportBuffer(Builder, PrimInfo[Attribute::UV0], UV0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.uv1AccessorId			= ExportBuffer(Builder, PrimInfo[Attribute::UV1], UV1.data(), PrimInfo.VertexCount, StartIndex);
		Prim.color0AccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Color0], Color0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.joints0AccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Joints0], Joints0.data(), PrimInfo.VertexCount, StartIndex);
		Prim.weights0AccessorId		= ExportBuffer(Builder, PrimInfo[Attribute::Weights0], Weights0.data(), PrimInfo.VertexCount, StartIndex);
	}
}

void MeshInfo::ExportInterleaved(BufferBuilder2& Builder, Mesh& OutMesh)
{
	auto PrimInfo = DetermineMeshFormat();
	std::vector<float> Min, Max;

	// Index output.
	if (m_Attributes.bIndices)
	{
		// Write indices.
		std::vector<uint8_t> OutIndices;
		WriteIndices(PrimInfo, OutIndices);

		// Add Buffer View for index list.
		size_t IndexSize = PrimInfo.GetIndexSize();
		Builder.AddBufferView(OutIndices, IndexSize, ELEMENT_ARRAY_BUFFER);

		// Create Index buffer accessor for each primitive at the appropriate byte offset.
		for (size_t i = 0; i < Primitives.size(); ++i)
		{
			const AccessorInfo& AccInfo = PrimInfo[Attribute::Indices];

			// Find the starting index of the buffer.
			auto FaceIt = std::find(FacePrims.begin(), FacePrims.end(), (uint32_t)i);
			size_t StartIndex = (FaceIt - FacePrims.begin()) * 3;

			// Find the min and max elements of the index list.
			FindMinMax(AccInfo, OutIndices.data(), IndexSize, StartIndex * IndexSize, Primitives[i].IndexCount, Min, Max);

			// Add the unique index accessor for this sub-mesh.
			Builder.AddAccessor(Primitives[i].IndexCount, StartIndex * IndexSize, AccInfo.Type, AccInfo.Dimension, Min, Max);
			OutMesh.primitives[i].indicesAccessorId = Builder.GetCurrentAccessor().id;
		}
	}

	// Vertex output.
	std::vector<uint8_t> OutVertices;
	WriteVertices(PrimInfo, OutVertices);

	size_t Stride;
	size_t Offsets[Attribute::Count];
	PrimInfo.GetVertexInfo(Stride, Offsets);

	std::string AccessorIds[Attribute::Count];

	Builder.AddBufferView(OutVertices, PrimInfo.GetVertexSize(), ARRAY_BUFFER);
	FOREACH_ATTRIBUTE_START(Attribute::Positions, [&](auto i)
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
		x.positionsAccessorId	= AccessorIds[Attribute::Positions];
		x.normalsAccessorId		= AccessorIds[Attribute::Normals];
		x.tangentsAccessorId	= AccessorIds[Attribute::Tangents];
		x.uv0AccessorId			= AccessorIds[Attribute::UV0];
		x.uv1AccessorId			= AccessorIds[Attribute::UV1];
		x.color0AccessorId		= AccessorIds[Attribute::Color0];
		x.joints0AccessorId		= AccessorIds[Attribute::Joints0];
		x.weights0AccessorId	= AccessorIds[Attribute::Weights0];
	}
}

size_t MeshInfo::GenerateInterleaved(void)
{
	auto Info = PrimitiveInfo::CreateMax(m_IndexCount, m_VertexCount, m_Attributes);
	WriteVertices(Info, VertexBuffer);
	return Info.GetVertexSize();
}

void MeshInfo::RegenerateSeparate(void)
{
	auto Info = PrimitiveInfo::CreateMax(m_IndexCount, m_VertexCount, m_Attributes);
	ReadVertices(Info, VertexBuffer);
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
