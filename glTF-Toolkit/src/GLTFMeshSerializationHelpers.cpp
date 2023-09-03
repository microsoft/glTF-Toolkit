// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "GLTFMeshSerializationHelpers.h"

#include "DeviceResources.h"
#include "GLTFMeshUtils.h"
#include "DirectXMathUtils.h"

#include <DirectXMesh.h>
#include <numeric>

#define EPSILON 1e-6f

// Attribute loop helpers
#define FOREACH_ATTRIBUTE_SUBRANGE(start, stop, op) for (size_t _i = start; _i < stop; ++_i) { op((Attribute)_i); }
#define FOREACH_ATTRIBUTE_SETSTART(start, op) FOREACH_ATTRIBUTE_SUBRANGE(start, Count, op)
#define FOREACH_ATTRIBUTE(op) FOREACH_ATTRIBUTE_SETSTART(0, op)


using namespace DirectX;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
    template <typename T>
    void PrintVec(std::ostream& s, const std::vector<T>& v)
    {
        std::for_each(v.begin(), v.end(), [&](auto& i) { XMSerializer<T>::Out(s, i); });
    }

    template <typename T, size_t N>
    constexpr auto ArrayCount(T(&)[N]) { return N; }

    bool IsNormalized(Attribute attr, ComponentType type)
    {
        if (attr == Indices || attr == Joints0)
        {
            return false; // Indices and joints are integral values.
        }
        else
        {
            return type == COMPONENT_UNSIGNED_BYTE
                || type == COMPONENT_BYTE
                || type == COMPONENT_UNSIGNED_SHORT
                || type == COMPONENT_SHORT;
        }
    }
}

namespace Microsoft::glTF::Toolkit
{
    //------------------------------------------
    // Serialization Helpers - these generally just perform switch cases down to the templated C++ types to allow for generic serialization.

    // ---- Reading ----

    template <typename From, typename To, size_t Dimension>
    void Read(To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count);

    template <typename From, typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count);

    template <typename To>
    void Read(const AccessorInfo& accessor, To* dest, const uint8_t* src, size_t stride, size_t offset, size_t count);

    template <typename From, typename To>
    void Read(std::shared_ptr<IStreamReader>& reader, const Document& Doc, const Accessor& accessor, std::vector<To>& output);

    template <typename To>
    void Read(std::shared_ptr<IStreamReader>& reader, const Document& Doc, const Accessor& accessor, std::vector<To>& output);

    template <typename To>
    bool ReadAccessor(std::shared_ptr<IStreamReader>& reader, const Document& doc, const std::string& accessorId, std::vector<To>& output, AccessorInfo& outInfo);


    // ---- Writing ----

    template <typename To, typename From, size_t Dimension>
    void Write(uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count);

    template <typename To, typename From>
    void Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count);

    template <typename From>
    size_t Write(const AccessorInfo& info, uint8_t* dest, size_t stride, size_t offset, const From* src, size_t count);

    template <typename From>
    size_t Write(const AccessorInfo& info, uint8_t* dest, const From* src, size_t count);


    // ---- Finding Min & Max ----

    template <typename T, size_t Dimension>
    void FindMinMax(const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max);

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max);

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const T* src, size_t count, std::vector<float>& min, std::vector<float>& max);

    template <typename T>
    void FindMinMax(const AccessorInfo& info, const std::vector<T>& src, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max);

    void FindMinMax(const AccessorInfo& info, const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max);

    template <typename T, typename RemapFunc>
    void LocalizeAttribute(const PrimitiveInfo& prim, const RemapFunc& remap, const std::vector<uint32_t>& indices, const std::vector<T>& global, std::vector<T>& local);
}
#include "GLTFMeshSerializationHelpers.inl"


// The insertion ID signals to the ID generation functions that it should use a specific ID instead of manually generating one.
std::string Microsoft::glTF::Toolkit::s_insertionId;
std::string Microsoft::glTF::Toolkit::s_attributeNames[] = {
    "_INDICES_",
    "POSITION",
    "NORMAL",
    "TANGENT",
    "TEXCOORD_0",
    "TEXCOORD_1",
    "COLOR_0",
    "JOINTS_0",
    "WEIGHTS_0",
};


AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
    AttributeList a ={ };
    a.Set(Indices, !p.indicesAccessorId.empty());
    FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { a.Set(i, p.HasAttribute(s_attributeNames[i])); });
    return a;
}

bool AccessorInfo::IsValid(void) const
{
    const uint8_t zeros[sizeof(AccessorInfo)] ={ };
    return std::memcmp(this, zeros, sizeof(AccessorInfo)) != 0;
}

size_t AccessorInfo::GetElementSize(void) const
{
    return Accessor::GetComponentTypeSize(type) * Accessor::GetTypeCount(dimension);
}

AccessorInfo AccessorInfo::Invalid(void)
{
    AccessorInfo info;
    info.type      = COMPONENT_UNKNOWN;
    info.dimension = TYPE_UNKNOWN;
    info.target    = UNKNOWN_BUFFER;
    return info;
}

AccessorInfo AccessorInfo::Create(ComponentType cType, AccessorType aType, BufferViewTarget target)
{
    AccessorInfo info;
    info.type      = cType;
    info.dimension = aType;
    info.target    = target;
    return info;
}

AccessorInfo AccessorInfo::Max(const AccessorInfo& a0, const AccessorInfo& a1)
{
    AccessorInfo maxInfo;
    maxInfo.target    = a0.target;
    maxInfo.type      = std::max(a0.type, a1.type);
    maxInfo.dimension = std::max(a0.dimension, a1.dimension);
    return maxInfo;
}

