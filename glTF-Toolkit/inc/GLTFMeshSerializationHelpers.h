// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/IResourceWriter.h>
#include <DirectXMath.h>
#include "BufferBuilder2.h"

namespace Microsoft::glTF::Toolkit
{
	enum class AttributeFormat : uint8_t;


	//------------------------------------------
	// Attribute

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


	//------------------------------------------
	// AttributeList

	struct AttributeList
	{
		uint32_t Mask;

		inline void SetAttribute(Attribute Attr, bool Value) 
		{
			Value ? AddAttribute(Attr) : ClearAttribute(Attr);
		}
		inline void AddAttribute(Attribute Attr) { Mask |= 1 << Attr; }
		inline void ClearAttribute(Attribute Attr) { Mask &= ~(1 << Attr); }
		inline bool HasAttribute(Attribute Attr) const { return (Mask & Attr) != 0; }

		static AttributeList FromPrimitive(const MeshPrimitive& p);
		
		inline bool operator==(const AttributeList& rhs) const { return Mask == rhs.Mask; }
		inline bool operator!=(const AttributeList& rhs) const { return Mask != rhs.Mask; }
	};


	//------------------------------------------
	// AccessorInfo

	struct AccessorInfo
	{
		ComponentType		Type;
		AccessorType		Dimension;
		BufferViewTarget	Target;

		bool IsValid(void) const;
		size_t GetElementSize(void) const;

		static AccessorInfo Invalid(void);
		static AccessorInfo Create(ComponentType CType, AccessorType AType, BufferViewTarget Target);
		static AccessorInfo Max(const AccessorInfo& a0, const AccessorInfo& a1);
	};


	//------------------------------------------
	// PrimitiveInfo

	struct PrimitiveInfo
	{
		size_t Offset; // Could be index or vertex offset.

		size_t IndexCount;
		size_t VertexCount;
		AccessorInfo Metadata[Count];

		size_t GetCount(void) const { return IndexCount > 0 ? IndexCount : VertexCount; };
		size_t FaceCount(void) const { return GetCount() / 3; }
		size_t GetIndexSize(void) const { return Accessor::GetComponentTypeSize(Metadata[Indices].Type); }
		size_t GetVertexSize(void) const;

		void GetVertexInfo(size_t& Stride, size_t(&Offsets)[Count]) const;

		AccessorInfo& operator[] (size_t Index) { return Metadata[Index]; }
		const AccessorInfo& operator[] (size_t Index) const { return Metadata[Index]; }

		static ComponentType GetIndexType(size_t IndexCount) { return IndexCount < USHORT_MAX ? (IndexCount < BYTE_MAX ? COMPONENT_UNSIGNED_BYTE : COMPONENT_UNSIGNED_SHORT) : COMPONENT_UNSIGNED_INT; }

		static PrimitiveInfo Create(size_t IndexCount, size_t VertexCount, AttributeList Attributes, std::pair<ComponentType, AccessorType>(&Types)[Count], size_t Offset = 0);
		static PrimitiveInfo CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset = 0);
		static PrimitiveInfo CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset = 0);
		static PrimitiveInfo Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1);
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
		void Export(const MeshOptions& Options, BufferBuilder2& Builder, Mesh& OutMesh) const;

		static bool CanParse(const Mesh& m);

	private:
		inline size_t GetFaceCount(void) const { return (m_Indices.size() > 0 ? m_Indices.size() : m_Positions.size()) / 3; }
		PrimitiveInfo DetermineMeshFormat(void) const;

		void InitSeparateAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);
		void InitSharedAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);

		void WriteIndices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const;
		void WriteVertInterleaved(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const;
		void WriteVertSeparated(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const;
		void ReadVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Input);

		MeshInfo CreatePrimitive(const PrimitiveInfo& Prim) const;
		void ExportSSI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Separate primitives, separate attributes, indexed
		void ExportSS(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Separate primitives, separate attributes, non-indexed
		void ExportCSI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, separate attributes, indexed
		void ExportCS(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, separate attributes, non-indexed
		void ExportSI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Separate primitives, interleave attributes
		void ExportCI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, interleave attributes

		template <typename From, typename RemapFunc>
		void LocalizeAttribute(const PrimitiveInfo& Prim, const RemapFunc& Remap, const std::vector<From>& Global, std::vector<From>& Local) const;
		
		static void RemapIndices(std::unordered_map<uint32_t, uint32_t>& Map, std::vector<uint32_t>& NewIndices, const uint32_t* Indices, size_t Count);
		static bool UsesSharedAccessors(const GLTFDocument& Doc, const Mesh& m);
		static PrimitiveFormat DetermineFormat(const GLTFDocument& Doc, const Mesh& m);

	private:
		std::string m_Name;
		std::vector<PrimitiveInfo> m_Primitives;

		std::vector<uint32_t>			m_Indices;
		std::vector<DirectX::XMFLOAT3>	m_Positions;
		std::vector<DirectX::XMFLOAT3>	m_Normals;
		std::vector<DirectX::XMFLOAT4>	m_Tangents;
		std::vector<DirectX::XMFLOAT2>	m_UV0;
		std::vector<DirectX::XMFLOAT2>	m_UV1;
		std::vector<DirectX::XMFLOAT4>	m_Color0;
		std::vector<DirectX::XMUINT4>	m_Joints0;
		std::vector<DirectX::XMFLOAT4>	m_Weights0;

		AttributeList	m_Attributes;
		PrimitiveFormat m_PrimFormat;
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

	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, const From* Src, size_t Count);


	// ---- Exporting Mesh Data to GLTFSDK BufferBuilder ----

	template <typename From>
	std::string ExportAccessor(BufferBuilder2& Builder, const AccessorInfo& Info, const From* Src, size_t Count, size_t Offset = 0);

	// Maps a global mesh buffer to a 
	template <typename From, typename RemapFunc = std::function<uint32_t(uint32_t)>>
	std::string ExportAccessorIndexed(BufferBuilder2& Builder, const AccessorInfo& Info, size_t VertexCount, const uint32_t* Indices, size_t IndexCount, const RemapFunc& Remap, const From* GlobalAttr);


	// ---- Finding Min & Max ----

	template <typename T, size_t Dimension>
	void FindMinMax(const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);

	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);
	
	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const T* Src, size_t Count, std::vector<float>& Min, std::vector<float>& Max);
	
	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const std::vector<T>& Src, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);

	void FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max);
}

#include "GLTFMeshSerializationHelpers.inl"