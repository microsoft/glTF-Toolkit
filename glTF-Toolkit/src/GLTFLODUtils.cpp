// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureCompressionUtils.h"
#include "GLTFTexturePackingUtils.h"
#include "GLTFLODUtils.h"

#include "GLTFSDK/GLTF.h"
#include "GLTFSDK/Constants.h"
#include "GLTFSDK/Deserialize.h"
#include "GLTFSDK/RapidJsonUtils.h"
#include "GLTFSDK/ExtensionsKHR.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <codecvt>
#include <filesystem>

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_LOD = "MSFT_lod";
const char* Microsoft::glTF::Toolkit::MSFT_LOD_IDS_KEY = "ids";

namespace
{
    inline void AddIndexOffset(std::string& id, size_t offset)
    {
        // an empty id string indicates that the id is not inuse and therefore should not be updated
        id = (id.empty()) ? "" : std::to_string(std::stoi(id) + offset);
    }

    inline void AddIndexOffset(MeshPrimitive& primitive, const char* attributeName, size_t offset)
    {
        // an empty id string indicates that the id is not inuse and therefore should not be updated
        auto attributeItr = primitive.attributes.find(attributeName);
        if (attributeItr != primitive.attributes.end())
        {
            attributeItr->second = std::to_string(std::stoi(attributeItr->second) + offset);
        }
    }

    inline void AddIndexOffsetPacked(rapidjson::Value& json, const char* textureId, size_t offset)
    {
        if (json.HasMember(textureId))
        {
            if (json[textureId].HasMember(MSFT_PACKING_INDEX_KEY))
            {
                auto index = json[textureId][MSFT_PACKING_INDEX_KEY].GetInt();
                json[textureId][MSFT_PACKING_INDEX_KEY] = index + offset;
            }
        }
    }

    std::vector<std::string> ParseExtensionMSFTLod(const Node& node)
    {
        std::vector<std::string> lodIds;

        auto lodExtension = node.extensions.find(Toolkit::EXTENSION_MSFT_LOD);
        if (lodExtension != node.extensions.end())
        {
            auto json = RapidJsonUtils::CreateDocumentFromString(lodExtension->second);

            auto idIt = json.FindMember(Toolkit::MSFT_LOD_IDS_KEY);
            if (idIt != json.MemberEnd())
            {
                for (rapidjson::Value::ConstValueIterator ait = idIt->value.Begin(); ait != idIt->value.End(); ++ait)
                {
                    lodIds.push_back(std::to_string(ait->GetInt()));
                }
            }
        }

        return lodIds;
    }

    template <typename T>
    std::string SerializeExtensionMSFTLod(const T&, const std::vector<std::string>& lods, const Document& document)
    {
        // Omit MSFT_lod entirely if no LODs are available
        if (lods.empty())
        {
            return std::string();
        }

        rapidjson::Document doc(rapidjson::kObjectType);
        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        std::vector<size_t> lodIndices;
        lodIndices.reserve(lods.size());

        if (std::is_same<T, Material>())
        {
            for (const auto& lodId : lods)
            {
                lodIndices.push_back(ToKnownSizeType(document.materials.GetIndex(lodId)));
            }
        }
        else if (std::is_same<T, Node>())
        {
            for (const auto& lodId : lods)
            {
                lodIndices.push_back(ToKnownSizeType(document.nodes.GetIndex(lodId)));
            }
        }
        else
        {
            throw GLTFException("LODs can only be applied to materials or nodes.");
        }

        doc.AddMember(RapidJsonUtils::ToStringValue(Toolkit::MSFT_LOD_IDS_KEY, a), RapidJsonUtils::ToJsonArray(lodIndices, a), a);

        rapidjson::StringBuffer stringBuffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);
        doc.Accept(writer);

