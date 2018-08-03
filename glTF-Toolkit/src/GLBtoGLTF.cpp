// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "GLBtoGLTF.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace
{
    class StreamMock : public IStreamReader
    {
    public:
        StreamMock() : m_stream(std::make_shared<std::stringstream>(std::ios_base::app | std::ios_base::binary | std::ios_base::in | std::ios_base::out))
        {
        }

        std::shared_ptr<std::istream> GetInputStream(const std::string&) const override
        {
            return m_stream;
        }

    private:
        std::shared_ptr<std::stringstream> m_stream;
    };

    size_t GetGLBBufferChunkOffset(std::ifstream* input)
    {
        // get offset from beginning of glb binary to beginning of buffer chunk
        input->seekg(GLB2_HEADER_BYTE_SIZE, std::ios::beg);
        uint32_t length = 0;
        for (int i = 0; i < GLB_CHUNK_TYPE_SIZE*CHAR_BIT; i += CHAR_BIT)
        {
            uint8_t c = static_cast<uint8_t>(input->get());
            length |= ((uint16_t)c << i);
        }
        // 28 is total length of non-json blocks from start of glb blob
        // 28 = (GLB2_HEADER_BYTE_SIZE = 12bytes) + (uint32 = 4bytes) * 4
        return length + GLB2_HEADER_BYTE_SIZE + GLB_CHUNK_TYPE_SIZE * 4;
    }
}

std::vector<char> GLBToGLTF::SaveBin(std::istream* input, const Document& glbDoc, const size_t bufferOffset, const size_t newBufferlength)
{
    if (newBufferlength == 0)
    {
        return {};
    }

    const auto images = glbDoc.images.Elements();
    const auto bufferViews = glbDoc.bufferViews.Elements();
    std::unordered_set<std::string> imagesBufferViews;
    for (const auto& im : images)
    {
        // save a copy of image buffer view IDs
        imagesBufferViews.insert(im.bufferViewId);
    }

    // gather all non-image bufferViews in UsedBufferViews
    std::vector<BufferView> usedBufferViews(bufferViews.size());
    auto last = copy_if(bufferViews.begin(), bufferViews.end(), usedBufferViews.begin(), [imagesBufferViews](const auto& a)
    {
        return imagesBufferViews.count(a.id) == 0;
    });
    usedBufferViews.resize(distance(usedBufferViews.begin(), last));

    // sort buffer views by offset
    sort(usedBufferViews.begin(), usedBufferViews.end(), [](const BufferView& a, const BufferView& b)
    {
        return a.byteOffset < b.byteOffset;
    });

    std::vector<char> result(newBufferlength);
    size_t vecpos = 0; // number of chunks read
    size_t currOffset = bufferOffset; // offset into buffer
    input->seekg(bufferOffset, std::ios::beg);

    for (const auto& bufferView : usedBufferViews)
    {
        // traverse through original buffer while grabbing non-image buffer segments
        size_t nextOffset = bufferOffset + bufferView.byteOffset;
        if (currOffset < nextOffset)
        {
            // skip over buffer segments of no interest
            size_t chunkLength = nextOffset - currOffset;
            input->seekg(chunkLength, std::ios::cur);
            currOffset += chunkLength;
        }

        if (vecpos % GLB_BUFFER_OFFSET_ALIGNMENT != 0)
        {
            // Alignment padding
            // Accessor component sizes can be 1, 2, 4.
            // Aligning to 4 will satisfy requirements but wastes space
            vecpos += (GLB_BUFFER_OFFSET_ALIGNMENT - (vecpos % GLB_BUFFER_OFFSET_ALIGNMENT));
        }

        // read and increment vecpos + offset
        input->read(&result[vecpos], bufferView.byteLength);
        currOffset += bufferView.byteLength;
        vecpos += bufferView.byteLength;
    }

    if (vecpos == 0)
    {
        return {};
    }

    return result;
}

