// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/IResourceWriter.h>
#include <GLTFSDK/BufferBuilder.h>

#include <DirectXMath.h>

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
        uint32_t mask;

        inline void Set(Attribute attr, bool cond) { cond ? Add(attr) : Remove(attr); }
        inline void Add(Attribute attr) { mask |= 1 << attr; }
        inline void Remove(Attribute attr) { mask &= ~(1 << attr); }
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
    // MeshInfo

    class MeshInfo
    {
    public:
        MeshInfo(void);
        MeshInfo(const MeshInfo& parent, size_t primIndex);

        // Populates the mesh with data from the specified glTF document & mesh.
        bool Initialize(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh);

        // Clears the existing mesh data.
        void Reset(void);

        // Leverages DirectXMesh facilities to optimize the mesh data.
        void Optimize(void);

        // Generates normal and optionally tangent data.
        void GenerateAttributes(void);

        // Exports the mesh to a BufferBuilder and Mesh in a format specified in the options.
        void Export(const MeshOptions& options, BufferBuilder& builder, Mesh& outMesh) const;

        // Determines whether a specific mesh exists in a supported format.
        static bool IsSupported(const Mesh& m);

        // Cleans up orphaned & unnecessary accessors, buffer views, and buffers caused by the mesh cleaning/formatting procedure.
        static void CopyAndCleanup(const IStreamReader& reader, BufferBuilder& builder, const GLTFDocument& oldDoc, GLTFDocument& newDoc);

    private:
        inline size_t GetFaceCount(void) const { return (m_indices.size() > 0 ? m_indices.size() : m_positions.size()) / 3; }
        PrimitiveInfo DetermineMeshFormat(void) const;

        void InitSeparateAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh);
        void InitSharedAccessors(const IStreamReader& reader, const GLTFDocument& doc, const Mesh& mesh);

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
        void ExportSharedView(BufferBuilder& builder, const PrimitiveInfo& info, Attribute attr, std::vector<T>(MeshInfo::*attributePtr), Mesh& outMesh) const;

        // Writes vertex attribute data as a block, and exports one buffer view & accessor to a BufferBuilder.
        template <typename T>
        std::string ExportAccessor(BufferBuilder& builder, const PrimitiveInfo& prim, Attribute attr, std::vector<T>(MeshInfo::*attributePtr)) const;

        // Writes mesh vertex data in an interleaved fashion, and exports one buffer view and shared accessors to a BufferBuilder.
        void ExportInterleaved(BufferBuilder& builder, const PrimitiveInfo& info, std::string (&outIds)[Count]) const;

        // Maps indices from global vertex list to local (per-primitive) index list.
        static void RemapIndices(std::unordered_map<uint32_t, uint32_t>& map, std::vector<uint32_t>& newIndices, const uint32_t* indices, size_t count);

        // Determines if all primitives within a glTF mesh shares accessors (aka interleaved vertex data.)
        static bool UsesSharedAccessors(const Mesh& m);

        // Determines whether primitives within a glTF mesh are combined into a global buffer, or separated into their own local buffers.
        static PrimitiveFormat DetermineFormat(const GLTFDocument& doc, const Mesh& m);

    private:
        std::string m_name;
        std::vector<PrimitiveInfo> m_primitives;

        std::vector<uint32_t>          m_indices;
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

        friend std::ostream& operator<<(std::ostream&, const MeshInfo&);
    };

    std::ostream& operator<<(std::ostream& s, const MeshInfo& m);


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
    void Read(const IStreamReader& reader, const GLTFDocument& Doc, const Accessor& accessor, std::vector<To>& output);

    template <typename To>
    void Read(const IStreamReader& reader, const GLTFDocument& Doc, const Accessor& accessor, std::vector<To>& output);

    template <typename To>
    bool ReadAccessor(const IStreamReader& reader, const GLTFDocument& doc, const std::string& accessorId, std::vector<To>& output, AccessorInfo& outInfo);


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