std::ostream& Microsoft::glTF::Toolkit::operator<<(std::ostream& s, const AccessorInfo& a)
{
    static const std::unordered_map<ComponentType, const char*> cMap ={
        { COMPONENT_UNKNOWN,        "Unknown" },
        { COMPONENT_BYTE,           "Byte" },
        { COMPONENT_UNSIGNED_BYTE,  "UByte" },
        { COMPONENT_SHORT,          "Short" },
        { COMPONENT_UNSIGNED_SHORT, "UShort" },
        { COMPONENT_UNSIGNED_INT,   "UInt" },
        { COMPONENT_FLOAT,          "Float" },
    };
    static const std::unordered_map<AccessorType, const char*> aMap ={
        { TYPE_UNKNOWN, "Unknown" },
        { TYPE_SCALAR,  "Scalar" },
        { TYPE_VEC2,    "Vec2" },
        { TYPE_VEC3,    "Vec3" },
        { TYPE_VEC4,    "Vec4" },
        { TYPE_MAT2,    "Mat2" },
        { TYPE_MAT3,    "Mat3" },
        { TYPE_MAT4,    "Mat4" },
    };
    static const std::unordered_map<BufferViewTarget, const char*> bMap ={
        { UNKNOWN_BUFFER,       "Unknown" },
        { ELEMENT_ARRAY_BUFFER, "Index" },
        { ARRAY_BUFFER,         "Vertex" },
    };

    s << "Type: " << cMap.at(a.type) << ", Count: " << aMap.at(a.dimension) << ", Target: " << bMap.at(a.target);
    return s;
}


size_t PrimitiveInfo::GetVertexSize(void) const
{
    size_t stride = 0;
    size_t offsets[Count];
    GetVertexInfo(stride, offsets);
    return stride;
}

void PrimitiveInfo::GetVertexInfo(size_t& stride, size_t(&offsets)[Count], size_t* pOutAlignment) const
{
    size_t maxCompSize = 0;
    stride = 0;
    std::fill(offsets, offsets + Count, -1);

    // Iterate through attributes
    for (size_t i = Positions; i < Count; ++i)
    {
        // Only include valid attributes.
        if (metadata[i].IsValid())
        {
            const size_t CompSize = Accessor::GetComponentTypeSize(metadata[i].type);

            // Pad to the component type's size.
            // TODO: Implement more complicated packing mechanism.
            if (stride % CompSize != 0)
            {
                stride += CompSize - stride % CompSize;
            }

            maxCompSize = std::max(maxCompSize, CompSize);
            offsets[i] = stride;
            stride += CompSize * Accessor::GetTypeCount(metadata[i].dimension);
        }
    }

    if (pOutAlignment)
    {
        *pOutAlignment = maxCompSize;
    }
}

void PrimitiveInfo::CopyMeta(const PrimitiveInfo& info)
{
    std::copy(info.metadata, info.metadata + Count, metadata);
    metadata[Indices].type = GetIndexType(vertexCount);
}

PrimitiveInfo PrimitiveInfo::Create(size_t indexCount, size_t vertexCount, AttributeList attributes, const std::pair<ComponentType, AccessorType>(&types)[Count], size_t offset)
{
    PrimitiveInfo info ={ 0 };
    info.offset      = offset;
    info.indexCount  = indexCount;
    info.vertexCount = vertexCount;

    FOREACH_ATTRIBUTE([&](auto i)
    {
        if (attributes.Has(i))
        {
            info[i] = AccessorInfo::Create(types[i].first, types[i].second, i == Indices ? ELEMENT_ARRAY_BUFFER : ARRAY_BUFFER);
        }
    });
    return info;
}

// Creates a descriptor containing the most compressed form of mesh given a vertex and index count.
PrimitiveInfo PrimitiveInfo::CreateMin(size_t indexCount, size_t vertexCount, AttributeList attributes, size_t offset)
{
    static const std::pair<ComponentType, AccessorType> types[] ={
        { GetIndexType(vertexCount), TYPE_SCALAR }, // Indices
        { COMPONENT_FLOAT,           TYPE_VEC3 },   // Position
        { COMPONENT_FLOAT,           TYPE_VEC3 },   // Normals
        { COMPONENT_FLOAT,           TYPE_VEC4 },   // Tangents
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC2 },   // UV0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC2 },   // UV1
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Color0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Joints0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Weights0
    };

    return Create(indexCount, vertexCount, attributes, types, offset);
}

// Creates a descriptor containing the maximimum precision index and vertex types.
PrimitiveInfo PrimitiveInfo::CreateMax(size_t indexCount, size_t vertexCount, AttributeList attributes, size_t offset)
{
    static const std::pair<ComponentType, AccessorType> types[] ={
        { COMPONENT_UNSIGNED_INT,   TYPE_SCALAR }, // Indices
        { COMPONENT_FLOAT,          TYPE_VEC3 },   // Position
        { COMPONENT_FLOAT,          TYPE_VEC3 },   // Normals
        { COMPONENT_FLOAT,          TYPE_VEC4 },   // Tangents
        { COMPONENT_FLOAT,          TYPE_VEC2 },   // UV0
        { COMPONENT_FLOAT,          TYPE_VEC2 },   // UV1
        { COMPONENT_FLOAT,          TYPE_VEC4 },   // Color0
        { COMPONENT_UNSIGNED_SHORT, TYPE_VEC4 },   // Joints0
        { COMPONENT_FLOAT,          TYPE_VEC4 },   // Weights0
    };

    return Create(indexCount, vertexCount, attributes, types, offset);
}

