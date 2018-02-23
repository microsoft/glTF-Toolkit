// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace Microsoft::glTF::Toolkit::UWP
{
    public ref class WindowsMRConversion sealed
    {
    public:
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder);
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder, size_t maxTextureSize);
    };
}
