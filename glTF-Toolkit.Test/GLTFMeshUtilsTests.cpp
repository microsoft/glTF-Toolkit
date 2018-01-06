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
#include "GLTFSDK/IStreamFactory.h"

#include <GLTFMeshUtils.h>
#include <experimental\filesystem>
#include <regex>

#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
    using namespace std::experimental::filesystem;

    const char* s_TestFiles[] ={
        "Resources\\gltf\\2CylinderEngine\\2CylinderEngine.gltf",
        "Resources\\gltf\\BoxAnimated\\BoxAnimated.gltf",
        "Resources\\gltf\\03_all_animations\\03_all_animations.gltf",
        "Resources\\gltf\\03_skinned_cylinder\\03_skinned_cylinder.gltf",
        "Resources\\gltf\\GearboxAssy\\GearboxAssy.gltf",
        "Resources\\gltf\\Primitives\\Primitives.gltf"
    };
    const size_t s_TestFileIdx = 5;

    TEST_CLASS(GLTFMeshUtilsTest)
    {
        void ExecuteTest(const char* GLTFRelPath, const MeshOptions options)
        {
            TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [&](const GLTFDocument& doc, const std::string& path)
            {
                std::regex dataUriRegex = std::regex(R"(^data:(?:application|image)/.+;base\d{1,2},)");

                std::string outputName = TestUtils::GetFilenameExt(path.c_str());
                std::string basePath = TestUtils::GetBasePath(path.c_str());
                std::string outputDirectory = basePath + "..\\" + TestUtils::GetFilename(outputName) + "_OpMesh\\";

                auto outputDoc = GLTFMeshUtils::ProcessMeshes(outputName, doc, TestStreamReader(path), options, outputDirectory);
                
                // Create output directory and copy files referenced by the output document.
                create_directories(outputDirectory);
                for (const auto& p : outputDoc.buffers.Elements())
                {
                    if (std::regex_search(p.uri, dataUriRegex))
                    {
                        continue;
                    }

                    std::string filePath = basePath + p.uri;
                    if (exists(filePath))
                    {
                        copy_file(filePath, outputDirectory + p.uri, copy_options::skip_existing);
                    }
                }
                for (const auto& p : outputDoc.images.Elements())
                {
                    if (std::regex_search(p.uri, dataUriRegex))
                    {
                        continue;
                    }

                    std::string filePath = basePath + p.uri;
                    if (exists(filePath))
                    {
                        copy_file(filePath, outputDirectory + p.uri, copy_options::skip_existing);
                    }
                }

                std::string Json = Serialize(outputDoc, SerializeFlags::Pretty);
                TestStreamWriter(outputDirectory).GetOutputStream(outputName)->write(Json.c_str(), Json.size());
            });
        }

        TEST_METHOD(GLTFMeshUtils_Default)
        {
            MeshOptions options;
            options.Optimize = true;
            options.GenerateTangentSpace = true;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_Optimize)
        {
            MeshOptions options;
            options.Optimize = true;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_Tangents)
        {
            MeshOptions options;
            options.Optimize = false;
            options.GenerateTangentSpace = true;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_CI)
        {
            MeshOptions options;
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Interleave;
            options.PrimitiveFormat = PrimitiveFormat::Combine;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_CS)
        {
            MeshOptions options;
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Combine;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_SI)
        {
            MeshOptions options;
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Interleave;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }

        TEST_METHOD(GLTFMeshUtils_SS)
        {
            MeshOptions options;
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            ExecuteTest(s_TestFiles[s_TestFileIdx], options);
        }
    };
}
