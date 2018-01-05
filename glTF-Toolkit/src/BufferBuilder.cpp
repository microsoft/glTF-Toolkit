/**************************************************************
*                                                             *
*  Copyright (c) Microsoft Corporation. All rights reserved.  *
*               Licensed under the MIT License.               *
*                                                             *
**************************************************************/

#include "pch.h"
#include "BufferBuilder.h"

using namespace Microsoft::glTF;
using namespace Microsoft::glTF::exp;

namespace
{
	size_t GetPadding(size_t offset, size_t alignment)
	{
		const auto padAlign = offset % alignment;
		const auto pad = padAlign ? alignment - padAlign : 0U;

		return pad;
	}

	size_t GetPadding(size_t offset, ComponentType componentType)
	{
		return GetPadding(offset, Accessor::GetComponentTypeSize(componentType));
	}

	size_t GetAlignment(const AccessorDesc& desc)
	{
		return Accessor::GetComponentTypeSize(desc.componentType);
	}

	size_t GetExtent(size_t byteStride, const AccessorDesc& desc)
	{
		if (byteStride == 0)
		{
			// Non-strided elements, aka contiguous chunks of data.
			// (offset to first element) + (size of element * count)
			return desc.byteOffset + desc.count * Accessor::GetComponentTypeSize(desc.componentType) * Accessor::GetTypeCount(desc.accessorType);
		}
		else
		{
			// Strided elements.
			// (offset to first element) + (stride * count) + (size of element)
			return desc.byteOffset + desc.count * byteStride + Accessor::GetComponentTypeSize(desc.componentType) * Accessor::GetTypeCount(desc.accessorType);
		}
	}
}

BufferBuilder::BufferBuilder(std::unique_ptr<ResourceWriter2>&& resourceWriter,
	FnGenId fnGenBufferId,
	FnGenId fnGenBufferViewId,
	FnGenId fnGenAccessorId) : m_resourceWriter(std::move(resourceWriter)),
	m_fnGenBufferId(std::move(fnGenBufferId)),
	m_fnGenBufferViewId(std::move(fnGenBufferViewId)),
	m_fnGenAccessorId(std::move(fnGenAccessorId))
{
}

const Buffer& BufferBuilder::AddBuffer(const char* bufferId)
{
	Buffer buffer;

	buffer.id = bufferId ? bufferId : m_fnGenBufferId(*this);
	buffer.byteLength = 0U;// The buffer's length is updated whenever an Accessor or BufferView is added (and data is written to the underlying buffer)
	buffer.uri = m_resourceWriter->GenerateBufferUri(buffer.id);

	m_buffers.push_back(std::move(buffer));
	return m_buffers.back();
}

const BufferView& BufferBuilder::AddBufferView(BufferViewTarget target)
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

const BufferView& BufferBuilder::AddBufferView(const void* data, size_t byteLength, size_t byteStride, BufferViewTarget target)
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

void BufferBuilder::AddAccessors(const void* data, size_t byteStride, const AccessorDesc* pDescs, size_t descCount, std::string* pOutIds)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	// Calculate the max alignment and max extent of the accessors.
	size_t alignment = 1, extent = 0;
	for (size_t i = 0; i < descCount; ++i)
	{
		const AccessorDesc& desc = pDescs[i];
		if (desc.count == 0 || desc.accessorType == TYPE_UNKNOWN || desc.componentType == COMPONENT_UNKNOWN)
		{
			continue;
		}

		alignment = std::max(alignment, GetAlignment(desc));
		extent = std::max(extent, GetExtent(byteStride, desc));
	}

	// ResourceWriter2 only supports writing full buffer views.
	if (bufferView.byteLength != 0U)
	{
		throw InvalidGLTFException("current buffer view already has written data - this interface doesn't support appending to an existing buffer view");
	}

	bufferView.byteStride = byteStride;
	bufferView.byteLength = extent;
	bufferView.byteOffset += ::GetPadding(bufferView.byteOffset, alignment);

	buffer.byteLength = bufferView.byteOffset + bufferView.byteLength;

	for (size_t i = 0; i < descCount; ++i)
	{
		const AccessorDesc& desc = pDescs[i];
		if (desc.count == 0 || desc.accessorType == TYPE_UNKNOWN || desc.componentType == COMPONENT_UNKNOWN)
		{
			continue;
		}

		AddAccessor(pDescs[i]);

		if (pOutIds != nullptr)
		{
			pOutIds[i] = GetCurrentAccessor().id;
		}
	}

	if (m_resourceWriter)
	{
		m_resourceWriter->Write(bufferView, data);
	}
}

