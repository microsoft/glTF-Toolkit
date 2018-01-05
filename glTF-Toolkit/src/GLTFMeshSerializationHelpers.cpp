// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <numeric>
#include <DirectXMesh.h>
#include <rapidjson/writer.h>

#include "DirectXMathUtils.h"
#include "GLTFMeshUtils.h"
#include "GLTFMeshSerializationHelpers.h"

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

    std::string(MeshPrimitive::*s_AccessorIds[Count]) =
    {
        &MeshPrimitive::indicesAccessorId,		// 0 Indices
        &MeshPrimitive::positionsAccessorId,	// 1 Positions
        &MeshPrimitive::normalsAccessorId,		// 2 Normals
        &MeshPrimitive::tangentsAccessorId,		// 3 Tangents
        &MeshPrimitive::uv0AccessorId,			// 4 UV0
        &MeshPrimitive::uv1AccessorId,			// 5 UV1
        &MeshPrimitive::color0AccessorId,		// 6 Color0
        &MeshPrimitive::joints0AccessorId,		// 7 Joints0
        &MeshPrimitive::weights0AccessorId,		// 8 Weights0
    };
}


AttributeList AttributeList::FromPrimitive(const MeshPrimitive& p)
{
    AttributeList a ={ 0 };
    FOREACH_ATTRIBUTE([&](auto i) { a.Set(i, !(p.*s_AccessorIds[i]).empty()); });
    return a;
}

bool AccessorInfo::IsValid(void) const
{
    const uint8_t zeros[sizeof(AccessorInfo)] ={ 0 };
    return std::memcmp(this, zeros, sizeof(AccessorInfo)) != 0;
}

size_t AccessorInfo::GetElementSize(void) const 
{ 
    return Accessor::GetComponentTypeSize(Type) * Accessor::GetTypeCount(Dimension); 
}

AccessorInfo AccessorInfo::Invalid(void)
{
    AccessorInfo info;
    info.Type		= COMPONENT_UNKNOWN;
    info.Dimension	= TYPE_UNKNOWN;
    info.Target		= UNKNOWN_BUFFER;
    return info;
}

AccessorInfo AccessorInfo::Create(ComponentType cType, AccessorType aType, BufferViewTarget target)
{
    AccessorInfo info;
    info.Type		= cType;
    info.Dimension	= aType;
    info.Target		= target;
    return info;
}

AccessorInfo AccessorInfo::Max(const AccessorInfo& a0, const AccessorInfo& a1)
{
    AccessorInfo maxInfo;
    maxInfo.Target		= a0.Target;
    maxInfo.Type		= std::max(a0.Type, a1.Type);
    maxInfo.Dimension	= std::max(a0.Dimension, a1.Dimension);
    return maxInfo;
}

std::ostream& Microsoft::glTF::Toolkit::operator<<(std::ostream& s, const AccessorInfo& a)
{
    static const std::unordered_map<ComponentType, const char*> cMap ={
        { COMPONENT_UNKNOWN, "Unknown" },
        { COMPONENT_BYTE, "Byte" },
        { COMPONENT_UNSIGNED_BYTE, "UByte" },
        { COMPONENT_SHORT, "Short" },
        { COMPONENT_UNSIGNED_SHORT, "UShort" },
        { COMPONENT_UNSIGNED_INT, "UInt" },
        { COMPONENT_FLOAT, "Float" },
    };
    static const std::unordered_map<AccessorType, const char*> aMap ={
        { TYPE_UNKNOWN, "Unknown" },
        { TYPE_SCALAR, "Scalar" },
        { TYPE_VEC2, "Vec2" },
        { TYPE_VEC3, "Vec3" },
        { TYPE_VEC4, "Vec4" },
        { TYPE_MAT2, "Mat2" },
        { TYPE_MAT3, "Mat3" },
        { TYPE_MAT4, "Mat4" },
    };
    static const std::unordered_map<BufferViewTarget, const char*> bMap ={
        { UNKNOWN_BUFFER, "Unknown" },
        { ELEMENT_ARRAY_BUFFER, "Index" },
        { ARRAY_BUFFER, "Vertex" },
    };

    s << "Type: " << cMap.at(a.Type) << ", Count: " << aMap.at(a.Dimension) << ", Target: " << bMap.at(a.Target);
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
        if (Metadata[i].IsValid())
        {
            const size_t CompSize = Accessor::GetComponentTypeSize(Metadata[i].Type);

            // Pad to the component type's size.
            // TODO: Implement more complicated packing mechanism.
            if (stride % CompSize != 0)
            {
                stride += CompSize - stride % CompSize;
            }

            maxCompSize = std::max(maxCompSize, CompSize);
            offsets[i] = stride;
            stride += CompSize * Accessor::GetTypeCount(Metadata[i].Dimension);
        }
    }

    if (pOutAlignment)
    {
        *pOutAlignment = maxCompSize;
    }
}