PrimitiveInfo PrimitiveInfo::Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1)
{
    PrimitiveInfo maxInfo;
    maxInfo.indexCount  = p0.indexCount;
    maxInfo.vertexCount = p0.vertexCount;
    maxInfo.offset      = p0.offset;

    FOREACH_ATTRIBUTE([&](auto i) { maxInfo[i] = AccessorInfo::Max(p0[i], p1[i]); });

    return maxInfo;
}

std::ostream& Microsoft::glTF::Toolkit::operator<<(std::ostream& s, const PrimitiveInfo& p)
{
    static const char* const s_displayNames[Count] ={
        "Indices",
        "Positions",
        "Normals",
        "Tangents",
        "UV0",
        "UV1",
        "Color0",
        "Joints0",
        "Weights0"
    };

    s << "Offset: " << p.offset << std::endl;
    s << "IndexCount: " << p.indexCount << std::endl;
    s << "VertexCount: " << p.vertexCount << std::endl;

    for (size_t i = 0; i < Count; ++i)
    {
        s << s_displayNames[i] << ": (" << p.metadata[i] << ")" << std::endl;
    }

    s << std::endl;
    return s;
}


MeshOptimizer::MeshOptimizer(void)
    : m_name()
    , m_primitives()
    , m_indices()
    , m_positions()
    , m_normals()
    , m_tangents()
    , m_uv0()
    , m_uv1()
    , m_color0()
    , m_joints0()
    , m_weights0()
    , m_attributes{}
    , m_primFormat{0}
{ }

MeshOptimizer::MeshOptimizer(const MeshOptimizer& parent, size_t primIndex)
    : m_name()
    , m_primitives()
    , m_indices()
    , m_positions()
    , m_normals()
    , m_tangents()
    , m_uv0()
    , m_uv1()
    , m_color0()
    , m_joints0()
    , m_weights0()
    , m_attributes(parent.m_attributes)
    , m_primFormat(parent.m_primFormat)
{
    m_attributes = parent.m_attributes;

    const auto& prim = parent.m_primitives[primIndex];

    if (m_attributes.Has(Attribute::Indices))
    {
        std::unordered_map<uint32_t, uint32_t> indexRemap;
        RemapIndices(indexRemap, m_indices, &parent.m_indices[prim.offset], prim.indexCount);

        auto remapFunc = [&](uint32_t i) { return indexRemap[i]; };

        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_positions, m_positions);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_normals, m_normals);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_tangents, m_tangents);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_uv0, m_uv0);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_uv1, m_uv1);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_color0, m_color0);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_joints0, m_joints0);
        LocalizeAttribute(prim, remapFunc, parent.m_indices, parent.m_weights0, m_weights0);
    }
    else
    {
        m_positions.assign(&parent.m_positions[prim.offset], &parent.m_positions[prim.offset + prim.vertexCount]);
        m_normals.assign(&parent.m_normals[prim.offset], &parent.m_normals[prim.offset + prim.vertexCount]);
        m_tangents.assign(&parent.m_tangents[prim.offset], &parent.m_tangents[prim.offset + prim.vertexCount]);
        m_uv0.assign(&parent.m_uv0[prim.offset], &parent.m_uv0[prim.offset + prim.vertexCount]);
        m_uv1.assign(&parent.m_uv1[prim.offset], &parent.m_uv1[prim.offset + prim.vertexCount]);
        m_color0.assign(&parent.m_color0[prim.offset], &parent.m_color0[prim.offset + prim.vertexCount]);
        m_joints0.assign(&parent.m_joints0[prim.offset], &parent.m_joints0[prim.offset + prim.vertexCount]);
        m_weights0.assign(&parent.m_weights0[prim.offset], &parent.m_weights0[prim.offset + prim.vertexCount]);
    }
}

bool MeshOptimizer::Initialize(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh)
{
    // Ensure mesh has the correct properties for us to process.
    if (!IsSupported(mesh))
    {
        return false;
    }

    // Clear the old state of the mesh (to allow warm starting of buffers.)
    Reset();

    // Pull in the mesh data and cache the metadata.
    m_primitives.resize(mesh.primitives.size());

    if (UsesSharedAccessors(mesh))
    {
        InitSharedAccessors(reader, doc, mesh);
    }
    else
    {
        InitSeparateAccessors(reader, doc, mesh);
    }

    if (m_positions.empty())
    {
        Reset();
        return false;
    }

    m_name = mesh.name;
    m_attributes = AttributeList::FromPrimitive(mesh.primitives[0]);
    m_primFormat = DetermineFormat(doc, mesh);

    return true;
}

void MeshOptimizer::InitSeparateAccessors(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh)
{
    for (size_t i = 0; i < mesh.primitives.size(); ++i)
    {
        const auto& p = mesh.primitives[i];
        auto& primInfo = m_primitives[i];

        uint32_t indexStart = (uint32_t)m_indices.size();
        uint32_t positionStart = (uint32_t)m_positions.size();

        ReadAccessor(reader, doc, p.indicesAccessorId, m_indices, primInfo[Indices]);
        if (p.HasAttribute(s_attributeNames[Positions])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Positions]), m_positions, primInfo[Positions]);
        if (p.HasAttribute(s_attributeNames[Normals])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Normals]), m_normals, primInfo[Normals]);
        if (p.HasAttribute(s_attributeNames[Tangents])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Tangents]), m_tangents, primInfo[Tangents]);
        if (p.HasAttribute(s_attributeNames[UV0])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[UV0]), m_uv0, primInfo[UV0]);
        if (p.HasAttribute(s_attributeNames[UV1])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[UV1]), m_uv1, primInfo[UV1]);
        if (p.HasAttribute(s_attributeNames[Color0])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Color0]), m_color0, primInfo[Color0]);
        if (p.HasAttribute(s_attributeNames[Joints0])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Joints0]), m_joints0, primInfo[Joints0]);
        if (p.HasAttribute(s_attributeNames[Weights0])) ReadAccessor(reader, doc, p.GetAttributeAccessorId(s_attributeNames[Weights0]), m_weights0, primInfo[Weights0]);

        primInfo.offset = m_indices.size() > 0 ? indexStart : positionStart;
        primInfo.indexCount = m_indices.size() - indexStart;
        primInfo.vertexCount = m_positions.size() - positionStart;

        // Conversion from local to global index buffer; add vertex offset to each index.
        if (positionStart > 0)
        {
            std::for_each(m_indices.begin() + indexStart, m_indices.end(), [=](auto& v) { v = v + positionStart; });
        }
    }
}

