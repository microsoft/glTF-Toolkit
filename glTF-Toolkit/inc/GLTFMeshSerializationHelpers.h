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
	enum class PrimitiveFormat : uint8_t;
	struct MeshOptions;

	//------------------------------------------
	// Attribute

	enum Attribute
	{
		Indices = 0,
		Positions = 1,
		Normals = 2,
		Tangents = 3,
		UV0 = 4,
		UV1 = 5,
		Color0 = 6,
		Joints0 = 7,
		Weights0 = 8,
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
		inline bool HasAttribute(Attribute Attr) const { return (Mask & (1 << Attr)) != 0; }

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
		size_t GetCount(Attribute Attr) const { return Attr == Indices ? IndexCount : VertexCount; };
		size_t FaceCount(void) const { return GetCount() / 3; }
		size_t GetIndexSize(void) const { return Accessor::GetComponentTypeSize(Metadata[Indices].Type); }
		size_t GetVertexSize(void) const;

		void GetVertexInfo(size_t& Stride, size_t(&Offsets)[Count], size_t* pAlignment = nullptr) const;

		AccessorInfo& operator[] (size_t Index) { return Metadata[Index]; }
		const AccessorInfo& operator[] (size_t Index) const { return Metadata[Index]; }

		static ComponentType GetIndexType(size_t IndexCount) { return IndexCount < USHORT_MAX ? (IndexCount < BYTE_MAX ? COMPONENT_UNSIGNED_BYTE : COMPONENT_UNSIGNED_SHORT) : COMPONENT_UNSIGNED_INT; }

		static PrimitiveInfo Create(size_t IndexCount, size_t VertexCount, AttributeList Attributes, std::pair<ComponentType, AccessorType>(&Types)[Count], size_t Offset = 0);
		static PrimitiveInfo CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset = 0);
		static PrimitiveInfo CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset = 0);
		static PrimitiveInfo Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1);
	};

	using namespace Microsoft::glTF::exp;

	//------------------------------------------
	// MeshInfo

	class MeshInfo
	{
	public:
		MeshInfo(void);
		MeshInfo(const MeshInfo& Parent, size_t PrimIndex);

		// Populates the mesh with data from the specified glTF document & mesh.
		bool Initialize(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);

		// Clears the existing mesh data.
		void Reset(void);

		// Leverages DirectXMesh facilities to optimize the mesh data.
		void Optimize(void);

		// Generates normal and optionally tangent data.
		void GenerateAttributes(bool GenerateTangentSpace);

		// Exports the mesh to a BufferBuilder and Mesh in a format specified in the options.
		void Export(const MeshOptions& Options, BufferBuilder2& Builder, Mesh& OutMesh) const;

		// Determines whether a specific mesh exists in a supported format.
		static bool IsSupported(const Mesh& m);

	private:
		template <typename T>
		void Out(int iStream, const std::vector<T>& v) const { std::for_each(v.begin(), v.end(), [&](const auto& i) { XMSerializer<T>::Out(GetStream(iStream), i); }); }
		void Out(int iStream) const;

		inline size_t GetFaceCount(void) const { return (m_Indices.size() > 0 ? m_Indices.size() : m_Positions.size()) / 3; }
		PrimitiveInfo DetermineMeshFormat(void) const;

		void InitSeparateAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);
		void InitSharedAccessors(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Mesh& Mesh);

		void WriteVertices(const PrimitiveInfo& Info, std::vector<uint8_t>& Output) const;
		void ReadVertices(const PrimitiveInfo& Info, const std::vector<uint8_t>& Input);

		// Exports the mesh data to a BufferBuilder and Mesh in a specific format.
		void ExportCSI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, separate attributes, indexed
		void ExportCS(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, separate attributes, non-indexed
		void ExportCI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Combine primitives, interleave attributes
		void ExportSS(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Separate primitives, separate attributes
		void ExportSI(BufferBuilder2& Builder, Mesh& OutMesh) const;	// Separate primitives, interleave attributes

		// Writes vertex attribute data as a block, and exports one buffer view & accessor to a BufferBuilder.
		template <typename T>
		void ExportSharedView(BufferBuilder2& Builder, const PrimitiveInfo& Info, Attribute Attr, std::vector<T>(MeshInfo::*AttributePtr), Mesh& OutMesh) const;

		// Writes vertex attribute data as a block, and exports one buffer view & accessor to a BufferBuilder.
		template <typename T>
		std::string ExportAccessor(BufferBuilder2& Builder, const PrimitiveInfo& Prim, Attribute Attr, std::vector<T>(MeshInfo::*AttributePtr)) const;

		// Writes mesh vertex data in an interleaved fashion, and exports one buffer view and shared accessors to a BufferBuilder.
		void ExportInterleaved(BufferBuilder2& Builder, const PrimitiveInfo& Info, std::string (&OutIds)[Count]) const;

		// Maps indices from global vertex list to local (per-primitive) index list.
		static void RemapIndices(std::unordered_map<uint32_t, uint32_t>& Map, std::vector<uint32_t>& NewIndices, const uint32_t* Indices, size_t Count);

		// Determines if all primitives within a glTF mesh shares accessors (aka interleaved vertex data.)
		static bool UsesSharedAccessors(const Mesh& m);

		// Determines whether primitives within a glTF mesh are combined into a global buffer, or separated into their own local buffers.
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

		mutable std::vector<uint8_t> m_Scratch;
		mutable std::vector<float> m_Min;
		mutable std::vector<float> m_Max;
	};


	//------------------------------------------
	// Serialization Helpers - these generally just perform switch cases down to the templated C++ types to allow for generic serialization.

	// ---- Reading ----

	template <typename From, typename To, size_t Dimension>
	void Read(To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count);

	template <typename From, typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count);

	template <typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count);

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
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count);

	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, const From* Src, size_t Count);


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


	template <typename T, typename RemapFunc>
	void LocalizeAttribute(const PrimitiveInfo& Prim, const RemapFunc& Remap, const std::vector<uint32_t>& Indices, const std::vector<T>& Global, std::vector<T>& Local);
}

#include "GLTFMeshSerializationHelpers.inl"