        return stringBuffer.GetString();
    }

    Document AddGLTFNodeLOD(const Document& primary, LODMap& primaryLods, const Document& lod, const std::wstring& relativePath = L"", bool sharedMaterials = false)
    {
        Microsoft::glTF::Document gltfLod(primary);

        auto primaryScenes = primary.scenes.Elements();
        auto lodScenes = lod.scenes.Elements();

        size_t MaxLODLevel = 0;

        // Both GLTF must have equivalent number and order of scenes and root nodes per scene otherwise merge will not be possible
        bool sceneNodeMatch = false;
        if (primaryScenes.size() == lodScenes.size())
        {
            for (size_t sceneIdx = 0; sceneIdx < primaryScenes.size(); sceneIdx++)
            {

                if ((primaryScenes[sceneIdx].nodes.size() == lodScenes[sceneIdx].nodes.size()) &&
                    (lodScenes[sceneIdx].nodes.size() == 1 ||
                        std::equal(primaryScenes[sceneIdx].nodes.begin(), primaryScenes[sceneIdx].nodes.end(), lodScenes[sceneIdx].nodes.begin()))
                    )
                {
                    sceneNodeMatch = true;
                    auto primaryRootNode = gltfLod.nodes.Get(primaryScenes[sceneIdx].nodes[0]);
                    MaxLODLevel = std::max(MaxLODLevel, primaryLods.at(primaryRootNode.id)->size());
                }
                else
                {
                    sceneNodeMatch = false;
                    break;
                }
            }
        }

        MaxLODLevel++;

        if (!sceneNodeMatch || primaryScenes.empty())
        {
            // Mis-match or empty scene; either way cannot merge Lod in
            throw new std::runtime_error("Primary Scene either empty or does not match scene node count of LOD gltf");
        }

        std::string nodeLodLabel = "_lod" + std::to_string(MaxLODLevel);

        // lod merge is performed from the lowest reference back upwards
        // e.g. buffers/samplers/extensions do not reference any other part of the gltf manifest    
        size_t buffersOffset = gltfLod.buffers.Size();
        size_t samplersOffset = sharedMaterials ? 0 : gltfLod.samplers.Size();
        {
            auto lodBuffers = lod.buffers.Elements();
            for (auto buffer : lodBuffers)
            {
                AddIndexOffset(buffer.id, buffersOffset);
                std::string relativePathUtf8 = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(relativePath);
                buffer.uri = relativePathUtf8 + buffer.uri;
                gltfLod.buffers.Append(std::move(buffer));
            }

            if (!sharedMaterials)
            {
                auto lodSamplers = lod.samplers.Elements();
                for (auto sampler : lodSamplers)
                {
                    AddIndexOffset(sampler.id, samplersOffset);
                    gltfLod.samplers.Append(std::move(sampler));
                }
            }

            for (const auto& extension : lod.extensionsUsed)
            {
                gltfLod.extensionsUsed.insert(extension);
            }
            // ensure that MSFT_LOD extension is specified as being used
            gltfLod.extensionsUsed.insert(Toolkit::EXTENSION_MSFT_LOD);
        }

        size_t accessorOffset = gltfLod.accessors.Size();
        size_t texturesOffset = gltfLod.textures.Size();
        {
            // Buffer Views depend upon Buffers
            size_t bufferViewsOffset = gltfLod.bufferViews.Size();
            auto lodBufferViews = lod.bufferViews.Elements();
            for (auto bufferView : lodBufferViews)
            {
                AddIndexOffset(bufferView.id, bufferViewsOffset);
                AddIndexOffset(bufferView.bufferId, buffersOffset);
                gltfLod.bufferViews.Append(std::move(bufferView));
            }

            // Accessors depend upon Buffer views        
            auto lodAccessors = lod.accessors.Elements();
            for (auto accessor : lodAccessors)
            {
                AddIndexOffset(accessor.id, accessorOffset);
                AddIndexOffset(accessor.bufferViewId, bufferViewsOffset);
                gltfLod.accessors.Append(std::move(accessor));
            }

            // Images depend upon Buffer views
            size_t imageOffset = sharedMaterials ? 0 : gltfLod.images.Size();
            if (!sharedMaterials)
            {
                auto lodImages = lod.images.Elements();
                for (auto image : lodImages)
                {

                    AddIndexOffset(image.id, imageOffset);
                    AddIndexOffset(image.bufferViewId, bufferViewsOffset);

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                    std::wstring uri = conv.from_bytes(image.uri);
                    if (std::experimental::filesystem::path(uri).is_relative()) {
                        // to be able to reference images with the same name, prefix with relative path
                        std::string relativePathUtf8 = conv.to_bytes(relativePath);
                        image.uri = relativePathUtf8 + image.uri;
                    }
                    gltfLod.images.Append(std::move(image));
                }

                // Textures depend upon Samplers and Images
                auto lodTextures = lod.textures.Elements();
                for (auto texture : lodTextures)
                {
                    AddIndexOffset(texture.id, texturesOffset);
                    AddIndexOffset(texture.samplerId, samplersOffset);
                    AddIndexOffset(texture.imageId, imageOffset);

                    // MSFT_texture_dds extension
                    auto ddsExtensionIt = texture.extensions.find(EXTENSION_MSFT_TEXTURE_DDS);
                    if (ddsExtensionIt != texture.extensions.end() && !ddsExtensionIt->second.empty())
                    {
                        rapidjson::Document ddsJson = RapidJsonUtils::CreateDocumentFromString(ddsExtensionIt->second);

                        if (ddsJson.HasMember("source"))
                        {
                            auto index = ddsJson["source"].GetInt();
                            ddsJson["source"] = index + imageOffset;
                        }

                        rapidjson::StringBuffer buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                        ddsJson.Accept(writer);

                        ddsExtensionIt->second = buffer.GetString();
                    }

                    gltfLod.textures.Append(std::move(texture));
                }
            }
        }

        // Material Merge
        // Note the extension KHR_materials_pbrSpecularGlossiness will be also updated
        // Materials depend upon textures
        size_t materialOffset = sharedMaterials ? 0 : gltfLod.materials.Size();
        if (!sharedMaterials)
        {
            auto lodMaterials = lod.materials.Elements();
            for (auto material : lodMaterials)
            {
                // post-fix with lod level indication; 
                // no functional reason other than making it easier to natively read gltf files with lods
                material.name += nodeLodLabel;
                AddIndexOffset(material.id, materialOffset);

                AddIndexOffset(material.normalTexture.textureId, texturesOffset);
                AddIndexOffset(material.occlusionTexture.textureId, texturesOffset);
                AddIndexOffset(material.emissiveTexture.textureId, texturesOffset);

                AddIndexOffset(material.metallicRoughness.baseColorTexture.textureId, texturesOffset);
                AddIndexOffset(material.metallicRoughness.metallicRoughnessTexture.textureId, texturesOffset);

                if (material.HasExtension<KHR::Materials::PBRSpecularGlossiness>())
                {
                    AddIndexOffset(material.GetExtension<KHR::Materials::PBRSpecularGlossiness>().diffuseTexture.textureId, texturesOffset);
                    AddIndexOffset(material.GetExtension<KHR::Materials::PBRSpecularGlossiness>().specularGlossinessTexture.textureId, texturesOffset);
                }

                // MSFT_packing_occlusionRoughnessMetallic packed textures
                auto ormExtensionIt = material.extensions.find(EXTENSION_MSFT_PACKING_ORM);
                if (ormExtensionIt != material.extensions.end() && !ormExtensionIt->second.empty())
                {
                    rapidjson::Document ormJson = RapidJsonUtils::CreateDocumentFromString(ormExtensionIt->second);

                    AddIndexOffsetPacked(ormJson, MSFT_PACKING_ORM_ORMTEXTURE_KEY, texturesOffset);
                    AddIndexOffsetPacked(ormJson, MSFT_PACKING_ORM_RMOTEXTURE_KEY, texturesOffset);
                    AddIndexOffsetPacked(ormJson, MSFT_PACKING_ORM_NORMALTEXTURE_KEY, texturesOffset);

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    ormJson.Accept(writer);

                    ormExtensionIt->second = buffer.GetString();
                }

                // MSFT_packing_normalRoughnessMetallic packed texture
                auto nrmExtensionIt = material.extensions.find(EXTENSION_MSFT_PACKING_NRM);
                if (nrmExtensionIt != material.extensions.end() && !nrmExtensionIt->second.empty())
                {
                    rapidjson::Document nrmJson = RapidJsonUtils::CreateDocumentFromString(nrmExtensionIt->second);

                    AddIndexOffsetPacked(nrmJson, MSFT_PACKING_NRM_KEY, texturesOffset);
                    
                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    nrmJson.Accept(writer);

                    nrmExtensionIt->second = buffer.GetString();
                }

                gltfLod.materials.Append(std::move(material));
            }
        }

        // Meshs depend upon Accessors and Materials
        size_t meshOffset = gltfLod.meshes.Size();
        {
            auto lodMeshes = lod.meshes.Elements();
            for (auto mesh : lodMeshes)
            {
                // post-fix with lod level indication; 
                // no functional reason other than making it easier to natively read gltf files with lods
                mesh.name += nodeLodLabel;
                AddIndexOffset(mesh.id, meshOffset);

                for (auto &primitive : mesh.primitives)
                {
                    AddIndexOffset(primitive.indicesAccessorId, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_POSITION, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_NORMAL, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_TEXCOORD_0, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_TEXCOORD_1, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_COLOR_0, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_TANGENT, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_JOINTS_0, accessorOffset);
                    AddIndexOffset(primitive, ACCESSOR_WEIGHTS_0, accessorOffset);

                    if (sharedMaterials)
                    {
                        // lower quality LODs can have fewer images and textures than the highest LOD,
                        // so we need to find the correct material index for the same material from the highest LOD

                        auto localMaterial = lod.materials.Get(primitive.materialId);

                        // find merged material index for the given material index in this LOD
                        auto iter = std::find_if(gltfLod.materials.Elements().begin(),
                                gltfLod.materials.Elements().end(),
                                [localMaterial](auto globalMaterial) {
                                    // check that the materials are the same, noting that the texture and material ids will differ
                                    return localMaterial.name == globalMaterial.name &&
                                           localMaterial.alphaMode == globalMaterial.alphaMode &&
                                           localMaterial.alphaCutoff == globalMaterial.alphaCutoff &&
                                           localMaterial.emissiveFactor == globalMaterial.emissiveFactor &&
                                           localMaterial.doubleSided == globalMaterial.doubleSided &&
                                           localMaterial.metallicRoughness.baseColorFactor == globalMaterial.metallicRoughness.baseColorFactor &&
                                           localMaterial.metallicRoughness.metallicFactor == globalMaterial.metallicRoughness.metallicFactor &&
                                           localMaterial.occlusionTexture.strength == globalMaterial.occlusionTexture.strength &&
                                           localMaterial.HasExtension<KHR::Materials::PBRSpecularGlossiness>() == globalMaterial.HasExtension<KHR::Materials::PBRSpecularGlossiness>() && 
                                           (!localMaterial.HasExtension<KHR::Materials::PBRSpecularGlossiness>() ||
                                             (localMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().diffuseFactor == globalMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().diffuseFactor &&
                                              localMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().glossinessFactor == globalMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().glossinessFactor &&
                                              localMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().specularFactor == globalMaterial.GetExtension<KHR::Materials::PBRSpecularGlossiness>().specularFactor)
                                           );
                                }
                        );

                        if (iter != gltfLod.materials.Elements().end())
                        {
                            size_t newMaterialIndex = std::distance(gltfLod.materials.Elements().begin(), iter);
                            primitive.materialId = std::to_string(newMaterialIndex);
                        }
                        else
                        {
                            throw new std::runtime_error("Couldn't find the shared material in the highest LOD.");
                        }
                    }
                    else
                    {
                        AddIndexOffset(primitive.materialId, materialOffset);
                    }
                }

                gltfLod.meshes.Append(std::move(mesh));
            }
        }

        // Nodes depend upon Nodes and Meshes
        size_t nodeOffset = gltfLod.nodes.Size();
        // Skins depend upon Nodes
        size_t skinOffset = gltfLod.skins.Size();
        {
            auto nodes = lod.nodes.Elements();
            for (auto node : nodes)
            {
                // post-fix with lod level indication; 
                // no functional reason other than making it easier to natively read gltf files with lods
                node.name += nodeLodLabel;
                AddIndexOffset(node.id, nodeOffset);
                AddIndexOffset(node.meshId, meshOffset);
                if (!node.skinId.empty())
                {
                    AddIndexOffset(node.skinId, skinOffset);
                }

                for (auto &child : node.children)
                {
                    AddIndexOffset(child, nodeOffset);
                }

                gltfLod.nodes.Append(std::move(node));
            }
        }

        {
            auto skins = lod.skins.Elements();
            for (auto skin : skins)
            {
                // post-fix with lod level indication; 
                // no functional reason other than making it easier to natively read gltf files with lods
                skin.name += nodeLodLabel;
                AddIndexOffset(skin.id, skinOffset);
                AddIndexOffset(skin.skeletonId, nodeOffset);
                AddIndexOffset(skin.inverseBindMatricesAccessorId, accessorOffset);

                for (auto &jointId : skin.jointIds)
                {
                    AddIndexOffset(jointId, nodeOffset);
                }

                gltfLod.skins.Append(std::move(skin));
            }
        }

        // Animation channels depend upon Nodes and Accessors
        {
            for (size_t animationIndex = 0; animationIndex < gltfLod.animations.Size(); animationIndex++)
            {
                const auto &baseAnimation = gltfLod.animations[animationIndex];
                Animation newAnimation(baseAnimation);
                const auto &lodAnimation = lod.animations[animationIndex];

                size_t samplerOffset = baseAnimation.samplers.Size();
                for (const auto &sampler : lodAnimation.samplers.Elements())
                {
                    AnimationSampler newSampler(sampler);
                    AddIndexOffset(newSampler.id, samplerOffset);
                    AddIndexOffset(newSampler.inputAccessorId, accessorOffset);
                    AddIndexOffset(newSampler.outputAccessorId, accessorOffset);
                    newAnimation.samplers.Append(std::move(newSampler));
                }
                
                size_t channelOffset = baseAnimation.channels.Size();
                for (auto channel : lodAnimation.channels.Elements())
                {
                    AddIndexOffset(channel.id, channelOffset);
                    AddIndexOffset(channel.target.nodeId, nodeOffset);
                    AddIndexOffset(channel.samplerId, samplerOffset);

                    newAnimation.channels.Append(std::move(channel));
                }
                gltfLod.animations.Replace(newAnimation);
            }
        }

        // update the primary GLTF root nodes lod extension to reference the new lod root node
        // N.B. new lods are always added to the back
        for (size_t sceneIdx = 0; sceneIdx < primaryScenes.size(); sceneIdx++)
        {
            for (size_t rootNodeIdx = 0; rootNodeIdx < primaryScenes[sceneIdx].nodes.size(); rootNodeIdx++)
            {
                auto idx = primaryScenes[sceneIdx].nodes[rootNodeIdx];
                Node nodeWithLods(gltfLod.nodes.Get(idx));
                int lodRootIdx = std::stoi(lodScenes[sceneIdx].nodes[rootNodeIdx]) + static_cast<int>(nodeOffset);
                auto primaryNodeLod = primaryLods.at(nodeWithLods.id);
                primaryNodeLod->emplace_back(std::to_string(lodRootIdx));
            }
        }

        return gltfLod;
    }
}

