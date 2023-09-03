// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.
#include "pch.h"
#include <CppUnitTest.h>  

#include <experimental\filesystem>
#include <regex>

#include <GLTFSDK/IStreamWriter.h>
#include <GLTFSDK/Serialize.h>
#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/GLTFResourceWriter.h>

#include <GLTFMeshUtils.h>

#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
    using namespace std::experimental::filesystem;

    const char* s_testFiles[] = {
        "Resources\\gltf\\2CylinderEngine\\2CylinderEngine.gltf",
        "Resources\\gltf\\BoxAnimated\\BoxAnimated.gltf",
        "Resources\\gltf\\03_all_animations\\03_all_animations.gltf",
        "Resources\\gltf\\03_skinned_cylinder\\03_skinned_cylinder.gltf",
        "Resources\\gltf\\GearboxAssy\\GearboxAssy.gltf",
        "Resources\\gltf\\WaterBottle\\WaterBottle.gltf",
        "Resources\\gltf\\Primitives\\Primitives.gltf",
    };

    TEST_CLASS(GLTFMeshUtilsTest)
    {
        TEST_METHOD(GLTFMeshUtils_Default)
        {
            MeshOptions options = {};
            options.Optimize = true;
            options.GenerateTangentSpace = true;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_Optimize)
        {
            MeshOptions options = {};
            options.Optimize = true;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_Tangents)
        {
            MeshOptions options = {};
            options.Optimize = false;
            options.GenerateTangentSpace = true;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_CombinedInterleaved)
        {
            MeshOptions options = {};
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Interleave;
            options.PrimitiveFormat = PrimitiveFormat::Combine;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_CombinedSeparated)
        {
            MeshOptions options = {};
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Combine;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_SeparateInterleaved)
        {
            MeshOptions options = {};
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Interleave;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        TEST_METHOD(GLTFMeshUtils_SeparateSeparate)
        {
            MeshOptions options = {};
            options.Optimize = false;
            options.GenerateTangentSpace = false;
            options.AttributeFormat = AttributeFormat::Separate;
            options.PrimitiveFormat = PrimitiveFormat::Separate;

            for (int i = 0; i < _countof(s_testFiles); ++i)
            {
                ExecuteTest(s_testFiles[i], options);
            }
        }

        void ExecuteTest(const char* GLTFRelPath, const MeshOptions& options)
        {
            TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [&](const Document& doc, const std::string& path)
            {
                std::string outputFilename = TestUtils::GetFilename(path.c_str());
                std::string basePath = TestUtils::GetBasePath(path.c_str());
                std::string baseName = TestUtils::GetFilenameNoExtension(path.c_str());

                std::string outputDirectory = canonical(basePath + "..\\tests\\" + baseName + "_" + StringifyOptions(options)).string() + "\\";
                printf(outputDirectory.c_str());
                create_directories(outputDirectory);

                Document outputDoc = GLTFMeshUtils::Process(doc, options, baseName, std::make_shared<TestStreamReader>(path), std::make_unique<TestStreamWriter>(outputDirectory));

                CopyAssetFiles(outputDoc, basePath, outputDirectory);

                std::string Json = Serialize(outputDoc, SerializeFlags::Pretty);
                TestStreamWriter(outputDirectory).GetOutputStream(outputFilename)->write(Json.c_str(), Json.size());
            });
        }

    private:
        static std::string StringifyOptions(const MeshOptions& options)
        {
            std::string ret;
            if (options.Optimize)
            {
                ret += "opt_";
            }

            if (options.GenerateTangentSpace)
            {
                ret += "tan_";
            }

            switch (options.PrimitiveFormat)
            {
            case PrimitiveFormat::Preserved:
                ret += "p";
                break;

            case PrimitiveFormat::Combine:
                ret += "c";
                break;

            case PrimitiveFormat::Separate:
                ret += "s";
                break;
            }

            switch (options.AttributeFormat)
            {
            case AttributeFormat::Interleave:
                ret += "i";
                break;

            case AttributeFormat::Separate:
                ret += "s";
                break;
            }

            return ret;
        }

        static void CopyAssetFiles(const Document& doc, const std::string& inDir, const std::string& outDir)
        {
            std::regex dataUriRegex = std::regex(R"(^data:(?:application|image)/.+;base\d{1,2},)");

            for (const auto& p : doc.buffers.Elements())
            {
                if (std::regex_search(p.uri, dataUriRegex))
                {
                    continue;
                }

                std::string filePath = inDir + p.uri;
                if (exists(filePath))
                {
                    copy_file(filePath, outDir + p.uri, copy_options::skip_existing);
                }
            }

            for (const auto& p : doc.images.Elements())
            {
                if (std::regex_search(p.uri, dataUriRegex))
                {
                    continue;
                }

                std::string filePath = inDir + p.uri;
                if (exists(filePath))
                {
                    copy_file(filePath, outDir + p.uri, copy_options::skip_existing);
                }
            }
        }
    };
}
