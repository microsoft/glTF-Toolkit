#pragma once

#include <pch.h>
#include <GLTFSDK/IStreamFactory.h>

namespace Microsoft::glTF::Toolkit
{
    class GLBStreamFactory : public Microsoft::glTF::IStreamFactory
    {
    public:
        GLBStreamFactory(const std::wstring& filename) :
            m_stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out)),
            m_tempStream(std::make_shared<std::stringstream>(std::ios_base::binary | std::ios_base::in | std::ios_base::out))
        { }

        std::shared_ptr<std::istream> GetInputStream(const std::string&) const override
        {
            throw std::logic_error("Not implemented");
        }

        std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override
        {
            return m_stream;
        }

        std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override
        {
            return m_tempStream;
        }
    private:
        std::shared_ptr<std::ofstream> m_stream;
        std::shared_ptr<std::stringstream> m_tempStream;
    };
}