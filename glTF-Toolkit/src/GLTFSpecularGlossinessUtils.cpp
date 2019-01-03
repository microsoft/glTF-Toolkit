// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureUtils.h"
#include "GLTFSDK/ExtensionsKHR.h"
#include "GLTFSDK/PBRUtils.h"

#include "GLTFSpecularGlossinessUtils.h"

// Usings for glTF
using namespace Microsoft::glTF;
using namespace Toolkit;
using namespace DirectX;

void ConvertEntrySpecularGlossinessToMetallicRoughness(
    const XMVECTORF32& diffuseColor,
    const XMVECTORF32& specGloss,
    XMVECTORF32& diffuseOut,
    float& metallicOut,
    float& roughnessOut)
{
    SpecularGlossinessValue sg;
    sg.diffuse = Color3(diffuseColor[0], diffuseColor[1], diffuseColor[2]);
    sg.opacity = diffuseColor[3];
    sg.specular = Color3(specGloss[0], specGloss[1], specGloss[2]);
    sg.glossiness = specGloss[3];

    MetallicRoughnessValue mr = SGToMR(sg);

    roughnessOut = mr.roughness;
    metallicOut = mr.metallic;
    diffuseOut = { mr.base.r, mr.base.g, mr.base.b, mr.opacity };
}


void ConvertTextureSpecularGlossinessToMetallicRoughness(
    ScratchImage& out_metallicRoughnessTexture,
    ScratchImage& out_modulatedDiffuseTexture, 
    const std::unique_ptr<ScratchImage>& diffuseTexture, 
    const XMVECTORF32& diffuseFactor,
    const std::unique_ptr<ScratchImage>& specularGlossinessTexture,
    const XMVECTORF32& specularFactor)
{
    size_t targetWidth = 4;
    size_t targetHeight = 4;

    uint8_t* diffusePixels = nullptr;
    if (diffuseTexture != nullptr)
    {
        targetWidth = diffuseTexture->GetMetadata().width;
        targetHeight = diffuseTexture->GetMetadata().height;
        diffusePixels = diffuseTexture->GetPixels();
    }
    else if (specularGlossinessTexture != nullptr)
    {
        targetWidth = specularGlossinessTexture->GetMetadata().width;
        targetHeight = specularGlossinessTexture->GetMetadata().height;
    }

    uint8_t* specGlossPixels = nullptr;
    if (specularGlossinessTexture)
    {
        GLTFTextureUtils::ResizeIfNeeded(specularGlossinessTexture, targetWidth, targetHeight);
        specGlossPixels = specularGlossinessTexture->GetPixels();
    }
    
    out_modulatedDiffuseTexture.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, targetWidth, targetHeight, 1, 1);
    out_metallicRoughnessTexture.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, targetWidth, targetHeight, 1, 1);

    auto diffuseOutPixels = out_modulatedDiffuseTexture.GetPixels();
    auto metalRoughPixels = out_metallicRoughnessTexture.GetPixels();

    for (uint32_t i = 0; i < targetHeight * targetWidth; ++i)
    {
        XMVECTORF32 diffuseColor { 1.0f, 1.0f, 1.0f, 1.0f };
        if (diffusePixels != nullptr)
        {
            memcpy_s(&diffuseColor, 16, GLTFTextureUtils::GetChannelValue(diffusePixels, i, Red), 16);
        }
        diffuseColor.v = diffuseColor * diffuseFactor;

        XMVECTORF32 specGloss { 1.0f, 1.0f, 1.0f, 1.0f };
        if (specularGlossinessTexture != nullptr)
        {
            memcpy_s(&specGloss, 16, GLTFTextureUtils::GetChannelValue(specGlossPixels, i, Red), 16);
        }
        specGloss.v = specGloss * specularFactor;

        float metallic;
        float roughness;
        XMVECTORF32 diffuseColorOut;
        ConvertEntrySpecularGlossinessToMetallicRoughness(diffuseColor, specGloss, diffuseColorOut, metallic, roughness);

        *GLTFTextureUtils::GetChannelValue(metalRoughPixels, i, Green) = roughness;
        *GLTFTextureUtils::GetChannelValue(metalRoughPixels, i, Blue) = metallic;
        auto diffuseOutPtr = GLTFTextureUtils::GetChannelValue(diffuseOutPixels, i, Red);
        memcpy_s(diffuseOutPtr, 16, diffuseColorOut, 16);
    }
}


