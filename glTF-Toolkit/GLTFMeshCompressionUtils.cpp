// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "AccessorUtils.h"

#include "GLTFMeshCompressionUtils.h"
#include "GLTFSDK/MeshPrimitiveUtils.h"
#include "GLTFSDK/ExtensionsKHR.h"
#include "GLTFSDK/BufferBuilder.h"

#pragma warning(push)
#pragma warning(disable: 4081)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4389)
#include "draco/compression/encode.h"
#include "draco/core/cycle_timer.h"
#include "draco/io/mesh_io.h"
#include "draco/io/point_cloud_io.h"
#pragma warning(pop)

// Usings for glTF
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

class FilepathStreamWriter : public IStreamWriter
{
public:
    FilepathStreamWriter(std::string uriBase) : m_uriBase(uriBase.begin(), uriBase.end()) {}

    virtual ~FilepathStreamWriter() override {}
    virtual std::shared_ptr<std::ostream> GetOutputStream(const std::string& filename) const override
    {
        std::wstring filenameW = std::wstring(filename.begin(), filename.end());

        wchar_t uriAbsoluteRaw[MAX_PATH];
        // Note: PathCchCombine will return the last argument if it's an absolute path
        if (FAILED(::PathCchCombine(uriAbsoluteRaw, ARRAYSIZE(uriAbsoluteRaw), m_uriBase.c_str(), filenameW.c_str())))
        {
            throw std::invalid_argument("Could not get the base path for the GLTF resources. Try specifying the full path.");
        }

        return std::make_shared<std::ofstream>(uriAbsoluteRaw, std::ios::binary);
    }
private:
    const std::wstring m_uriBase;
};

draco::GeometryAttribute::Type GetTypeFromAttributeName(const std::string& name)
{
    if (name == ACCESSOR_POSITION)
    {
        return draco::GeometryAttribute::Type::POSITION;
    }
    if (name == ACCESSOR_NORMAL)
    {
        return draco::GeometryAttribute::Type::NORMAL;
    }
    if (name == ACCESSOR_TEXCOORD_0)
    {
        return draco::GeometryAttribute::Type::TEX_COORD;
    }
    if (name == ACCESSOR_TEXCOORD_1)
    {
        return draco::GeometryAttribute::Type::TEX_COORD;
    }
    if (name == ACCESSOR_COLOR_0)
    {
        return draco::GeometryAttribute::Type::COLOR;
    }
    if (name == ACCESSOR_JOINTS_0)
    {
        return draco::GeometryAttribute::Type::GENERIC;
    }
    if (name == ACCESSOR_WEIGHTS_0)
    {
        return draco::GeometryAttribute::Type::GENERIC;
    }
    return draco::GeometryAttribute::Type::INVALID;
}

draco::DataType GetDataType(const Accessor& accessor)
{
    switch (accessor.componentType)
    {
    case COMPONENT_BYTE: return draco::DataType::DT_INT8;
    case COMPONENT_UNSIGNED_BYTE: return draco::DataType::DT_UINT8;
    case COMPONENT_SHORT: return draco::DataType::DT_INT16;
    case COMPONENT_UNSIGNED_SHORT: return draco::DataType::DT_UINT16;
    case COMPONENT_UNSIGNED_INT: return draco::DataType::DT_UINT32;
    case COMPONENT_FLOAT: return draco::DataType::DT_FLOAT32;
    }
    return draco::DataType::DT_INVALID;
}

template<typename T>
int InitializePointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, const Document& doc, GLTFResourceReader& reader, Accessor& accessor)
{
    auto stride = sizeof(T) * Accessor::GetTypeCount(accessor.type);
    auto numComponents = Accessor::GetTypeCount(accessor.type);
    draco::PointAttribute pointAttr;
    pointAttr.Init(GetTypeFromAttributeName(attributeName), nullptr, numComponents, GetDataType(accessor), accessor.normalized, stride, 0);
    int attId = dracoMesh.AddAttribute(pointAttr, true, accessor.count);
    auto attrActual = dracoMesh.attribute(attId);

    std::vector<T> values = reader.ReadBinaryData<T>(doc, accessor);

    if ((accessor.min.empty() || accessor.max.empty()) && !values.empty())
    {
        auto minmax = AccessorUtils::CalculateMinMax(accessor, values);
        accessor.min = minmax.first;
        accessor.max = minmax.second;
    }

    for (draco::PointIndex i(0); i < accessor.count; ++i)
    {
        attrActual->SetAttributeValue(attrActual->mapped_index(i), &values[i.value() * numComponents]);
    }
    if (dracoMesh.num_points() == 0) 
    {
        dracoMesh.set_num_points(accessor.count);
    }
    else if (dracoMesh.num_points() != accessor.count)
    {
        throw GLTFException("Inconsistent points count.");
    }

    return attId;
}

void SetEncoderOptions(draco::Encoder& encoder, const CompressionOptions& options)
{
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, options.PositionQuantizationBits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, options.TexCoordQuantizationBits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, options.NormalQuantizationBits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, options.ColorQuantizationBits);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, options.GenericQuantizationBits);
    encoder.SetSpeedOptions(options.Speed, options.Speed);
    encoder.SetTrackEncodedProperties(true);
}

