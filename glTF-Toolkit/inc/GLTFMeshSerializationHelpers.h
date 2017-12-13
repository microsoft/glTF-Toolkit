// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/IResourceWriter.h>
#include <DirectXMath.h>
#include "BufferBuilder2.h"

namespace Microsoft { namespace glTF { namespace Toolkit
{
	enum class MeshOutputFormat;

	enum Attribute
	{
		Indices = 0,
		Positions,
		Normals,
		Tangents,
		UV0,
		UV1,
		Color0,
		Joints0,
		Weights0,
		Count
	};

// Attribute loop helpers
#define FOREACH_ATTRIBUTE_SUBRANGE(start, stop, op) for (size_t i = start; i < stop; ++i) { op((Attribute)i); }
#define FOREACH_ATTRIBUTE_START(start, op) FOREACH_ATTRIBUTE_SUBRANGE(start, Attribute::Count, op)
#define FOREACH_ATTRIBUTE(op) FOREACH_ATTRIBUTE_SUBRANGE(0, Attribute::Count, op)

	struct AttributeList
	{
		union
		{
			uint32_t Mask;
			struct
			{
				bool bIndices : 1;
				bool bPositions : 1;
				bool bNormals : 1;
				bool bTangents : 1;
				bool bUV0 : 1;
				bool bUV1 : 1;
				bool bColor0 : 1;
				bool bJoints0 : 1;
				bool bWeights0 : 1;
			};
		};

		inline void AddAttribute(Attribute Attr) { Mask = 1 << Attr; }
		inline bool HasAttribute(Attribute Attr) const { return (Mask & Attr) != 0; }

		static AttributeList FromPrimitive(const MeshPrimitive& p);
		
		inline bool operator==(const AttributeList& rhs) const { return Mask == rhs.Mask; }
		inline bool operator!=(const AttributeList& rhs) const { return Mask != rhs.Mask; }
	};

	struct AccessorInfo
	{
		ComponentType		Type;
		AccessorType		Dimension;
		BufferViewTarget	Target;

		bool IsValid(void) const;

		static AccessorInfo Invalid(void);
		static AccessorInfo Create(ComponentType CType, AccessorType AType, BufferViewTarget Target);
		static AccessorInfo Max(const AccessorInfo& a0, const AccessorInfo& a1);
	};


	//------------------------------------------
	// PrimitiveInfo

	struct PrimitiveInfo
	{
		size_t IndexCount;
		size_t VertexCount;
		AccessorInfo Metadata[Count];

		size_t FaceCount(void) const { return (IndexCount > 0 ? IndexCount : VertexCount) / 3; }
		size_t GetIndexSize(void) const { return Accessor::GetComponentTypeSize(Metadata[Indices].Type); }
		size_t GetVertexSize(void) const;

		void GetVertexInfo(size_t& Stride, size_t(&Offsets)[Count]) const;

		static ComponentType GetIndexType(size_t IndexCount) { return IndexCount < USHORT_MAX ? (IndexCount < BYTE_MAX ? COMPONENT_UNSIGNED_BYTE : COMPONENT_UNSIGNED_SHORT) : COMPONENT_UNSIGNED_INT; }
		static PrimitiveInfo Create(size_t IndexCount, size_t VertexCount, AttributeList Attributes, std::pair<ComponentType, AccessorType>(&Types)[Count]);
		static PrimitiveInfo CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes);
		static PrimitiveInfo CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes);
		static PrimitiveInfo Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1);

