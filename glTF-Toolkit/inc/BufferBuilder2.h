// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceWriter2.h>

#include <functional>

namespace Microsoft::glTF::exp
{
	class BufferBuilder2
	{
		using FnGenId = std::function<size_t(const BufferBuilder2&)>;

	public:
		BufferBuilder2(std::unique_ptr<ResourceWriter2>&& resourceWriter,
			FnGenId&& fnGenBufferId = DefaultFnGenBufferId,
			FnGenId&& fnGenBufferViewId = DefaultFnGenBufferViewId,
			FnGenId&& fnGenAccessorId = DefaultFnGenAccessorId);

		const Buffer& AddBuffer(const char* bufferId = nullptr);

		template<typename T>
		const BufferView& AddBufferView(const std::vector<T>& data, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER, size_t byteAlignment = 1);
		const BufferView& AddBufferView(const void* data, size_t byteLength, size_t byteStride = 0, BufferViewTarget target = BufferViewTarget::UNKNOWN_BUFFER, size_t byteAlignment = 1);
		const BufferView& AddBufferView(BufferViewTarget target, size_t byteAlignment = 1);

		template<typename T>
		const Accessor& AddAccessor(const std::vector<T>& data, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues);
		const Accessor& AddAccessor(const void* data, size_t count, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues);
		const Accessor& AddAccessor(size_t count, size_t byteOffset, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues);

		void Output(GLTFDocument& gltfDocument);

		const Buffer&     GetCurrentBuffer(void) const { return m_buffers.back(); }
		const BufferView& GetCurrentBufferView(void) const { return m_bufferViews.back(); }
		const Accessor&   GetCurrentAccessor(void) const { return m_accessors.back(); }

		size_t GetBufferCount(void) const { return m_buffers.size(); }
		size_t GetBufferViewCount(void) const { return m_bufferViews.size(); }
		size_t GetAccessorCount(void) const { return m_accessors.size(); }

		ResourceWriter2& GetResourceWriter(void) { return *m_resourceWriter; }
		const ResourceWriter2& GetResourceWriter(void) const { return *m_resourceWriter; }

	private:
		static size_t DefaultFnGenBufferId(const BufferBuilder2& builder) { return builder.GetBufferCount(); }
		static size_t DefaultFnGenBufferViewId(const BufferBuilder2& builder) { return builder.GetBufferViewCount(); }
		static size_t DefaultFnGenAccessorId(const BufferBuilder2& builder) { return builder.GetAccessorCount(); }

		std::unique_ptr<ResourceWriter2> m_resourceWriter;

		std::vector<Buffer>     m_buffers;
		std::vector<BufferView> m_bufferViews;
		std::vector<Accessor>   m_accessors;

		FnGenId m_fnGenBufferId;
		FnGenId m_fnGenBufferViewId;
		FnGenId m_fnGenAccessorId;
	};
}

#include "BufferBuilder2.inl"