std::unordered_map<std::string, std::vector<char>> GLBToGLTF::GetImagesData(std::istream* input, const Document& glbDoc, const std::string& name, const size_t bufferOffset)
{
    input->seekg(0, std::ios::beg);
    std::unordered_map<std::string, int> imageIDs;
    std::vector<Image> images = std::vector<Image>(glbDoc.images.Elements());
    if (images.size() == 0)
    {
        return {};
    }

    int imgId = 0;
    for (const auto& img : images)
    {
        // save mapping of original image order
        imageIDs[img.bufferViewId] = imgId;
        imgId++;
    }

    // sort images by buffer offset so only traverse once
    sort(images.begin(), images.end(), [glbDoc](const auto& a, const auto& b)
    {
        return glbDoc.bufferViews.Get(a.bufferViewId).byteOffset < glbDoc.bufferViews.Get(b.bufferViewId).byteOffset;
    });

    size_t currOffset = bufferOffset; // offset into buffer
    input->seekg(bufferOffset, std::ios::beg);

    std::unordered_map<std::string, std::vector<char>> imageStream;
    for (const auto& img : images)
    {
        // traverse through buffer while saving images
        auto bufferView = glbDoc.bufferViews.Get(img.bufferViewId);
        size_t nextImageOffset = bufferOffset + bufferView.byteOffset;
        if (currOffset < nextImageOffset)
        {
            // skip over non-image buffer segments
            size_t chunkLength = nextImageOffset - currOffset;
            input->seekg(chunkLength, std::ios::cur);
            currOffset = nextImageOffset;
        }
        // read and increment offset
        std::vector<char> result;
        result.resize(bufferView.byteLength);
        input->read(&result[0], bufferView.byteLength);
        currOffset += bufferView.byteLength;

        // write image file
        std::string outname;
        if (img.mimeType == MIMETYPE_PNG)
        {
            outname = name + "_image" + std::to_string(imageIDs[img.bufferViewId]) + "." + FILE_EXT_PNG;
        }
        else if (img.mimeType == MIMETYPE_JPEG)
        {
            outname = name + "_image" + std::to_string(imageIDs[img.bufferViewId]) + "." + FILE_EXT_JPEG;
        }
        else
        {
            // unknown mimetype
            outname = name + "_image" + std::to_string(imageIDs[img.bufferViewId]);
        }

        imageStream[outname] = result;
    }
    return imageStream;
}

