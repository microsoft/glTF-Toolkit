// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK\GLTF.h>
#include <GLTFSDK\GLTFDocument.h>

namespace Validation
{
    std::wstring ValidateWindowsMRAsset(const Microsoft::glTF::GLTFDocument& document);
};

