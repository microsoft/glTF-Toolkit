// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <memory>
#include <string>
#include <sstream>

#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/IStreamWriter.h>

namespace Microsoft::glTF::Toolkit::Test
{
    class StreamMock : public Microsoft::glTF::IStreamWriter, public Microsoft::glTF::IStreamReader
    {
    public:
        StreamMock() : m_stream(std::make_shared<std::stringstream>(std::ios_base::app | std::ios_base::binary | std::ios_base::in | std::ios_base::out))
        {
        }

        std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override
        {
            return m_stream;
        }

        std::shared_ptr<std::istream> GetInputStream(const std::string&) const override
        {
            return m_stream;
        }
    private:
        std::shared_ptr<std::stringstream> m_stream;
    };
}