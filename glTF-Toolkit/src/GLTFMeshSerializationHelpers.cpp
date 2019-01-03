// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>
#include <DirectXMesh.h>

#include "GLTFMeshUtils.h"
#include "GLTFMeshSerializationHelpers.h"

#include "DeviceResources.h"

#define EPSILON 1e-6f

// Attribute loop helpers
#define FOREACH_ATTRIBUTE_SUBRANGE(start, stop, op) for (size_t _i = start; _i < stop; ++_i) { op((Attribute)_i); }
#define FOREACH_ATTRIBUTE_SETSTART(start, op) FOREACH_ATTRIBUTE_SUBRANGE(start, Count, op)
#define FOREACH_ATTRIBUTE(op) FOREACH_ATTRIBUTE_SUBRANGE(0, Count, op)

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
}


AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
    AttributeList a ={ };
    FOREACH_ATTRIBUTE([&](auto i) { a.Set(i, !(p.*s_AccessorIds[i]).empty()); });
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
PrimitiveInfo PrimitiveInfo::CreateMin(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset)
{
    static const std::pair<ComponentType, AccessorType> Types[] ={
        { GetIndexType(VertexCount), TYPE_SCALAR }, // Indices
        { COMPONENT_FLOAT,           TYPE_VEC3 },   // Position
        { COMPONENT_FLOAT,           TYPE_VEC3 },   // Normals
        { COMPONENT_FLOAT,           TYPE_VEC4 },   // Tangents
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC2 },   // UV0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC2 },   // UV1
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Color0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Joints0
        { COMPONENT_UNSIGNED_BYTE,   TYPE_VEC4 },   // Weights0
    };

    return Create(IndexCount, VertexCount, Attributes, Types, Offset);
}

