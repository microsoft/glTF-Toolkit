// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft::glTF::Toolkit
{
    template <typename T>
    void MeshInfo::ExportSharedView(BufferBuilder& builder, const PrimitiveInfo& info, Attribute attr, std::vector<T>(MeshInfo::*attributePtr), Mesh& outMesh) const
    {
        if (!m_attributes.Has(attr))
        {
            return;
        }

        const auto& data = this->*attributePtr;
        const auto stride = info[attr].GetElementSize();

        m_scratch.resize(data.size() * stride);
        Write(info[attr], m_scratch.data(), data.data(), data.size());
        
        for (size_t i = 0; i < m_primitives.size(); ++i)
        {
            const auto& p = m_primitives[i];

            AccessorDesc desc;
            desc.byteOffset = stride * p.offset;
            desc.componentType = info[attr].type;
            desc.accessorType = info[attr].dimension;
            FindMinMax(info[attr], m_scratch.data(), stride, stride * p.offset, p.GetCount(attr), desc.minValues, desc.maxValues);

            builder.AddBufferView(info[attr].target);
            builder.AddAccessor(m_scratch.data() + stride * p.offset, p.GetCount(attr), desc);

            outMesh.primitives[i].*s_AccessorIds[attr] = builder.GetCurrentAccessor().id;
        }
    }

    template <typename T>
    std::string MeshInfo::ExportAccessor(BufferBuilder& builder, const PrimitiveInfo& p, Attribute attr, std::vector<T>(MeshInfo::*attributePtr)) const
    {
        if (!m_attributes.Has(attr))
        {
            return std::string();
        }

        const auto& data = this->*attributePtr;
        const auto& a = p[attr];

        const size_t dimension = Accessor::GetTypeCount(a.dimension);
        const size_t componentSize = Accessor::GetComponentTypeSize(a.type);
        const size_t byteStride = dimension * componentSize;

        m_scratch.resize(data.size() * byteStride);

        Write(a, m_scratch.data(), byteStride, 0, data.data(), data.size());
        FindMinMax(a, m_scratch.data(), byteStride, 0, data.size(), m_min, m_max);

        builder.AddBufferView(a.target);
        builder.AddAccessor(m_scratch.data(), p.GetCount(attr), { a.dimension, a.type, false, m_min, m_max, 0 });
        return builder.GetCurrentAccessor().id;
    }


    template <typename From, typename To, size_t Dimension>
    void Read(To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        const uint8_t* ptr = src + offset;
        for (size_t i = 0; i < count; ++i, ptr += stride)
        {
            XMSerializer<To>::template Read<From, Dimension>(*(dest + i), (From*)ptr);
        }
    }

    template <typename From, typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        switch (accessor.dimension)
        {
        case TYPE_SCALAR: Read<From, To, 1>(dest, src, stride, offset, count); break;
        case TYPE_VEC2:   Read<From, To, 2>(dest, src, stride, offset, count); break;
        case TYPE_VEC3:   Read<From, To, 3>(dest, src, stride, offset, count); break;
        case TYPE_VEC4:   Read<From, To, 4>(dest, src, stride, offset, count); break;
        }
    }

    template <typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count)
    {
        if (offset == -1)
        {
            return;
        }

        switch (accessor.type)
        {
        case COMPONENT_UNSIGNED_BYTE:  Read<uint8_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_UNSIGNED_SHORT: Read<uint16_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_UNSIGNED_INT:   Read<uint32_t, To>(accessor, dest, src, stride, offset, count); break;
        case COMPONENT_FLOAT:          Read<float, To>(accessor, dest, src, stride, offset, count); break;
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
        case TYPE_SCALAR: Read<From, To, 1>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC2:   Read<From, To, 2>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC3:   Read<From, To, 3>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        case TYPE_VEC4:   Read<From, To, 4>(output.data() + oldSize, (uint8_t*)buffer.data(), compSize * compCount, 0, count); break;
        }
    }

    template <typename To>
    void Read(const IStreamReader& reader, const GLTFDocument& doc, const Accessor& accessor, std::vector<To>& output)
    {
        switch (accessor.componentType)
        {
        case COMPONENT_UNSIGNED_BYTE:  Read<uint8_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_UNSIGNED_SHORT: Read<uint16_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_UNSIGNED_INT:   Read<uint32_t, To>(reader, doc, accessor, output); break;
        case COMPONENT_FLOAT:          Read<float, To>(reader, doc, accessor, output); break;
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
        outInfo.type      = accessor.componentType;
        outInfo.dimension = accessor.type;
        outInfo.target    = bufferView.target;

        Read(reader, doc, accessor, output);

        return true;
    }


    template <typename To, typename From, size_t Dimension>
    void Write(uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        uint8_t* ptr = dest + offset;
        for (size_t i = 0; i < count; ++i, ptr += stride)
        {
            XMSerializer<From>::template Write<To, Dimension>((To*)ptr, src[i]);
        }
    }

    template <typename To, typename From>
    void Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        switch (info.dimension)
        {
        case TYPE_SCALAR: Write<To, From, 1>(dest, stride, offset, src, count); break;
        case TYPE_VEC2:   Write<To, From, 2>(dest, stride, offset, src, count); break;
        case TYPE_VEC3:   Write<To, From, 3>(dest, stride, offset, src, count); break;
        case TYPE_VEC4:   Write<To, From, 4>(dest, stride, offset, src, count); break;
        }
    }

    template <typename From>
    size_t Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count)
    {
        switch (info.type)
        {
        case COMPONENT_UNSIGNED_BYTE:  Write<uint8_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_UNSIGNED_SHORT: Write<uint16_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_UNSIGNED_INT:   Write<uint16_t, From>(info, dest, stride, offset, src, count); break;
        case COMPONENT_FLOAT:          Write<float, From>(info, dest, stride, offset, src, count); break;
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

        const size_t stride = Accessor::GetComponentTypeSize(info.type) * Accessor::GetTypeCount(info.dimension);
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
        switch (info.dimension)
        {
        case TYPE_SCALAR: FindMinMax<T, 1>(src, stride, offset, count, min, max); break;
        case TYPE_VEC2:   FindMinMax<T, 2>(src, stride, offset, count, min, max); break;
        case TYPE_VEC3:   FindMinMax<T, 3>(src, stride, offset, count, min, max); break;
        case TYPE_VEC4:   FindMinMax<T, 4>(src, stride, offset, count, min, max); break;
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
        if (global.empty())
        {
            return;
        }

        local.resize(prim.vertexCount);
        for (size_t i = 0; i < prim.indexCount; ++i)
        {
            uint32_t index = indices[prim.offset + i];
            uint32_t newIndex = remap(index);
            local[newIndex] = global[index];
        }
    }
}
