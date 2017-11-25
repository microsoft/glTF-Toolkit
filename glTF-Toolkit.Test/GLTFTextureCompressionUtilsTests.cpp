// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.
#include "pch.h"
#include <CppUnitTest.h>  

#include "GLTFSDK/IStreamWriter.h"
#include "GLTFSDK/GLTFConstants.h"
#include "GLTFSDK/Serialize.h"
#include "GLTFSDK/Deserialize.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLTFResourceWriter.h"

#include "GLTFTextureCompressionUtils.h"

#include "Helpers/WStringUtils.h"
#include "Helpers/StreamMock.h"
#include "Helpers/TestUtils.h"

#include <DirectXTex.h>


using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft
{
    namespace  glTF
    {
        namespace Test
        {
            // Note: some tests are using BC3 since it's faster to run that algorithm vs. BC7

            TEST_CLASS(GLTFTextureCompressionUtilsTests)
            {
                const char* c_baseColorPng = "Resources\\gltf\\WaterBottle_ORM\\WaterBottle_baseColor.png";
                const char* c_baseColorBC7 = "Resources\\gltf\\WaterBottle_ORM\\WaterBottle_baseColor.DDS";
                const char* c_waterBottleORMJson = "Resources\\gltf\\WaterBottle_ORM\\WaterBottle.gltf";

                TEST_METHOD(GLTFTextureCompressionUtils_CompressImage_BC7)
                {
                    // Load png
                    auto png = TestUtils::ReadLocalAsset(TestUtils::GetAbsolutePath(c_baseColorPng));
                    std::vector<uint8_t> pngData = StreamUtils::ReadBinaryFull<uint8_t>(*png);

                    // ddsImage <= load DDS
                    DirectX::ScratchImage ddsImage;
                    DirectX::TexMetadata info;
                    DirectX::LoadFromDDSFile(TestUtils::GetAbsolutePathW(c_baseColorBC7).c_str(), DirectX::WIC_FLAGS_NONE, &info, ddsImage);

                    // compressedPng <= convert using BC7
                    DirectX::ScratchImage compressedPng;
                    DirectX::LoadFromWICMemory(pngData.data(), pngData.size(), DirectX::WIC_FLAGS_NONE, &info, compressedPng);

                    GLTFTextureCompressionUtils::CompressImage(compressedPng, TextureCompression::BC7);

                    auto ddsMip0 = ddsImage.GetImage(0, 0, 0);
                    size_t ddsImageSize = ddsMip0->height * ddsMip0->width;
                    Assert::AreEqual(ddsImageSize, compressedPng.GetPixelsSize(), L"ddsImage and compressedPng lengths are not the same");

                    Assert::IsTrue(memcmp(ddsMip0->pixels, compressedPng.GetPixels(), ddsImageSize), L"ddsImage and compressedPng are not the same");
                }

                TEST_METHOD(GLTFTextureCompressionUtils_CompressTextureAsDDS_NoCompression)
                {
                    // This asset has all textures
                    TestUtils::LoadAndExecuteGLTFTest(c_waterBottleORMJson, [](auto doc, auto path)
                    {
                        auto compressedDoc = GLTFTextureCompressionUtils::CompressTextureAsDDS(TestStreamReader(path), doc, doc.textures.Get("0"), TextureCompression::None, "");

                        // Check that nothing changed
                        Assert::IsTrue(doc == compressedDoc);
                    });
                }

                TEST_METHOD(GLTFTextureCompressionUtils_CompressTextureAsDDS_CompressBC3_NoMips_Retain)
                {
                    // This asset has all textures
                    TestUtils::LoadAndExecuteGLTFTest(c_waterBottleORMJson, [](auto doc, auto path)
                    {
                        auto generateMipMaps = false;
                        auto retainOriginalImages = true;
                        auto compressedDoc = GLTFTextureCompressionUtils::CompressTextureAsDDS(TestStreamReader(path), doc, doc.textures.Get("0"), TextureCompression::BC3, "", generateMipMaps, retainOriginalImages);

                        auto originalTexture = doc.textures.Get("0");
                        auto compressedTexture = compressedDoc.textures.Get("0");

                        // Check that the image has not been replaced
                        Assert::IsTrue(originalTexture.imageId == compressedTexture.imageId);

                        // Check that the image has been added
                        Assert::IsTrue(doc.images.Size() + 1 == compressedDoc.images.Size());

                        // Check that the texture now has the extension
                        Assert::IsTrue(originalTexture.extensions.size() + 1 == compressedTexture.extensions.size());

                        // Check the new extension is not empty
                        auto ddsExtension = compressedTexture.extensions.at(std::string(EXTENSION_MSFT_TEXTURE_DDS));
                        Assert::IsFalse(ddsExtension.empty());

                        // Check the new extension contains a DDS image
                        rapidjson::Document ddsJson;
                        ddsJson.Parse(ddsExtension.c_str());

                        Assert::IsTrue(ddsJson["source"].IsInt());

                        auto ddsImageId = std::to_string(ddsJson["source"].GetInt());

                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).mimeType == "image/vnd-ms.dds");
                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).uri == "texture_0_nomips_BC3.dds");
                    });
                }

                TEST_METHOD(GLTFTextureCompressionUtils_CompressTextureAsDDS_CompressBC3_NoMips_Replace)
                {
                    // This asset has all textures
                    TestUtils::LoadAndExecuteGLTFTest(c_waterBottleORMJson, [](auto doc, auto path)
                    {
                        auto generateMipMaps = false;
                        auto retainOriginalImages = false;
                        auto compressedDoc = GLTFTextureCompressionUtils::CompressTextureAsDDS(TestStreamReader(path), doc, doc.textures.Get("0"), TextureCompression::BC3, "", generateMipMaps, retainOriginalImages);

                        auto originalTexture = doc.textures.Get("0");
                        auto compressedTexture = compressedDoc.textures.Get("0");

                        // Check that the texture is still pointing to the same image
                        Assert::IsTrue(originalTexture.imageId == compressedTexture.imageId);

                        // Check that an image has not been added
                        Assert::IsTrue(doc.images.Size() == compressedDoc.images.Size());

                        // Check that the texture now has the extension
                        Assert::IsTrue(originalTexture.extensions.size() + 1 == compressedTexture.extensions.size());

                        // Check the new extension is not empty
                        auto ddsExtension = compressedTexture.extensions.at(std::string(EXTENSION_MSFT_TEXTURE_DDS));
                        Assert::IsFalse(ddsExtension.empty());

                        // Check the new extension contains a DDS image
                        rapidjson::Document ddsJson;
                        ddsJson.Parse(ddsExtension.c_str());

                        Assert::IsTrue(ddsJson["source"].IsInt());

                        auto ddsImageId = std::to_string(ddsJson["source"].GetInt());

                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).mimeType == "image/vnd-ms.dds");
                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).uri == "texture_0_nomips_BC3.dds");

                        // Check the extension points to the same image as the source (image was replaced)
                        Assert::AreEqual(compressedTexture.imageId, ddsImageId);
                    });
                }

                TEST_METHOD(GLTFTextureCompressionUtils_CompressTextureAsDDS_CompressBC7_Mips_Retain)
                {
                    // This asset has all textures
                    TestUtils::LoadAndExecuteGLTFTest(c_waterBottleORMJson, [](auto doc, auto path)
                    {
                        auto generateMipMaps = true;
                        auto retainOriginalImages = true;
                        auto compressedDoc = GLTFTextureCompressionUtils::CompressTextureAsDDS(TestStreamReader(path), doc, doc.textures.Get("0"), TextureCompression::BC7, "", generateMipMaps, retainOriginalImages);

                        auto originalTexture = doc.textures.Get("0");
                        auto compressedTexture = compressedDoc.textures.Get("0");

                        // Check that the image has not been replaced
                        Assert::IsTrue(originalTexture.imageId == compressedTexture.imageId);

                        // Check that the image has been added
                        Assert::IsTrue(doc.images.Size() + 1 == compressedDoc.images.Size());

                        // Check that the texture now has the extension
                        Assert::IsTrue(originalTexture.extensions.size() + 1 == compressedTexture.extensions.size());

                        // Check the new extension is not empty
                        auto ddsExtension = compressedTexture.extensions.at(std::string(EXTENSION_MSFT_TEXTURE_DDS));
                        Assert::IsFalse(ddsExtension.empty());

                        // Check the new extension contains a DDS image
                        rapidjson::Document ddsJson;
                        ddsJson.Parse(ddsExtension.c_str());

                        Assert::IsTrue(ddsJson["source"].IsInt());

                        auto ddsImageId = std::to_string(ddsJson["source"].GetInt());

                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).mimeType == "image/vnd-ms.dds");
                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).uri == "texture_0_BC7.dds");
                    });
                }

                TEST_METHOD(GLTFTextureCompressionUtils_CompressAllTexturesForWindowsMR_Retain)
                {
                    // This asset has all textures
                    TestUtils::LoadAndExecuteGLTFTest(c_waterBottleORMJson, [](auto doc, auto path)
                    {
                        auto retainOriginalImages = true;
                        auto compressedDoc = GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(TestStreamReader(path), doc, "", retainOriginalImages);

                        // Check that the materials and textures have not been replaced
                        // Check that the textures has not been replaced
                        Assert::IsTrue(doc.textures.Size() == compressedDoc.textures.Size());
                        Assert::IsTrue(doc.materials.Size() == compressedDoc.materials.Size());

                        // Check that the images have been added (base, emissive, RMO and normal)
                        Assert::AreEqual(doc.images.Size() + 4, compressedDoc.images.Size());

                        auto originalMaterial = doc.materials.Get("0");
                        auto compressedMaterial = compressedDoc.materials.Get("0");

                        // Check that all relevant textures now have the extension
                        Assert::IsTrue(doc.textures.Get(originalMaterial.metallicRoughness.baseColorTextureId).extensions.size() + 1 == compressedDoc.textures.Get(compressedMaterial.metallicRoughness.baseColorTextureId).extensions.size());
                        Assert::IsTrue(doc.textures.Get(originalMaterial.emissiveTextureId).extensions.size() + 1 == compressedDoc.textures.Get(compressedMaterial.emissiveTextureId).extensions.size());
                        // TODO: read the WMR (MSFT_packing...) textures as well

                        // Check the new extension is not empty
                        auto ddsExtension = compressedDoc.textures.Get(compressedMaterial.emissiveTextureId).extensions.at(std::string(EXTENSION_MSFT_TEXTURE_DDS));
                        Assert::IsFalse(ddsExtension.empty());

                        // Check the new extension contains a DDS image
                        rapidjson::Document ddsJson;
                        ddsJson.Parse(ddsExtension.c_str());

                        Assert::IsTrue(ddsJson["source"].IsInt());

                        auto ddsImageId = std::to_string(ddsJson["source"].GetInt());

                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).mimeType == "image/vnd-ms.dds");
                        std::string expectedSuffix = "_BC7.dds";
                        Assert::IsTrue(compressedDoc.images.Get(ddsImageId).uri.compare(9, expectedSuffix.size(), expectedSuffix) == 0); // The emissive texture should have mips and be BC7
                    });
                }
            };
        }
    }
}