Document GLTFMeshCompressionUtils::CompressMesh(std::shared_ptr<IStreamReader> streamReader, const Document & doc, CompressionOptions options, const Mesh & mesh, BufferBuilder* builder)
{
    GLTFResourceReader reader(streamReader);
    Document resultDocument(doc);
    draco::Encoder encoder;
    SetEncoderOptions(encoder, options);

    Mesh resultMesh(mesh);
    resultMesh.primitives.clear();
    for (const auto& primitive : mesh.primitives)
    {
        auto dracoExtension = std::make_unique<KHR::MeshPrimitives::DracoMeshCompression>();
        draco::Mesh dracoMesh;
        auto indices = MeshPrimitiveUtils::GetIndices32(doc, reader, primitive);
        size_t numFaces = indices.size() / 3;
        dracoMesh.SetNumFaces(numFaces);
        for (size_t i = 0; i < numFaces; i++)
        {
            draco::Mesh::Face face;
            face[0] = indices[(i * 3) + 0];
            face[1] = indices[(i * 3) + 1];
            face[2] = indices[(i * 3) + 2];
            dracoMesh.SetFace(draco::FaceIndex(i), face);
        }

        Accessor indiciesAccessor(doc.accessors[primitive.indicesAccessorId]);
        if (resultDocument.bufferViews.Has(indiciesAccessor.bufferViewId))
        {
            resultDocument.bufferViews.Remove(indiciesAccessor.bufferViewId);
        }
        indiciesAccessor.bufferViewId = "";
        indiciesAccessor.byteOffset = 0;
        resultDocument.accessors.Replace(indiciesAccessor);

        for (const auto& attribute : primitive.attributes)
        {
            const auto& accessor = doc.accessors[attribute.second];
            Accessor attributeAccessor(accessor);
            int attId;
            switch (accessor.componentType)
            {
            case COMPONENT_BYTE:           attId = InitializePointAttribute<int8_t>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            case COMPONENT_UNSIGNED_BYTE:  attId = InitializePointAttribute<uint8_t>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            case COMPONENT_SHORT:          attId = InitializePointAttribute<int16_t>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            case COMPONENT_UNSIGNED_SHORT: attId = InitializePointAttribute<uint16_t>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            case COMPONENT_UNSIGNED_INT:   attId = InitializePointAttribute<uint32_t>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            case COMPONENT_FLOAT:          attId = InitializePointAttribute<float>(dracoMesh, attribute.first, doc, reader, attributeAccessor); break;
            default: throw GLTFException("Unknown component type.");
            }
            
            if (resultDocument.bufferViews.Has(accessor.bufferViewId))
            {
                resultDocument.bufferViews.Remove(accessor.bufferViewId);
            }
            attributeAccessor.bufferViewId = "";
            attributeAccessor.byteOffset = 0;
            resultDocument.accessors.Replace(attributeAccessor);

            dracoExtension->attributes.emplace(attribute.first, dracoMesh.attribute(attId)->unique_id());
        }
        if (primitive.targets.size() > 0)
        {
            // Set sequential encoding to preserve order of vertices.
            encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
        }

        dracoMesh.DeduplicateAttributeValues();
        dracoMesh.DeduplicatePointIds();
        draco::EncoderBuffer buffer;
        const draco::Status status = encoder.EncodeMeshToBuffer(dracoMesh, &buffer);
        if (!status.ok()) {
            throw GLTFException(std::string("Failed to encode the mesh: ") + status.error_msg());
        }

        // We must update the original accessors to the encoding out values.
        Accessor encodedIndexAccessor(resultDocument.accessors[primitive.indicesAccessorId]);
        encodedIndexAccessor.count = encoder.num_encoded_faces() * 3;
        resultDocument.accessors.Replace(encodedIndexAccessor);

        for (const auto& dracoAttribute : dracoExtension->attributes)
        {
            auto accessorId = primitive.attributes.at(dracoAttribute.first);
            Accessor encodedAccessor(resultDocument.accessors[accessorId]);
            encodedAccessor.count = encoder.num_encoded_points();
            resultDocument.accessors.Replace(encodedAccessor);
        }

        // Finally put the encoded data in place.
        auto bufferView = builder->AddBufferView(buffer.data(), buffer.size());
        dracoExtension->bufferViewId = bufferView.id;
        MeshPrimitive resultPrim(primitive);
        resultPrim.SetExtension(std::move(dracoExtension));
        resultMesh.primitives.emplace_back(resultPrim);
    }
    resultDocument.meshes.Replace(resultMesh);

    return resultDocument;
}

Document GLTFMeshCompressionUtils::CompressMeshes(std::shared_ptr<IStreamReader> streamReader, const Document & doc, CompressionOptions options, const std::string& outputDirectory)
{
    Document resultDocument(doc);

    auto writerStream = std::make_shared<FilepathStreamWriter>(outputDirectory);
    auto writer = std::make_unique<GLTFResourceWriter>(writerStream);
    writer->SetUriPrefix(outputDirectory);
    std::unique_ptr<BufferBuilder> builder = std::make_unique<BufferBuilder>(std::move(writer),
        [&resultDocument](const BufferBuilder& builder) { return std::to_string(resultDocument.buffers.Size() + builder.GetBufferCount()); },
        [&resultDocument](const BufferBuilder& builder) { return std::to_string(resultDocument.bufferViews.Size() + builder.GetBufferViewCount()); },
        [&resultDocument](const BufferBuilder& builder) { return std::to_string(resultDocument.accessors.Size() + builder.GetAccessorCount()); });
    auto buffer = builder->AddBuffer();
    for (const auto& mesh : doc.meshes.Elements())
    {
        resultDocument = CompressMesh(streamReader, resultDocument, options, mesh, builder.get());
    }

    builder->Output(resultDocument);
    resultDocument.extensionsUsed.emplace(KHR::MeshPrimitives::DRACOMESHCOMPRESSION_NAME);
    resultDocument.extensionsRequired.emplace(KHR::MeshPrimitives::DRACOMESHCOMPRESSION_NAME);

    return resultDocument;
}