void MeshOptimizer::InitSharedAccessors(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh)
{
    const auto& p0 = mesh.primitives[0];
    auto& primInfo0 = m_primitives[0];

    // Combined meshes can only be segmented into primitives (sub-meshes) by index offsets + counts; otherwise it better be only one primitive.
    assert(mesh.primitives.size() > 1 || !p0.indicesAccessorId.empty());

    if (p0.HasAttribute(s_attributeNames[Positions])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Positions]), m_positions, primInfo0[Positions]);
    if (p0.HasAttribute(s_attributeNames[Normals])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Normals]), m_normals, primInfo0[Normals]);
    if (p0.HasAttribute(s_attributeNames[Tangents])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Tangents]), m_tangents, primInfo0[Tangents]);
    if (p0.HasAttribute(s_attributeNames[UV0])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[UV0]), m_uv0, primInfo0[UV0]);
    if (p0.HasAttribute(s_attributeNames[UV1])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[UV1]), m_uv1, primInfo0[UV1]);
    if (p0.HasAttribute(s_attributeNames[Color0])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Color0]), m_color0, primInfo0[Color0]);
    if (p0.HasAttribute(s_attributeNames[Joints0])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Joints0]), m_joints0, primInfo0[Joints0]);
    if (p0.HasAttribute(s_attributeNames[Weights0])) ReadAccessor(reader, doc, p0.GetAttributeAccessorId(s_attributeNames[Weights0]), m_weights0, primInfo0[Weights0]);

    // If there are indices, grab the vertex count for each primitive by determining the number of unique indices in its index set.
    if (!p0.indicesAccessorId.empty())
    {
        if (mesh.primitives.size() == 1)
        {
            ReadAccessor(reader, doc, p0.indicesAccessorId, m_indices, primInfo0[Indices]);

            primInfo0.offset = 0;
            primInfo0.indexCount = m_indices.size();
            primInfo0.vertexCount = m_positions.size();
        }
        else
        {
            // Use the uniqueness count to determine number of vertices for each primitive.
            std::unordered_set<uint32_t> UniqueVertices;

            for (size_t i = 0; i < mesh.primitives.size(); ++i)
            {
                const auto& p = mesh.primitives[i];
                auto& primInfo = m_primitives[i];

                uint32_t IndexStart = (uint32_t)m_indices.size();
                ReadAccessor(reader, doc, p.indicesAccessorId, m_indices, primInfo[Indices]);

                // Generate the unique vertex set.
                UniqueVertices.clear();
                UniqueVertices.insert(m_indices.begin() + IndexStart, m_indices.end());

                primInfo.offset = IndexStart;
                primInfo.indexCount = m_indices.size() - IndexStart;
                primInfo.vertexCount = UniqueVertices.size();
                primInfo.CopyMeta(primInfo);
            }
        }
    }
    else
    {
        primInfo0.offset = 0;
        primInfo0.indexCount = 0;
        primInfo0.vertexCount = m_positions.size();
    }
}

void MeshOptimizer::Reset(void)
{
    m_name.clear();
    m_primitives.clear();

    m_indices.clear();
    m_positions.clear();
    m_normals.clear();
    m_tangents.clear();
    m_uv0.clear();
    m_uv1.clear();
    m_color0.clear();
    m_joints0.clear();
    m_weights0.clear();

    m_attributes ={};
    m_primFormat = PrimitiveFormat::Combine;
}

void MeshOptimizer::Optimize(void)
{
    if (!m_attributes.Has(Indices))
    {
        std::cout << "Mesh '" << m_name << "': optimize operation failed - this operation requires mesh to use indices.";
        return;
    }

    // DirectXMesh intermediate data
    std::vector<uint32_t> facePrims; // Mapping from face index to primitive index.
    std::vector<uint32_t> pointReps;
    std::vector<uint32_t> dupVerts;
    std::vector<uint32_t> faceRemap;
    std::vector<uint32_t> vertRemap;

    const size_t indexCount = m_indices.size();
    const size_t vertexCount = m_positions.size();
    const size_t faceCount = GetFaceCount();

    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        // Populate attribute id array with primitive index denoting this is a distinct sub-mesh.
        size_t oldFaceCount = facePrims.size();
        facePrims.resize(oldFaceCount + m_primitives[i].FaceCount());
        std::fill(facePrims.begin() + oldFaceCount, facePrims.end(), (uint32_t)i);
    }

    // Ensure intermediate buffer sizes.
    pointReps.resize(vertexCount);
    faceRemap.resize(facePrims.size());
    vertRemap.resize(vertexCount);

    // DirectXMesh optimization - Forsyth algorithm. https://github.com/Microsoft/DirectXMesh/wiki/DirectXMesh
    DX::ThrowIfFailed(DirectX::Clean(m_indices.data(), faceCount, vertexCount, nullptr, facePrims.data(), dupVerts));
    DX::ThrowIfFailed(DirectX::AttributeSort(faceCount, facePrims.data(), faceRemap.data()));
    DX::ThrowIfFailed(DirectX::ReorderIB(m_indices.data(), faceCount, faceRemap.data()));
    DX::ThrowIfFailed(DirectX::OptimizeFacesLRU(m_indices.data(), faceCount, faceRemap.data()));
    DX::ThrowIfFailed(DirectX::ReorderIB(m_indices.data(), faceCount, faceRemap.data()));
    DX::ThrowIfFailed(DirectX::OptimizeVertices(m_indices.data(), faceCount, vertexCount, vertRemap.data()));
    DX::ThrowIfFailed(DirectX::FinalizeIB(m_indices.data(), faceCount, vertRemap.data(), vertexCount));

    auto info = PrimitiveInfo::CreateMax(indexCount, vertexCount, m_attributes);

    WriteVertices(info, m_scratch);
    DX::ThrowIfFailed(DirectX::FinalizeVBAndPointReps(m_scratch.data(), info.GetVertexSize(), vertexCount, pointReps.data(), vertRemap.data()));
    ReadVertices(info, m_scratch);

    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        auto it = std::find(facePrims.begin(), facePrims.end(), (uint32_t)i);
        assert(it != facePrims.end());

        m_primitives[i].offset = size_t(it - facePrims.begin()) * 3;
    }
}

