#pragma once

#include <pch.h>
#include <GLTFSDK/IStreamReader.h>

namespace Microsoft::glTF::Toolkit
{
    class GLTFStreamReader : public IStreamReader
    {
    public:
        GLTFStreamReader(std::wstring uriBase) : m_uriBase(uriBase) {}

        virtual ~GLTFStreamReader() override {}
        virtual std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
        {
            std::wstring filenameW = std::wstring(filename.begin(), filename.end());

            wchar_t uriAbsoluteRaw[MAX_PATH];
            // Note: PathCchCombine will return the last argument if it's an absolute path
            if (FAILED(::PathCchCombine(uriAbsoluteRaw, ARRAYSIZE(uriAbsoluteRaw), m_uriBase.c_str(), filenameW.c_str())))
            {
                throw std::invalid_argument("Could not get the base path for the GLTF resources. Try specifying the full path.");
            }

            return std::make_shared<std::ifstream>(uriAbsoluteRaw, std::ios::binary);
        }
    private:
        const std::wstring m_uriBase;
    };
}