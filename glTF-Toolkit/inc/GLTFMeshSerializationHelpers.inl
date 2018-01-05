// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft::glTF::Toolkit
{
    template <typename T>
    void MeshInfo::ExportSharedView(BufferBuilder& builder, const PrimitiveInfo& info, Attribute attr, std::vector<T>(MeshInfo::*attributePtr), Mesh& outMesh) const
    {
        if (!m_Attributes.Has(attr))
        {
            return;
        }

        const auto& data = this->*attributePtr;
        const auto stride = info[attr].GetElementSize();

        m_Scratch.resize(data.size() * stride);
        Write(info[attr], m_Scratch.data(), data.data(), data.size());

        builder.AddBufferView(info[attr].Target);

        std::vector<AccessorDesc> descs;
        descs.resize(m_Primitives.size());

        for (size_t i = 0; i < m_Primitives.size(); ++i)
        {
            const auto& p = m_Primitives[i];

            auto& Desc = descs[i];
            Desc.count = p.GetCount(attr);
            Desc.byteOffset = stride * p.Offset;
            Desc.componentType = info[attr].Type;
            Desc.accessorType = info[attr].Dimension;
            FindMinMax(info[attr], m_Scratch.data(), stride, stride * p.Offset, p.GetCount(attr), Desc.minValues, Desc.maxValues);
        }

        std::vector<std::string> ids;
        ids.resize(m_Primitives.size());

        builder.AddAccessors(m_Scratch.data(), 0, descs.data(), descs.size(), ids.data());

        for (size_t i = 0; i < m_Primitives.size(); ++i)
        {
            outMesh.primitives[i].*AccessorIds[attr] = ids[i];
        }
    }

    template <typename T>
    std::string MeshInfo::ExportAccessor(BufferBuilder& builder, const PrimitiveInfo& p, Attribute attr, std::vector<T>(MeshInfo::*attributePtr)) const
    {
        if (!m_Attributes.Has(attr))
        {
            return std::string();
        }

        const auto& Data = this->*attributePtr;
        const auto& a = p[attr];

        const size_t dimension = Accessor::GetTypeCount(a.Dimension);
        const size_t componentSize = Accessor::GetComponentTypeSize(a.Type);
        const size_t byteStride = dimension * componentSize;

        m_Scratch.resize(Data.size() * byteStride);

        Write(a, m_Scratch.data(), byteStride, 0, Data.data(), Data.size());
        FindMinMax(a, m_Scratch.data(), byteStride, 0, Data.size(), m_Min, m_Max);

        builder.AddBufferView(a.Target);
        builder.AddAccessor(m_Scratch.data(), { a.Dimension, a.Type, p.GetCount(attr), 0, false, m_Min, m_Max });
        return builder.GetCurrentAccessor().id;
    }


    template <typename From, typename To, size_t Dimension>
    void Read(To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        const uint8_t* ptr = src + offset;
        for (size_t i = 0; i < count; ++i, ptr += stride)
        {
            XMSerializer<To>::Read<From, Dimension>(*(dest + i), (From*)ptr);
        }
    }

    template <typename From, typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        switch (accessor.Dimension)
        {
        case TYPE_SCALAR:	Read<From, To, 1>(dest, src, stride, offset, count); break;
        case TYPE_VEC2:		Read<From, To, 2>(dest, src, stride, offset, count); break;
        case TYPE_VEC3:		Read<From, To, 3>(dest, src, stride, offset, count); break;
        case TYPE_VEC4:		Read<From, To, 4>(dest, src, stride, offset, count); break;
        }
    }

    template <typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        if (offset == -1)
        {
            return;
        }

        switch (accessor.Type)
        {
        case COMPONENT_UNSIGNED_BYTE:	Read<uint8_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_UNSIGNED_SHORT:	Read<uint16_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_UNSIGNED_INT:	Read<uint32_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_FLOAT:			Read<float, To>(accessor, dest, src, stride, offset, count); break;
        }
    }

    template <typename From, typename To>
    void Read(const IStreamReader& reader, const GLTFDocument& doc, const Accessor& accessor, std::vector<To>& output)
    {
        GLTFResourceReader r = GLTFResourceReader(reader);
        auto buffer = r.ReadBinaryData<From>(doc, accessor);

        size_t compSize = Accessor::GetComponentTypeSize(accessor.componentType);
        size_t compCount = Accessor::GetTypeCount(accessor.type);

        size_t count = buffer.size() / compCount;
        size_t oldSize = output.size();
        output.resize(oldSize + count);

        switch (accessor.type)
        {
        case TYPE_SCALAR:	Read<From, To, 1>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC2:		Read<From, To, 2>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC3:		Read<From, To, 3>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC4:		Read<From, To, 4>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        }
    }

    template <typename To>
    void Read(const IStreamReader& reader, const GLTFDocument& doc, const Accessor& accessor, std::vector<To>& output)
    {
        switch (accessor.componentType)
        {
        case COMPONENT_UNSIGNED_BYTE:	Read<uint8_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_UNSIGNED_SHORT:	Read<uint16_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_UNSIGNED_INT:	Read<uint32_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_FLOAT:			Read<float, To>(reader, doc, accessor, output); break;
        }
    }

    template <typename To>
    bool ReadAccessor(const IStreamReader& reader, const GLTFDocument& doc, const std::string& accessorId, std::vector<To>& output, AccessorInfo& outInfo)
    {
        if (accessorId.empty())
        {
            return false;
        }

        auto& accessor = doc.accessors[accessorId];
        auto& bufferView = doc.bufferViews[accessor.bufferViewId];

        // Cache off the accessor metadata and read in the data into output buffer.
        outInfo.Type		= accessor.componentType;
        outInfo.Dimension	= accessor.type;
        outInfo.Target		= bufferView.target;

        Read(reader, doc, accessor, output);

        return true;
    }


    template <typename To, typename From, size_t Dimension>
    void Write(uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        uint8_t* ptr = dest + offset;
        for (size_t i = 0; i < count; ++i, ptr += stride)
        {
            XMSerializer<From>::Write<To, Dimension>((To*)ptr, src[i]);
        }
    }

    template <typename To, typename From>
    void Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        switch (info.Dimension)
        {
        case TYPE_SCALAR:	Write<To, From, 1>(dest, stride, offset, src, count); break;
        case TYPE_VEC2:		Write<To, From, 2>(dest, stride, offset, src, count); break;
        case TYPE_VEC3:		Write<To, From, 3>(dest, stride, offset, src, count); break;
        case TYPE_VEC4:		Write<To, From, 4>(dest, stride, offset, src, count); break;
        }
    }

    template <typename From>
    size_t Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        switch (info.Type)
        {
        case COMPONENT_UNSIGNED_BYTE:	Write<uint8_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_UNSIGNED_SHORT:	Write<uint16_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_UNSIGNED_INT:	Write<uint16_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_FLOAT:			Write<float, From>(info, dest, stride, offset, src, count); break;
        }

        return stride * count;
    }

    template <typename From>
    size_t Write(const AccessorInfo& info, uint8_t* dest, const From* src, size_t count)
    {
        if (count == 0)
        {
            return 0;
        }

        const size_t stride = Accessor::GetComponentTypeSize(info.Type) * Accessor::GetTypeCount(info.Dimension);
        return Write(info, dest, stride, 0, src, count);
    }


    template <typename T, size_t Dimension>
    void FindMinMax(const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max)
    {
        // Size to the correct dimension of the current accessor.
        min.resize(Dimension);
        max.resize(Dimension);

        // Fill with default extreme values.
        std::fill(min.begin(), min.end(), FLT_MAX);
        std::fill(max.begin(), max.end(), -FLT_MAX);

        // Iterate over offset strided data, finding min and maxs.
        const uint8_t* Ptr = src + offset;
        for (size_t i = 0; i < count; ++i)
        {
            for (size_t j = 0; j < Dimension; ++j)
            {
                T* pComp = (T*)(Ptr + i * stride) + j;

                min[j] = std::min(min[j], (float)*pComp);
                max[j] = std::max(max[j], (float)*pComp);
            }
        }
    }

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max)
    {
        switch (info.Dimension)
        {
        case TYPE_SCALAR:	FindMinMax<T, 1>(src, stride, offset, count, min, max); break;
        case TYPE_VEC2:		FindMinMax<T, 2>(src, stride, offset, count, min, max); break;
        case TYPE_VEC3:		FindMinMax<T, 3>(src, stride, offset, count, min, max); break;
        case TYPE_VEC4:		FindMinMax<T, 4>(src, stride, offset, count, min, max); break;
        }
    }

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const T* src, size_t count, std::vector<float>& min, std::vector<float>& max)
    {
        FindMinMax<T>(info, (uint8_t*)src, sizeof(T), 0, count, min, max);
    }

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const std::vector<T>& src, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max)
    {
        FindMinMax<T>(info, (uint8_t*)src, sizeof(T), sizeof(T) * offset, count, min, max);
    }


    template <typename T, typename RemapFunc>
    void LocalizeAttribute(const PrimitiveInfo& prim, const RemapFunc& remap, const std::vector<uint32_t>& indices, const std::vector<T>& global, std::vector<T>& local)
    {
        if (global.size() == 0)
        {
            return;
        }

        local.resize(prim.VertexCount);
        for (size_t i = 0; i < prim.IndexCount; ++i)
        {
            uint32_t index = indices[prim.Offset + i];
            uint32_t newIndex = remap(index);
            local[newIndex] = global[index];
        }
    }
}