void MeshOptimizer::GenerateAttributes(void)
{
    if (!m_attributes.Has(Indices))
    {
        std::cout << "Mesh '" << m_name << "': normal/tangent generation operation failed - this operation requires mesh to use indices.";
        return;
    }

    const size_t indexCount = m_indices.size();
    const size_t vertexCount = m_positions.size();
    const size_t faceCount = GetFaceCount();

    // Generate normals if not present.
    if (m_normals.empty())
    {
        m_attributes.Add(Normals);
        std::for_each(m_primitives.begin(), m_primitives.end(), [](auto& p) { p[Normals] = AccessorInfo::Create(COMPONENT_FLOAT, TYPE_VEC3, ARRAY_BUFFER); });

        m_normals.resize(vertexCount);
        DirectX::ComputeNormals(m_indices.data(), faceCount, m_positions.data(), vertexCount, CNORM_DEFAULT, m_normals.data());

        // Prompt recompute of tangents if they were supplied (however unlikely if normals weren't supplied.)
        m_tangents.clear();
        m_attributes.Remove(Tangents);
    }

    // Generate tangents if not present. Requires a UV set.
    if (m_tangents.empty() && !m_uv0.empty())
    {
        m_attributes.Add(Tangents);
        std::for_each(m_primitives.begin(), m_primitives.end(), [](auto& p) { p[Tangents] = AccessorInfo::Create(COMPONENT_FLOAT, TYPE_VEC4, ARRAY_BUFFER); });

        m_tangents.resize(vertexCount);
        DirectX::ComputeTangentFrame(m_indices.data(), faceCount, m_positions.data(), m_normals.data(), m_uv0.data(), vertexCount, m_tangents.data());
    }
}

void MeshOptimizer::Export(const MeshOptions& options, BufferBuilder& builder, Mesh& outMesh) const
{
    auto primFormat = options.PrimitiveFormat == PrimitiveFormat::Preserved ? m_primFormat : options.PrimitiveFormat;

    if (primFormat == PrimitiveFormat::Combine)
    {
        if (options.AttributeFormat == AttributeFormat::Interleave)
        {
            ExportCI(builder, outMesh);
        }
        else
        {
            if (m_indices.empty())
            {
                ExportCS(builder, outMesh);
            }
            else
            {
                ExportCSI(builder, outMesh);
            }
        }
    }
    else
    {
        if (options.AttributeFormat == AttributeFormat::Interleave)
        {
            ExportSI(builder, outMesh);
        }
        else
        {
            ExportSS(builder, outMesh);
        }
    }
}

bool MeshOptimizer::IsSupported(const Mesh& m)
{
    if (m.primitives.empty())
    {
        return false; // Mesh has no data.
    }

    // All primitives of the mesh must be composed of triangles (which DirectXMesh requires) and have identical vertex attributes.
    AttributeList attrs = AttributeList::FromPrimitive(m.primitives[0]);

    for (auto& p : m.primitives)
    {
        if (p.mode != MESH_TRIANGLES || attrs != AttributeList::FromPrimitive(p))
        {
            return false;
        }
    }

    return true;
}

void MeshOptimizer::FindRestrictedIds(const Document& doc, std::unordered_set<std::string>& accessorIds, std::unordered_set<std::string>& bufferViewIds, std::unordered_set<std::string>& bufferIds)
{
    std::for_each(doc.accessors.Elements().begin(), doc.accessors.Elements().end(), [&](auto& a) { accessorIds.insert(a.id); });
    std::for_each(doc.bufferViews.Elements().begin(), doc.bufferViews.Elements().end(), [&](auto& bv) { bufferViewIds.insert(bv.id); });
    std::for_each(doc.buffers.Elements().begin(), doc.buffers.Elements().end(), [&](auto& b) { bufferIds.insert(b.id); });

    for (const auto& m : doc.meshes.Elements())
    {
        if (!MeshOptimizer::IsSupported(m))
        {
            continue;
        }

        for (const auto& p : m.primitives)
        {
            for (size_t i = Indices; i < Count; ++i)
            {
                std::string aid;
                if (i == Indices)
                {
                    if (p.indicesAccessorId.empty())
                    {
                        continue;
                    }

                    aid = p.indicesAccessorId;
                }
                else if (!p.TryGetAttributeAccessorId(s_attributeNames[i], aid))
                {
                    continue;
                }

                auto& bvid = doc.accessors[aid].bufferViewId;
                auto& bid = doc.bufferViews[bvid].bufferId;

                accessorIds.erase(aid);
                bufferViewIds.erase(bvid);
                bufferIds.erase(bid);
            }
        }
    }
}


