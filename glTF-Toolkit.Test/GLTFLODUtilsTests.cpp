// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include <CppUnitTest.h>  

#include "GLTFSDK/IStreamWriter.h"
#include "GLTFSDK/Constants.h"
#include "GLTFSDK/Serialize.h"
#include "GLTFSDK/Deserialize.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLTFResourceWriter.h"
#include "GLTFSDK/RapidJsonUtils.h"
#include "GLTFSDK/ExtensionsKHR.h"

#include "GLTFLODUtils.h"

#include "Helpers/WStringUtils.h"
#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
    TEST_CLASS(GLTFLODUtilsTests)
    {
        static void CheckGLTFLODNodeCountAgainstOriginal(Document& doc, Document& docWLod, size_t lodCount)
        {
            // All elements in the lod'd doc should be double the original
            Assert::IsTrue(doc.buffers.Size() * lodCount == docWLod.buffers.Size());
            Assert::IsTrue(doc.accessors.Size() * lodCount == docWLod.accessors.Size());
            Assert::IsTrue(doc.bufferViews.Size() * lodCount == docWLod.bufferViews.Size());
            Assert::IsTrue(doc.materials.Size() * lodCount == docWLod.materials.Size());
            Assert::IsTrue(doc.images.Size() * lodCount == docWLod.images.Size());
            Assert::IsTrue(doc.meshes.Size() * lodCount == docWLod.meshes.Size());
            Assert::IsTrue(doc.nodes.Size() * lodCount == docWLod.nodes.Size());
            Assert::IsTrue(doc.textures.Size() * lodCount == docWLod.textures.Size());
            Assert::IsTrue(doc.samplers.Size() * lodCount == docWLod.samplers.Size());

            // Scene count should be untouched
            Assert::IsTrue(doc.scenes.Size() == docWLod.scenes.Size());
        }

        static void CheckGLTFLODCount(const char * gltfDocPath, uint32_t expectedNumberOfLods)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(gltfDocPath));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                GLTFResourceReader resourceReader(readwriter);
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson, KHR::GetKHRExtensionDeserializer());
                auto lods = GLTFLODUtils::ParseDocumentNodeLODs(doc);

                Assert::IsTrue(GLTFLODUtils::NumberOfNodeLODLevels(doc, lods) == expectedNumberOfLods);
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }

        static std::shared_ptr<Document> ImportGLTF(const std::shared_ptr<IStreamReader>& streamReader, const std::shared_ptr<std::istream>& stream)
        {
            GLTFResourceReader resourceReader(streamReader);
            auto json = std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());

            auto doc = Deserialize(json, KHR::GetKHRExtensionDeserializer());

            return std::make_shared<Document>(doc);
        }

        const char* c_cubeAsset3DJson = "Resources\\gltf\\cubeAsset3D.gltf";
        const char* c_cubeWithLODJson = "Resources\\gltf\\cubeWithLOD.gltf";

        TEST_METHOD(GLTFLODUtils_NodeLodCount)
        {
            CheckGLTFLODCount(c_cubeAsset3DJson, 0);
        }
        TEST_METHOD(GLTFLODUtils_NodeLodCount_DocWithLODs)
        {
            CheckGLTFLODCount(c_cubeWithLODJson, 1);
        }

        TEST_METHOD(GLTFLODUtils_GLTFNodeLODMerge)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_cubeAsset3DJson));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                // Deserialize input json
                GLTFResourceReader resourceReader(readwriter);
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson, KHR::GetKHRExtensionDeserializer());

                std::vector<Document> docs;
                docs.push_back(doc);
                docs.push_back(doc);
                auto newlodgltfDoc = GLTFLODUtils::MergeDocumentsAsLODs(docs);

                // Serialize Document back to json
                auto outputJson = Serialize(newlodgltfDoc, KHR::GetKHRExtensionSerializer());

                CheckGLTFLODNodeCountAgainstOriginal(doc, newlodgltfDoc, 2);

                // Check Node Lods are correctly stored and labelled in the document
                auto nodes = newlodgltfDoc.nodes.Elements();
                auto lods = GLTFLODUtils::ParseDocumentNodeLODs(newlodgltfDoc);
                bool validLodExtension = false;
                bool containsLOD1RootNode = false;
                bool containsLOD1PolyNode = false;
                for (auto node : nodes)
                {
                    if (node.name == "root" && (std::find(lods[node.id]->begin(), lods[node.id]->end(), "3") != lods[node.id]->end()))
                    {
                        validLodExtension = true;
                    }
                    if (node.name == "root_lod1")
                    {
                        containsLOD1RootNode = true;
                    }
                    if (node.name == "polygon_lod1")
                    {
                        containsLOD1PolyNode = true;
                    }
                }
                Assert::IsTrue(validLodExtension);
                Assert::IsTrue(containsLOD1RootNode);
                Assert::IsTrue(containsLOD1PolyNode);
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }

        TEST_METHOD(GLTFLODUTils_GLTFNodeLODMergeMultiple)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_cubeAsset3DJson));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                // Deserialize input json
                GLTFResourceReader resourceReader(readwriter);
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson, KHR::GetKHRExtensionDeserializer());

                std::vector<Document> docs;
                docs.push_back(doc);
                docs.push_back(doc);
                docs.push_back(doc);

                auto newlodgltfDoc = GLTFLODUtils::MergeDocumentsAsLODs(docs);

                CheckGLTFLODNodeCountAgainstOriginal(doc, newlodgltfDoc, 3);

                // Check Node Lods are correctly stored and labelled in the document
                auto nodes = newlodgltfDoc.nodes.Elements();
                auto lods = GLTFLODUtils::ParseDocumentNodeLODs(newlodgltfDoc);
                bool validLodExtension = false;
                bool containsLOD1RootNode = false;
                bool containsLOD1PolyNode = false;
                bool containsLOD2RootNode = false;
                bool containsLOD2PolyNode = false;
                for (auto node : nodes)
                {
                    if (node.name == "root" &&
                        (std::find(lods[node.id]->begin(), lods[node.id]->end(), "3") != lods[node.id]->end()) &&
                        (std::find(lods[node.id]->begin(), lods[node.id]->end(), "5") != lods[node.id]->end())
                        )
                    {
                        validLodExtension = true;
                    }
                    if (node.name == "root_lod1") containsLOD1RootNode = true;
                    if (node.name == "polygon_lod1") containsLOD1PolyNode = true;
                    if (node.name == "root_lod2") containsLOD2RootNode = true;
                    if (node.name == "polygon_lod2") containsLOD2PolyNode = true;
                }

                Assert::IsTrue(validLodExtension);
                Assert::IsTrue(containsLOD1RootNode);
                Assert::IsTrue(containsLOD1PolyNode);
                Assert::IsTrue(containsLOD2RootNode);
                Assert::IsTrue(containsLOD2PolyNode);

                // Serialize Document back to json
                auto outputJson = Serialize(newlodgltfDoc, KHR::GetKHRExtensionSerializer());
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }

        TEST_METHOD(GLTFLODUtils_GLTFNodeLODMergeScreenCoverage)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_cubeAsset3DJson));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                // Deserialize input json
                GLTFResourceReader resourceReader(readwriter);
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson, KHR::GetKHRExtensionDeserializer());

                std::vector<Document> docs;
                docs.push_back(doc);
                docs.push_back(doc);
                docs.push_back(doc);

                std::vector<double> screenCoverages{ 0.5, 0.2, 0.01 };

                auto newlodgltfDoc = GLTFLODUtils::MergeDocumentsAsLODs(docs, screenCoverages);

                CheckGLTFLODNodeCountAgainstOriginal(doc, newlodgltfDoc, 3);

                // Check Node Lods have correct screen coverage values
                auto nodes = newlodgltfDoc.nodes.Elements();
                bool rootNodeContainsCoverage = false;
                for (auto node : nodes)
                {
                    if (node.name == "root" && !node.extras.empty())
                    {
                        auto extrasJson = RapidJsonUtils::CreateDocumentFromString(node.extras);

                        Assert::IsTrue(extrasJson.IsObject());
                        Assert::IsTrue(extrasJson["MSFT_screencoverage"].IsArray());
                        Assert::IsTrue(extrasJson["MSFT_screencoverage"].GetArray().Size() == 3);

                        rootNodeContainsCoverage = true;
                    }
                }

                Assert::IsTrue(rootNodeContainsCoverage);

                // Serialize Document back to json
                auto outputJson = Serialize(newlodgltfDoc, KHR::GetKHRExtensionSerializer());
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }

        TEST_METHOD(GLTFLODUtils_DeserialiseNodeLODExtension)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_cubeWithLODJson));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                auto gltfDoc = ImportGLTF(readwriter, input);
                auto nodes = gltfDoc->nodes.Elements();
                Assert::IsTrue(nodes.size() == 4);
                auto lods = GLTFLODUtils::ParseDocumentNodeLODs(*gltfDoc);
                bool validLodExtension = false;
                for (auto node : nodes)
                {
                    if (node.name == "root" && (std::find(lods[node.id]->begin(), lods[node.id]->end(), "3") != lods[node.id]->end()))
                    {
                        validLodExtension = true;
                        break;
                    }
                }
                Assert::IsTrue(validLodExtension);
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }

        TEST_METHOD(GLTFLODUtils_DeserializeSerializeLoopNodeLODExtension)
        {
            auto input = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_cubeWithLODJson));
            auto readwriter = std::make_shared<StreamMock>();
            try
            {
                // Deserialize input json
                GLTFResourceReader resourceReader(readwriter);
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson, KHR::GetKHRExtensionDeserializer());

                // Serialize Document back to json
                auto outputJson = Serialize(doc, KHR::GetKHRExtensionSerializer());
                auto outputDoc = Deserialize(outputJson, KHR::GetKHRExtensionDeserializer());

                // Compare input and output GLTFDocuments
                Assert::AreNotSame(doc == outputDoc, true, L"Input gltf and output gltf are not equal");

                // Specifically ensure Node LODs are preserved through de/serialization loop
                auto nodes = outputDoc.nodes.Elements();
                Assert::IsTrue(nodes.size() == 4);
                auto lods = GLTFLODUtils::ParseDocumentNodeLODs(outputDoc);
                bool validLodExtension = false;
                for (auto node : nodes)
                {
                    if (node.name == "root" && (std::find(lods[node.id]->begin(), lods[node.id]->end(), "3") != lods[node.id]->end()))
                    {
                        validLodExtension = true;
                        break;
                    }
                }
                Assert::IsTrue(validLodExtension);
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

