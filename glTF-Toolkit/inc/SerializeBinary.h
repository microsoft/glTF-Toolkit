// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.#pragma once

#include <GLTFSDK/GLTFDocument.h>
#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/IStreamFactory.h>
#include <memory>
#include <vector>
#include <rapidjson/document.h>

namespace Microsoft { namespace glTF { namespace Toolkit
{
    void SerializeBinary(const GLTFDocument& gltfDocument, const IStreamReader& inputStreamReader, std::unique_ptr<const IStreamFactory>& outputStreamFactory);
}}}