void MeshOptimizer::Finalize(std::shared_ptr<IStreamReader>& streamReader, BufferBuilder& builder, const Document& oldDoc, Document& newDoc)
{
    // Since our BufferBuilder has all the processed mesh geometry within its own buffer, 
    // we need to remove references to old mesh buffers by removing each primitives' accessors,
    // buffer views, and buffers from the new document. 
    //
    // We must also copy non-mesh data that resides in those buffers to the new buffer so we can 
    // eliminate all references to the old buffer. In this case the IDs of the buffer views must
    // remain constant since an unknowable number of document items might reference that view.
    //
    // The BufferBuilder::Output(...) call will repopulate the new document with the appropriate
    // accessors, buffer views, and buffer that reference the new data.

    // 1. Find and remove all accessors, buffer views, and buffers that contain mesh data.
    auto meshBufferViews = std::unordered_set<std::string>(oldDoc.bufferViews.Size());
    auto meshBuffers = std::unordered_set<std::string>(oldDoc.buffers.Size());

    for (const auto& m : oldDoc.meshes.Elements())
    {
        // Only check meshes which we have processed.
        if (!MeshOptimizer::IsSupported(m))
        {
            continue;
        }

        for (const auto& p : m.primitives)
        {
            for (size_t i = Indices; i < Count; ++i)
            {
                std::string aid;
                if (i == Indices)
                {
                    if (p.indicesAccessorId.empty())
                    {
                        continue;
                    }

                    aid = p.indicesAccessorId;
                }
                else if (!p.TryGetAttributeAccessorId(s_attributeNames[i], aid))
                {
                    continue;
                }

                // Invalidate old mesh accessors in new document.
                if (newDoc.accessors.Has(aid))
                {
                    newDoc.accessors.Remove(aid);
                }

                // Invalidate old mesh buffer views in new document.
                auto& bvid = oldDoc.accessors[aid].bufferViewId;
                if (newDoc.bufferViews.Has(bvid))
                {
                    newDoc.bufferViews.Remove(bvid);
                }

                // Invalidate old mesh buffers in new document.
                auto& bid = oldDoc.bufferViews[bvid].bufferId;
                if (newDoc.buffers.Has(bid))
                {
                    newDoc.buffers.Remove(bid);
                }

                // Add buffer view and buffer to set of items that reference mesh data.
                meshBufferViews.insert(bvid);
                meshBuffers.insert(bid);
            }
        }
    }

    // 2. Find and remove all non-mesh buffer views that also reference buffers which hold outdated mesh 
    //    data - copy their data to the new buffer.
    GLTFResourceReader reader = GLTFResourceReader(streamReader);
    for (auto& bv : oldDoc.bufferViews.Elements())
    {
        // We've already handled buffer views that point to mesh data.
        if (meshBufferViews.count(bv.id) > 0)
        {
            continue;
        }

        // Determine if this buffer view references data in a buffer that also holds outdated mesh data.
        auto& obv = oldDoc.bufferViews[bv.id];
        if (meshBuffers.count(obv.bufferId) == 0)
        {
            continue; // The buffer doesn't contain outdated mesh data.
        }

        // The buffer view ID must be consistent between documents, as any number of document items
        // may continue to hold reference to it.
        s_insertionId = bv.id;

        // Read data from old buffer and copy to our new buffer.
        std::vector<uint8_t> buffer = reader.ReadBinaryData<uint8_t>(oldDoc, obv);
        builder.AddBufferView(buffer, obv.byteStride, obv.target);

        // Remove the old buffer view from the new document.
        newDoc.bufferViews.Remove(bv.id);
    }

    // 3. Output BufferBuilder to the new document, which will populate the document with updated
    //    accessors & buffer views that reference the new buffer.
    builder.Output(newDoc);
}

PrimitiveInfo MeshOptimizer::DetermineMeshFormat(void) const
{
    if (m_primitives.empty())
    {
        return PrimitiveInfo{};
    }

    // Start at most compressed vertex attribute formats.
    PrimitiveInfo maxInfo = PrimitiveInfo::CreateMin(m_indices.size(), m_positions.size(), m_attributes);

    // Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        maxInfo = PrimitiveInfo::Max(maxInfo, m_primitives[i]);
    }

    return maxInfo;
}

void MeshOptimizer::WriteVertices(const PrimitiveInfo& info, std::vector<uint8_t>& output) const
{
    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets);

    output.resize(info.vertexCount * stride);

    Write(info[Positions], output.data(), stride, offsets[Positions], m_positions.data(), m_positions.size());
    Write(info[Normals], output.data(), stride, offsets[Normals], m_normals.data(), m_normals.size());
    Write(info[Tangents], output.data(), stride, offsets[Tangents], m_tangents.data(), m_tangents.size());
    Write(info[UV0], output.data(), stride, offsets[UV0], m_uv0.data(), m_uv0.size());
    Write(info[UV1], output.data(), stride, offsets[UV1], m_uv1.data(), m_uv1.size());
    Write(info[Color0], output.data(), stride, offsets[Color0], m_color0.data(), m_color0.size());
    Write(info[Joints0], output.data(), stride, offsets[Joints0], m_joints0.data(), m_joints0.size());
    Write(info[Weights0], output.data(), stride, offsets[Weights0], m_weights0.data(), m_weights0.size());
}

