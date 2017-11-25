// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/Serialize.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/GLBResourceReader.h>

namespace Microsoft { namespace glTF { namespace Toolkit 
{
    class GLBToGLTF
    {
    public:
        static std::vector<char> SaveBin(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const size_t bufferOffset, const size_t newBufferlength);
        static std::unordered_map<std::string, std::vector<char>> GetImagesData(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name, const size_t bufferOffset);
        static Microsoft::glTF::GLTFDocument RecalculateOffsetsGLTFDocument(const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name);
        static Microsoft::glTF::GLTFDocument CreateGLTFDocument(const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name);
        static void UnpackGLB(std::string name, std::string path);
        static size_t GetGLBBufferChunkOffset(std::ifstream* in);
    };
}}}