void PrimitiveInfo::CopyMeta(const PrimitiveInfo& info)
{
    std::copy(info.Metadata, info.Metadata + Count, Metadata);
    Metadata[Indices].Type = GetIndexType(VertexCount);
}

PrimitiveInfo PrimitiveInfo::Create(size_t indexCount, size_t vertexCount, AttributeList attributes, const std::pair<ComponentType, AccessorType>(&types)[Count], size_t offset)
{
    PrimitiveInfo info ={ 0 };
    info.Offset			= offset;
    info.IndexCount		= indexCount;
    info.VertexCount	= vertexCount;
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
        { GetIndexType(VertexCount),TYPE_SCALAR },	// Indices
        { COMPONENT_FLOAT,			TYPE_VEC3 },	// Position
        { COMPONENT_FLOAT,			TYPE_VEC3 },	// Normals
        { COMPONENT_FLOAT,			TYPE_VEC4 },	// Tangents
        { COMPONENT_UNSIGNED_BYTE,	TYPE_VEC2 },	// UV0
        { COMPONENT_UNSIGNED_BYTE,	TYPE_VEC2 },	// UV1
        { COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Color0
        { COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Joints0
        { COMPONENT_UNSIGNED_BYTE,	TYPE_VEC4 },	// Weights0
    };

    return Create(IndexCount, VertexCount, Attributes, Types, Offset);
}

// Creates a descriptor containing the maximimum precision index and vertex type.
PrimitiveInfo PrimitiveInfo::CreateMax(size_t IndexCount, size_t VertexCount, AttributeList Attributes, size_t Offset)
{
    static const std::pair<ComponentType, AccessorType> Types[] ={
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

    return Create(IndexCount, VertexCount, Attributes, Types, Offset);
}

PrimitiveInfo PrimitiveInfo::Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1)
{
    PrimitiveInfo maxInfo;
    maxInfo.IndexCount	= p0.IndexCount;
    maxInfo.VertexCount = p0.VertexCount;
    maxInfo.Offset		= p0.Offset;

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

    s << "Offset: " << p.Offset << std::endl;
    s << "IndexCount: " << p.IndexCount << std::endl;
    s << "VertexCount: " << p.VertexCount << std::endl;

    for (size_t i = 0; i < Count; ++i)
    {
        s << s_Names[i] << ": (" << p.Metadata[i] << ")" << std::endl;
    }

    s << std::endl;
    return s;
}


MeshInfo::MeshInfo(void)
    : m_Name()
    , m_Primitives()
    , m_Indices()
    , m_Positions()
    , m_Normals()
    , m_Tangents()
    , m_UV0()
    , m_UV1()
    , m_Color0()
    , m_Joints0()
    , m_Weights0()
    , m_Attributes{ 0 }
    , m_PrimFormat{ 0 }
{ }

MeshInfo::MeshInfo(const MeshInfo& parent, size_t primIndex)
    : m_Name()
    , m_Primitives()
    , m_Indices()
    , m_Positions()
    , m_Normals()
    , m_Tangents()
    , m_UV0()
    , m_UV1()
    , m_Color0()
    , m_Joints0()
    , m_Weights0()
    , m_Attributes(parent.m_Attributes)
    , m_PrimFormat(parent.m_PrimFormat)
{
    m_Attributes = parent.m_Attributes;

    const auto& prim = parent.m_Primitives[primIndex];

    if (m_Attributes.Has(Attribute::Indices))
    {
        std::unordered_map<uint32_t, uint32_t> indexRemap;
        RemapIndices(indexRemap, m_Indices, &parent.m_Indices[prim.Offset], prim.IndexCount);

        auto remapFunc = [&](uint32_t i) { return indexRemap[i]; };

        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Positions, m_Positions);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Normals, m_Normals);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Tangents, m_Tangents);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_UV0, m_UV0);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_UV1, m_UV1);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Color0, m_Color0);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Joints0, m_Joints0);
        LocalizeAttribute(prim, remapFunc, parent.m_Indices, parent.m_Weights0, m_Weights0);
    }
    else
    {
        m_Positions.assign(&parent.m_Positions[prim.Offset], &parent.m_Positions[prim.Offset + prim.VertexCount]);
        m_Normals.assign(&parent.m_Normals[prim.Offset], &parent.m_Normals[prim.Offset + prim.VertexCount]);
        m_Tangents.assign(&parent.m_Tangents[prim.Offset], &parent.m_Tangents[prim.Offset + prim.VertexCount]);
        m_UV0.assign(&parent.m_UV0[prim.Offset], &parent.m_UV0[prim.Offset + prim.VertexCount]);
        m_UV1.assign(&parent.m_UV1[prim.Offset], &parent.m_UV1[prim.Offset + prim.VertexCount]);
        m_Color0.assign(&parent.m_Color0[prim.Offset], &parent.m_Color0[prim.Offset + prim.VertexCount]);
        m_Joints0.assign(&parent.m_Joints0[prim.Offset], &parent.m_Joints0[prim.Offset + prim.VertexCount]);
        m_Weights0.assign(&parent.m_Weights0[prim.Offset], &parent.m_Weights0[prim.Offset + prim.VertexCount]);
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
    m_Primitives.resize(mesh.primitives.size());

    if (UsesSharedAccessors(mesh))
    {
        InitSharedAccessors(reader, doc, mesh);
    }
    else
    {
        InitSeparateAccessors(reader, doc, mesh);
    }

    if (m_Positions.empty())
    {
        Reset();
        return false;
    }

    m_Name = mesh.name;
    m_Attributes = AttributeList::FromPrimitive(mesh.primitives[0]);
    m_PrimFormat = DetermineFormat(doc, mesh);

    return true;
}

