// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFTexturePackingUtils.h>

namespace Microsoft::glTF::Toolkit::UWP
{
    [Platform::Metadata::Flags]
    public enum class TexturePacking : unsigned int
    {
        None = Toolkit::TexturePacking::None,
        OcclusionRoughnessMetallic = Toolkit::TexturePacking::OcclusionRoughnessMetallic,
        RoughnessMetallicOcclusion = Toolkit::TexturePacking::RoughnessMetallicOcclusion,
        NormalRoughnessMetallic = Toolkit::TexturePacking::NormalRoughnessMetallic
    };

    public ref class WindowsMRConversion sealed
    {
    public:
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder);
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder, size_t maxTextureSize);
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder, size_t maxTextureSize, UWP::TexturePacking packing);
        static Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile^>^ ConvertAssetForWindowsMR(Windows::Storage::StorageFile^ gltfOrGlbFile, Windows::Storage::StorageFolder^ outputFolder, size_t maxTextureSize, UWP::TexturePacking packing, bool meshCompression);
    };
}