void MeshOptimizer::ReadVertices(const PrimitiveInfo& info, const std::vector<uint8_t>& input)
{
    if (m_attributes.Has(Positions)) m_positions.resize(info.vertexCount);
    if (m_attributes.Has(Normals)) m_normals.resize(info.vertexCount);
    if (m_attributes.Has(Tangents)) m_tangents.resize(info.vertexCount);
    if (m_attributes.Has(UV0)) m_uv0.resize(info.vertexCount);
    if (m_attributes.Has(UV1)) m_uv1.resize(info.vertexCount);
    if (m_attributes.Has(Color0)) m_color0.resize(info.vertexCount);
    if (m_attributes.Has(Joints0)) m_joints0.resize(info.vertexCount);
    if (m_attributes.Has(Weights0)) m_weights0.resize(info.vertexCount);

    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets);

    Read(info[Positions], m_positions.data(), input.data(), stride, offsets[Positions], info.vertexCount);
    Read(info[Normals], m_normals.data(), input.data(), stride, offsets[Normals], info.vertexCount);
    Read(info[Tangents], m_tangents.data(), input.data(), stride, offsets[Tangents], info.vertexCount);
    Read(info[UV0], m_uv0.data(), input.data(), stride, offsets[UV0], info.vertexCount);
    Read(info[UV1], m_uv1.data(), input.data(), stride, offsets[UV1], info.vertexCount);
    Read(info[Color0], m_color0.data(), input.data(), stride, offsets[Color0], info.vertexCount);
    Read(info[Joints0], m_joints0.data(), input.data(), stride, offsets[Joints0], info.vertexCount);
    Read(info[Weights0], m_weights0.data(), input.data(), stride, offsets[Weights0], info.vertexCount);
}

// Combine primitives, separate attributes, indexed
void MeshOptimizer::ExportCSI(BufferBuilder& builder, Mesh& outMesh) const
{
    const auto PrimInfo = DetermineMeshFormat();

    ExportSharedView(builder, PrimInfo, Indices, &MeshOptimizer::m_indices, outMesh);

    std::string ids[Count];
    ExportAccessor(builder, PrimInfo, Positions, &MeshOptimizer::m_positions, ids[Positions]);
    ExportAccessor(builder, PrimInfo, Normals, &MeshOptimizer::m_normals, ids[Normals]);
    ExportAccessor(builder, PrimInfo, Tangents, &MeshOptimizer::m_tangents, ids[Tangents]);
    ExportAccessor(builder, PrimInfo, UV0, &MeshOptimizer::m_uv0, ids[UV0]);
    ExportAccessor(builder, PrimInfo, UV1, &MeshOptimizer::m_uv1, ids[UV1]);
    ExportAccessor(builder, PrimInfo, Color0, &MeshOptimizer::m_color0, ids[Color0]);
    ExportAccessor(builder, PrimInfo, Joints0, &MeshOptimizer::m_joints0, ids[Joints0]);
    ExportAccessor(builder, PrimInfo, Weights0, &MeshOptimizer::m_weights0, ids[Weights0]);

    // Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
    for (auto& x : outMesh.primitives)
    {
        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) 
        {
            if (!ids[i].empty())
            {
                x.attributes[s_attributeNames[i]] = ids[i];
            }
        })
    }
}

// Combine primitives, separate attributes, non-indexed
void MeshOptimizer::ExportCS(BufferBuilder& builder, Mesh& outMesh) const
{
    const auto primInfo = DetermineMeshFormat();

    ExportSharedView(builder, primInfo, Positions, &MeshOptimizer::m_positions, outMesh);
    ExportSharedView(builder, primInfo, Normals, &MeshOptimizer::m_normals, outMesh);
    ExportSharedView(builder, primInfo, Tangents, &MeshOptimizer::m_tangents, outMesh);
    ExportSharedView(builder, primInfo, UV0, &MeshOptimizer::m_uv0, outMesh);
    ExportSharedView(builder, primInfo, UV1, &MeshOptimizer::m_uv1, outMesh);
    ExportSharedView(builder, primInfo, Color0, &MeshOptimizer::m_color0, outMesh);
    ExportSharedView(builder, primInfo, Joints0, &MeshOptimizer::m_joints0, outMesh);
    ExportSharedView(builder, primInfo, Weights0, &MeshOptimizer::m_weights0, outMesh);
}

// Combine primitives, interleave attributes
void MeshOptimizer::ExportCI(BufferBuilder& builder, Mesh& outMesh) const
{
    // Can't write a non-indexed combined mesh with multiple primitives.
    if (!m_attributes.Has(Indices) && m_primitives.size() > 1)
    {
        ExportSI(builder, outMesh);
    }

    auto primInfo = DetermineMeshFormat();
    ExportSharedView(builder, primInfo, Indices, &MeshOptimizer::m_indices, outMesh);

    std::string ids[Count];
    ExportInterleaved(builder, primInfo, ids);

    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j) 
        {
            if (!ids[j].empty())
            {
                outMesh.primitives[i].attributes[s_attributeNames[j]] = ids[j];
            }
        });
    }
}

// Separate primitives, separate attributes
void MeshOptimizer::ExportSS(BufferBuilder& builder, Mesh& outMesh) const
{
    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        MeshOptimizer prim = MeshOptimizer(*this, i);

        prim.ExportAccessor(builder, m_primitives[i], Indices, &MeshOptimizer::m_indices, outMesh.primitives[i].indicesAccessorId);

        std::string id;
        if (prim.ExportAccessor(builder, m_primitives[i], Positions, &MeshOptimizer::m_positions, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Positions]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], Normals, &MeshOptimizer::m_normals, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Normals]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], Tangents, &MeshOptimizer::m_tangents, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Tangents]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], UV0, &MeshOptimizer::m_uv0, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[UV0]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], UV1, &MeshOptimizer::m_uv1, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[UV1]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], Color0, &MeshOptimizer::m_color0, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Color0]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], Joints0, &MeshOptimizer::m_joints0, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Joints0]] = id;
        }
        if (prim.ExportAccessor(builder, m_primitives[i], Weights0, &MeshOptimizer::m_weights0, id))
        {
            outMesh.primitives[i].attributes[s_attributeNames[Weights0]] = id;
        }
    }
}