void MeshInfo::InitSeparateAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh)
{
    for (size_t i = 0; i < mesh.primitives.size(); ++i)
    {
        const auto& p = mesh.primitives[i];
        auto& primInfo = m_Primitives[i];

        uint32_t indexStart = (uint32_t)m_Indices.size();
        uint32_t positionStart = (uint32_t)m_Positions.size();

        ReadAccessor(reader, doc, p.indicesAccessorId, m_Indices, primInfo[Indices]);
        ReadAccessor(reader, doc, p.positionsAccessorId, m_Positions, primInfo[Positions]);
        ReadAccessor(reader, doc, p.normalsAccessorId, m_Normals, primInfo[Normals]);
        ReadAccessor(reader, doc, p.tangentsAccessorId, m_Tangents, primInfo[Tangents]);
        ReadAccessor(reader, doc, p.uv0AccessorId, m_UV0, primInfo[UV0]);
        ReadAccessor(reader, doc, p.uv1AccessorId, m_UV1, primInfo[UV1]);
        ReadAccessor(reader, doc, p.color0AccessorId, m_Color0, primInfo[Color0]);
        ReadAccessor(reader, doc, p.joints0AccessorId, m_Joints0, primInfo[Joints0]);
        ReadAccessor(reader, doc, p.weights0AccessorId, m_Weights0, primInfo[Weights0]);

        primInfo.Offset = m_Indices.size() > 0 ? indexStart : positionStart;
        primInfo.IndexCount = m_Indices.size() - indexStart;
        primInfo.VertexCount = m_Positions.size() - positionStart;

        // Conversion from local to global index buffer; add vertex offset to each index.
        if (positionStart > 0)
        {
            std::for_each(m_Indices.begin() + indexStart, m_Indices.end(), [=](auto& v) { v = v + positionStart; });
        }
    }
}