// Creates a descriptor containing the maximimum precision index and vertex type.
PrimitiveInfo PrimitiveInfo::CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset)
{
    static const std::pair<ComponentType, AccessorType> Types[] ={
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

    return Create(IndexCount, VertexCount, Attributes, Types, Offset);
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
    static const char* const s_Names[Count] ={
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
        s << s_Names[i] << ": (" << p.metadata[i] << ")" << std::endl;
    }

    s << std::endl;
    return s;
}


MeshInfo::MeshInfo(void)
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
    , m_attributes{ 0 }
    , m_primFormat{ 0 }
{ }

MeshInfo::MeshInfo(const MeshInfo& parent, size_t primIndex)
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

bool MeshInfo::Initialize(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh)
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

void MeshInfo::InitSeparateAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh)
{
    for (size_t i = 0; i < mesh.primitives.size(); ++i)
    {
        const auto& p = mesh.primitives[i];
        auto& primInfo = m_primitives[i];

        uint32_t indexStart = (uint32_t)m_indices.size();
        uint32_t positionStart = (uint32_t)m_positions.size();

        ReadAccessor(reader, doc, p.indicesAccessorId, m_indices, primInfo[Indices]);
        ReadAccessor(reader, doc, p.positionsAccessorId, m_positions, primInfo[Positions]);
        ReadAccessor(reader, doc, p.normalsAccessorId, m_normals, primInfo[Normals]);
        ReadAccessor(reader, doc, p.tangentsAccessorId, m_tangents, primInfo[Tangents]);
        ReadAccessor(reader, doc, p.uv0AccessorId, m_uv0, primInfo[UV0]);
        ReadAccessor(reader, doc, p.uv1AccessorId, m_uv1, primInfo[UV1]);
        ReadAccessor(reader, doc, p.color0AccessorId, m_color0, primInfo[Color0]);
        ReadAccessor(reader, doc, p.joints0AccessorId, m_joints0, primInfo[Joints0]);
        ReadAccessor(reader, doc, p.weights0AccessorId, m_weights0, primInfo[Weights0]);

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

void MeshInfo::InitSharedAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh)
{
    const auto& p0 = mesh.primitives[0];
    auto& primInfo0 = m_primitives[0];

    // Combined meshes can only be segmented into primitives (sub-meshes) by index offsets + counts; otherwise it better be only one primitive.
    assert(mesh.primitives.size() > 1 || !p0.indicesAccessorId.empty());

    ReadAccessor(reader, doc, p0.positionsAccessorId, m_positions, primInfo0[Positions]);
    ReadAccessor(reader, doc, p0.normalsAccessorId, m_normals, primInfo0[Normals]);
    ReadAccessor(reader, doc, p0.tangentsAccessorId, m_tangents, primInfo0[Tangents]);
    ReadAccessor(reader, doc, p0.uv0AccessorId, m_uv0, primInfo0[UV0]);
    ReadAccessor(reader, doc, p0.uv1AccessorId, m_uv1, primInfo0[UV1]);
    ReadAccessor(reader, doc, p0.color0AccessorId, m_color0, primInfo0[Color0]);
    ReadAccessor(reader, doc, p0.joints0AccessorId, m_joints0, primInfo0[Joints0]);
    ReadAccessor(reader, doc, p0.weights0AccessorId, m_weights0, primInfo0[Weights0]);

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
                primInfo.CopyMeta(primInfo0);
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

void MeshInfo::Reset(void)
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

    m_attributes ={ 0 };
    m_primFormat = PrimitiveFormat::Combine;
}

void MeshInfo::Optimize(void)
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

void MeshInfo::GenerateAttributes(void)
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

void MeshInfo::Export(const MeshOptions& options, BufferBuilder& builder, Mesh& outMesh) const
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

bool MeshInfo::IsSupported(const Mesh& m)
{
    if (m.primitives.empty())
    {
        return false;
    }

    AttributeList attrs = AttributeList::FromPrimitive(m.primitives[0]);
    for (size_t i = 0; i < m.primitives.size(); ++i)
    {
        const auto& p = m.primitives[i];

        // DirectXMesh expects triangle lists - can't optimize other topologies.
        if (p.mode != MeshMode::MESH_TRIANGLES)
        {
            return false;
        }

        // Check for inconsistent index usage and/or vertex formats.
        if (attrs != AttributeList::FromPrimitive(p))
        {
            return false;
        }
    }

    return true;
}

void MeshInfo::CopyAndCleanup(const IStreamReader& reader, BufferBuilder& Builder, const GLTFDocument& oldDoc, GLTFDocument& newDoc)
{
    // Find all mesh buffers & buffer views.
    auto meshBufferViews = std::unordered_set<std::string>(oldDoc.bufferViews.Size());
    auto meshBuffers = std::unordered_set<std::string>(oldDoc.buffers.Size());

    for (const auto& m : oldDoc.meshes.Elements())
    {
        if (!MeshInfo::IsSupported(m))
        {
            continue;
        }

        for (const auto& p : m.primitives)
        {
            for (size_t i = Indices; i < Count; ++i)
            {
                auto& aid = p.*s_AccessorIds[i];
                if (aid.empty())
                {
                    continue;
                }

                // Remove all mesh accessors for new document.
                if (newDoc.accessors.Has(aid))
                {
                    newDoc.accessors.Remove(aid);
                }

                // Remove all mesh buffer views for new document.
                auto& bvid = oldDoc.accessors[aid].bufferViewId;
                if (newDoc.bufferViews.Has(bvid))
                {
                    newDoc.bufferViews.Remove(bvid);
                }
                meshBufferViews.insert(bvid);

                // Remove all mesh buffers for new document.
                auto& bid = oldDoc.bufferViews[bvid].bufferId;
                if (newDoc.buffers.Has(bid))
                {
                    newDoc.buffers.Remove(bid);
                }
                meshBuffers.insert(bid);
            }
        }
    }

    // Copy-pasta data to new buffer, replacing the old buffer views with new ones that contain proper byte offsets.
    GLTFResourceReader Reader = GLTFResourceReader(reader);
    for (auto& bv : oldDoc.bufferViews.Elements())
    {
        // Check if this is a mesh buffer view.
        if (meshBufferViews.count(bv.id) > 0)
        {
            // Disregard mesh buffer views.
            continue;
        }

        // Grab the buffer view.
        auto& obv = oldDoc.bufferViews[bv.id];

        // Check if this buffer contains mesh data.
        if (meshBuffers.count(obv.bufferId) == 0)
        {
            // The target buffer doesn't contain mesh data, so transferral is unnecessary.
            continue;
        }

        // Read from old bin file.
        std::vector<uint8_t> buffer = Reader.ReadBinaryData<uint8_t>(oldDoc, obv);
        // Write to the new bin file.
        Builder.AddBufferView(buffer, obv.byteStride, obv.target);

        // Bit of a hack: 
        // BufferView references can exist in any number of arbitrary document locations due to extensions - thus we must ensure the original ids remain intact.
        // Could do this a more legal way by having the id generator lambda function observe a global string variable, but this seemed more contained & less work.
        auto& nbv = const_cast<BufferView&>(Builder.GetCurrentBufferView());
        nbv.id = bv.id;

        // Remove the old buffer view from the new document, which will be replaced by the BufferBuilder::Output(...) call.
        newDoc.bufferViews.Remove(bv.id);
    }
}

PrimitiveInfo MeshInfo::DetermineMeshFormat(void) const
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

void MeshInfo::WriteVertices(const PrimitiveInfo& info, std::vector<uint8_t>& output) const
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

void MeshInfo::ReadVertices(const PrimitiveInfo& info, const std::vector<uint8_t>& input)
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
void MeshInfo::ExportCSI(BufferBuilder& builder, Mesh& outMesh) const
{
    const auto PrimInfo = DetermineMeshFormat();

    ExportSharedView(builder, PrimInfo, Indices, &MeshInfo::m_indices, outMesh);

    std::string ids[Count];
    ids[Positions] = ExportAccessor(builder, PrimInfo, Positions, &MeshInfo::m_positions);
    ids[Normals]   = ExportAccessor(builder, PrimInfo, Normals, &MeshInfo::m_normals);
    ids[Tangents]  = ExportAccessor(builder, PrimInfo, Tangents, &MeshInfo::m_tangents);
    ids[UV0]       = ExportAccessor(builder, PrimInfo, UV0, &MeshInfo::m_uv0);
    ids[UV1]       = ExportAccessor(builder, PrimInfo, UV1, &MeshInfo::m_uv1);
    ids[Color0]    = ExportAccessor(builder, PrimInfo, Color0, &MeshInfo::m_color0);
    ids[Joints0]   = ExportAccessor(builder, PrimInfo, Joints0, &MeshInfo::m_joints0);
    ids[Weights0]  = ExportAccessor(builder, PrimInfo, Weights0, &MeshInfo::m_weights0);

    // Push the accessor ids to the output GLTF mesh primitives (all share the same accessors.)
    for (auto& x : outMesh.primitives)
    {
        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { x.*s_AccessorIds[i] = ids[i]; })
    }
}

