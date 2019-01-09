// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/BufferBuilder.h>

namespace Microsoft::glTF::Toolkit
{
    enum class AttributeFormat : uint8_t;
    enum class PrimitiveFormat : uint8_t;
    struct MeshOptions;

    extern std::string s_insertionId;
    extern std::string s_attributeNames[];

    //------------------------------------------
    // Attribute

    enum Attribute : uint32_t
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
        Attribute mask;

        inline void Set(Attribute attr, bool cond) { cond ? Add(attr) : Remove(attr); }
        inline void Add(Attribute attr) { mask = (Attribute)(mask | (1 << attr)); }
        inline void Remove(Attribute attr) { mask = (Attribute)(mask & ~(1 << attr)); }
        inline bool Has(Attribute attr) const { return (mask & (1 << attr)) != 0; }

        static AttributeList FromPrimitive(const MeshPrimitive& p);
        
        inline bool operator==(const AttributeList& rhs) const { return mask == rhs.mask; }
        inline bool operator!=(const AttributeList& rhs) const { return mask != rhs.mask; }
    };


    //------------------------------------------
    // AccessorInfo

    struct AccessorInfo
    {
        ComponentType    type;
        AccessorType     dimension;
        BufferViewTarget target;

        bool IsValid(void) const;
        size_t GetElementSize(void) const;

        static AccessorInfo Invalid(void);
        static AccessorInfo Create(ComponentType cType, AccessorType aType, BufferViewTarget target);
        static AccessorInfo Max(const AccessorInfo& a0, const AccessorInfo& a1);

        friend std::ostream& operator<<(std::ostream&, const AccessorInfo&);
    };
    std::ostream& operator<<(std::ostream& s, const AccessorInfo& a);


    //------------------------------------------
    // PrimitiveInfo

    struct PrimitiveInfo
    {
        size_t offset; // Could be index or vertex offset.

        size_t indexCount;
        size_t vertexCount;
        AccessorInfo metadata[Count];

        size_t GetCount(void) const { return indexCount > 0 ? indexCount : vertexCount; };
        size_t GetCount(Attribute attr) const { return attr == Indices ? indexCount : vertexCount; };
        size_t FaceCount(void) const { return GetCount() / 3; }
        size_t GetIndexSize(void) const { return Accessor::GetComponentTypeSize(metadata[Indices].type); }
        size_t GetVertexSize(void) const;

        void GetVertexInfo(size_t& stride, size_t(&offsets)[Count], size_t* pOutAlignment = nullptr) const;

        void CopyMeta(const PrimitiveInfo& info);

        AccessorInfo& operator[] (size_t index) { return metadata[index]; }
        const AccessorInfo& operator[] (size_t index) const { return metadata[index]; }
        
        static ComponentType GetIndexType(size_t vertexCount) { return vertexCount < USHORT_MAX ? (vertexCount < BYTE_MAX ? COMPONENT_UNSIGNED_BYTE : COMPONENT_UNSIGNED_SHORT) : COMPONENT_UNSIGNED_INT; }

        static PrimitiveInfo Create(size_t indexCount, size_t vertexCount, AttributeList attributes, const std::pair<ComponentType, AccessorType>(&types)[Count], size_t offset = 0);
        static PrimitiveInfo CreateMin(size_t indexCount, size_t vertexCount, AttributeList attributes, size_t offset = 0);
        static PrimitiveInfo CreateMax(size_t indexCount, size_t vertexCount, AttributeList attributes, size_t offset = 0);
        static PrimitiveInfo Max(const PrimitiveInfo& p0, const PrimitiveInfo& p1);

        friend std::ostream& operator<<(std::ostream&, const PrimitiveInfo&);
    };
    std::ostream& operator<<(std::ostream& s, const PrimitiveInfo& p);


    //------------------------------------------
    // MeshOptimizer

    class MeshOptimizer
    {
    public:
        MeshOptimizer(void);
        MeshOptimizer(const MeshOptimizer& parent, size_t primIndex);

        // Populates the mesh with data from the specified glTF document & mesh.
        bool Initialize(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh);

        // Clears the existing mesh data.
        void Reset(void);

        // Leverages DirectXMesh facilities to optimize the mesh data.
        void Optimize(void);

        // Generates normal and optionally tangent data.
        void GenerateAttributes(void);

        // Exports the mesh to a BufferBuilder and Mesh in a format specified by MeshOptions.
        void Export(const MeshOptions& options, BufferBuilder& builder, Mesh& outMesh) const;

        // Determines whether a specific mesh exists in a supported format.
        static bool IsSupported(const Mesh& m);

        // Finds the ids of all accessors, buffer views, and buffers which reference mesh data by meshes affected by optimization.
        static void FindRestrictedIds(const Document& doc, std::unordered_set<std::string>& accessorIds, std::unordered_set<std::string>& bufferViewIds, std::unordered_set<std::string>& bufferIds);

        // Cleans up orphaned & unnecessary accessors, buffer views, and buffers caused by the mesh cleaning/formatting procedure.
        static void Finalize(std::shared_ptr<IStreamReader>& reader, BufferBuilder& builder, const Document& oldDoc, Document& newDoc);

    private:
        inline size_t GetFaceCount(void) const { return (m_indices.size() > 0 ? m_indices.size() : m_positions.size()) / 3; }
        PrimitiveInfo DetermineMeshFormat(void) const;

        void InitSeparateAccessors(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh);
        void InitSharedAccessors(std::shared_ptr<IStreamReader>& reader, const Document& doc, const Mesh& mesh);

        void WriteVertices(const PrimitiveInfo& info, std::vector<uint8_t>& output) const;
        void ReadVertices(const PrimitiveInfo& info, const std::vector<uint8_t>& input);

        // Exports the mesh data to a BufferBuilder and Mesh in a specific format.
        void ExportCSI(BufferBuilder& builder, Mesh& outMesh) const; // Combine primitives, separate attributes, indexed
        void ExportCS(BufferBuilder& builder, Mesh& outMesh) const;  // Combine primitives, separate attributes, non-indexed
        void ExportCI(BufferBuilder& builder, Mesh& outMesh) const;  // Combine primitives, interleave attributes
        void ExportSS(BufferBuilder& builder, Mesh& outMesh) const;  // Separate primitives, separate attributes
        void ExportSI(BufferBuilder& builder, Mesh& outMesh) const;  // Separate primitives, interleave attributes

        // Writes vertex attribute data as a block, and exports one buffer view & accessor to a BufferBuilder.
        template <typename T>
        void ExportSharedView(BufferBuilder& builder, const PrimitiveInfo& info, Attribute attr, std::vector<T>(MeshOptimizer::*attributePtr), Mesh& outMesh) const;

        // Writes vertex attribute data as a block, and exports one buffer view & accessor to a BufferBuilder.
        template <typename T>
        bool ExportAccessor(BufferBuilder& builder, const PrimitiveInfo& prim, Attribute attr, std::vector<T>(MeshOptimizer::*attributePtr), std::string& outString) const;

        // Writes mesh vertex data in an interleaved fashion, and exports one buffer view and shared accessors to a BufferBuilder.
        void ExportInterleaved(BufferBuilder& builder, const PrimitiveInfo& info, std::string (&outIds)[Count]) const;

        // Maps indices from global vertex list to local (per-primitive) index list.
        static void RemapIndices(std::unordered_map<uint32_t, uint32_t>& map, std::vector<uint32_t>& newIndices, const uint32_t* indices, size_t count);

        // Determines if all primitives within a glTF mesh shares accessors (aka interleaved vertex data.)
        static bool UsesSharedAccessors(const Mesh& m);

        // Determines whether primitives within a glTF mesh are combined into a global buffer, or separated into their own local buffers.
        static PrimitiveFormat DetermineFormat(const Document& doc, const Mesh& m);

    private:
        std::string m_name;
        std::vector<PrimitiveInfo> m_primitives;

        std::vector<std::string> m_ids;

        std::vector<uint32_t> m_indices;
        std::vector<DirectX::XMFLOAT3> m_positions;
        std::vector<DirectX::XMFLOAT3> m_normals;
        std::vector<DirectX::XMFLOAT4> m_tangents;
        std::vector<DirectX::XMFLOAT2> m_uv0;
        std::vector<DirectX::XMFLOAT2> m_uv1;
        std::vector<DirectX::XMFLOAT4> m_color0;
        std::vector<DirectX::XMUINT4>  m_joints0;
        std::vector<DirectX::XMFLOAT4> m_weights0;

        AttributeList m_attributes;
        PrimitiveFormat m_primFormat;

        mutable std::vector<uint8_t> m_scratch; // Temp staging buffer used when organizing data for writes to buffer files.
        mutable std::vector<float> m_min; // Temp min buffer used when calculing min components of accessor data.
        mutable std::vector<float> m_max; // Temp max buffer used when calculing max components of accessor data.

        friend std::ostream& operator<<(std::ostream&, const MeshOptimizer&);
    };
    std::ostream& operator<<(std::ostream& s, const MeshOptimizer& m);
}
