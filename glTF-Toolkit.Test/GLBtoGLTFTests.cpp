// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include <CppUnitTest.h>  

#include <numeric>
#include <GLTFSDK/GLTFDocument.h>
#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/Serialize.h>
#include <GLBtoGLTF.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
    std::wstring utf8Decode(const std::string& s)
    {
        std::wstring ret;
        std::for_each(s.begin(), s.end(), [&ret](char c)
        {
            ret += (wchar_t)c;
        });
        return ret;
    }

    std::string binBufferString(const std::vector<char>& vec)
    {
        std::string ret = "{";
        for (int i = 0; i < vec.size(); i++)
        {
            ret += (i ? "," : "") + std::to_string((int)vec[i]);
        }
        return ret += "}";
    }

    // Setup a GLTF document with 3 bufferviews and 2 images
    GLTFDocument setupGLBDocument1()
    {
        GLTFDocument glbDoc("0", {});
        Scene sc; sc.id = "0";
        glbDoc.scenes.Append(std::move(sc));
        Accessor acc0; acc0.bufferViewId = "0"; acc0.byteOffset = 0; acc0.id = "0";
        Accessor acc1; acc1.bufferViewId = "2"; acc1.byteOffset = 12; acc1.id = "1";
        Accessor acc2; acc2.bufferViewId = "1"; acc2.byteOffset = 4; acc2.id = "2";
        const std::vector<Accessor> accessors = { acc0, acc1, acc2 };
        std::for_each(accessors.begin(), accessors.end(), [&glbDoc](auto a)
        {
            glbDoc.accessors.Append(std::move(a));
        });
        BufferView bv0; bv0.bufferId = "0"; bv0.byteOffset = 0; bv0.byteLength = 8; bv0.id = "0";
        BufferView bv1; bv1.bufferId = "0"; bv1.byteOffset = 32; bv1.byteLength = 4; bv1.id = "1";
        BufferView bv2; bv2.bufferId = "0"; bv2.byteOffset = 72; bv2.byteLength = 2; bv2.id = "2";
        const std::vector<BufferView> bufferViews = { bv0, bv1, bv2 };
        std::for_each(bufferViews.begin(), bufferViews.end(), [&glbDoc](auto b)
        {
            glbDoc.bufferViews.Append(std::move(b));
        });
        Buffer b; b.id = "0", b.byteLength = 100;
        const std::vector<Buffer> buffers = { b };
        std::for_each(buffers.begin(), buffers.end(), [&glbDoc](auto b)
        {
            glbDoc.buffers.Append(std::move(b));
        });
        Image img0; img0.id = "0"; img0.mimeType = "image/png"; img0.bufferViewId = "1";
        Image img1; img1.id = "1"; img1.mimeType = "image/jpeg"; img1.bufferViewId = "2";
        const std::vector<Image> images = { img0, img1 };
        std::for_each(images.begin(), images.end(), [&glbDoc](auto img)
        {
            glbDoc.images.Append(std::move(img));
        });
        return glbDoc;
    }

    //sets up a stream with 'size' number of bytes, where reading the stream k times returns k
    std::stringstream* setupGLBStream(char size)
    {
        std::stringstream* ss = new std::stringstream();
        char* inputStream = new char[size];
        for (int i = 0; i < size; i++) inputStream[i] = (char)i;
        ss->write(inputStream, size);
        return ss;
    }

    TEST_CLASS(GLBToGLTFTests)
    {
        TEST_METHOD(GLBtoGLTF_NoImagesJSON)
        {
            GLTFDocument glbDoc("0", {});
            Scene s1; s1.id = "0";
            glbDoc.scenes.Append(std::move(s1));
            Accessor acc; acc.bufferViewId = "0"; acc.byteOffset = 36; acc.id = "0";
            const std::vector<Accessor> accessors = {};
            std::for_each(accessors.begin(), accessors.end(), [&glbDoc](auto a)
            {
                glbDoc.accessors.Append(std::move(a));
            });
            const std::vector<BufferView> bufferViews = {};
            std::for_each(bufferViews.begin(), bufferViews.end(), [&glbDoc](auto b)
            {
                glbDoc.bufferViews.Append(std::move(b));
            });
            const std::vector<Buffer> buffers = {};
            std::for_each(buffers.begin(), buffers.end(), [&glbDoc](auto b)
            {
                glbDoc.buffers.Append(std::move(b));
            });
            const std::vector<Image> images = {};
            std::for_each(images.begin(), images.end(), [&glbDoc](auto img)
            {
                glbDoc.images.Append(std::move(img));
            });
            GLTFDocument expectedGLTFDoc("0", {});
            Scene s2; s2.id = "0";
            expectedGLTFDoc.scenes.Append(std::move(s2));
            auto actualGLTFDoc = GLBToGLTF::CreateGLTFDocument(glbDoc, "name");

            // for debugging
            const auto expectedJSON = Serialize(expectedGLTFDoc, SerializeFlags::Pretty);
            const auto actualJSON = Serialize(actualGLTFDoc, SerializeFlags::Pretty);
            Assert::IsTrue(expectedGLTFDoc == actualGLTFDoc, utf8Decode(expectedJSON + "\n\n" + actualJSON).c_str());
        }

        TEST_METHOD(GLBtoGLTF_ImagesWithOffsetJSON)
        {
            GLTFDocument glbDoc("0", {});
            Scene sc; sc.id = "0";
            glbDoc.scenes.Append(std::move(sc));
            Accessor acc0; acc0.bufferViewId = "0"; acc0.byteOffset = 0; acc0.id = "0";
            Accessor acc1; acc1.bufferViewId = "3"; acc1.byteOffset = 12; acc1.id = "1";
            Accessor acc2; acc2.bufferViewId = "1"; acc2.byteOffset = 4; acc2.id = "2";
            Accessor acc3; acc3.bufferViewId = "2"; acc3.byteOffset = 4; acc3.id = "3";
            const std::vector<Accessor> accessors = { acc0, acc1, acc2, acc3 };
            std::for_each(accessors.begin(), accessors.end(), [&glbDoc](auto a)
            {
                glbDoc.accessors.Append(std::move(a));
            });
            BufferView bv0; bv0.bufferId = "0"; bv0.byteOffset = 0; bv0.byteLength = 400; bv0.id = "0";
            BufferView bv1; bv1.bufferId = "0"; bv1.byteOffset = 420; bv1.byteLength = 200; bv1.id = "1";
            BufferView bv2; bv2.bufferId = "0"; bv2.byteOffset = 620; bv2.byteLength = 320; bv2.id = "2";
            BufferView bv3; bv3.bufferId = "0"; bv3.byteOffset = 960; bv3.byteLength = 2000; bv3.id = "3";
            const std::vector<BufferView> bufferViews = { bv0, bv1, bv2, bv3 };
            std::for_each(bufferViews.begin(), bufferViews.end(), [&glbDoc](auto b)
            {
                glbDoc.bufferViews.Append(std::move(b));
            });
            Buffer b; b.id = "0", b.byteLength = 3000;
            const std::vector<Buffer> buffers = { b };
            std::for_each(buffers.begin(), buffers.end(), [&glbDoc](auto b)
            {
                glbDoc.buffers.Append(std::move(b));
            });
            Image img0; img0.id = "0"; img0.mimeType = "image/png"; img0.bufferViewId = "1";
            Image img1; img1.id = "1"; img1.mimeType = "image/jpeg"; img1.bufferViewId = "3";
            const std::vector<Image> images = { img0, img1 };
            std::for_each(images.begin(), images.end(), [&glbDoc](auto img)
            {
                glbDoc.images.Append(std::move(img));
            });
            auto actualGLTFDoc = GLBToGLTF::CreateGLTFDocument(glbDoc, "test");

            GLTFDocument expectedGLTFDoc("0", {});
            Accessor exp_acc0; exp_acc0.bufferViewId = "0"; exp_acc0.byteOffset = 0; exp_acc0.id = "0";
            Accessor exp_acc1; exp_acc1.bufferViewId = "1"; exp_acc1.byteOffset = 4; exp_acc1.id = "3";
            expectedGLTFDoc.accessors.Append(std::move(exp_acc0));
            expectedGLTFDoc.accessors.Append(std::move(exp_acc1));

            BufferView exp_bv0; exp_bv0.bufferId = "0"; exp_bv0.byteOffset = 0; exp_bv0.byteLength = 400; exp_bv0.id = "0";
            BufferView exp_bv1; exp_bv1.bufferId = "0"; exp_bv1.byteOffset = 400; exp_bv1.byteLength = 320; exp_bv1.id = "1";
            expectedGLTFDoc.bufferViews.Append(std::move(exp_bv0));
            expectedGLTFDoc.bufferViews.Append(std::move(exp_bv1));

            Image exp_img0; exp_img0.id = "0"; exp_img0.uri = "test_image0.png";
            Image exp_img1; exp_img1.id = "1"; exp_img1.uri = "test_image1.jpg";
            expectedGLTFDoc.images.Append(std::move(exp_img0));
            expectedGLTFDoc.images.Append(std::move(exp_img1));

            Buffer exp_b; exp_b.id = "0"; exp_b.byteLength = 720; exp_b.uri = "test.bin";
            expectedGLTFDoc.buffers.Append(std::move(exp_b));

            Scene sc2; sc2.id = "0";
            expectedGLTFDoc.scenes.Append(std::move(sc2));

            // for debugging
            const auto expectedJSON = Serialize(expectedGLTFDoc, SerializeFlags::Pretty);
            const auto actualJSON = Serialize(actualGLTFDoc, SerializeFlags::Pretty);
            Assert::IsTrue(expectedGLTFDoc == actualGLTFDoc, utf8Decode(expectedJSON + "\n\n" + actualJSON).c_str());
        }

        TEST_METHOD(GLBtoGLTF_ImageDataTest)
        {
            auto glbDoc = setupGLBDocument1();
            auto glbStream = setupGLBStream(100);
            const std::string TEST_NAME = "test3";
            const size_t BYTE_OFFSET = 12;
            auto imageData = GLBToGLTF::GetImagesData(glbStream, glbDoc, TEST_NAME, BYTE_OFFSET);
            delete glbStream;

            // these bytes corresponds to bytes of image0 and image1 in setupGLBDocument1
            std::vector<std::vector<char>> expectedImage = {
                {BYTE_OFFSET + 32, BYTE_OFFSET + 33, BYTE_OFFSET + 34, BYTE_OFFSET + 35},
                {BYTE_OFFSET + 72, BYTE_OFFSET + 73},
            };

            int imgId = 0;
            for (auto it = imageData.begin(); it != imageData.end(); it++, imgId++)
            {
                Assert::IsTrue(it->second == expectedImage[imgId], utf8Decode(binBufferString(it->second) + '\n' + binBufferString(expectedImage[imgId])).c_str());
            }
        }

        TEST_METHOD(GLBtoGLTF_MeshDataTest)
        {
            auto glbDoc = setupGLBDocument1();
            auto glbStream = setupGLBStream(100);
            const size_t BYTE_OFFSET = 12;
            auto actualData = GLBToGLTF::SaveBin(glbStream, glbDoc, BYTE_OFFSET, 8);

            //these bytes correspond to bytes of bufferviews in steupGLTFDocument1 which don't belong to any image
            std::vector<char> expectedData = { BYTE_OFFSET + 0, BYTE_OFFSET + 1, BYTE_OFFSET + 2, BYTE_OFFSET + 3,
                BYTE_OFFSET + 4, BYTE_OFFSET + 5, BYTE_OFFSET + 6, BYTE_OFFSET + 7 };
            Assert::IsTrue(actualData == expectedData, utf8Decode(binBufferString(actualData) + '\n' + binBufferString(expectedData)).c_str());
        }
    };
}