void MeshInfo::InitSharedAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh)
{
    const auto& p0 = mesh.primitives[0];
    auto& primInfo0 = m_Primitives[0];

    // Combined meshes can only be segmented into primitives (sub-meshes) by index offsets + counts; otherwise it better be only one primitive.
    assert(mesh.primitives.size() > 1 || !p0.indicesAccessorId.empty());

    ReadAccessor(reader, doc, p0.positionsAccessorId, m_Positions, primInfo0[Positions]);
    ReadAccessor(reader, doc, p0.normalsAccessorId, m_Normals, primInfo0[Normals]);
    ReadAccessor(reader, doc, p0.tangentsAccessorId, m_Tangents, primInfo0[Tangents]);
    ReadAccessor(reader, doc, p0.uv0AccessorId, m_UV0, primInfo0[UV0]);
    ReadAccessor(reader, doc, p0.uv1AccessorId, m_UV1, primInfo0[UV1]);
    ReadAccessor(reader, doc, p0.color0AccessorId, m_Color0, primInfo0[Color0]);
    ReadAccessor(reader, doc, p0.joints0AccessorId, m_Joints0, primInfo0[Joints0]);
    ReadAccessor(reader, doc, p0.weights0AccessorId, m_Weights0, primInfo0[Weights0]);

    // If there are indices, grab the vertex count for each primitive by determining the number of unique indices in its index set.
    if (!p0.indicesAccessorId.empty())
    {
        if (mesh.primitives.size() == 1)
        {
            ReadAccessor(reader, doc, p0.indicesAccessorId, m_Indices, primInfo0[Indices]);

            primInfo0.Offset = 0;
            primInfo0.IndexCount = m_Indices.size();
            primInfo0.VertexCount = m_Positions.size();
        }
        else
        {
            // Use the uniqueness count to determine number of vertices for each primitive.
            std::unordered_set<uint32_t> UniqueVertices;

            for (size_t i = 0; i < mesh.primitives.size(); ++i)
            {
                const auto& p = mesh.primitives[i];
                auto& primInfo = m_Primitives[i];

                uint32_t IndexStart = (uint32_t)m_Indices.size();
                ReadAccessor(reader, doc, p.indicesAccessorId, m_Indices, primInfo[Indices]);

                // Generate the unique vertex set.
                UniqueVertices.clear();
                UniqueVertices.insert(m_Indices.begin() + IndexStart, m_Indices.end());

                primInfo.Offset = IndexStart;
                primInfo.IndexCount = m_Indices.size() - IndexStart;
                primInfo.VertexCount = UniqueVertices.size(); 
                primInfo.CopyMeta(primInfo0);
            }
        }
    }
    else
    {
        primInfo0.Offset = 0;
        primInfo0.IndexCount = 0;
        primInfo0.VertexCount = m_Positions.size();
    }
}

void MeshInfo::Reset(void)
{
    m_Name.clear();
    m_Primitives.clear();

    m_Indices.clear();
    m_Positions.clear();
    m_Normals.clear();
    m_Tangents.clear();
    m_UV0.clear();
    m_UV1.clear();
    m_Color0.clear();
    m_Joints0.clear();
    m_Weights0.clear();

    m_Attributes ={ 0 };
    m_PrimFormat = PrimitiveFormat::Combine;
}