// Create modified gltf from original by removing image buffer segments and updating
// images, bufferViews and accessors fields accordingly
Document GLBToGLTF::CreateGLTFDocument(const Document& glbDoc, const std::string& name)
{
    Document gltfDoc(glbDoc);

    gltfDoc.images.Clear();
    gltfDoc.buffers.Clear();
    gltfDoc.bufferViews.Clear();
    gltfDoc.accessors.Clear();

    const auto images = glbDoc.images.Elements();
    const auto buffers = glbDoc.buffers.Elements();
    const auto bufferViews = glbDoc.bufferViews.Elements();
    const auto accessors = glbDoc.accessors.Elements();
    std::unordered_set<std::string> imagesBufferViews;
    std::unordered_map<std::string, std::string> bufferViewIndex;

    size_t updatedBufferSize = 0;
    for (const auto& im : images)
    {
        // find which buffer segments correspond to images
        imagesBufferViews.insert(im.bufferViewId);
    }

    // gather all non-image bufferViews in UsedBufferViews
    std::vector<BufferView> usedBufferViews(bufferViews.size());
    auto last = copy_if(bufferViews.begin(), bufferViews.end(), usedBufferViews.begin(), [imagesBufferViews](const auto& a)
    {
        return imagesBufferViews.count(a.id) == 0;
    });

    usedBufferViews.resize(distance(usedBufferViews.begin(), last));

    // group buffer views by buffer, then sort them by byteOffset to calculate their new byteOffsets
    sort(usedBufferViews.begin(), usedBufferViews.end(), [](const auto& a, const auto& b)
    {
        return a.byteOffset < b.byteOffset;
    });

    int updatedBufferViewId = 0;
    size_t currentOffset = 0;
    for (const auto& b : usedBufferViews)
    {
        // provide new byte ranges for bufferviews
        size_t padding = 0;
        auto updatedBufferView = b;
        updatedBufferView.id = std::to_string(updatedBufferViewId);

        if (currentOffset % GLB_BUFFER_OFFSET_ALIGNMENT != 0)
        {
            // alignment padding as in SaveBin
            padding = (GLB_BUFFER_OFFSET_ALIGNMENT - (currentOffset % GLB_BUFFER_OFFSET_ALIGNMENT));
            currentOffset += padding;
        }

        updatedBufferView.byteOffset = currentOffset;
        currentOffset += b.byteLength;
        gltfDoc.bufferViews.Append(std::move(updatedBufferView));
        bufferViewIndex[b.id] = std::to_string(updatedBufferViewId);
        updatedBufferSize += (b.byteLength + padding);
        updatedBufferViewId++;
    }

    if (!buffers.empty())
    {
        auto updatedBuffer = buffers[0];
        updatedBuffer.byteLength = updatedBufferSize;
        updatedBuffer.uri = name + "." + BUFFER_EXTENSION;
        gltfDoc.buffers.Append(std::move(updatedBuffer));
    }

    for (const auto& a : accessors)
    {
        if (imagesBufferViews.find(a.bufferViewId) == imagesBufferViews.end())
        {
            // update acessors with new bufferview IDs, the above check may not be needed
            auto updatedAccessor = a;
            updatedAccessor.bufferViewId = bufferViewIndex[a.bufferViewId];
            gltfDoc.accessors.Append(std::move(updatedAccessor));
        }
    }

    int imgId = 0;
    for (const auto& im : images)
    {
        // update image fields with image names instead of buffer views
        Image updatedImage;
        updatedImage.id = std::to_string(imgId);
        if (im.mimeType == MIMETYPE_PNG)
        {
            updatedImage.uri = name + "_image" + std::to_string(imgId) + "." + FILE_EXT_PNG;
        }
        else if (im.mimeType == MIMETYPE_JPEG)
        {
            updatedImage.uri = name + "_image" + std::to_string(imgId) + "." + FILE_EXT_JPEG;
        }
        else
        {
            // unknown mimetype
            updatedImage.uri = name + "_image" + std::to_string(imgId);
        }

        gltfDoc.images.Append(std::move(updatedImage));
        imgId++;
    }

    return gltfDoc;
}

void GLBToGLTF::UnpackGLB(const std::string& glbPath, const std::string& outDirectory, const std::string& gltfName)
{
    // read glb file into json
    auto glbStream = std::make_shared<std::ifstream>(glbPath, std::ios::binary);
    auto streamReader = std::make_shared<StreamMock>();
    GLBResourceReader reader(streamReader, glbStream);

    // get original json
    auto json = reader.GetJson();
    auto doc = Deserialize(json);

    // create new modified json
    auto gltfDoc = GLBToGLTF::CreateGLTFDocument(doc, gltfName);

    // serialize and write new gltf json
    auto gltfJson = Serialize(gltfDoc);
    std::ofstream outputStream(outDirectory + gltfName + "." + GLTF_EXTENSION);
    outputStream << gltfJson;
    outputStream.flush();

    // write images
    size_t bufferOffset = GetGLBBufferChunkOffset(glbStream.get());
    for (auto image : GLBToGLTF::GetImagesData(glbStream.get(), doc, gltfName, bufferOffset))
    {
        std::ofstream out(outDirectory + image.first, std::ios::binary);
        out.write(&image.second[0], image.second.size());
    }

    // get new buffer size and write new buffer
    if (gltfDoc.buffers.Size() != 0)
    {
        size_t newBufferSize = gltfDoc.buffers[0].byteLength;
        auto binFileData = GLBToGLTF::SaveBin(glbStream.get(), doc, bufferOffset, newBufferSize);
        std::ofstream out(outDirectory + gltfName + "." + BUFFER_EXTENSION, std::ios::binary);
        out.write(&binFileData[0], binFileData.size());
    }
}