		AccessorInfo& operator[] (size_t Index) { return Metadata[Index]; }
		const AccessorInfo& operator[] (size_t Index) const { return Metadata[Index]; }
	};


	//------------------------------------------
	// MeshInfo

	class MeshInfo
	{
	public:
		MeshInfo(void);

		bool Initialize(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);
		void Reset(void);
		void Optimize(void);
		void GenerateAttributes(bool GenerateTangentSpace);
		void Export(BufferBuilder2& Builder, Mesh& OutMesh, MeshOutputFormat StreamType);

		static bool CanParse(const Mesh& m);

	private:
		inline size_t GetFaceCount(void) const { return (m_IndexCount > 0 ? m_IndexCount : m_VertexCount) / 3; }
		PrimitiveInfo DetermineMeshFormat(void);

		void WriteIndices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output);
		void WriteVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output);
		void ReadVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Input);
		void ExportIndexedSeparate(BufferBuilder2& Builder, Mesh& OutMesh);
		void WriteNonIndexedSeparate(BufferBuilder2& Builder, Mesh& OutMesh);
		void ExportInterleaved(BufferBuilder2& Builder, Mesh& OutMesh);

		size_t GenerateInterleaved(void);
		void RegenerateSeparate(void);

		static void RemapIndices(std::unordered_map<uint32_t, uint32_t>& Map, std::vector<uint32_t>& NewIndices, const uint32_t* Indices, size_t Count);

	private:
		std::vector<PrimitiveInfo> Primitives;

		std::vector<uint32_t> FacePrims; // Mapping from face index to primitive index.
		std::vector<uint32_t> Indices;

		// Geometry data
		std::vector<DirectX::XMFLOAT3> Positions;
		std::vector<DirectX::XMFLOAT3> Normals;
		std::vector<DirectX::XMFLOAT4> Tangents;
		std::vector<DirectX::XMFLOAT2> UV0;
		std::vector<DirectX::XMFLOAT2> UV1;
		std::vector<DirectX::XMFLOAT4> Color0;
		std::vector<DirectX::XMUINT4>  Joints0;
		std::vector<DirectX::XMFLOAT4> Weights0;

		std::vector<uint8_t> VertexBuffer;

		// DirectXMesh intermediate data
		std::vector<uint32_t> PointReps;
		std::vector<uint32_t> Adjacency;
		std::vector<uint32_t> DupVerts;
		std::vector<uint32_t> FaceRemap;
		std::vector<uint32_t> VertRemap;

		size_t m_VertexCount;
		size_t m_IndexCount;
		AttributeList m_Attributes;
	};


	//------------------------------------------
	// Serialization Helpers - these generally just perform switch cases down to the templated C++ types to allow for generic serialization.

	// ---- Reading ----

	template <typename From, typename To, size_t Dimension>
	void Read(To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count);

	template <typename From, typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count);

	template <typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Count, size_t Stride, size_t Offset);

	template <typename From, typename To>
	void Read(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Accessor& Accessor, std::vector<To>& Output);

	template <typename To>
	void Read(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Accessor& Accessor, std::vector<To>& Output);

	template <typename To>
	bool ReadAccessor(const IStreamReader& StreamReader, const GLTFDocument& Doc, const std::string& AccessorId, std::vector<To>& Output, AccessorInfo& OutInfo);


	// ---- Writing ----

	template <typename To, typename From, size_t Dimension>
	void Write(uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count);

	template <typename To, typename From>
	void Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count);

	template <typename From>
	void Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count);

	// Write series of adjacent data to a raw stream.
	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, const From* Src, size_t Count);


	// ---- Finding Min & Max ----

	template <typename T, size_t Dimension>
	void FindMinMax(const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);

	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);

	void FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);

	template <typename From>
	std::string ExportBuffer(BufferBuilder2& Builder, const AccessorInfo& Info, const From* Src, size_t Count, size_t Offset = 0);

	// Maps a global mesh buffer to a 
	template <typename From, typename RemapFunc = std::function<uint32_t(uint32_t)>>
	std::string ExportBufferIndexed(BufferBuilder2& Builder, const AccessorInfo& Info, size_t VertexCount, const uint32_t* Indices, size_t IndexCount, const RemapFunc& Remap, const From* GlobalAttr);
}}}

#include "GLTFMeshSerializationHelpers.inl"