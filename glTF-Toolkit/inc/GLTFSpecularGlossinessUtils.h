// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "GLTFSDK.h"

namespace Microsoft::glTF::Toolkit
{
    /// <summary>
    /// Utilities to remove Specular Glossiness from a glTF asset.
    /// </summary>
    class GLTFSpecularGlossinessUtils
    {
    public:
        /// <summary>
        /// Applies <see cref="ConvertMaterial" /> to every material in the document, following the same parameter structure as that function.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the mesh will be loaded.</param>
        /// <param name="outputDirectory">The output directory to which compressed data should be saved.</param>
        /// <returns>
        /// A new glTF manifest without the KHR_materials_pbrSpecularGlossiness extension.
        /// </returns>
        static Document ConvertMaterials(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const std::string& outputDirectory);

        /// <summary>
        /// Removes the KHR_materials_pbrSpecularGlossiness extension by converting the parameters to Metal Roughness.
        /// </summary>
        /// <param name="streamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
        /// <param name="doc">The document from which the mesh will be loaded.</param>
        /// <param name="material">The material to be converted.</param>
        /// <param name="outputDirectory">The output directory to which compressed data should be saved.</param>
        /// <returns>
        /// A new glTF manifest without the KHR_materials_pbrSpecularGlossiness extension.
        /// </returns>
        static Document ConvertMaterial(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const Material & material, const std::string& outputDirectory);

    };
}