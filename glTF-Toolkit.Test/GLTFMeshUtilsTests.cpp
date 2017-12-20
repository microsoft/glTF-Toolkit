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
		const char* c_UntitledJson = "Resources\\gltf\\Primitives\\Primitives.gltf";

		const char* c_OutputDirectory = "C:\\Users\\Matt\\Desktop\\GLTFMeshUtils\\";
		const char*& c_TestFile = c_UntitledJson;

		void ExecuteTest(const char* GLTFRelPath, const char* OutputDir, const MeshOptions& Options)
		{
			TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [&](const GLTFDocument& Doc, const std::string& Path)
			{
				std::string OutName = TestUtils::GetFilenameExt(Path.c_str());
				std::string Directory = OutputDir + TestUtils::GetFilename(OutName) + "\\";
				
				create_directories(Directory);
				for (const auto& p : directory_iterator(TestUtils::GetBasePath(Path.c_str())))
				{
					copy_file(p, Directory + p.path().filename().string(), copy_options::overwrite_existing);
				}

				auto OutputDoc = GLTFMeshUtils::ProcessMeshes(TestStreamReader(Path), Doc, Options, Directory);

				std::string Json = Serialize(OutputDoc, SerializeFlags::Pretty);
				TestStreamWriter(Directory).GetOutputStream(OutName)->write(Json.c_str(), Json.size());
			});
		}

		TEST_METHOD(GLTFMeshUtils_Basic)
		{
			ExecuteTest(c_TestFile, c_OutputDirectory, MeshOptions::Defaults());
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