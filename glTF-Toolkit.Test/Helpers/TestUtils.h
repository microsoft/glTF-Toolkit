// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <CppUnitTest.h>

#include <memory>
#include "GLTFSDK/IStreamWriter.h"
#include "GLTFSDK/Constants.h"
#include "GLTFSDK/Serialize.h"
#include "GLTFSDK/Deserialize.h"

#include <locale>
#include <string>
#include <memory>
#include "WStringUtils.h"
#include "StreamMock.h"
#include <fstream>
#include <mutex>
#include <algorithm>
#include <string>
#include <iostream>

namespace Microsoft::glTF::Toolkit::Test
{
    class TestUtils
    {
    public:

        static std::string GetBasePath(const char * absolutePath)
        {
            std::string path(absolutePath);
#ifdef __APPLE__
            return path.substr(0, path.rfind('/') + 1);
#else
            return path.substr(0, path.rfind('\\') + 1);
#endif
        }

		static std::string GetFileExtension(const std::string& absolutePath)
		{
#ifdef __APPLE__
			const char delimiter = '/';
#else
			const char delimiter = '\\';
#endif
			if (absolutePath.back() == delimiter)
			{
				return std::string();
			}

			auto Pos = absolutePath.find_last_of(delimiter);
			return Pos == std::string::npos ? absolutePath : absolutePath.substr(Pos + 1);
		}

		static std::string GetFilename(const std::string& absolutePath)
		{
			std::string filename = GetFileExtension(absolutePath);
			auto pos = filename.find_last_of('.');
			return pos == std::string::npos ? absolutePath : absolutePath.substr(0, pos);
		}

        static std::string GetAbsolutePath(const char * relativePath)
        {
#ifdef __APPLE__
            // Leaving Win32 alone (below), but macOS requires working directory to be set
            std::string finalPath(relativePath);
            std::replace(finalPath.begin(), finalPath.end(), '\\', '/');
            return finalPath;
#else
            std::string currentPath = __FILE__;
            std::string sourcePath = currentPath.substr(0, currentPath.rfind('\\'));
            std::string resourcePath = sourcePath.substr(0, sourcePath.rfind('\\'));
            return resourcePath + "\\" + relativePath;
#endif
        }

        static std::wstring GetAbsolutePathW(const char * relativePath)
        {
            std::string absolutePath = GetAbsolutePath(relativePath);
            std::wstringstream wss;
            wss << absolutePath.c_str();
            return wss.str();
        }

        static std::shared_ptr<std::istream> ReadLocalAsset(const std::string& filename)
        {
            // Read local file
            int64_t m_readPosition = 0;
            std::shared_ptr<const std::vector<int8_t>> m_buffer;
            std::ifstream ifs;
            ifs.open(filename.c_str(), std::ifstream::in | std::ifstream::binary);
            if (ifs.is_open())
            {
                std::streampos start = ifs.tellg();
                ifs.seekg(0, std::ios::end);
                m_buffer = std::make_shared<const std::vector<int8_t>>(static_cast<unsigned int>(ifs.tellg() - start));
                ifs.seekg(0, std::ios::beg);
                ifs.read(reinterpret_cast<char*>(const_cast<int8_t*>(m_buffer->data())), m_buffer->size());
                ifs.close();
            }
            else
            {
                throw std::runtime_error("Could not open the file for reading");
            }

            // To IStream
            unsigned long writeBufferLength = 4096L * 1024L;
            auto tempStream = std::make_shared<std::stringstream>();
            auto tempBuffer = new char[writeBufferLength];
            // Read the file for as long as we can fill the buffer completely.
            // This means there is more content to be read.
            unsigned long bytesRead;
            do
            {
                auto bytesAvailable = m_buffer->size() - m_readPosition;
                unsigned long br = std::min(static_cast<unsigned long>(bytesAvailable), writeBufferLength);
#ifdef _WIN32
                memcpy_s(tempBuffer, br, m_buffer->data() + m_readPosition, br);
#else
                memcpy(tempBuffer, m_buffer->data() + m_readPosition, br);
#endif
                m_readPosition += br;
                bytesRead = br;

                tempStream->write(tempBuffer, bytesRead);
            } while (bytesRead == writeBufferLength);

            delete[] tempBuffer;

            if (tempStream.get()->bad())
            {
                throw std::runtime_error("Bad std::stringstream after copying the file");
            }

            return tempStream;
        }

        typedef std::function<void(const Document& doc, const std::string& gltfAbsolutePath)> GLTFAction;

        static void LoadAndExecuteGLTFTest(const char * gltfRelativePath, GLTFAction action)
        {
            // This asset has all textures
            auto absolutePath = TestUtils::GetAbsolutePath(gltfRelativePath);
            auto input = TestUtils::ReadLocalAsset(absolutePath);
            try
            {
                // Deserialize input json
                auto inputJson = std::string(std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>());
                auto doc = Deserialize(inputJson);

                action(doc, absolutePath);
            }
            catch (std::exception ex)
            {
                std::stringstream ss;
                ss << "Received exception was unexpected. Got: " << ex.what();
                Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail(WStringUtils::ToWString(ss).c_str());
            }
        }
    };

    class TestStreamReader : public IStreamReader
    {
    public:
        TestStreamReader(std::string gltfAbsolutePath) : m_basePath(TestUtils::GetBasePath(gltfAbsolutePath.c_str())) {}

        virtual ~TestStreamReader() override {}
        virtual std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
        {
            auto path = m_basePath;

#ifdef __APPLE__
            path += "/" + filename;
#else
            path += "\\" + filename;
#endif

            return std::make_shared<std::ifstream>(path, std::ios::binary);
        }
    private:
        const std::string m_basePath;
    };

	class TestStreamWriter : public IStreamWriter
	{
	public:
		TestStreamWriter(const char* gltfAbsolutePath) 
			: m_basePath(TestUtils::GetBasePath(gltfAbsolutePath))
		{ }

		TestStreamWriter(const std::string& gltfAbsolutePath) 
			: TestStreamWriter(gltfAbsolutePath.c_str())
		{ }

		std::shared_ptr<std::ostream> GetOutputStream(const std::string& filename) const override
		{
			auto path = m_basePath;

#ifdef __APPLE__
			path += "/" + filename;
#else
			path += "\\" + filename;
#endif

			return std::make_shared<std::ofstream>(path, std::ios::binary);
		}
	private:
		const std::string m_basePath;
	};
}