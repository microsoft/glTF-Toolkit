// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Windows.ApplicationModel;
using Windows.Foundation;
using Windows.Security.Cryptography;
using Windows.Storage;

namespace Microsoft.glTF.Toolkit.UWP.Test
{
    [TestClass]
    public class UWPTest
    {
        private async Task<StorageFile> CopyFileToTempFolderAsync(Uri uri)
        {
            StorageFile appxAssetFile = await StorageFile.GetFileFromApplicationUriAsync(uri);
            StorageFile destinationFile = await appxAssetFile.CopyAsync(ApplicationData.Current.TemporaryFolder, appxAssetFile.Name, NameCollisionOption.ReplaceExisting);
            return destinationFile;
        }

        private IAsyncOperation<StorageFolder> CreateTemporaryOutputFolderAsync(string folderName)
        {
            return ApplicationData.Current.TemporaryFolder.CreateFolderAsync(folderName, CreationCollisionOption.ReplaceExisting);
        }

        private async Task<bool> CompareFilesAsync(StorageFile file1, StorageFile file2)
        {
            var buffer1 = await FileIO.ReadBufferAsync(file1);
            var buffer2 = await FileIO.ReadBufferAsync(file2);

            return CryptographicBuffer.Compare(buffer1, buffer2);
        }

        [TestMethod]
        public async Task GLBDeserializeSerialize()
        {
            const string glbBaseName = "WaterBottle";
            const string glbFileName = glbBaseName + ".glb";

            StorageFile sourceGlbFile = await CopyFileToTempFolderAsync(new Uri("ms-appx:///Assets/3DModels/" + glbFileName));

            StorageFolder outputFolder = await CreateTemporaryOutputFolderAsync("Out_" + glbBaseName);

            // unpack the glb into gltf and all its companion files
            await GLTFSerialization.UnpackGLBAsync(sourceGlbFile, outputFolder);

            // compare one of the extracted images to the source images
            StorageFile sourceImageFile = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/3DModels/WaterBottle_diffuse.png"));
            StorageFile outputImageFile = await outputFolder.GetFileAsync(glbBaseName + "_image5.png");
            Assert.IsTrue(await CompareFilesAsync(sourceImageFile, outputImageFile));

            // compare the extracted model (.bin) to the source model (.bin) file
            StorageFile sourceBinFile = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/3DModels/" + glbBaseName + ".bin"));
            StorageFile outputBinFile = await outputFolder.GetFileAsync(glbBaseName + ".bin");
            Assert.IsTrue(await CompareFilesAsync(sourceBinFile, outputBinFile));

            // Pack the gltf back into a glb file
            StorageFile gltfFile = await outputFolder.GetFileAsync(glbBaseName + ".gltf");
            StorageFile outputGlbFile = await GLTFSerialization.PackGLTFAsync(gltfFile, outputFolder, glbFileName);

            // compare the new glb to the old glb
            Assert.IsTrue(await CompareFilesAsync(sourceGlbFile, outputGlbFile));
        }

        [TestMethod]
        public async Task GLBConvertToWindowsMR()
        {
            const string glbBaseName = "WaterBottle";
            const string glbFileName = glbBaseName + ".glb";

            StorageFile sourceGlbFile = await CopyFileToTempFolderAsync(new Uri("ms-appx:///Assets/3DModels/" + glbFileName));

            StorageFolder outputFolder = await CreateTemporaryOutputFolderAsync("Out_" + glbBaseName);

            var converted = await WindowsMRConversion.ConvertAssetForWindowsMR(sourceGlbFile, outputFolder, 1, TexturePacking.OcclusionRoughnessMetallic);

            Assert.IsTrue(converted.Name == "WaterBottle_converted.glb");

            var basicProperties = await converted.GetBasicPropertiesAsync();
            Assert.IsTrue(basicProperties.Size > 0);
        }

        [TestMethod]
        public async Task GLTFConvertToWindowsMR()
        {
            const string gltfFileName = "Avocado.gltf";

            // Copy all the resources to a temp folder, and save a reference to the GLTF file
            var folder = await StorageFolder.GetFolderFromPathAsync(Path.Combine(Package.Current.InstalledLocation.Path, "Assets", "Avocado"));
            StorageFile copiedGltfFile = null;

            foreach (var resource in await folder.GetFilesAsync())
            {
                var copied = await resource.CopyAsync(ApplicationData.Current.TemporaryFolder, resource.Name, NameCollisionOption.ReplaceExisting);

                if (resource.Name == gltfFileName)
                {
                    copiedGltfFile = copied;
                }
            }

            Assert.IsNotNull(copiedGltfFile);

            // Convert the file for Windows MR and pack it.
            StorageFolder outputFolder = await CreateTemporaryOutputFolderAsync("Out_" + Path.GetFileNameWithoutExtension(gltfFileName));

            var converted = await WindowsMRConversion.ConvertAssetForWindowsMR(copiedGltfFile, outputFolder, 1, TexturePacking.OcclusionRoughnessMetallic);

            Assert.IsTrue(converted.Name == "Avocado.glb");

            var basicProperties = await converted.GetBasicPropertiesAsync();
            Assert.IsTrue(basicProperties.Size > 0);
        }

        [TestMethod]
        public async Task GLTFConvertToWindowsMRFromLocalState()
        {
            const string gltfFileName = "Avocado.gltf";

            // Copy all the resources to a temp folder, and save a reference to the GLTF file
            var folder = await StorageFolder.GetFolderFromPathAsync(Path.Combine(Package.Current.InstalledLocation.Path, "Assets", "Avocado"));
            StorageFile copiedGltfFile = null;

            foreach (var resource in await folder.GetFilesAsync())
            {
                var copied = await resource.CopyAsync(ApplicationData.Current.LocalFolder, resource.Name, NameCollisionOption.ReplaceExisting);

                if (resource.Name == gltfFileName)
                {
                    copiedGltfFile = copied;
                }
            }

            Assert.IsNotNull(copiedGltfFile);

            // Convert the file for Windows MR and pack it.
            StorageFolder outputFolder = await CreateTemporaryOutputFolderAsync("Out_" + Path.GetFileNameWithoutExtension(gltfFileName));

            var converted = await WindowsMRConversion.ConvertAssetForWindowsMR(copiedGltfFile, outputFolder, 1, TexturePacking.OcclusionRoughnessMetallic);

            Assert.IsTrue(converted.Name == "Avocado.glb");

            var basicProperties = await converted.GetBasicPropertiesAsync();
            Assert.IsTrue(basicProperties.Size > 0);
        }
    }
}