void MeshInfo::Optimize(void)
{
    if (!m_Attributes.Has(Indices))
    {
        std::cout << "Mesh '" << m_Name << "': optimize operation failed - this operation requires mesh to use indices.";
        return;
    }

    // DirectXMesh intermediate data
    std::vector<uint32_t> facePrims; // Mapping from face index to primitive index.
    std::vector<uint32_t> pointReps;
    std::vector<uint32_t> adjacency;
    std::vector<uint32_t> dupVerts;
    std::vector<uint32_t> faceRemap;
    std::vector<uint32_t> vertRemap;

    const size_t indexCount = m_Indices.size();
    const size_t vertexCount = m_Positions.size();
    const size_t faceCount = GetFaceCount();

    for (size_t i = 0; i < m_Primitives.size(); ++i)
    {
        // Populate attribute id array with primitive index denoting this is a distinct sub-mesh.
        size_t oldFaceCount = facePrims.size();
        facePrims.resize(oldFaceCount + m_Primitives[i].FaceCount());
        std::fill(facePrims.begin() + oldFaceCount, facePrims.end(), (uint32_t)i);
    }

    // Ensure intermediate buffer sizes.
    pointReps.resize(vertexCount);
    adjacency.resize(GetFaceCount() * 3);
    faceRemap.resize(facePrims.size());
    vertRemap.resize(vertexCount);

    // Perform DirectXMesh optimizations
    DirectX::GenerateAdjacencyAndPointReps(m_Indices.data(), faceCount, m_Positions.data(), vertexCount, EPSILON, pointReps.data(), adjacency.data());
    DirectX::Clean(m_Indices.data(), faceCount, vertexCount, adjacency.data(), facePrims.data(), dupVerts);
    DirectX::AttributeSort(faceCount, facePrims.data(), faceRemap.data());
    DirectX::ReorderIBAndAdjacency(m_Indices.data(), faceCount, adjacency.data(), faceRemap.data());
    DirectX::OptimizeFacesEx(m_Indices.data(), faceCount, adjacency.data(), facePrims.data(), faceRemap.data());
    DirectX::ReorderIB(m_Indices.data(), faceCount, faceRemap.data());
    DirectX::OptimizeVertices(m_Indices.data(), faceCount, vertexCount, vertRemap.data());
    DirectX::FinalizeIB(m_Indices.data(), faceCount, vertRemap.data(), vertexCount);

    auto info = PrimitiveInfo::CreateMax(indexCount, vertexCount, m_Attributes);

    WriteVertices(info, m_Scratch);
    DirectX::FinalizeVBAndPointReps(m_Scratch.data(), info.GetVertexSize(), vertexCount, pointReps.data(), vertRemap.data());
    ReadVertices(info, m_Scratch);

    for (size_t i = 0; i < m_Primitives.size(); ++i)
    {
        auto it = std::find(facePrims.begin(), facePrims.end(), (uint32_t)i);
        assert(it != facePrims.end());

        m_Primitives[i].Offset = size_t(it - facePrims.begin()) * 3;
    }
}

void MeshInfo::GenerateAttributes(void)
{
    if (!m_Attributes.Has(Indices))
    {
        std::cout << "Mesh '" << m_Name << "': normal/tangent generation operation failed - this operation requires mesh to use indices.";
        return;
    }

    const size_t indexCount = m_Indices.size();
    const size_t vertexCount = m_Positions.size();
    const size_t faceCount = GetFaceCount();

    // Generate normals if not present.
    if (m_Normals.empty())
    {
        m_Attributes.Add(Normals);
        std::for_each(m_Primitives.begin(), m_Primitives.end(), [](auto& p) { p[Normals] = AccessorInfo::Create(COMPONENT_FLOAT, TYPE_VEC3, ARRAY_BUFFER); });


        m_Normals.resize(vertexCount);
        DirectX::ComputeNormals(m_Indices.data(), faceCount, m_Positions.data(), vertexCount, CNORM_DEFAULT, m_Normals.data());

        // Prompt recompute of tangents if they were supplied (however unlikely if normals weren't supplied.)
        m_Tangents.clear();
        m_Attributes.Remove(Tangents);
    }

    // Generate tangents if not present. Requires a UV set.
    if (m_Tangents.empty() && !m_UV0.empty())
    {
        m_Attributes.Add(Tangents);
        std::for_each(m_Primitives.begin(), m_Primitives.end(), [](auto& p) { p[Tangents] = AccessorInfo::Create(COMPONENT_FLOAT, TYPE_VEC4, ARRAY_BUFFER); });

        m_Tangents.resize(vertexCount);
        DirectX::ComputeTangentFrame(m_Indices.data(), faceCount, m_Positions.data(), m_Normals.data(), m_UV0.data(), vertexCount, m_Tangents.data());
    }
}

