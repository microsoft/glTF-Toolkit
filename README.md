# Microsoft glTF Toolkit

This project contains a collection of tools and libraries to modify and optimize glTF assets.

Additionally the repository includes a command line tool that uses these steps in sequence in order to convert a glTF 2.0 core asset for use in the Windows Mixed Reality home, following the published [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home). The latest release of the Windows Mixed Reality Asset converter is available on the [releases tab](https://github.com/Microsoft/glTF-Toolkit/releases).

[![Build status](https://ci.appveyor.com/api/projects/status/4n8m94mpc03dcuxt?svg=true)](https://ci.appveyor.com/project/robertos/gltf-toolkit)

## Features

The current release includes code for:

- Packing PBR material textures using [DirectXTex](http://github.com/Microsoft/DirectXTex) for use with the [MSFT_packing_occlusionRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_occlusionRoughnessMetallic) and [MSFT_packing_normalRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_normalRoughnessMetallic) extensions.
- Compressing textures as BC3, BC5 or BC7 and generate mip maps using [DirectXTex](http://github.com/Microsoft/DirectXTex) for use with the [MSFT_texture_dds](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_texture_dds) extension.
- Removing [KHR_materials_pbrSpecularGlossiness](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness) by converting material prameters to metallic-roughness.
- Mesh compression using [KHR_draco_mesh_compression](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_draco_mesh_compression) extension; this can only be used on 1809 and later and should only be used for assets that are transmitted over the network as load time is increased with compression.
- Merging multiple glTF assets into a asset with multiple levels of detail using the [MSFT_lod](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_lod) extension.
- A command line tool that combines these components to create optimized glTF assets for the Windows Mixed Reality Home
- A UWP compatible Windows Runtime component to perform conversions between GLTF and GLB, as well as optimize assets for Windows Mixed Reality at runtime

## Dependencies

This project consumes the following projects through NuGet packages:

- [Microsoft.glTF.CPP](https://www.nuget.org/packages/Microsoft.glTF.CPP), licensed under the MIT license
- [DirectXTex](http://github.com/Microsoft/DirectXTex), licensed under the MIT license
- [RapidJSON](https://github.com/Tencent/rapidjson/), licensed under the MIT license
- [Draco](https://github.com/google/draco/), licensed under Apache License 2.0

## Building

This project can be built using Visual Studio 2017 Update 4 on Windows 10 Fall Creators Update (16299.0).

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## License

Copyright (c) Microsoft Corporation. All rights reserved.

Licensed under the [MIT License](LICENSE).
