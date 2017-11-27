// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

extern const wchar_t * EXTENSION_GLTF;
extern const wchar_t * EXTENSION_GLB;

enum class AssetType
{
    GLTF,
    GLB
};

namespace AssetTypeUtils
{
    AssetType AssetTypeFromFilePath(const std::wstring& assetPath);
}