void MeshInfo::Export(const MeshOptions& options, BufferBuilder& builder, Mesh& outMesh) const
{
    auto primFormat = options.PrimitiveFormat == PrimitiveFormat::Preserved ? m_PrimFormat : options.PrimitiveFormat;

    if (primFormat == PrimitiveFormat::Combine)
    {
        if (options.AttributeFormat == AttributeFormat::Interleave)
        {
            ExportCI(builder, outMesh);
        }
        else
        {
            if (m_Indices.empty())
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
    if (m_Primitives.empty())
    {
        return PrimitiveInfo{};
    }

    // Start at most compressed vertex attribute formats.
    PrimitiveInfo maxInfo = PrimitiveInfo::CreateMin(m_Indices.size(), m_Positions.size(), m_Attributes);

    // Accumulate the minimum compression capability of each primitive to determine our overall vertex format.
    for (size_t i = 0; i < m_Primitives.size(); ++i)
    {
        maxInfo = PrimitiveInfo::Max(maxInfo, m_Primitives[i]);
    }

    return maxInfo;
}

void MeshInfo::WriteVertices(const PrimitiveInfo& info, std::vector<uint8_t>& output) const
{
    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets);

    output.resize(info.VertexCount * stride);

    Write(info[Positions], output.data(), stride, offsets[Positions], m_Positions.data(), m_Positions.size());
    Write(info[Normals], output.data(), stride, offsets[Normals], m_Normals.data(), m_Normals.size());
    Write(info[Tangents], output.data(), stride, offsets[Tangents], m_Tangents.data(), m_Tangents.size());
    Write(info[UV0], output.data(), stride, offsets[UV0], m_UV0.data(), m_UV0.size());
    Write(info[UV1], output.data(), stride, offsets[UV1], m_UV1.data(), m_UV1.size());
    Write(info[Color0], output.data(), stride, offsets[Color0], m_Color0.data(), m_Color0.size());
    Write(info[Joints0], output.data(), stride, offsets[Joints0], m_Joints0.data(), m_Joints0.size());
    Write(info[Weights0], output.data(), stride, offsets[Weights0], m_Weights0.data(), m_Weights0.size());
}

void MeshInfo::ReadVertices(const PrimitiveInfo& info, const std::vector<uint8_t>& input)
{
    if (m_Attributes.Has(Positions)) m_Positions.resize(info.VertexCount);
    if (m_Attributes.Has(Normals)) m_Normals.resize(info.VertexCount);
    if (m_Attributes.Has(Tangents)) m_Tangents.resize(info.VertexCount);
    if (m_Attributes.Has(UV0)) m_UV0.resize(info.VertexCount);
    if (m_Attributes.Has(UV1)) m_UV1.resize(info.VertexCount);
    if (m_Attributes.Has(Color0)) m_Color0.resize(info.VertexCount);
    if (m_Attributes.Has(Joints0)) m_Joints0.resize(info.VertexCount);
    if (m_Attributes.Has(Weights0)) m_Weights0.resize(info.VertexCount);

    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets);

    Read(info[Positions], m_Positions.data(), input.data(), stride, offsets[Positions], info.VertexCount);
    Read(info[Normals], m_Normals.data(), input.data(), stride, offsets[Normals], info.VertexCount);
    Read(info[Tangents], m_Tangents.data(), input.data(), stride, offsets[Tangents], info.VertexCount);
    Read(info[UV0], m_UV0.data(), input.data(), stride, offsets[UV0], info.VertexCount);
    Read(info[UV1], m_UV1.data(), input.data(), stride, offsets[UV1], info.VertexCount);
    Read(info[Color0], m_Color0.data(), input.data(), stride, offsets[Color0], info.VertexCount);
    Read(info[Joints0], m_Joints0.data(), input.data(), stride, offsets[Joints0], info.VertexCount);
    Read(info[Weights0], m_Weights0.data(), input.data(), stride, offsets[Weights0], info.VertexCount);
}

