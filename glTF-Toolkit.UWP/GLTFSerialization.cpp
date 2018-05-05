// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "GLTFSerialization.h"
#include "GLTFStreams.h"

#include <GLBtoGLTF.h>
#include <SerializeBinary.h>

#include <GLTFSDK/GLTFDocument.h>

using namespace Concurrency;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;
using namespace Microsoft::glTF::Toolkit::UWP;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

IAsyncOperation<StorageFile^>^ GLTFSerialization::UnpackGLBAsync(StorageFile^ glbFile, StorageFolder^ outputFolder)
{
    String^ glbFilePath = glbFile->Path;
    std::wstring glbPathW = glbFilePath->Data();
    std::string glbPathA = std::string(glbPathW.begin(), glbPathW.end());

    String^ outputFolderPath = outputFolder->Path + "\\";
    std::wstring outputFolderPathW = outputFolderPath->Data();
    std::string outputFolderPathA = std::string(outputFolderPathW.begin(), outputFolderPathW.end());

    String^ baseFileName = glbFile->DisplayName;
    std::wstring baseFileNameW = baseFileName->Data();
    std::string baseFileNameA = std::string(baseFileNameW.begin(), baseFileNameW.end());

    return create_async([glbPathA, outputFolderPathA, baseFileNameA, outputFolder, baseFileNameW]
    {
        GLBToGLTF::UnpackGLB(glbPathA, outputFolderPathA, baseFileNameA);

        return outputFolder->GetFileAsync(ref new String((baseFileNameW + L".gltf").c_str()));
    });
}

IAsyncOperation<StorageFile^>^ GLTFSerialization::PackGLTFAsync(StorageFile^ sourceGltf, StorageFolder^ outputFolder, String^ glbName)
{
    return create_async([sourceGltf, outputFolder, glbName]() 
    {
        std::wstring gltfPathW = sourceGltf->Path->Data();

        auto stream = std::make_shared<std::ifstream>(gltfPathW, std::ios::in);

        return create_task([stream]()
        {
            return std::make_shared<GLTFDocument>(DeserializeJson(*stream));
        })
        .then([sourceGltf, outputFolder, glbName](std::shared_ptr<GLTFDocument> document)
        {
            return create_task(sourceGltf->GetParentAsync())
            .then([outputFolder, glbName, document](StorageFolder^ gltfFolder)
            { 
                GLTFStreamReader streamReader(gltfFolder);

                String^ outputGlbPath = outputFolder->Path + "\\" + glbName;
                std::wstring outputGlbPathW = outputGlbPath->Data();
                SerializeBinary(*document, streamReader, std::make_unique<GLBStreamFactory>(outputGlbPathW));

                return outputFolder->GetFileAsync(glbName);
            });
        });
    });
}
