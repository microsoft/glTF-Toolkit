// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/Serialize.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/GLBResourceReader.h>

namespace Microsoft::glTF::Toolkit
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

        /// <summary>
        /// Extracts the contents of all buffer views from a GLB file into a 
        /// byte vector that can be saves as a bin file to be used in a glTF file.
        /// </summary>
        /// <param name="in">A stream pointing to the GLB file.</param>
        /// <param name="glbDoc">The manifest describing the GLB asset.</param>
        /// <param name="bufferOffset">The offset on the input file where the GLB buffer starts.</param>
        /// <param name="newBufferLength">The length of the new buffer (sum of all buffer view lengths).</param>
        /// <returns>
        /// The binary content of the buffer views as a vector.
        /// </returns>
        static std::vector<char> SaveBin(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const size_t bufferOffset, const size_t newBufferlength);

        /// <summary>
        /// Loads all images in a glTF-Binary (GLB) asset into a map relating each image identifier to the contents of that image.
        /// </summary>
        /// <param name="in">A stream pointing to the GLB file.</param>
        /// <param name="glbDoc">The manifest describing the GLB asset.</param>
        /// <param name="name">The name that should be used when creating the identifiers for the image files.</param>
        /// <param name="bufferOffset">The offset on the input file where the GLB buffer starts.</param>
        /// <returns>
        /// A map relating each image identifier to the contents of that image.
        /// </returns>
        static std::unordered_map<std::string, std::vector<char>> GetImagesData(std::istream* in, const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name, const size_t bufferOffset);

        /// <summary>
        /// Creates the glTF manifest that represents a GLB file after unpacking.
        /// </summary>
        /// <param name="glbDoc">The original manifest contained in the GLB file.</param>
        /// <param name="name">The name that should be used when creating the identifiers for the image and bin files when unpacking.</param>
        /// <returns>
        /// A new glTF manifest that represents the same file, but with images and resources referenced by URI instead of embedded ina GLB buffer.
        /// </returns>
        static Microsoft::glTF::GLTFDocument CreateGLTFDocument(const Microsoft::glTF::GLTFDocument& glbDoc, const std::string& name);
    };
}
