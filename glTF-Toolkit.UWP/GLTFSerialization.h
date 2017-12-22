// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace Microsoft::glTF::Toolkit::UWP
{
    public ref class GLTFSerialization sealed
    {
    public:
        /// <summary>
        /// Unpacks a GLB asset into a GLTF manifest and its 
        /// resources (bin files and images).
        /// </summary>
        /// <param name="glbFile">The GLB file to unpack. The name of the GLB file, without the extension, 
        /// will be used as a prefix to all unpacked resources.</param>
        /// <param name="outputFolder">The output folder to which the glTF manifest and resources will be unpacked.</param>
        static Windows::Foundation::IAsyncAction^ UnpackGLBAsync(Windows::Storage::StorageFile^ glbFile, Windows::Storage::StorageFolder^ outputFolder);

        /// <summary>
        /// Serializes a glTF asset as a glTF binary (GLB) file.
        /// </summary>
        /// <param name="sourceGltf">The glTF file to be serialized.</param>
        /// <param name="outputFolder">The output folder where you want the glb file to be placed.</param>
        /// <param name="glbName">The glb filename.</param>
        /// <returns>
        /// The resulting GLB file, named with glbName and located in outputFolder.
        /// </returns>
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ PackGLTFAsync(Windows::Storage::StorageFile^ sourceGltf, Windows::Storage::StorageFolder^ outputFolder, Platform::String^ glbName);
    };
}
