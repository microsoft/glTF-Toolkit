// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "AccessorUtils.h"
#include "SerializeBinary.h"

#include "GLTFSDK/GLTF.h"
#include "GLTFSDK/Document.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLBResourceWriter.h"
#include "GLTFSDK/Serialize.h"
#include "GLTFSDK/BufferBuilder.h"
#include "GLTFSDK/ExtensionsKHR.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
    static std::string MimeTypeFromUri(const std::string& uri)
    {
        auto extension = uri.substr(uri.rfind('.') + 1, 3);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](char c) { return static_cast<char>(::tolower(static_cast<int>(c))); });

        if (extension == "dds")
        {
            return "image/vnd-ms.dds";
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
    void SaveAccessor(const Accessor& accessor, const std::vector<T> accessorContents, BufferBuilder& builder)
    {
        auto min = accessor.min;
        auto max = accessor.max;
        if ((min.empty() || max.empty()) && !accessorContents.empty())
        {
            auto minmax = AccessorUtils::CalculateMinMax(accessor, accessorContents);
            min = minmax.first;
            max = minmax.second;
        }

        builder.AddAccessor(accessorContents, AccessorDesc(accessor.type, accessor.componentType, accessor.normalized, min, max));
    }

    template <typename OriginalType, typename NewType>
    static std::vector<NewType> vector_static_cast(const std::vector<OriginalType>& original)
    {
        auto newData = std::vector<NewType>(original.size());

        std::transform(original.begin(), original.end(), newData.begin(),
            [](const OriginalType& element)
        {
            return static_cast<NewType>(element);
        });

        return newData;
    }

    template <typename T>
    void ConvertAndSaveAccessor(const Accessor& accessor, const std::vector<T> accessorContents, BufferBuilder& builder)
    {
        switch (accessor.componentType)
        {
        case COMPONENT_BYTE:
            SaveAccessor(accessor, vector_static_cast<T, int8_t>(accessorContents), builder);
            break;
        case COMPONENT_UNSIGNED_BYTE:
            SaveAccessor(accessor, vector_static_cast<T, uint8_t>(accessorContents), builder);
            break;
        case COMPONENT_SHORT:
            SaveAccessor(accessor, vector_static_cast<T, int16_t>(accessorContents), builder);
            break;
        case COMPONENT_UNSIGNED_SHORT:
            SaveAccessor(accessor, vector_static_cast<T, uint16_t>(accessorContents), builder);
            break;
        case COMPONENT_UNSIGNED_INT:
            SaveAccessor(accessor, vector_static_cast<T, uint32_t>(accessorContents), builder);
            break;
        case COMPONENT_FLOAT:
            SaveAccessor(accessor, vector_static_cast<T, float>(accessorContents), builder);
            break;
        default:
            throw GLTFException("Unsupported accessor ComponentType");
        }
    }

    template <typename T>
    void SerializeAccessor(const Accessor& accessor, const Document& doc, const GLTFResourceReader& reader, BufferBuilder& builder, const AccessorConversionStrategy& accessorConversion)
    {
        builder.AddBufferView(doc.bufferViews.Get(accessor.bufferViewId).target);
        const std::vector<T>& accessorContents = reader.ReadBinaryData<T>(doc, accessor);

        if (accessorConversion != nullptr && accessorConversion(accessor) != accessor.componentType)
        {
            Accessor updatedAccessor(accessor);
            updatedAccessor.componentType = accessorConversion(accessor);

            // Force recalculation of min and max
            updatedAccessor.min.clear();
            updatedAccessor.max.clear();

            ConvertAndSaveAccessor(updatedAccessor, accessorContents, builder);
        }
        else
        {
            SaveAccessor(accessor, accessorContents, builder);
        }
    }

    void SerializeAccessor(const Accessor& accessor, const Document& doc, const GLTFResourceReader& reader, BufferBuilder& builder, const AccessorConversionStrategy& accessorConversion)
    {
        switch (accessor.componentType)
        {
        case COMPONENT_BYTE:
            SerializeAccessor<int8_t>(accessor, doc, reader, builder, accessorConversion);
            break;
        case COMPONENT_UNSIGNED_BYTE:
            SerializeAccessor<uint8_t>(accessor, doc, reader, builder, accessorConversion);
            break;
        case COMPONENT_SHORT:
            SerializeAccessor<int16_t>(accessor, doc, reader, builder, accessorConversion);
            break;
        case COMPONENT_UNSIGNED_SHORT:
            SerializeAccessor<uint16_t>(accessor, doc, reader, builder, accessorConversion);
            break;
        case COMPONENT_UNSIGNED_INT:
            SerializeAccessor<uint32_t>(accessor, doc, reader, builder, accessorConversion);
            break;
        case COMPONENT_FLOAT:
            SerializeAccessor<float>(accessor, doc, reader, builder, accessorConversion);
            break;
        default:
            throw GLTFException("Unsupported accessor ComponentType");
        }
    }
}

