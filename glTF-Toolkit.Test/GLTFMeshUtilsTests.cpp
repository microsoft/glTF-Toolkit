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

#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;


namespace Microsoft::glTF::Toolkit::Test
{
	TEST_CLASS(GLTFMeshUtilsTest)
	{
		const char* c_WaterBottleJson = "Resources\\gltf\\WaterBottle\\WaterBottle.gltf";
		const char*& c_TestFile = c_WaterBottleJson;

		void ExecuteTest(const char* GLTFRelPath, const MeshOptions& Options)
		{
			TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [&](const GLTFDocument& Doc, const std::string& Path)
			{
				std::string FilePath = Path;
				std::string Directory = TestUtils::GetBasePath(Path.c_str());
				std::string OutName;

				auto Pos = Path.find_last_of("\\/");

				auto Pos = FilePath.find_last_of(".\\/");
				if (Pos != std::string::npos && Path[Pos] == '.')
				{
					Pos = FilePath.find_last_of("\\/");
					if (Pos != std::string::npos)
					{
						Directory = FilePath.substr(0, Pos + 1);
						OutName = Path.substr(Pos + 1);
					}
				}
				else
				{
					Directory = FilePath.substr(0, Pos + 1);
					OutName = Path.substr(Pos + 1);
				}

				auto OutputDoc = GLTFMeshUtils::ProcessMeshes(TestStreamReader(Path), Doc, Options, Directory);

				std::string Json = Serialize(OutputDoc, SerializeFlags::Pretty);
				TestStreamWriter(Directory).GetOutputStream(OutName)->write(Json.c_str(), Json.size());
			});
		}

		TEST_METHOD(GLTFMeshUtils_Basic)
		{
			ExecuteTest(c_TestFile, MeshOptions::Defaults());
		}

		TEST_METHOD(GLTFMeshUtils_CI)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Combine;

			ExecuteTest(c_TestFile, Options);
		}

		TEST_METHOD(GLTFMeshUtils_CS)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Separate;
			Options.PrimitiveFormat = PrimitiveFormat::Combine;

			ExecuteTest(c_TestFile, Options);
		}

		TEST_METHOD(GLTFMeshUtils_SI)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Interleave;
			Options.PrimitiveFormat = PrimitiveFormat::Separate;

			ExecuteTest(c_TestFile, Options);
		}

		TEST_METHOD(GLTFMeshUtils_SS)
		{
			MeshOptions Options;
			Options.Optimize = false;
			Options.GenerateTangentSpace = false;
			Options.AttributeFormat = AttributeFormat::Separate;
			Options.PrimitiveFormat = PrimitiveFormat::Separate;

			ExecuteTest(c_TestFile, Options);
		}
	};
}