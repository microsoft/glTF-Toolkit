// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <filesystem>
#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/IStreamWriter.h>

namespace Microsoft::glTF::Toolkit::UWP
{
    class GLTFStreamReader : public IStreamReader
    {
    public:
        GLTFStreamReader(Windows::Storage::StorageFolder^ gltfFolder) 
        {
            m_uriBase = std::experimental::filesystem::path(gltfFolder->Path->Data());
        }

        virtual ~GLTFStreamReader() override {}
        virtual std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
        {
            std::wstring filenameW(filename.begin(), filename.end());
            std::experimental::filesystem::path path(filenameW);

            auto absolutePath = path.wstring();

            if (path.is_relative())
            {
                absolutePath = (m_uriBase / path).wstring();
            }

            return std::make_shared<std::ifstream>(absolutePath, std::ios::binary);
        }
    private:
        std::experimental::filesystem::path m_uriBase;
    };

    class GLBStreamWriter : public Microsoft::glTF::IStreamWriter
    {
    public:
        GLBStreamWriter(const std::wstring& filename) :
            m_stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out))
        { }

        std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override
        {
            return m_stream;
        }

    private:
        std::shared_ptr<std::ofstream> m_stream;
    };
}