// Combine primitives, separate attributes, non-indexed
void MeshInfo::ExportCS(BufferBuilder& builder, Mesh& outMesh) const
{
    const auto primInfo = DetermineMeshFormat();

    ExportSharedView(builder, primInfo, Positions, &MeshInfo::m_positions, outMesh);
    ExportSharedView(builder, primInfo, Normals, &MeshInfo::m_normals, outMesh);
    ExportSharedView(builder, primInfo, Tangents, &MeshInfo::m_tangents, outMesh);
    ExportSharedView(builder, primInfo, UV0, &MeshInfo::m_uv0, outMesh);
    ExportSharedView(builder, primInfo, UV1, &MeshInfo::m_uv1, outMesh);
    ExportSharedView(builder, primInfo, Color0, &MeshInfo::m_color0, outMesh);
    ExportSharedView(builder, primInfo, Joints0, &MeshInfo::m_joints0, outMesh);
    ExportSharedView(builder, primInfo, Weights0, &MeshInfo::m_weights0, outMesh);
}

// Combine primitives, interleave attributes
void MeshInfo::ExportCI(BufferBuilder& builder, Mesh& outMesh) const
{
    // Can't write a non-indexed combined mesh with multiple primitives.
    if (!m_attributes.Has(Indices) && m_primitives.size() > 1)
    {
        ExportSI(builder, outMesh);
    }

    auto primInfo = DetermineMeshFormat();
    ExportSharedView(builder, primInfo, Indices, &MeshInfo::m_indices, outMesh);

    std::string ids[Count];
    ExportInterleaved(builder, primInfo, ids);

    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
        {
            outMesh.primitives[i].*s_AccessorIds[j] = ids[j];
        });
    }
}

