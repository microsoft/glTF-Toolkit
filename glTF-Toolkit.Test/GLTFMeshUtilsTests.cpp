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


class GLBStreamFactory : public IStreamFactory
{
public:
	GLBStreamFactory(const std::wstring& filename) 
		: m_stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out))
		, m_tempStream(std::make_shared<std::stringstream>(std::ios_base::binary | std::ios_base::in | std::ios_base::out))
	{ }

	std::shared_ptr<std::istream> GetInputStream(const std::string&) const override { throw std::logic_error("Not implemented"); }
	std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override { return m_stream; }
	std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override { return m_tempStream; }

private:
	std::shared_ptr<std::ofstream> m_stream;
	std::shared_ptr<std::stringstream> m_tempStream;
};


namespace Microsoft::glTF::Toolkit::Test
{
	TEST_CLASS(GLTFMeshUtilsTest)
	{
		const char* c_waterBottleJson = "Resources\\gltf\\WaterBottle\\WaterBottle.gltf";

		void ExecuteOptimizationTest(const char* GLTFRelPath, const MeshOptions& Options)
		{
			TestUtils::LoadAndExecuteGLTFTest(GLTFRelPath, [=](auto Doc, auto Path)
			{
				auto OptimizedDoc = GLTFMeshUtils::ProcessMeshes(TestStreamReader(Path), std::make_unique<GLBStreamFactory>(L"optimized_mesh.bin"), Doc, Options, "");
			});
		}

		TEST_METHOD(GLTFMeshUtils_Basic)
		{
			ExecuteOptimizationTest(c_waterBottleJson, MeshOptions::Defaults());
		}
	};
}