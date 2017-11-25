// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include <CppUnitTest.h>  

#include "GLTFSDK/IStreamWriter.h"
#include "GLTFSDK/GLTFConstants.h"
#include "GLTFSDK/Deserialize.h"
#include "GLTFSDK/GLBResourceWriter.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLTFResourceWriter.h"

#include "SerializeBinary.h"

#include "Helpers/TestUtils.h"
#include "Helpers/WStringUtils.h"
#include "Helpers/StreamMock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft
{
    namespace  glTF
    {
        namespace Test
        {
            class InMemoryStreamFactory : public Microsoft::glTF::IStreamFactory
            {
            public:
                InMemoryStreamFactory(std::shared_ptr<std::stringstream> stream) :
                    m_stream(stream),
                    m_tempStream(std::make_shared<std::stringstream>(std::ios_base::app | std::ios_base::binary | std::ios_base::in | std::ios_base::out))
                { }

                std::shared_ptr<std::istream> GetInputStream(const std::string& uri) const override
                {
                    return uri == GLB_BUFFER_ID ? m_tempStream : m_stream;
                }

                std::shared_ptr<std::ostream> GetOutputStream(const std::string& uri) const override
                {
                    return uri == GLB_BUFFER_ID ? m_tempStream : m_stream;
                }

                std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override
                {
                    return m_tempStream;
                }
            private:
                std::shared_ptr<std::stringstream> m_stream;
                std::shared_ptr<std::stringstream> m_tempStream;
            };

            TEST_CLASS(GLBSerializerTests)
            {

                static std::string ReadLocalJson(const char * relativePath)
                {
                    auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(relativePath));
                    auto json = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                    return json;
                }

                static std::shared_ptr<GLTFDocument> ImportGLB(const std::shared_ptr<IStreamReader>& streamReader, const std::shared_ptr<std::istream>& glbStream)
                {
                    GLBResourceReader resourceReader(*streamReader, glbStream);
                    auto json = resourceReader.GetJson();

                    auto doc = DeserializeJson(json);

                    return std::make_shared<GLTFDocument>(doc);
                }

                static std::shared_ptr<GLTFDocument> ImportGLTF(const std::shared_ptr<IStreamReader>& streamReader, const std::shared_ptr<std::istream>& stream)
                {
                    GLTFResourceReader resourceReader(*streamReader);
                    auto json = std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());

                    auto doc = DeserializeJson(json);

                    return std::make_shared<GLTFDocument>(doc);
                }

                const char* c_waterBottleJson = "Resources\\gltf\\WaterBottle\\WaterBottle.gltf";

                TEST_METHOD(GLBSerializerTests_RoundTrip_Simple)
                {
                    auto data = ReadLocalJson(c_waterBottleJson);
                    auto input = std::make_shared<std::stringstream>(data);
                    try
                    {
                        // Deserialize input json
                        auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                        auto doc = DeserializeJson(inputJson);

                        // Serialize GLTFDocument to GLB
                        TestStreamReader streamReader(TestUtils::GetAbsolutePath(c_waterBottleJson));
                        auto stream = std::make_shared<std::stringstream>(std::ios_base::app | std::ios_base::binary | std::ios_base::in | std::ios_base::out);
                        std::unique_ptr<const IStreamFactory> streamFactory = std::make_unique<InMemoryStreamFactory>(stream);
                        SerializeBinary(doc, streamReader, streamFactory);

                        // Deserialize the GLB again
                        auto glbReader = std::make_unique<GLBResourceReader>(streamReader, stream);
                        auto outputJson = glbReader->GetJson();
                        auto outputDoc = DeserializeJson(outputJson);

                        // Check some structural elements
                        Assert::AreEqual(doc.nodes.Size(), outputDoc.nodes.Size());
                        Assert::AreEqual(doc.images.Size(), outputDoc.images.Size());

                        // There must be only one buffer, and it can't have a URI
                        Assert::AreEqual(static_cast<size_t>(1), outputDoc.buffers.Size());
                        auto glbBuffer = outputDoc.buffers.Elements()[0];
                        Assert::IsTrue(glbBuffer.uri.empty());

                        // Check that the images that were stored as URI are now bufferViews
                        for (auto image : outputDoc.images.Elements())
                        {
                            // Images in GLB don't have a URI
                            Assert::IsTrue(image.uri.empty());

                            // Images in GLB are stored in a buffer
                            Assert::IsFalse(image.bufferViewId.empty());

                            // Images in GLB don't have a URI
                            Assert::IsFalse(doc.images.Get(image.id).uri.empty());
                        }

                        // All buffer views must point to the GLB buffer
                        for (auto bufferView : outputDoc.bufferViews.Elements())
                        {
                            Assert::IsTrue(bufferView.bufferId == glbBuffer.id);
                        }

                        // Read one of the images and check it's identical
                        auto gltfReader = std::make_unique<GLTFResourceReader>(streamReader);
                        std::vector<uint8_t> gltfImage = gltfReader->ReadBinaryData(doc, doc.images.Elements()[0]);
                        std::vector<uint8_t> glbImage = glbReader->ReadBinaryData(outputDoc, outputDoc.images.Elements()[0]);
                        Assert::IsTrue(gltfImage == glbImage); // Vector comparison
                    }
                    catch (std::exception ex)
                    {
                        std::stringstream ss;
                        ss << "Received exception was unexpected. Got: " << ex.what();
                        Assert::Fail(WStringUtils::ToWString(ss).c_str());
                    }
                }
            };
        }
    }
}