// Combine primitives, separate attributes, indexed
void MeshInfo::ExportCSI(BufferBuilder& builder, Mesh& outMesh) const
{
    const auto PrimInfo = DetermineMeshFormat();

    ExportSharedView(builder, PrimInfo, Indices, &MeshInfo::m_Indices, outMesh);

    std::string ids[Count];
    ids[Positions]	= ExportAccessor(builder, PrimInfo, Positions, &MeshInfo::m_Positions);
    ids[Normals]	= ExportAccessor(builder, PrimInfo, Normals, &MeshInfo::m_Normals);
    ids[Tangents]	= ExportAccessor(builder, PrimInfo, Tangents, &MeshInfo::m_Tangents);
    ids[UV0]		= ExportAccessor(builder, PrimInfo, UV0, &MeshInfo::m_UV0);
    ids[UV1]		= ExportAccessor(builder, PrimInfo, UV1, &MeshInfo::m_UV1);
    ids[Color0]		= ExportAccessor(builder, PrimInfo, Color0, &MeshInfo::m_Color0);
    ids[Joints0]	= ExportAccessor(builder, PrimInfo, Joints0, &MeshInfo::m_Joints0);
    ids[Weights0]	= ExportAccessor(builder, PrimInfo, Weights0, &MeshInfo::m_Weights0);

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

    ExportSharedView(builder, primInfo, Positions, &MeshInfo::m_Positions, outMesh);
    ExportSharedView(builder, primInfo, Normals, &MeshInfo::m_Normals, outMesh);
    ExportSharedView(builder, primInfo, Tangents, &MeshInfo::m_Tangents, outMesh);
    ExportSharedView(builder, primInfo, UV0, &MeshInfo::m_UV0, outMesh);
    ExportSharedView(builder, primInfo, UV1, &MeshInfo::m_UV1, outMesh);
    ExportSharedView(builder, primInfo, Color0, &MeshInfo::m_Color0, outMesh);
    ExportSharedView(builder, primInfo, Joints0, &MeshInfo::m_Joints0, outMesh);
    ExportSharedView(builder, primInfo, Weights0, &MeshInfo::m_Weights0, outMesh);
}

// Combine primitives, interleave attributes
void MeshInfo::ExportCI(BufferBuilder& builder, Mesh& outMesh) const
{
    // Can't write a non-indexed combined mesh with multiple primitives.
    if (!m_Attributes.Has(Indices) && m_Primitives.size() > 1)
    {
        ExportSI(builder, outMesh);
    }

    auto primInfo = DetermineMeshFormat();
    ExportSharedView(builder, primInfo, Indices, &MeshInfo::m_Indices, outMesh);

    std::string ids[Count];
    ExportInterleaved(builder, primInfo, ids);

    for (size_t i = 0; i < m_Primitives.size(); ++i)
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
    for (size_t i = 0; i < m_Primitives.size(); ++i)
    {
        MeshInfo prim = MeshInfo(*this, i);

        OutMesh.primitives[i].indicesAccessorId		= prim.ExportAccessor(Builder, m_Primitives[i], Indices, &MeshInfo::m_Indices);
        OutMesh.primitives[i].positionsAccessorId	= prim.ExportAccessor(Builder, m_Primitives[i], Positions, &MeshInfo::m_Positions);
        OutMesh.primitives[i].normalsAccessorId		= prim.ExportAccessor(Builder, m_Primitives[i], Normals, &MeshInfo::m_Normals);
        OutMesh.primitives[i].tangentsAccessorId	= prim.ExportAccessor(Builder, m_Primitives[i], Tangents, &MeshInfo::m_Tangents);
        OutMesh.primitives[i].uv0AccessorId			= prim.ExportAccessor(Builder, m_Primitives[i], UV0, &MeshInfo::m_UV0);
        OutMesh.primitives[i].uv1AccessorId			= prim.ExportAccessor(Builder, m_Primitives[i], UV1, &MeshInfo::m_UV1);
        OutMesh.primitives[i].color0AccessorId		= prim.ExportAccessor(Builder, m_Primitives[i], Color0, &MeshInfo::m_Color0);
        OutMesh.primitives[i].joints0AccessorId		= prim.ExportAccessor(Builder, m_Primitives[i], Joints0, &MeshInfo::m_Joints0);
        OutMesh.primitives[i].weights0AccessorId	= prim.ExportAccessor(Builder, m_Primitives[i], Weights0, &MeshInfo::m_Weights0);
    }
}

