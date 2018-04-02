# Windows Mixed Reality Asset Converter

A command line tool to convert core GLTF 2.0 for use in the Windows Mixed Reality home, following the published [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home).

Note that this tool does not enforce any limits specified in the documentation (polygon count, texture size, etc.), so you might still encounter issues placing models if you do not conform to those limits.

## Usage
WindowsMRAssetConverter _&lt;path to GLTF/GLB&gt;_

## Optional arguments
- `-o <output file path>`
  - Specifies the output file name and directory for the output GLB.
  - If the file is a GLB and the output name is not specified, the tool defaults to the same name as input + "_converted.glb".

- `-platform <all | desktop | holographic>`
  - **Default:** `desktop` 
  - `desktop`: optimizes assets for immersive PC-based headsets using the [MSFT_packing_occlusionRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_occlusionRoughnessMetallic) extension.
  - `holographic`: optimizes assets for HoloLens using the [MSFT_packing_normalRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_normalRoughnessMetallic) extension.
  - `all`: creates assets optimized for both holographic and desktop devices, but with a larger file size.

- `-compatibility-mode <on | off>`
  - **Default:** `on`
  - Generates assets that are also compatible with versions of Windows 10 older than the Spring Creators Update (version 1803).
  - Enabling compatibility mode results in assets with a larger file size. If your app is compatible with Windows 10 1803+ only, it is recommended to set `-compatibility-mode off`.
  - This setting does not have any effect on the Holographic platform.

- `-lod <path to each lower LOD asset in descending order of quality>`
  - Specifies a list of assets that represent levels of detail, from higher to lower, that should be merged with the main asset and used as alternates when the asset is displayed from a distance (with limited screen coverage).

- `-screen-coverage <LOD screen coverage values>`
  - Specifies the maximum screen coverage values for each of the levels of detail, according to the [MSFT_lod](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_lod) extension specification.

- `-share-materials`
  - If enabled, creates assets that share materials between different levels of detail. 
  - This assumes all LOD documents use the same indices for each material, and uses the textures from the most detailed level.

- `-temp-directory <temporary folder>`
  - **Default:** system temp folder for the user
  - Allows overriding the temporary folder where intermediate files (packed/compressed textures, converted GLBs) will be placed.

- `-max-texture-size <Max texture size in pixels>`
  - **Default:** 512
  - Allows overriding the maximum texture dimension (width/height) when compressing textures. The recommended maximum dimension in the [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home#texture_resolutions_and_workflow) is 512, and the allowed maximum is 4096.

- `-replace-textures`
  - If enabled, replaces all textures with their DDS compressed equivalents during the compression step. 
  - This results in a smaller file size, but the resulting file will not be compatible with most glTF viewers.


## Example
`WindowsMRAssetConverter FileToConvert.gltf -o ConvertedFile.glb -platform all -lod Lod1.gltf Lod2.gltf -screen-coverage 0.5 0.2 0.01`

The above will convert _FileToConvert.gltf_ into _ConvertedFile.glb_ in the current directory.

## Pipeline overview

Each asset goes through the following steps when converting for compatibility with the Windows Mixed Reality home:

1. **Conversion from GLB** - Any GLB files are converted to loose glTF + assets, to simplify the code for reading resources
1. **Texture packing** - The textures that are relevant for the Windows MR home are packed according to the [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home#materials) using the [MSFT\_packing\_occlusionRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_occlusionRoughnessMetallic) and [MSFT\_packing\_normalRoughnessMetallic](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_normalRoughnessMetallic) extensions as necessary
1. **Texture compression** - All textures that are used in the Windows MR home must be compressed as DDS BC5 or BC7 according to the [documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home#materials). This step also generates mip maps for the textures, and resizes them down if necessary
1. **LOD merging** - All assets that represent levels of detail are merged into the main asset using the [MSFT_lod](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_lod) extension
1. **GLB export** - The resulting assets are exported as a GLB with all resources. As part of this step, accessors are modified to conform to the [glTF implementation notes in the documentation](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home#gltf_implementation_notes): component types are converted to types supported by the Windows MR home, and the min and max values are calculated before serializing the accessors to the GLB

## Additional resources

- [Creating 3D models for use in the Windows Mixed Reality home](https://developer.microsoft.com/en-us/windows/mixed-reality/creating_3d_models_for_use_in_the_windows_mixed_reality_home)
- [Microsoft glTF LOD Extension Specification](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_lod)
- [PC Mixed Reality Texture Packing Extensions Specification](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_occlusionRoughnessMetallic)
- [Holographic Mixed Reality Texture Packing Extensions Specification](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_packing_normalRoughnessMetallic)
- [Microsoft DDS Textures glTF extensions specification](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_texture_dds)
- [Implementing 3D app launchers](https://developer.microsoft.com/en-us/windows/mixed-reality/implementing_3d_app_launchers)
- [Implementing 3D deep links for your app in the Windows Mixed Reality home](https://developer.microsoft.com/en-us/windows/mixed-reality/implementing_3d_deep_links_for_your_app_in_the_windows_mixed_reality_home)
- [Navigating the Windows Mixed Reality home](https://developer.microsoft.com/en-us/windows/mixed-reality/navigating_the_windows_mixed_reality_home)