const Accessor& BufferBuilder::AddAccessor(const void* data, AccessorDesc desc)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	// If the bufferView has not yet been written to then ensure it is correctly aligned for this accessor's component type
	if (bufferView.byteLength == 0U)
	{
		bufferView.byteOffset += ::GetPadding(bufferView.byteOffset, desc.componentType);
	}

	desc.byteOffset = bufferView.byteLength;
	Accessor& accessor = AddAccessor(desc);

	bufferView.byteLength += accessor.GetByteLength();
	buffer.byteLength = bufferView.byteOffset + bufferView.byteLength;

	if (m_resourceWriter)
	{
		m_resourceWriter->Write(bufferView, data, accessor);
	}

	return m_accessors.back();
}

void BufferBuilder::Output(GLTFDocument& gltfDocument)
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

const Buffer& BufferBuilder::GetCurrentBuffer() const
{
	return m_buffers.back();
}

const BufferView& BufferBuilder::GetCurrentBufferView() const
{
	return m_bufferViews.back();
}

const Accessor& BufferBuilder::GetCurrentAccessor() const
{
	return m_accessors.back();
}

size_t BufferBuilder::GetBufferCount() const
{
	return m_buffers.size();
}

size_t BufferBuilder::GetBufferViewCount() const
{
	return m_bufferViews.size();
}

size_t BufferBuilder::GetAccessorCount() const
{
	return m_accessors.size();
}

ResourceWriter2& BufferBuilder::GetResourceWriter()
{
	return *m_resourceWriter;
}

const ResourceWriter2& BufferBuilder::GetResourceWriter() const
{
	return *m_resourceWriter;
}

Accessor& BufferBuilder::AddAccessor(const AccessorDesc& desc)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	if (desc.count == 0)
	{
		throw GLTFException("Invalid accessor count: 0");
	}

	if (desc.accessorType == TYPE_UNKNOWN)
	{
		throw GLTFException("Invalid accessorType: TYPE_UNKNOWN");
	}

	if (desc.componentType == COMPONENT_UNKNOWN)
	{
		throw GLTFException("Invalid componentType: COMPONENT_UNKNOWN");
	}

	const auto accessorTypeSize = Accessor::GetTypeCount(desc.accessorType);
	size_t componentTypeSize = Accessor::GetComponentTypeSize(desc.componentType);

	if (buffer.id != bufferView.bufferId)
	{
		throw InvalidGLTFException("bufferView.bufferId does not match buffer.id");
	}

	// Only check for a valid number of min and max values if they exist
	if ((!desc.minValues.empty() || !desc.maxValues.empty()) &&
		((desc.minValues.size() != accessorTypeSize) || (desc.maxValues.size() != accessorTypeSize)))
	{
		throw InvalidGLTFException("the number of min and max values must be equal to the number of elements to be stored in the accessor");
	}

	if (desc.byteOffset % componentTypeSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer view must be a multiple of the component size");
	}

	if ((desc.byteOffset + bufferView.byteOffset) % componentTypeSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer must be a multiple of the component size");
	}

	Accessor accessor;

	// TODO: make accessor min & max members be vectors of doubles
	accessor.min = desc.minValues;
	accessor.max = desc.maxValues;

	accessor.id = m_fnGenAccessorId(*this);
	accessor.bufferViewId = bufferView.id;
	accessor.count = desc.count;
	accessor.byteOffset = desc.byteOffset;
	accessor.type = desc.accessorType;
	accessor.componentType = desc.componentType;
	//accessor.normalized = desc.normalized;

	m_accessors.push_back(std::move(accessor));
	return m_accessors.back();
}
