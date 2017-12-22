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

#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
	using namespace std::experimental::filesystem;

	TEST_CLASS(GLTFMeshUtilsTest)
	{
		const char* c_WaterBottleJson = "Resources\\gltf\\WaterBottle\\WaterBottle.gltf";
		const char* c_PrimitivesJson = "Resources\\gltf\\Primitives\\Primitives.gltf";

		const char* c_OutputDirectory = "C:\\Users\\mahurlim\\Desktop\\GLTFMeshUtils\\";
		const char*& c_TestFile = c_WaterBottleJson;

		void ExecuteTest(const char* GLTFRelPath, const char* OutputDir, const MeshOptions& Options)
		{
			TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [&](const GLTFDocument& Doc, const std::string& Path)
			{
				std::string OutputName = TestUtils::GetFilenameExt(Path.c_str());
				std::string OutputDirectory = OutputDir + TestUtils::GetFilename(OutputName) + "\\";
				std::string BasePath = TestUtils::GetBasePath(Path.c_str());

				auto OutputDoc = GLTFMeshUtils::ProcessMeshes(TestStreamReader(Path), Doc, Options, OutputDirectory);
				
				// Create output directory and copy files referenced by the output document.
				create_directories(OutputDirectory);
				for (const auto& p : OutputDoc.buffers.Elements())
				{
					std::string FilePath = BasePath + p.uri;

					if (exists(FilePath))
					{
						copy_file(FilePath, OutputDirectory + p.uri, copy_options::overwrite_existing);
					}
				}
				for (const auto& p : OutputDoc.images.Elements())
				{
					std::string FilePath = BasePath + p.uri;

					if (exists(FilePath))
					{
						copy_file(FilePath, OutputDirectory + p.uri, copy_options::overwrite_existing);
					}
				}

				std::string Json = Serialize(OutputDoc, SerializeFlags::Pretty);
				TestStreamWriter(OutputDirectory).GetOutputStream(OutputName)->write(Json.c_str(), Json.size());
			});
		}

		TEST_METHOD(GLTFMeshUtils_Default)
		{
			MeshOptions Options;
			Options.Optimize = true;
			Options.GenerateTangentSpace = true;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Preserved;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_Optimize)
		{
			MeshOptions Options;
			Options.Optimize = true;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Preserved;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_Tangents)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = true;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Combine;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_CI)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Combine;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_CS)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Separate;
			Options.PrimitiveFormat = PrimitiveFormat::Combine;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_SI)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Separate;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}

		TEST_METHOD(GLTFMeshUtils_SS)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Separate;
			Options.PrimitiveFormat = PrimitiveFormat::Separate;

			ExecuteTest(c_TestFile, c_OutputDirectory, Options);
		}
	};
}