// Separate primitives, separate attributes
void MeshInfo::ExportSS(BufferBuilder& Builder, Mesh& OutMesh) const
{
    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        MeshInfo prim = MeshInfo(*this, i);

        OutMesh.primitives[i].indicesAccessorId   = prim.ExportAccessor(Builder, m_primitives[i], Indices, &MeshInfo::m_indices);
        OutMesh.primitives[i].positionsAccessorId = prim.ExportAccessor(Builder, m_primitives[i], Positions, &MeshInfo::m_positions);
        OutMesh.primitives[i].normalsAccessorId   = prim.ExportAccessor(Builder, m_primitives[i], Normals, &MeshInfo::m_normals);
        OutMesh.primitives[i].tangentsAccessorId  = prim.ExportAccessor(Builder, m_primitives[i], Tangents, &MeshInfo::m_tangents);
        OutMesh.primitives[i].uv0AccessorId       = prim.ExportAccessor(Builder, m_primitives[i], UV0, &MeshInfo::m_uv0);
        OutMesh.primitives[i].uv1AccessorId       = prim.ExportAccessor(Builder, m_primitives[i], UV1, &MeshInfo::m_uv1);
        OutMesh.primitives[i].color0AccessorId    = prim.ExportAccessor(Builder, m_primitives[i], Color0, &MeshInfo::m_color0);
        OutMesh.primitives[i].joints0AccessorId   = prim.ExportAccessor(Builder, m_primitives[i], Joints0, &MeshInfo::m_joints0);
        OutMesh.primitives[i].weights0AccessorId  = prim.ExportAccessor(Builder, m_primitives[i], Weights0, &MeshInfo::m_weights0);
    }
}

// Separate primitives, interleave attributes
void MeshInfo::ExportSI(BufferBuilder& Builder, Mesh& OutMesh) const
{
    for (size_t i = 0; i < m_primitives.size(); ++i)
    {
        MeshInfo prim = MeshInfo(*this, i);

        OutMesh.primitives[i].indicesAccessorId = prim.ExportAccessor(Builder, m_primitives[i], Indices, &MeshInfo::m_indices);

        std::string ids[Count];
        prim.ExportInterleaved(Builder, m_primitives[i], ids);

        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
        {
            OutMesh.primitives[i].*s_AccessorIds[j] = ids[j];
        });
    }
}

void MeshInfo::ExportInterleaved(BufferBuilder& builder, const PrimitiveInfo& info, std::string(&outIds)[Count]) const
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
            FindMinMax(info[i], m_scratch.data(), stride, offsets[i], info.vertexCount, desc.minValues, desc.maxValues);

            descs.emplace_back(std::move(desc));
        }
    }

    std::vector<std::string> ids;
    ids.resize(descs.size());

    builder.AddAccessors(m_scratch.data(), info.vertexCount, stride, descs.data(), descs.size(), ids.data());

    for (size_t i = Positions, j = 0; i < Count; ++i)
    {
        if (m_attributes.Has((Attribute)i))
        {
            outIds[i] = ids[j++];
        }
    }
}

void MeshInfo::RemapIndices(std::unordered_map<uint32_t, uint32_t>& map, std::vector<uint32_t>& newIndices, const uint32_t* indices, size_t count)
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

PrimitiveFormat MeshInfo::DetermineFormat(const GLTFDocument& Doc, const Mesh& m)
{
    auto getBufferViewId = [&](const std::string& AccessorId)
    {
        if (AccessorId.empty()) return std::string();
        int aid = std::stoi(AccessorId);

        if (aid >= 0 && (size_t)aid < Doc.accessors.Size()) return std::string();
        return Doc.accessors[aid].bufferViewId;
    };

    std::string viewIds[Count];
    FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { viewIds[i] = getBufferViewId(m.primitives[0].*s_AccessorIds[i]); });

    // Combined vs. separate primitives is determined by whether the vertex data is combined into a single or separate accessors.
    for (size_t i = 1; i < m.primitives.size(); ++i)
    {
        for (size_t j = Positions; j < Count; ++j)
        {
            if (viewIds[j] != getBufferViewId(m.primitives[i].*s_AccessorIds[j]))
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
            if (m.primitives[0].*s_AccessorIds[j] != m.primitives[i].*s_AccessorIds[j])
            {
                return false;
            }
        }
    }

    return true;
}

std::ostream& Microsoft::glTF::Toolkit::operator<<(std::ostream& s, const MeshInfo& m)
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
