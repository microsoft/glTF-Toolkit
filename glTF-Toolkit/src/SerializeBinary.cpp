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
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

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
}

void Microsoft::glTF::Toolkit::SerializeBinary(const GLTFDocument& gltfDocument, const IStreamReader& inputStreamReader, std::unique_ptr<const IStreamFactory>& outputStreamFactory)
{
    auto writer = std::make_unique<GLBResourceWriter2>(std::move(outputStreamFactory), std::string());

    GLTFDocument outputDoc(gltfDocument);

    outputDoc.buffers.Clear();
    outputDoc.bufferViews.Clear();

    GLTFResourceReader gltfResourceReader(inputStreamReader);

    std::unique_ptr<BufferBuilder> builder = std::make_unique<BufferBuilder>(std::move(writer));

    // GLB buffer
    builder->AddBuffer(GLB_BUFFER_ID);

    for (auto bufferView : gltfDocument.bufferViews.Elements())
    {
        const std::vector<uint8_t>& bufferViewContents = gltfResourceReader.ReadBinaryData<uint8_t>(gltfDocument, bufferView);
        builder->AddBufferView<uint8_t>(bufferViewContents, bufferView.byteStride, bufferView.target);
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