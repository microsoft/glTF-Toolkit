/**************************************************************
*                                                             *
*  Copyright (c) Microsoft Corporation. All rights reserved.  *
*               Licensed under the MIT License.               *
*                                                             *
**************************************************************/

#pragma once

#include "GLTFSDK/IResourceWriter.h"

#include <functional>

namespace Microsoft::glTF
{
	class GLTFDocument;

	namespace exp
	{
		struct AccessorDesc
		{
			AccessorDesc() = default;

			AccessorDesc(AccessorType accessorType, ComponentType componentType, bool normalized)
				: AccessorDesc(accessorType, componentType, {}, {}, 0, normalized)
			{ }

			AccessorDesc(AccessorType accessorType, ComponentType componentType, std::vector<float> minValues ={}, std::vector<float> maxValues ={},
				size_t byteOffset = 0, bool normalized = false)
				: accessorType(accessorType), componentType(componentType), normalized(normalized),
				byteOffset(byteOffset), minValues(std::move(minValues)), maxValues(std::move(maxValues))
			{ }

			bool IsValid() const { return accessorType != TYPE_UNKNOWN && componentType != COMPONENT_UNKNOWN; }

			AccessorType accessorType;
			ComponentType componentType;
			bool normalized;
			size_t byteOffset;
			std::vector<float> minValues;
			std::vector<float> maxValues;
		};

		class BufferBuilder final
		{
			typedef std::function<std::string(const BufferBuilder&)> FnGenId;

		public:
			BufferBuilder(std::unique_ptr<ResourceWriter2>&& resourceWriter,
				FnGenId fnGenBufferId = DefaultFnGenBufferId,
				FnGenId fnGenBufferViewId = DefaultFnGenBufferViewId,
				FnGenId fnGenAccessorId = DefaultFnGenAccessorId);

			const Buffer&     AddBuffer(const char* bufferId = nullptr);
			const BufferView& AddBufferView(BufferViewTarget target);
			const BufferView& AddBufferView(const void* data, size_t byteLength, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER);

			template<typename T>
			const BufferView& AddBufferView(const std::vector<T>& data, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER)
			{
				return AddBufferView(data.data(), data.size() * sizeof(T), byteStride, target);
			}

			void AddAccessors(const void* data, size_t count, size_t byteStride, const AccessorDesc* pDescs, size_t descCount, std::string* pOutIds = nullptr);

			const Accessor& AddAccessor(const void* data, size_t count, AccessorDesc accessorDesc);

			template<typename T>
			const Accessor& AddAccessor(const std::vector<T>& data, AccessorDesc accessorDesc)
			{
				const auto accessorTypeSize = Accessor::GetTypeCount(accessorDesc.accessorType);

				if (data.size() % accessorTypeSize)
				{
					throw InvalidGLTFException("vector size is not a multiple of accessor type size");
				}

				return AddAccessor(data.data(), data.size() / accessorTypeSize, std::move(accessorDesc));
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
			Accessor& AddAccessor(size_t count, AccessorDesc desc);

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
