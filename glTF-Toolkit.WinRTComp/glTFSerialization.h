#pragma once

namespace glTF_Toolkit_WinRTComp
{
    public ref class glTFSerialization sealed
    {
    public:
        static void UnpackGLB(Platform::String^ glbPath, Platform::String^ outDirectory);
        static void PackGLTF(Platform::String ^ gltfPath, Platform::String ^ glbPath);
    };
}