// Separate primitives, interleave attributes
void MeshInfo::ExportSI(BufferBuilder& Builder, Mesh& OutMesh) const
{
    for (size_t i = 0; i < m_Primitives.size(); ++i)
    {
        MeshInfo prim = MeshInfo(*this, i);

        OutMesh.primitives[i].indicesAccessorId = prim.ExportAccessor(Builder, m_Primitives[i], Indices, &MeshInfo::m_Indices);

        std::string ids[Count];
        prim.ExportInterleaved(Builder, m_Primitives[i], ids);

        FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto j)
        {
            OutMesh.primitives[i].*s_AccessorIds[j] = ids[j];
        });
    }
}

void MeshInfo::ExportInterleaved(BufferBuilder& builder, const PrimitiveInfo& info, std::string(&outIds)[Count]) const
{
    WriteVertices(info, m_Scratch);

    size_t alignment = 1;
    size_t stride;
    size_t offsets[Count];
    info.GetVertexInfo(stride, offsets, &alignment);

    builder.AddBufferView(ARRAY_BUFFER);

    AccessorDesc Descs[Count] ={ };
    for (size_t i = Positions; i < Count; ++i)
    {
        if (m_Attributes.Has((Attribute)i))
        {
            Descs[i].count = info.VertexCount;
            Descs[i].byteOffset = offsets[i];
            Descs[i].accessorType = info[i].Dimension;
            Descs[i].componentType = info[i].Type;

            FindMinMax(info[i], m_Scratch.data(), stride, offsets[i], info.VertexCount, Descs[i].minValues, Descs[i].maxValues);
        }
    }

    builder.AddAccessors(m_Scratch.data(), stride, Descs, ArrayCount(Descs), outIds);
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
    FOREACH_ATTRIBUTE_SETSTART(Positions, [&](auto i) { viewIds[i] = getBufferViewId(m.primitives[0].*s_AccessorIds[i]); })

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
    for (size_t i = 0; i < m.m_Primitives.size(); ++i)
    {
        s << "Primitive: " << i << std::endl;
        s << m.m_Primitives[i];
    }

    PrintVec(s, m.m_Indices);
    PrintVec(s, m.m_Positions);
    PrintVec(s, m.m_Normals);
    PrintVec(s, m.m_Tangents);
    PrintVec(s, m.m_UV0);
    PrintVec(s, m.m_UV1);
    PrintVec(s, m.m_Color0);
    PrintVec(s, m.m_Joints0);
    PrintVec(s, m.m_Weights0);

    return s;
}

void Microsoft::glTF::Toolkit::FindMinMax(const AccessorInfo& info, const uint8_t* src, size_t stride, size_t offset, size_t count, std::vector<float>& min, std::vector<float>& max)
{
    switch (info.Type)
    {
    case COMPONENT_UNSIGNED_BYTE:	FindMinMax<uint8_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_UNSIGNED_SHORT:	FindMinMax<uint16_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_UNSIGNED_INT:	FindMinMax<uint16_t>(info, src, stride, offset, count, min, max); break;
    case COMPONENT_FLOAT:			FindMinMax<float>(info, src, stride, offset, count, min, max); break;
    }
}
