#include "pch.h"
#include "glTFSerialization.h"
#include <GLBtoGLTF.h>
#include <SerializeBinary.h>

#include <GLTFSDK/GLTFDocument.h>
#include <GLTFSDK/IStreamReader.h>
#include <GLTFSDK/IStreamFactory.h>


using namespace glTF_Toolkit_WinRTComp;

using namespace Concurrency;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;
using namespace Platform;
using namespace Windows::Storage;

namespace Microsoft::glTF::Toolkit
{
    class GLTFStreamReader : public IStreamReader
    {
    public:
        GLTFStreamReader(StorageFolder^ gltfFolder) : m_uriBase(std::wstring(gltfFolder->Path->Data()) + L"\\") {}

        virtual ~GLTFStreamReader() override {}
        virtual std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
        {
            std::wstring filenameW = std::wstring(filename.begin(), filename.end());
            std::wstring uriAbsoluteRaw = m_uriBase + filenameW;

            return std::make_shared<std::ifstream>(uriAbsoluteRaw, std::ios::binary);
        }
    private:
        const std::wstring m_uriBase;
    };

    class GLBStreamFactory : public Microsoft::glTF::IStreamFactory
    {
    public:
        GLBStreamFactory(const std::wstring& filename) :
            m_stream(std::make_shared<std::ofstream>(filename, std::ios_base::binary | std::ios_base::out)),
            m_tempStream(std::make_shared<std::stringstream>(std::ios_base::binary | std::ios_base::in | std::ios_base::out))
        { }

        std::shared_ptr<std::istream> GetInputStream(const std::string&) const override
        {
            throw std::logic_error("Not implemented");
        }

        std::shared_ptr<std::ostream> GetOutputStream(const std::string&) const override
        {
            return m_stream;
        }

        std::shared_ptr<std::iostream> GetTemporaryStream(const std::string&) const override
        {
            return m_tempStream;
        }
    private:
        std::shared_ptr<std::ofstream> m_stream;
        std::shared_ptr<std::stringstream> m_tempStream;
    };
}

void glTFSerialization::UnpackGLB(StorageFile^ glbFile, StorageFolder^ outputFolder)
{
    String^ glpFilePath = glbFile->Path;
    std::wstring glbPathW = glpFilePath->Data();
    std::string glbPathA = std::string(glbPathW.begin(), glbPathW.end());

    String^ outputFolderPath = outputFolder->Path + "\\";
    std::wstring outputFolderPathW = outputFolderPath->Data();
    std::string outputFolderPathA = std::string(outputFolderPathW.begin(), outputFolderPathW.end());

    String^ baseFileName = glbFile->DisplayName;
    std::wstring baseFileNameW = baseFileName->Data();
    std::string baseFileNameA = std::string(baseFileNameW.begin(), baseFileNameW.end());

    GLBToGLTF::UnpackGLB(glbPathA, outputFolderPathA, baseFileNameA);
}

StorageFile^ glTFSerialization::PackGLTF(StorageFile^ sourceGltf, StorageFolder^ outputFolder, String^ glbName)
{
    std::wstring gltfPathW = sourceGltf->Path->Data();

    auto stream = std::make_shared<std::ifstream>(gltfPathW, std::ios::binary);
    GLTFDocument document = DeserializeJson(*stream);

    auto getParentTask = create_task(sourceGltf->GetParentAsync());
    StorageFolder^ gltfFolder = getParentTask.get();
    GLTFStreamReader streamReader(gltfFolder);

    String^ outputGlbPath = outputFolder->Path + "\\" + glbName;
    std::wstring outputGlbPathW = outputGlbPath->Data();
    std::unique_ptr<const IStreamFactory> streamFactory = std::make_unique<GLBStreamFactory>(outputGlbPathW);
    SerializeBinary(document, streamReader, streamFactory);

    auto getGlbFileTask = create_task(outputFolder->GetFileAsync(glbName));
    return getGlbFileTask.get();
}
