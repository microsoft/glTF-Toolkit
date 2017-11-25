// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <string>
#include <sstream>
#include <locale>
#include <codecvt>

namespace Microsoft
{
    namespace glTF
    {
        namespace Test
        {
            class WStringUtils
            {
            public:
                static std::wstring ToWString(const std::string& str)
                {
                    return ToWString(str.c_str());
                }

                static std::wstring ToWString(const std::stringstream& ss)
                {
                    return ToWString(ss.str());
                }

                static std::wstring ToWString(const char* str)
                {
                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                    return converter.from_bytes(str);
                }
            };
        }
    }
}