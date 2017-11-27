// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/Serialize.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/GLBResourceReader.h>

namespace Microsoft { namespace glTF { namespace Toolkit 
{
    /// <summary>
    /// Utilities to convert glTF-Binary files (GLB) to
    /// unpacked glTF assets.
    /// </summary>
    class GLBToGLTF
    {
    public:
        /// <summary>
        /// Unpacks a GLB asset into a GLTF manifest and its 
        /// resources (bin files and images).
        /// </summary>
        /// <param name="glbPath">The path to the GLB file to unpack.</param>
        /// <param name="outDirectory">The directory to which the glTF manifest and resources will be unpacked.</param>
        /// <param name="gltfName">
        /// The name of the output glTF manifest file, without the extension. 
        /// This name will be used as a prefix to all unpacked resources.
        /// </param>
        static void UnpackGLB(std::string glbPath, std::string outDirectory, std::string gltfName);

        static std::vector<char> SaveBin(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const size_t bufferOffset, const size_t newBufferlength);
        static std::unordered_map<std::string, std::vector<char>> GetImagesData(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name, const size_t bufferOffset);
        static Microsoft::glTF::GLTFDocument CreateGLTFDocument(const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name);

    private:
        static Microsoft::glTF::GLTFDocument RecalculateOffsetsGLTFDocument(const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name);
        static size_t GetGLBBufferChunkOffset(std::ifstream* in);
    };
}}}
