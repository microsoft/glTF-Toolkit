// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft::glTF::Toolkit
{
	template <typename T>
	void MeshInfo::ExportSharedView(BufferBuilder& Builder, const PrimitiveInfo& Info, Attribute Attr, std::vector<T>(MeshInfo::*AttributePtr), Mesh& OutMesh) const
	{
		if (!m_Attributes.Has(Attr))
		{
			return;
		}

		const auto& Data = this->*AttributePtr;
		const auto Stride = Info[Attr].GetElementSize();

		m_Scratch.resize(Data.size() * Stride);
		Write(Info[Attr], m_Scratch.data(), Data.data(), Data.size());

		Builder.AddBufferView(m_Scratch, 0, Info[Attr].Target);

		for (size_t i = 0; i < m_Primitives.size(); ++i)
		{
			const auto& p = m_Primitives[i];

			FindMinMax(Info[Attr], m_Scratch.data(), Stride, Stride * p.Offset, p.GetCount(Attr), m_Min, m_Max);
			Builder.AddAccessor(p.GetCount(Attr), Stride * p.Offset, Info[Attr].Type, Info[Attr].Dimension, m_Min, m_Max);

			OutMesh.primitives[i].*AccessorIds[Attr] = Builder.GetCurrentAccessor().id;
		}
	}

	template <typename T>
	std::string MeshInfo::ExportAccessor(BufferBuilder& Builder, const PrimitiveInfo& p, Attribute Attr, std::vector<T>(MeshInfo::*AttributePtr)) const
	{
		if (!m_Attributes.Has(Attr))
		{
			return std::string();
		}

		const auto& Data = this->*AttributePtr;
		const auto& a = p[Attr];

		const size_t Dimension = Accessor::GetTypeCount(a.Dimension);
		const size_t ComponentSize = Accessor::GetComponentTypeSize(a.Type);
		const size_t ByteStride = Dimension * ComponentSize;

		m_Scratch.resize(Data.size() * ByteStride);

		Write(a, m_Scratch.data(), ByteStride, 0, Data.data(), Data.size());
		FindMinMax(a, m_Scratch.data(), ByteStride, 0, Data.size(), m_Min, m_Max);

		Builder.AddBufferView(a.Target);
		Builder.AddAccessor(m_Scratch.data(), p.GetCount(Attr), a.Type, a.Dimension, m_Min, m_Max);
		return Builder.GetCurrentAccessor().id;
	}


	template <typename From, typename To, size_t Dimension>
	void Read(To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count)
	{
		const uint8_t* Ptr = Src + Offset;
		for (size_t i = 0; i < Count; ++i, Ptr += Stride)
		{
			XMSerializer<To>::Create<From, Dimension>(*(Dest + i), (From*)Ptr);
		}
	}

	template <typename From, typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count)
	{
		switch (Accessor.Dimension)
		{
		case TYPE_SCALAR:	Read<From, To, 1>(Dest, Src, Stride, Offset, Count); break;
		case TYPE_VEC2:		Read<From, To, 2>(Dest, Src, Stride, Offset, Count); break;
		case TYPE_VEC3:		Read<From, To, 3>(Dest, Src, Stride, Offset, Count); break;
		case TYPE_VEC4:		Read<From, To, 4>(Dest, Src, Stride, Offset, Count); break;
		}
	}

	template <typename To>
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count)
	{
		if (Offset == -1)
		{
			return;
		}

		switch (Accessor.Type)
		{
		case COMPONENT_UNSIGNED_BYTE:	Read<uint8_t, To>(Accessor, Dest, Src, Stride, Offset, Count); break;
		case COMPONENT_UNSIGNED_SHORT:	Read<uint16_t, To>(Accessor, Dest, Src, Stride, Offset, Count); break;
		case COMPONENT_UNSIGNED_INT:	Read<uint32_t, To>(Accessor, Dest, Src, Stride, Offset, Count); break;
		case COMPONENT_FLOAT:			Read<float, To>(Accessor, Dest, Src, Stride, Offset, Count); break;
		}
	}

	template <typename From, typename To>
	void Read(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Accessor& Accessor, std::vector<To>& Output)
	{
		GLTFResourceReader r = GLTFResourceReader(StreamReader);
		auto Buffer = r.ReadBinaryData<From>(Doc, Accessor);

		size_t CompSize = Accessor::GetComponentTypeSize(Accessor.componentType);
		size_t CompCount = Accessor::GetTypeCount(Accessor.type);

		size_t Count = Buffer.size() / CompCount;
		size_t OldSize = Output.size();
		Output.resize(OldSize + Count);

		switch (Accessor.type)
		{
		case TYPE_SCALAR:	Read<From, To, 1>(Output.data() + OldSize, (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Count); break;
		case TYPE_VEC2:		Read<From, To, 2>(Output.data() + OldSize, (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Count); break;
		case TYPE_VEC3:		Read<From, To, 3>(Output.data() + OldSize, (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Count); break;
		case TYPE_VEC4:		Read<From, To, 4>(Output.data() + OldSize, (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Count); break;
		}
	}

	template <typename To>
	void Read(const IStreamReader& StreamReader, const GLTFDocument& Doc, const Accessor& Accessor, std::vector<To>& Output)
	{
		switch (Accessor.componentType)
		{
		case COMPONENT_UNSIGNED_BYTE:	Read<uint8_t, To>(StreamReader, Doc, Accessor, Output); break;
		case COMPONENT_UNSIGNED_SHORT:	Read<uint16_t, To>(StreamReader, Doc, Accessor, Output); break;
		case COMPONENT_UNSIGNED_INT:	Read<uint32_t, To>(StreamReader, Doc, Accessor, Output); break;
		case COMPONENT_FLOAT:			Read<float, To>(StreamReader, Doc, Accessor, Output); break;
		}
	}

	template <typename To>
	bool ReadAccessor(const IStreamReader& StreamReader, const GLTFDocument& Doc, const std::string& AccessorId, std::vector<To>& Output, AccessorInfo& OutInfo)
	{
		// Parse the string ids.
		if (AccessorId.empty())
		{
			return false;
		}

		int AccessorIndex = std::stoi(AccessorId);
		if (AccessorIndex < 0 || (size_t)AccessorIndex >= Doc.accessors.Size())
		{
			return false;
		}
		auto& Accessor = Doc.accessors.Get(AccessorIndex);

		if (Accessor.bufferViewId.empty())
		{
			return false;
		}
		int BufferViewIndex = std::stoi(Accessor.bufferViewId);
		if (BufferViewIndex < 0 || (size_t)BufferViewIndex >= Doc.bufferViews.Size())
		{
			return false;
		}
		auto& BufferView = Doc.bufferViews.Get(BufferViewIndex);

		// Cache off the accessor metadata and read in the data into output buffer.
		OutInfo.Type		= Accessor.componentType;
		OutInfo.Dimension	= Accessor.type;
		OutInfo.Target		= BufferView.target;

		Read(StreamReader, Doc, Accessor, Output);

		return true;
	}


	template <typename To, typename From, size_t Dimension>
	void Write(uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count)
	{
		uint8_t* Ptr = Dest + Offset;
		for (size_t i = 0; i < Count; ++i, Ptr += Stride)
		{
			XMSerializer<From>::Write<To, Dimension>((To*)Ptr, Src[i]);
		}
	}

	template <typename To, typename From>
	void Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count)
	{
		switch (Info.Dimension)
		{
		case TYPE_SCALAR:	Write<To, From, 1>(Dest, Stride, Offset, Src, Count); break;
		case TYPE_VEC2:		Write<To, From, 2>(Dest, Stride, Offset, Src, Count); break;
		case TYPE_VEC3:		Write<To, From, 3>(Dest, Stride, Offset, Src, Count); break;
		case TYPE_VEC4:		Write<To, From, 4>(Dest, Stride, Offset, Src, Count); break;
		}
	}

	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count)
	{
		switch (Info.Type)
		{
		case COMPONENT_UNSIGNED_BYTE:	Write<uint8_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_UNSIGNED_SHORT:	Write<uint16_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_UNSIGNED_INT:	Write<uint16_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_FLOAT:			Write<float, From>(Info, Dest, Stride, Offset, Src, Count); break;
		}

		return Stride * Count;
	}

	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, const From* Src, size_t Count)
	{
		if (Count == 0)
		{
			return 0;
		}

		const size_t Stride = Accessor::GetComponentTypeSize(Info.Type) * Accessor::GetTypeCount(Info.Dimension);
		return Write(Info, Dest, Stride, 0, Src, Count);
	}


	template <typename T, size_t Dimension>
	void FindMinMax(const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max)
	{
		// Size to the correct dimension of the current accessor.
		Min.resize(Dimension);
		Max.resize(Dimension);

		// Fill with default extreme values.
		std::fill(Min.begin(), Min.end(), FLT_MAX);
		std::fill(Max.begin(), Max.end(), -FLT_MAX);

		// Iterate over offset strided data, finding min and maxs.
		const uint8_t* Ptr = Src + Offset;
		for (size_t i = 0; i < Count; ++i)
		{
			for (size_t j = 0; j < Dimension; ++j)
			{
				T* pComp = (T*)(Ptr + i * Stride) + j;

				Min[j] = std::min(Min[j], (float)*pComp);
				Max[j] = std::max(Max[j], (float)*pComp);
			}
		}
	}

	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const uint8_t* Src, size_t Stride, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max)
	{
		switch (Info.Dimension)
		{
		case TYPE_SCALAR:	FindMinMax<T, 1>(Src, Stride, Offset, Count, Min, Max); break;
		case TYPE_VEC2:		FindMinMax<T, 2>(Src, Stride, Offset, Count, Min, Max); break;
		case TYPE_VEC3:		FindMinMax<T, 3>(Src, Stride, Offset, Count, Min, Max); break;
		case TYPE_VEC4:		FindMinMax<T, 4>(Src, Stride, Offset, Count, Min, Max); break;
		}
	}

	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const T* Src, size_t Count, std::vector<float>& Min, std::vector<float>& Max)
	{
		FindMinMax<T>(Info, (uint8_t*)Src, sizeof(T), 0, Count, Min, Max);
	}

	template <typename T>
	void FindMinMax(const AccessorInfo& Info, const std::vector<T>& Src, size_t Offset, size_t Count, std::vector<float>& Min, std::vector<float>& Max)
	{
		FindMinMax<T>(Info, (uint8_t*)Src, sizeof(T), sizeof(T) * Offset, Count, Min, Max);
	}


	template <typename T, typename RemapFunc>
	void LocalizeAttribute(const PrimitiveInfo& Prim, const RemapFunc& Remap, const std::vector<uint32_t>& Indices, const std::vector<T>& Global, std::vector<T>& Local)
	{
		if (Global.size() == 0)
		{
			return;
		}

		Local.resize(Prim.VertexCount);
		for (size_t i = 0; i < Prim.IndexCount; ++i)
		{
			uint32_t Index = Indices[Prim.Offset + i];
			uint32_t NewIndex = Remap(Index);
			Local[NewIndex] = Global[Index];
		}

		//std::for_each(&Indices[Prim.Offset], &Indices[Prim.Offset + Prim.IndexCount], [&](auto& i) { Local[Remap(i)] = Global[i]; });
	}
}
