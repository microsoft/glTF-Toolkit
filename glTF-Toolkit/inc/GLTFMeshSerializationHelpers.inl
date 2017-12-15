// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft::glTF::Toolkit
{
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
	void Read(const AccessorInfo& Accessor, To* Dest, const uint8_t* Src, size_t Count, size_t Stride, size_t Offset)
	{
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
		Output.resize(Output.size() + Count);

		switch (Accessor.type)
		{
		case TYPE_SCALAR:	Read<From, To, 1>(Output.data(), (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Buffer.size()); break;
		case TYPE_VEC2:		Read<From, To, 2>(Output.data(), (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Buffer.size()); break;
		case TYPE_VEC3:		Read<From, To, 3>(Output.data(), (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Buffer.size()); break;
		case TYPE_VEC4:		Read<From, To, 4>(Output.data(), (uint8_t*)Buffer.data(), CompSize * CompCount, 0, Buffer.size()); break;
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
		if (AccessorIndex < 0 || AccessorIndex >= Doc.accessors.Size())
		{
			return false;
		}
		auto& Accessor = Doc.accessors.Get(AccessorIndex);

		if (Accessor.bufferViewId.empty())
		{
			return false;
		}
		int BufferViewIndex = std::stoi(Accessor.bufferViewId);
		if (BufferViewIndex < 0 || BufferViewIndex >= Doc.bufferViews.Size())
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
	void Write(const AccessorInfo& Info, uint8_t* Dest, size_t Stride, size_t Offset, const From* Src, size_t Count)
	{
		switch (Info.Type)
		{
		case COMPONENT_UNSIGNED_BYTE:	Write<uint8_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_UNSIGNED_SHORT:	Write<uint16_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_UNSIGNED_INT:	Write<uint16_t, From>(Info, Dest, Stride, Offset, Src, Count); break;
		case COMPONENT_FLOAT:			Write<float, From>(Info, Dest, Stride, Offset, Src, Count); break;
		}
	}

	template <typename From>
	size_t Write(const AccessorInfo& Info, uint8_t* Dest, const From* Src, size_t Count)
	{
		size_t Stride = Accessor::GetComponentTypeSize(Info.Type) * Accessor::GetTypeCount(Info.Dimension);
		Write(Info, Dest, Stride, 0, Src, Count);
		return Stride;
	}


	template <typename From, typename RemapFunc>
	void LocalizeAttributes(std::vector<From>& AttributesLocal, const uint32_t* Indices, size_t IndexCount, const RemapFunc& Remap, const From* Attributes)
	{
		// Convert from globally indexed buffer to local buffer.
		std::for_each(Indices, Indices + IndexCount, [&](auto& i)
		{
			auto LocalIndex = Remap(i);

			if (LocalIndex >= AttributesLocal.size())
			{
				AttributesLocal.resize(LocalIndex + 1);
			}

			AttributesLocal[LocalIndex] = Attributes[i];
		});
	}

	template <typename From>
	std::string ExportBufferView(BufferBuilder2& Builder, const AccessorInfo& Info, const From* Src, size_t Count, size_t Offset)
	{
		size_t Dimension = Accessor::GetTypeCount(Info.Dimension);
		size_t ComponentSize = Accessor::GetComponentTypeSize(Info.Type);

		size_t ByteStride = Dimension * ComponentSize;
		size_t ByteOffset = Offset * ComponentSize;

		auto Buffer = std::vector<uint8_t>(Count * ByteStride);
		Write(Info, Buffer.data(), ByteStride, ByteOffset, Src, Count);

		Builder.AddBufferView(Buffer.data(), Buffer.size(), ByteStride, Info.Target);
		return Builder.GetCurrentBufferView().id;
	}

	template <typename From>
	std::string ExportPrimitive(BufferBuilder2& Builder, const AccessorInfo& Info, const From* Src, size_t Count, size_t Offset)
	{
		size_t Dimension = Accessor::GetTypeCount(Info.Dimension);
		size_t ComponentSize = Accessor::GetComponentTypeSize(Info.Type);

		size_t ByteStride = Dimension * ComponentSize;
		size_t ByteOffset = Offset * ComponentSize;

		auto Buffer = std::vector<uint8_t>(Count * ByteStride);
		Write(Info, Buffer.data(), ByteStride, ByteOffset, Src, Count);

		std::vector<float> Min, Max;
		FindMinMax(Info, Buffer.data(), ByteStride, ByteOffset, Count, Min, Max);

		Builder.AddBufferView(Buffer.data(), Buffer.size(), ByteStride, Info.Target);
		return Builder.GetCurrentBufferView().id;
	}

	template <typename From>
	std::string ExportAccessor(BufferBuilder2& Builder, const AccessorInfo& Info, const From* Src, size_t Count, size_t Offset)
	{
		ExportBufferView(Builder, Info, Src, Count Offset);

		std::vector<float> Min, Max;
		FindMinMax(Info, Buffer.data(), ByteStride, ByteOffset, Count, Min, Max);

		Builder.AddAccessor(Count, ByteOffset, Info.Type, Info.Dimension, Min, Max);
		return Builder.GetCurrentAccessor().id;
	}

	template <typename From, typename RemapFunc>
	std::string ExportAccessorIndexed(BufferBuilder2& Builder, const AccessorInfo& Info, size_t VertexCount, const uint32_t* Indices, size_t IndexCount, const RemapFunc& Remap, const From* Attributes)
	{
		auto Local = std::vector<From>(VertexCount);
		LocalizeAttributes(Local, Indices, IndexCount, Remap, Attributes);
		return ExportAccessor(Builder, Info, LocalAttr.data(), IndexCount);
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
}
