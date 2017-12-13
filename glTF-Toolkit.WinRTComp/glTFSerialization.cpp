#include "pch.h"
#include "glTFSerialization.h"
#include <GLBtoGLTF.h>
#include <SerializeBinary.h>

using namespace glTF_Toolkit_WinRTComp;
using namespace Microsoft::glTF::Toolkit;
using namespace Platform;

void glTFSerialization::UnpackGLB(String^ glbPath, String^ outDirectory)
{
    std::wstring glbPathW = glbPath->Data();
    std::wstring outDirectoryW = outDirectory->Data();

    GLBToGLTF::UnpackGLB(glbPathW, outDirectoryW);
}

void glTFSerialization::PackGLTF(String ^ gltfPath, String ^ glbPath)
{
    std::wstring gltfPathW = gltfPath->Data();
    std::wstring glbPathW = glbPath->Data();

    SerializeBinary(gltfPathW, glbPathW);
}
