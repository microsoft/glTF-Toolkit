/**************************************************************
*                                                             *
*  Copyright (c) Microsoft Corporation. All rights reserved.  *
*               Licensed under the MIT License.               *
*                                                             *
**************************************************************/

#pragma once

#include "GLTFSDK/IResourceWriter.h"

#include <functional>

namespace Microsoft 
{
	namespace glTF 
	{
		class GLTFDocument;

		namespace exp
		{
			class BufferBuilder final
			{
				typedef std::function<std::string(const BufferBuilder&)> FnGenId;

			public:
				BufferBuilder(std::unique_ptr<ResourceWriter2>&& resourceWriter,
					FnGenId fnGenBufferId = DefaultFnGenBufferId,
					FnGenId fnGenBufferViewId = DefaultFnGenBufferViewId,
					FnGenId fnGenAccessorId = DefaultFnGenAccessorId);

				const Buffer&     AddBuffer(const char* bufferId = nullptr);
				const BufferView& AddBufferView(BufferViewTarget target, size_t byteAlignment = 1);
				const BufferView& AddBufferView(const void* data, size_t byteLength, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER, size_t byteAlignment = 1);

				template<typename T>
				const BufferView& AddBufferView(const std::vector<T>& data, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER, size_t byteAlignment = 1)
				{
					return AddBufferView(data.data(), data.size() * sizeof(T), byteStride, target, byteAlignment);
				}

				const Accessor& AddAccessor(size_t count, size_t byteOffset, ComponentType componentType, AccessorType accessorType,
					std::vector<float> minValues ={}, std::vector<float> maxValues ={});
				const Accessor& AddAccessor(const void* data, size_t count, ComponentType componentType, AccessorType accessorType,
					std::vector<float> minValues ={}, std::vector<float> maxValues ={});

				template<typename T>
				const Accessor& AddAccessor(const std::vector<T>& data, ComponentType componentType, AccessorType accessorType,
					std::vector<float> minValues ={}, std::vector<float> maxValues ={})
				{
					const auto accessorTypeSize = Accessor::GetTypeCount(accessorType);

					if (data.size() % accessorTypeSize)
					{
						throw InvalidGLTFException("vector size is not a multiple of accessor type size");
					}

					return AddAccessor(data.data(), data.size() / accessorTypeSize, componentType, accessorType, std::move(minValues), std::move(maxValues));
				}

				void Output(GLTFDocument& gltfDocument);

				const Buffer&     GetCurrentBuffer() const;
				const BufferView& GetCurrentBufferView() const;
				const Accessor&   GetCurrentAccessor() const;

				size_t GetBufferCount() const;
				size_t GetBufferViewCount() const;
				size_t GetAccessorCount() const;

				ResourceWriter2& GetResourceWriter();
				const ResourceWriter2& GetResourceWriter() const;

			private:
				Accessor CreateAccessor(size_t count, size_t byteOffset, ComponentType componentType, AccessorType accessorType, std::vector<float> minValues, std::vector<float> maxValues);
				static std::string DefaultFnGenBufferId(const BufferBuilder& builder)
				{
					return std::to_string(builder.GetBufferCount());
				}

				static std::string DefaultFnGenBufferViewId(const BufferBuilder& builder)
				{
					return std::to_string(builder.GetBufferViewCount());
				}

				static std::string DefaultFnGenAccessorId(const BufferBuilder& builder)
				{
					return std::to_string(builder.GetAccessorCount());
				}

				std::unique_ptr<ResourceWriter2> m_resourceWriter;

				std::vector<Buffer>     m_buffers;
				std::vector<BufferView> m_bufferViews;
				std::vector<Accessor>   m_accessors;

				FnGenId m_fnGenBufferId;
				FnGenId m_fnGenBufferViewId;
				FnGenId m_fnGenAccessorId;
			};
		}
	}
}