void Microsoft::glTF::Toolkit::SerializeBinary(const Document& document,
                                               const GLTFResourceReader& resourceReader,
                                               std::shared_ptr<const IStreamWriter> outputStreamWriter,
                                               const AccessorConversionStrategy& accessorConversion)
{
    auto writer = std::make_unique<GLBResourceWriter>(std::move(outputStreamWriter));

    Document outputDoc(document);

    outputDoc.buffers.Clear();
    outputDoc.bufferViews.Clear();
    outputDoc.accessors.Clear();

    // Get the collection of bufferViews we won't move around
    IndexedContainer<const BufferView> staticBufferViews = document.bufferViews;
    for (const auto& accessor : document.accessors.Elements())
    {
        if (!accessor.bufferViewId.empty() && staticBufferViews.Has(accessor.bufferViewId))
        {
            staticBufferViews.Remove(accessor.bufferViewId);
        }
    }

    for (const auto& image : outputDoc.images.Elements())
    {
        if (!image.bufferViewId.empty() && staticBufferViews.Has(image.bufferViewId))
        {
            staticBufferViews.Remove(image.bufferViewId);
        }
    }

    size_t currentAccessorId = 0;
    std::string currentAccessorIdStr = std::to_string(currentAccessorId);
    size_t currentBufferViewId = 0;
    std::string currentBufferViewIdStr = std::to_string(currentBufferViewId);
    auto AdvanceAccessorId = [&currentAccessorId, &currentAccessorIdStr]()
    {
        currentAccessorId++;
        currentAccessorIdStr = std::to_string(currentAccessorId);
    };
    auto AdvanceBufferViewId = [&currentBufferViewId, &currentBufferViewIdStr, &staticBufferViews]()
    {
        do
        {
            currentBufferViewId++;
            currentBufferViewIdStr = std::to_string(currentBufferViewId);
        } while (staticBufferViews.Has(currentBufferViewIdStr));
    };
    std::unique_ptr<BufferBuilder> builder = std::make_unique<BufferBuilder>(std::move(writer),
        [](const BufferBuilder&) { return GLB_BUFFER_ID; },
        [&currentBufferViewIdStr](const BufferBuilder&) { return currentBufferViewIdStr; },
        [&currentAccessorIdStr](const BufferBuilder&) { return currentAccessorIdStr; });

    // GLB buffer
    builder->AddBuffer(GLB_BUFFER_ID);

    // Add those bufferView to the builder.
    for (const auto& bufferView : staticBufferViews.Elements())
    {
        currentBufferViewIdStr = bufferView.id;
        auto data = resourceReader.ReadBinaryData<uint8_t>(document, bufferView);
        builder->AddBufferView(data);
    }
    // Return value to tracked state
    currentBufferViewIdStr = std::to_string(currentBufferViewId);
    if (staticBufferViews.Has(currentBufferViewIdStr))
    {
        AdvanceBufferViewId();
    }

    // Serialize accessors
    for (const auto& accessor : document.accessors.Elements())
    {
        if (!accessor.bufferViewId.empty() && accessor.count > 0)
        {
            SerializeAccessor(accessor, document, resourceReader, *builder, accessorConversion);
            AdvanceBufferViewId();
        }
        else
        {
            outputDoc.accessors.Append(accessor);
        }
        AdvanceAccessorId();
    }

    // Serialize images
    for (const auto& image : outputDoc.images.Elements())
    {
        Image newImage(image);

        auto data = resourceReader.ReadBinaryData(document, image);

        auto imageBufferView = builder->AddBufferView(data);
        AdvanceBufferViewId();

        newImage.bufferViewId = imageBufferView.id;
        if (image.mimeType.empty())
        {
            newImage.mimeType = MimeTypeFromUri(image.uri);
        }

        newImage.uri.clear();

        outputDoc.images.Replace(newImage);
    }

    // Collect anything in extensions that looks like it should to be packed for the GLB.
    for (auto& extension : outputDoc.extensions)
    {
        rapidjson::Document extensionJson;
        extensionJson.Parse(extension.second.c_str());
        if (!extensionJson.IsObject())
        {
            continue;
        }
        for (auto& member : extensionJson.GetObject())
        {
            if (!member.value.IsArray())
            {
                continue;
            }
            for (auto& possibleBuffer : member.value.GetArray())
            {
                if (!possibleBuffer.IsObject())
                {
                    continue;
                }
                // Build an Image to object to use to load the data from.
                Image tmpImg;
                if (possibleBuffer.HasMember("uri"))
                {
                    tmpImg.uri = possibleBuffer["uri"].GetString();
                }
                else
                {
                    continue;
                }
                try
                {
                    auto data = resourceReader.ReadBinaryData(document, tmpImg);
                    auto bufferView = builder->AddBufferView(data);
                    AdvanceBufferViewId();

                    possibleBuffer.RemoveMember("uri");
                    possibleBuffer.RemoveMember("bufferView");
                    possibleBuffer.AddMember("bufferView", rapidjson::Value(std::stoi(bufferView.id)), extensionJson.GetAllocator());
                }
                catch (...) 
                {
                    // Didn't work out.
                    continue;
                }
            }
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(buffer);
        extensionJson.Accept(jsonWriter);

        extension.second = buffer.GetString();
    }

    // Fill in any gaps in the bufferViewList.
    for (const auto& bufferView : staticBufferViews.Elements())
    {
        auto bufferViewId = std::stoul(bufferView.id);
        while (bufferViewId > currentBufferViewId)
        {
            std::vector<uint8_t> data;
            data.resize(4);
            builder->AddBufferView(data);
            AdvanceBufferViewId();
        }
    }

    builder->Output(outputDoc);

    // Add extensions and extras to bufferViews, if any
    for (auto bufferView : document.bufferViews.Elements())
    {
        auto fixedBufferView = outputDoc.bufferViews.Get(bufferView.id);
        fixedBufferView.extensions = bufferView.extensions;
        fixedBufferView.extras = bufferView.extras;

        outputDoc.bufferViews.Replace(fixedBufferView);
    }

    // We may have put the bufferViews in the IndexedContainer out of order sort them now.
    auto finalBufferViewList = outputDoc.bufferViews;
    outputDoc.bufferViews.Clear();
    for (size_t i = 0; i < finalBufferViewList.Size(); i++)
    {
        outputDoc.bufferViews.Append(finalBufferViewList[std::to_string(i)]);
    }

    auto manifest = Serialize(outputDoc, KHR::GetKHRExtensionSerializer());

    auto outputWriter = dynamic_cast<GLBResourceWriter*>(&builder->GetResourceWriter());
    if (outputWriter != nullptr)
    {
        outputWriter->Flush(manifest, std::string());
    }
}

void Microsoft::glTF::Toolkit::SerializeBinary(const Document& document, std::shared_ptr<const IStreamReader> inputStreamReader,
                                               std::shared_ptr<const IStreamWriter> outputStreamWriter,
                                               const AccessorConversionStrategy& accessorConversion)
{
    SerializeBinary(document, GLTFResourceReader{inputStreamReader}, std::move(outputStreamWriter), accessorConversion);
}
