// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "BufferBuilder2.h"

#include <GLTFSDK/BufferBuilder.h>


using namespace Microsoft::glTF;

namespace
{
	size_t GetPadding(size_t offset, ComponentType componentType)
	{
		const size_t typeSize = Accessor::GetComponentTypeSize(componentType);

		const auto padAlign = offset % typeSize;
		const auto pad = padAlign ? typeSize - padAlign : 0U;

		return pad;
	}
}

BufferBuilder2::BufferBuilder2(std::unique_ptr<ResourceWriter2>&& resourceWriter,
	FnGenId fnGenBufferId,
	FnGenId fnGenBufferViewId,
	FnGenId fnGenAccessorId) : m_resourceWriter(std::move(resourceWriter)),
	m_fnGenBufferId(std::move(fnGenBufferId)),
	m_fnGenBufferViewId(std::move(fnGenBufferViewId)),
	m_fnGenAccessorId(std::move(fnGenAccessorId))
{
}

const Buffer& BufferBuilder2::AddBuffer(const char* bufferId)
{
	Buffer buffer;

	buffer.id = bufferId ? bufferId : m_fnGenBufferId(*this);
	buffer.byteLength = 0U;// The buffer's length is updated whenever an Accessor or BufferView is added (and data is written to the underlying buffer)
	buffer.uri = m_resourceWriter->GenerateBufferUri(buffer.id);

	m_buffers.push_back(std::move(buffer));
	return m_buffers.back();
}

const BufferView& BufferBuilder2::AddBufferView(BufferViewTarget target)
{
	Buffer& buffer = m_buffers.back();
	BufferView bufferView;

	bufferView.id = m_fnGenBufferViewId(*this);
	bufferView.bufferId = buffer.id;
	bufferView.byteOffset = buffer.byteLength;
	bufferView.byteLength = 0U;// The BufferView's length is updated whenever an Accessor is added (and data is written to the underlying buffer)
	bufferView.target = target;

	m_bufferViews.push_back(std::move(bufferView));
	return m_bufferViews.back();
}

const BufferView& BufferBuilder2::AddBufferView(const void* data, size_t byteLength, size_t byteStride, BufferViewTarget target)
{
	Buffer& buffer = m_buffers.back();
	BufferView bufferView;

	bufferView.id = m_fnGenBufferViewId(*this);
	bufferView.bufferId = buffer.id;
	bufferView.byteOffset = buffer.byteLength;
	bufferView.byteLength = byteLength;
	bufferView.byteStride = byteStride;
	bufferView.target = target;

	buffer.byteLength = bufferView.byteOffset + bufferView.byteLength;

	if (m_resourceWriter)
	{
		m_resourceWriter->Write(bufferView, data);
	}

	m_bufferViews.push_back(std::move(bufferView));
	return m_bufferViews.back();
}

const Accessor& BufferBuilder2::AddAccessor(size_t count, size_t byteOffset, ComponentType componentType, AccessorType accessorType,
	std::vector<float> minValues, std::vector<float> maxValues)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	const auto accessorTypeSize = Accessor::GetTypeCount(accessorType);

	if (buffer.id != bufferView.bufferId)
	{
		throw InvalidGLTFException("bufferView.bufferId does not match buffer.id");
	}

	// Only check for a valid number of min and max values if they exist
	if ((!minValues.empty() || !maxValues.empty()) &&
		((minValues.size() != accessorTypeSize) || (maxValues.size() != accessorTypeSize)))
	{
		throw InvalidGLTFException("the number of min and max values must be equal to the number of elements to be stored in the accessor");
	}

	size_t ComponentSize = Accessor::GetComponentTypeSize(componentType);
	if (byteOffset % ComponentSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer view must be a multiple of the component size");
	}

	if ((byteOffset + bufferView.byteOffset) % ComponentSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer must be a multiple of the component size");
	}

	Accessor accessor;

	// TODO: make accessor min & max members be vectors of doubles
	accessor.min = std::move(minValues);
	accessor.max = std::move(maxValues);

	accessor.id = m_fnGenAccessorId(*this);
	accessor.bufferViewId = bufferView.id;
	accessor.count = count;
	accessor.byteOffset = byteOffset;
	accessor.type = accessorType;
	accessor.componentType = componentType;

	m_accessors.push_back(std::move(accessor));
	return m_accessors.back();
}

const Accessor& BufferBuilder2::AddAccessor(const void* data, size_t count, ComponentType componentType, AccessorType accessorType,
	std::vector<float> minValues, std::vector<float> maxValues)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	const auto accessorTypeSize = Accessor::GetTypeCount(accessorType);

	if (buffer.id != bufferView.bufferId)
	{
		throw InvalidGLTFException("bufferView.bufferId does not match buffer.id");
	}

	// Only check for a valid number of min and max values if they exist
	if ((!minValues.empty() || !maxValues.empty()) &&
		((minValues.size() != accessorTypeSize) || (maxValues.size() != accessorTypeSize)))
	{
		throw InvalidGLTFException("the number of min and max values must be equal to the number of elements to be stored in the accessor");
	}

	// If the bufferView has not yet been written to then ensure it is correctly aligned for this accessor's component type
	if (bufferView.byteLength == 0U)
	{
		bufferView.byteOffset += ::GetPadding(bufferView.byteOffset, componentType);
	}

	Accessor accessor;

	// TODO: make accessor min & max members be vectors of doubles
	accessor.min = std::move(minValues);
	accessor.max = std::move(maxValues);

	accessor.id = m_fnGenAccessorId(*this);
	accessor.bufferViewId = bufferView.id;
	accessor.count = count;
	accessor.byteOffset = bufferView.byteLength;
	accessor.type = accessorType;
	accessor.componentType = componentType;

	bufferView.byteLength += accessor.GetByteLength();
	buffer.byteLength = bufferView.byteOffset + bufferView.byteLength;

	if (m_resourceWriter)
	{
		m_resourceWriter->Write(bufferView, data, accessor);
	}

	m_accessors.push_back(std::move(accessor));
	return m_accessors.back();
}

void BufferBuilder2::Output(GLTFDocument& gltfDocument)
{
	for (auto& buffer : m_buffers)
	{
		gltfDocument.buffers.Append(std::move(buffer));
	}

	m_buffers.clear();

	for (auto& bufferView : m_bufferViews)
	{
		gltfDocument.bufferViews.Append(std::move(bufferView));
	}

	m_bufferViews.clear();

	for (auto& accessor : m_accessors)
	{
		gltfDocument.accessors.Append(std::move(accessor));
	}

	m_accessors.clear();
}

const Buffer& BufferBuilder2::GetCurrentBuffer() const
{
	return m_buffers.back();
}

const BufferView& BufferBuilder2::GetCurrentBufferView() const
{
	return m_bufferViews.back();
}

const Accessor& BufferBuilder2::GetCurrentAccessor() const
{
	return m_accessors.back();
}

size_t BufferBuilder2::GetBufferCount() const
{
	return m_buffers.size();
}

size_t BufferBuilder2::GetBufferViewCount() const
{
	return m_bufferViews.size();
}

size_t BufferBuilder2::GetAccessorCount() const
{
	return m_accessors.size();
}

ResourceWriter2& BufferBuilder2::GetResourceWriter()
{
	return *m_resourceWriter;
}

const ResourceWriter2& BufferBuilder2::GetResourceWriter() const
{
	return *m_resourceWriter;
}