// Separate primitives, interleave attributes
void MeshOptimizer::ExportSI(BufferBuilder& builder, Mesh& outMesh) const
{
    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        MeshOptimizer prim = MeshOptimizer(*this, i);

        prim.ExportAccessor(builder, m_primitives[i], Indices, &MeshOptimizer::m_indices, outMesh.primitives[i].indicesAccessorId);

        std::string ids[Count];
        prim.ExportInterleaved(builder, m_primitives[i], ids);

        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
        {
            if (!ids[j].empty())
            {
                outMesh.primitives[i].attributes[s_attributeNames[j]] = ids[j];
            }
        });
    }
}

void MeshOptimizer::ExportInterleaved(BufferBuilder& builder, const PrimitiveInfo& info, std::string(&outIds)[Count]) const
{
    WriteVertices(info, m_scratch);

    size_t alignment = 1;
    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets, &alignment);

    builder.AddBufferView(ARRAY_BUFFER);

    std::vector<AccessorDesc> descs;
    for (size_t i = Positions; i < Count; ++i)
    {
        if (m_attributes.Has((Attribute)i))
        {
            AccessorDesc desc;
            desc.byteOffset = offsets[i];
            desc.accessorType = info[i].dimension;
            desc.componentType = info[i].type;
            desc.normalized = IsNormalized((Attribute)i, info[i].type);

            FindMinMax(info[i], m_scratch.data(), stride, offsets[i], info.vertexCount, desc.minValues, desc.maxValues);

            descs.emplace_back(std::move(desc));
        }
    }

    std::vector<std::string> ids;
    ids.resize(descs.size());

    builder.AddAccessors(m_scratch.data(), info.vertexCount, stride, descs.data(), descs.size(), ids.data());

    int j = 0;
    FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i)
    {
        if (m_attributes.Has(i))
        {
            outIds[i] = ids[j++];
        }
    });
}

void MeshOptimizer::RemapIndices(std::unordered_map<uint32_t, uint32_t>& map, std::vector<uint32_t>& newIndices, const uint32_t* indices, size_t count)
{
    map.clear();
    newIndices.clear();

    uint32_t j = 0;
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t Index = indices[i];

        auto it = map.find(Index);
        if (it == map.end())
        {
            map.insert(std::make_pair(Index, j));
            newIndices.push_back(j++);
        }
        else
        {
            newIndices.push_back(it->second);
        }
    }
}

PrimitiveFormat MeshOptimizer::DetermineFormat(const Document& doc, const Mesh& m)
{
    auto getBufferViewId = [&](const std::string& accessorId)
    {
        if (accessorId.empty()) return std::string();
        int aid = std::stoi(accessorId);

        if (aid >= 0 && (size_t)aid < doc.accessors.Size()) return std::string();
        return doc.accessors[aid].bufferViewId;
    };

    std::string viewIds[Count];
    FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { m.primitives[0].TryGetAttributeAccessorId(s_attributeNames[i], viewIds[i]); });

    // Combined vs. separate primitives is determined by whether the vertex data is combined into a single or separate accessors.
    for (size_t i = 1; i < m.primitives.size(); ++i)
    {
        for (size_t j = Positions; j < Count; ++j)
        {
            if (viewIds[j] != getBufferViewId(m.primitives[i].GetAttributeAccessorId(s_attributeNames[j])))
            {
                return PrimitiveFormat::Separate;
            }
        }
    }

    return PrimitiveFormat::Combine;
}

bool MeshOptimizer::UsesSharedAccessors(const Mesh& m)
{
    if (m.primitives[0].indicesAccessorId.empty())
    {
        return false;
    }

    for (size_t i = 1; i < m.primitives.size(); ++i)
    {
        for (size_t j = Positions; j < Count; ++j)
        {
            std::string aid0, aid1;
            m.primitives[0].TryGetAttributeAccessorId(s_attributeNames[j], aid0);
            m.primitives[i].TryGetAttributeAccessorId(s_attributeNames[j], aid1);

            if (aid0 != aid1)
            {
                return false;
            }
        }
    }

    return true;
}

std::ostream& Microsoft::glTF::Toolkit::operator<<(std::ostream& s, const MeshOptimizer& m)
{
    for (size_t i = 0; i < m.m_primitives.size(); ++i)
    {
        s << "Primitive: " << i << std::endl;
        s << m.m_primitives[i];
    }

    PrintVec(s, m.m_indices);
    PrintVec(s, m.m_positions);
    PrintVec(s, m.m_normals);
    PrintVec(s, m.m_tangents);
    PrintVec(s, m.m_uv0);
    PrintVec(s, m.m_uv1);
    PrintVec(s, m.m_color0);
    PrintVec(s, m.m_joints0);
    PrintVec(s, m.m_weights0);

    return s;
}

void Microsoft::glTF::Toolkit::FindMinMax(const AccessorInfo& info, const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max)
{
    switch (info.type)
    {
    case COMPONENT_UNSIGNED_BYTE:  FindMinMax<uint8_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_UNSIGNED_SHORT: FindMinMax<uint16_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_UNSIGNED_INT:   FindMinMax<uint16_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_FLOAT:          FindMinMax<float>(info, src, stride, offset, count, min, max); break;
    }
}
