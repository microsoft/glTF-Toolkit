// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "AssetType.h"

const wchar_t * EXTENSION_GLTF = L".gltf";
const wchar_t * EXTENSION_GLB = L".glb";

AssetType AssetTypeUtils::AssetTypeFromFilePath(const std::wstring& assetPath)
{
    const wchar_t *inputExtensionRaw = nullptr;
    if (FAILED(PathCchFindExtension(assetPath.c_str(), assetPath.length() + 1, &inputExtensionRaw)))
    {
        throw std::invalid_argument("Invalid input file extension.");
    }

    if (_wcsicmp(inputExtensionRaw, EXTENSION_GLTF) == 0)
    {
        return AssetType::GLTF;
    }
    else if (_wcsicmp(inputExtensionRaw, EXTENSION_GLB) == 0)
    {
        return AssetType::GLB;
    }
    else
    {
        throw std::invalid_argument("Invalid file, please provide a GLTF or GLB.");
    }
}