LODMap GLTFLODUtils::ParseDocumentNodeLODs(const Document& doc)
{
    LODMap lodMap;

    for (auto node : doc.nodes.Elements())
    {
        lodMap.emplace(node.id, std::move(std::make_shared<std::vector<std::string>>(ParseExtensionMSFTLod(node))));
    }

    return lodMap;
}

Document GLTFLODUtils::MergeDocumentsAsLODs(const std::vector<Document>& docs, const std::vector<std::wstring>& relativePaths, const bool& sharedMaterials)
{
    if (docs.empty())
    {
        throw std::invalid_argument("MergeDocumentsAsLODs passed empty vector");
    }

    Document gltfPrimary(docs[0]);
    LODMap lods = ParseDocumentNodeLODs(gltfPrimary);

    for (size_t i = 1; i < docs.size(); i++)
    {
        gltfPrimary = AddGLTFNodeLOD(gltfPrimary, lods, docs[i], (relativePaths.size() == docs.size() - 1 ? relativePaths[i - 1] : L""), sharedMaterials);
    }

    for (auto lod : lods)
    {
        if (lod.second == nullptr || lod.second->size() == 0)
        {
            continue;
        }

        auto node = gltfPrimary.nodes.Get(lod.first);

        auto lodExtensionValue = SerializeExtensionMSFTLod<Node>(node, *lod.second, gltfPrimary);
        if (!lodExtensionValue.empty())
        {
            node.extensions.emplace(EXTENSION_MSFT_LOD, lodExtensionValue);
            gltfPrimary.nodes.Replace(node);
        }
    }

    return gltfPrimary;
}