Document GLTFSpecularGlossinessUtils::ConvertMaterial(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const Material & material, const std::string& outputDirectory)
{
    if (!material.HasExtension<KHR::Materials::PBRSpecularGlossiness>())
    {
        return doc;
    }

    Document resultDoc(doc);
    Material resultMaterial(material);
    resultMaterial.RemoveExtension<KHR::Materials::PBRSpecularGlossiness>();

    const auto& specularGlossiness = material.GetExtension<KHR::Materials::PBRSpecularGlossiness>();

    XMVECTORF32 diffuseFactorIn = {
        specularGlossiness.diffuseFactor.r,
        specularGlossiness.diffuseFactor.g,
        specularGlossiness.diffuseFactor.b,
        specularGlossiness.diffuseFactor.a
    };

    XMVECTORF32 specularFactor = {
        specularGlossiness.specularFactor.r,
        specularGlossiness.specularFactor.g,
        specularGlossiness.specularFactor.b,
        specularGlossiness.glossinessFactor
    };

    // First, we check if there actually is a diffuse or specular glossiness texture to convert.
    // If not, we only perform the conversion on the materials parameters and early out.
    if (specularGlossiness.diffuseTexture.textureId.empty() &&
        specularGlossiness.specularGlossinessTexture.textureId.empty())
    {
        XMVECTORF32 diffuseFactor;
        float metallicFactor;
        float roughnessFactor;

        ConvertEntrySpecularGlossinessToMetallicRoughness(diffuseFactorIn, specularFactor, diffuseFactor, metallicFactor, roughnessFactor);
        resultMaterial.metallicRoughness.baseColorFactor.r = diffuseFactor.f[0];
        resultMaterial.metallicRoughness.baseColorFactor.g = diffuseFactor.f[1];
        resultMaterial.metallicRoughness.baseColorFactor.b = diffuseFactor.f[2];
        resultMaterial.metallicRoughness.baseColorFactor.a = diffuseFactor.f[3];
        resultMaterial.metallicRoughness.metallicFactor = metallicFactor;
        resultMaterial.metallicRoughness.roughnessFactor = roughnessFactor;

        resultDoc.materials.Replace(resultMaterial);
    }

    std::string samplerId;

    // Diffuse texture
    std::unique_ptr<ScratchImage> diffuseTexture;
    if (!specularGlossiness.diffuseTexture.textureId.empty())
    {
        try
        {
            diffuseTexture = std::make_unique<ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, specularGlossiness.diffuseTexture.textureId, false));
            samplerId = doc.textures[specularGlossiness.diffuseTexture.textureId].samplerId;
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load diffuse texture.");
        }
    }

    // SpecularGlossiness texture
    std::unique_ptr<ScratchImage> specularGlossinessTexture;
    if (!specularGlossiness.specularGlossinessTexture.textureId.empty())
    {
        try
        {
            specularGlossinessTexture = std::make_unique<ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, specularGlossiness.specularGlossinessTexture.textureId, false));
            samplerId = samplerId.empty() ? doc.textures[specularGlossiness.specularGlossinessTexture.textureId].samplerId : samplerId;
        }
        catch (GLTFException)
        {
            throw GLTFException("Failed to load specular glossiness texture.");
        }
    }

    ScratchImage metallicRoughnessTexture;
    ScratchImage modulatedDiffuseTexture;

    ConvertTextureSpecularGlossinessToMetallicRoughness(
        metallicRoughnessTexture,
        modulatedDiffuseTexture,
        diffuseTexture,
        diffuseFactorIn, // will be baked into texture
        specularGlossinessTexture,
        specularFactor);

    Material::PBRMetallicRoughness gltfPBRMetallicRoughness;

    {
        DirectX::ScratchImage converted;
        if (FAILED(DirectX::Convert(*metallicRoughnessTexture.GetImage(0, 0, 0), DXGI_FORMAT_B8G8R8X8_UNORM, DirectX::TEX_FILTER_SRGB_IN, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
        {
            throw GLTFException("Failed to convert texture to DXGI_FORMAT_B8G8R8X8_UNORM for processing.");
        }

        auto metallicRoughnessPath = GLTFTextureUtils::SaveAsPng(&converted, "metallicRoughness_" + material.id + ".png", outputDirectory);
        auto metallicRoughnessImageId = GLTFTextureUtils::AddImageToDocument(resultDoc, metallicRoughnessPath);
        Texture mrTexture;
        mrTexture.samplerId = samplerId;
        mrTexture.imageId = metallicRoughnessImageId;
        gltfPBRMetallicRoughness.metallicRoughnessTexture.textureId = resultDoc.textures.Append(mrTexture, AppendIdPolicy::GenerateOnEmpty).id;
    }

    {
        DirectX::ScratchImage converted;
        if (FAILED(DirectX::Convert(*modulatedDiffuseTexture.GetImage(0, 0, 0), DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
        {
            throw GLTFException("Failed to convert texture to DXGI_FORMAT_B8G8R8A8_UNORM_SRGB for processing.");
        }
        auto diffusePath = GLTFTextureUtils::SaveAsPng(&converted, "diffuse_" + material.id + ".png", outputDirectory, &GUID_WICPixelFormat32bppBGRA);
        auto diffuseImageId = GLTFTextureUtils::AddImageToDocument(resultDoc, diffusePath);
        Texture diffusGltfTexture;
        diffusGltfTexture.samplerId = samplerId;
        diffusGltfTexture.imageId = diffuseImageId;
        gltfPBRMetallicRoughness.baseColorTexture.textureId = resultDoc.textures.Append(diffusGltfTexture, AppendIdPolicy::GenerateOnEmpty).id;
    }

    resultMaterial.metallicRoughness = gltfPBRMetallicRoughness;
    resultDoc.materials.Replace(resultMaterial);

    return resultDoc;
}


Document GLTFSpecularGlossinessUtils::ConvertMaterials(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const std::string & outputDirectory)
{
    Document resultDocument(doc);
    for (const auto& material : doc.materials.Elements())
    {
        resultDocument = ConvertMaterial(streamReader, resultDocument, material, outputDirectory);
    }

    resultDocument.extensionsUsed.erase(KHR::Materials::PBRSPECULARGLOSSINESS_NAME);
    resultDocument.extensionsRequired.erase(KHR::Materials::PBRSPECULARGLOSSINESS_NAME);

    return resultDocument;
}
