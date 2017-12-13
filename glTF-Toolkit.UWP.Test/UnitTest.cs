
using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Windows.Storage;
using Windows.Storage.Streams;

namespace glTF_Toolkit.UWP.Test
{
    [TestClass]
    public class SerializationTest
    {
        private async Task<StorageFile> CopyFileToTempFolder(Uri uri)
        {
            StorageFile appxAssetFile = await StorageFile.GetFileFromApplicationUriAsync(uri);
            StorageFile destinationFile = await appxAssetFile.CopyAsync(ApplicationData.Current.TemporaryFolder);
            return destinationFile;
        }

        private async Task<StorageFolder> CreateTemporaryOutputFolder(string folderName)
        {
            StorageFolder folder = await ApplicationData.Current.TemporaryFolder.CreateFolderAsync(folderName);
            return folder;
        }

        private async Task<StorageFile> OpenFileInFolder(StorageFolder folder, string fileName)
        {
            StorageFile outputFile = await folder.GetFileAsync(fileName);
            return outputFile;
        }

        private async Task<bool> AreStorageFilesEqual(StorageFile file1, StorageFile file2)
        {
            IRandomAccessStreamWithContentType file1Stream = await file1.OpenReadAsync();
            IRandomAccessStreamWithContentType file2Stream = await file2.OpenReadAsync();

            Stream stream1 = file1Stream.AsStreamForRead();
            Stream stream2 = file2Stream.AsStreamForRead();

            if (stream1.Length != stream2.Length)
            {
                return false;
            }

            for (int i = 0; i < stream1.Length; i++)
            {
                if (stream1.ReadByte() != stream2.ReadByte())
                {
                    return false;
                }
            }

            return true;
        }

        [TestMethod]
        public async Task GLBDeserializeSerialize()
        {
            try
            {
                const string glbBaseName = "WaterBottle";
                const string glbFileName = glbBaseName + ".glb";

                StorageFile sourceGlbFile = await CopyFileToTempFolder(new Uri("ms-appx:///Assets/3DModels/" + glbFileName));

                StorageFolder outputFolder = await CreateTemporaryOutputFolder("Out_" + glbBaseName);

                // unpack the glb into gltf and all its companion files
                glTF_Toolkit_WinRTComp.glTFSerialization.UnpackGLB(sourceGlbFile.Path, outputFolder.Path);

                bool areFilesEqual = false;

                // compare one of the extracted images to the source images
                StorageFile sourceImageFile = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/3DModels/WaterBottle_diffuse.png"));
                StorageFile outputImageFile = await OpenFileInFolder(outputFolder, glbBaseName + "_image5.png");
                areFilesEqual = await AreStorageFilesEqual(sourceImageFile, outputImageFile);
                Assert.IsTrue(areFilesEqual);

                // compare the extracted model (.bin) to the source model (.bin) file
                StorageFile sourceBinFile = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/3DModels/" + glbBaseName + ".bin"));
                StorageFile outputBinFile = await OpenFileInFolder(outputFolder, glbBaseName + ".bin");
                areFilesEqual = await AreStorageFilesEqual(sourceBinFile, outputBinFile);
                Assert.IsTrue(areFilesEqual);

                // Pack the gltf back into a glb file
                string outputGlbPath = outputFolder.Path + "\\" + glbFileName;
                glTF_Toolkit_WinRTComp.glTFSerialization.PackGLTF(outputFolder.Path + "\\" + glbBaseName + ".gltf", outputGlbPath);

                // compare the new glb to the old glb
                StorageFile outputGlbFile = await OpenFileInFolder(outputFolder, glbFileName);
                areFilesEqual = await AreStorageFilesEqual(sourceGlbFile, outputGlbFile);

                Assert.IsTrue(areFilesEqual);
            }
            catch(Exception e)
            {
                Assert.Fail(e.Message);
            }
        }
    }
}