Document GLTFLODUtils::MergeDocumentsAsLODs(const std::vector<Document>& docs, const std::vector<double>& screenCoveragePercentages, const std::vector<std::wstring>& relativePaths, const bool& sharedMaterials)
{
    Document merged = MergeDocumentsAsLODs(docs, relativePaths, sharedMaterials);

    if (screenCoveragePercentages.size() == 0)
    {
        return merged;
    }

    for (auto scene : merged.scenes.Elements())
    {
        for (auto rootNodeIndex : scene.nodes)
        {
            auto primaryRootNode = merged.nodes.Get(rootNodeIndex);

            rapidjson::Document extrasJson(rapidjson::kObjectType);
            if (!primaryRootNode.extras.empty())
            {
                extrasJson.Parse(primaryRootNode.extras.c_str());
            }
            rapidjson::Document::AllocatorType& allocator = extrasJson.GetAllocator();

            rapidjson::Value screenCoverageArray = RapidJsonUtils::ToJsonArray(screenCoveragePercentages, allocator);

            extrasJson.AddMember("MSFT_screencoverage", screenCoverageArray, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            extrasJson.Accept(writer);

            primaryRootNode.extras = buffer.GetString();

            merged.nodes.Replace(primaryRootNode);
        }
    }

    return merged;
}

uint32_t GLTFLODUtils::NumberOfNodeLODLevels(const Document& doc, const LODMap& lods)
{
    size_t maxLODLevel = 0;
    for (auto node : doc.nodes.Elements())
    {
        maxLODLevel = std::max(maxLODLevel, lods.at(node.id)->size());
    }

    return static_cast<uint32_t>(maxLODLevel);
}

