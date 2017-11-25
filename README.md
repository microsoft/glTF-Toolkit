# Microsoft glTF Toolkit

This project contains a collection of tools and libraries to modify and optimize glTF assets.

## Features

The current release includes code for:
- Packing PBR material textures using [DirectXTex](http://github.com/Microsoft/DirectXTex) for use with the [MSFT_packing_occlusionRoughnessMetallic](https://github.com/sbtron/glTF/tree/MSFT_lod/extensions/Vendor/MSFT_packing_occlusionRoughnessMetallic) extension.
- Compressing textures as BC3, BC5 and BC7 and generate mip maps using [DirectXTex](http://github.com/Microsoft/DirectXTex) for use with the [MSFT_texture_dds](https://github.com/sbtron/glTF/tree/MSFT_lod/extensions/Vendor/MSFT_texture_dds) extension.
- Merging multiple documents into a single document with levels of detail using the [MSFT_lod](https://github.com/sbtron/glTF/tree/MSFT_lod/extensions/Vendor/MSFT_lod) extension.

It also includes a command line tool that uses these steps in sequence in order to convert a glTF 2.0 core asset for use in the Windows Mixed Reality home, following the published [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home).

## Dependencies

This project consumes the following projects through NuGet packages:
- Microsoft GLTF SDK, licensed under the MIT license
- [DirectXTex](http://github.com/Microsoft/DirectXTex), licensed under the MIT license
- [RapidJSON](https://github.com/Tencent/rapidjson/), licensed under the MIT license


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