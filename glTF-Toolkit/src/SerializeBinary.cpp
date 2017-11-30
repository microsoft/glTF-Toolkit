// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "SerializeBinary.h"

#include "GLTFSDK/GLTF.h"
#include "GLTFSDK/GLTFDocument.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLBResourceWriter.h"
#include "GLTFSDK/BufferBuilder.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
    static std::string MimeTypeFromUri(const std::string& uri)
    {
        auto extension = uri.substr(uri.rfind('.') + 1, 3);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](char c) { return static_cast<char>(::tolower(static_cast<int>(c))); });

        if (extension == FILE_EXT_DDS)
        {
            return MIMETYPE_DDS;
        }

        if (extension == FILE_EXT_JPEG)
        {
            return MIMETYPE_JPEG;
        }

        if (extension == FILE_EXT_PNG)
        {
            return MIMETYPE_PNG;
        }

        return "text/plain";
    }

    template <typename T>
    void SerializeAccessor(const Accessor& accessor, const GLTFDocument& doc, const GLTFResourceReader& reader, BufferBuilder& builder)
    {
        builder.AddBufferView(doc.bufferViews.Get(accessor.bufferViewId).target);
        const std::vector<T>& accessorContents = reader.ReadBinaryData<T>(doc, accessor);

        auto min = accessor.min;
        auto max = accessor.max;
        if (min.empty() || max.empty())
        {
            // Calculate min and max as part of the serialization
            auto typeCount = Accessor::GetTypeCount(accessor.type);
            min = std::vector<float>(typeCount);
            max = std::vector<float>(typeCount);

            // Initialize min and max with the first elements of the array
            for (size_t j = 0; j < typeCount; j++)
            {
                auto current = static_cast<float>(accessorContents[j]);
                min[j] = current;
                max[j] = current;
            }

            for (size_t i = 1; i < accessor.count; i++)
            {
                for (size_t j = 0; j < typeCount; j++)
                {
                    auto current = static_cast<float>(accessorContents[i * typeCount + j]);
                    min[j] = std::min<T>(min[j], current);
                    max[j] = std::max<T>(max[j], current);
                }
            }
        }

        builder.AddAccessor(accessorContents, accessor.componentType, accessor.type, min, max);
    }
}

void Microsoft::glTF::Toolkit::SerializeBinary(const GLTFDocument& gltfDocument, const IStreamReader& inputStreamReader, std::unique_ptr<const IStreamFactory>& outputStreamFactory)
{
    auto writer = std::make_unique<GLBResourceWriter2>(std::move(outputStreamFactory), std::string());

    GLTFDocument outputDoc(gltfDocument);

    outputDoc.buffers.Clear();
    outputDoc.bufferViews.Clear();
    outputDoc.accessors.Clear();

    GLTFResourceReader gltfResourceReader(inputStreamReader);

    std::unique_ptr<BufferBuilder> builder = std::make_unique<BufferBuilder>(std::move(writer));

    // GLB buffer
    builder->AddBuffer(GLB_BUFFER_ID);

    // Serialize accessors
    for (auto accessor : gltfDocument.accessors.Elements())
    {
        switch (accessor.componentType)
        {
        case COMPONENT_BYTE:
            SerializeAccessor<int8_t>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        case COMPONENT_UNSIGNED_BYTE:
            SerializeAccessor<uint8_t>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        case COMPONENT_SHORT:
            SerializeAccessor<int16_t>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        case COMPONENT_UNSIGNED_SHORT:
            SerializeAccessor<uint16_t>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        case COMPONENT_UNSIGNED_INT:
            SerializeAccessor<uint32_t>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        case COMPONENT_FLOAT:
            SerializeAccessor<float>(accessor, gltfDocument, gltfResourceReader, *builder);
            break;
        default:
            throw GLTFException("Unsupported accessor ComponentType");
        }
    }

    // Serialize images
    for (auto image : outputDoc.images.Elements())
    {
        if (!image.uri.empty())
        {
            Image newImage(image);

            auto data = gltfResourceReader.ReadBinaryData(gltfDocument, image);

            auto imageBufferView = builder->AddBufferView(data);

            newImage.bufferViewId = imageBufferView.id;
            if (image.mimeType.empty())
            {
                newImage.mimeType = MimeTypeFromUri(image.uri);
            }

            newImage.uri.clear();

            outputDoc.images.Replace(newImage);
        }
    }

    builder->Output(outputDoc);

    // Add extensions and extras to bufferViews, if any
    for (auto bufferView : gltfDocument.bufferViews.Elements())
    {
        auto fixedBufferView = outputDoc.bufferViews.Get(bufferView.id);
        fixedBufferView.extensions = bufferView.extensions;
        fixedBufferView.extras = bufferView.extras;

        outputDoc.bufferViews.Replace(fixedBufferView);
    }

    auto manifest = Serialize(outputDoc);

    auto outputWriter = dynamic_cast<GLBResourceWriter2 *>(&builder->GetResourceWriter());
    if (outputWriter != nullptr)
    {
        outputWriter->Flush(manifest, std::string());
    }
}