// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "BufferBuilder2.h"


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
}

BufferBuilder2::BufferBuilder2(std::unique_ptr<ResourceWriter2>&& resourceWriter,
	FnGenId&& fnGenBufferId,
	FnGenId&& fnGenBufferViewId,
	FnGenId&& fnGenAccessorId) 
	: m_resourceWriter(std::move(resourceWriter))
	, m_fnGenBufferId(std::move(fnGenBufferId))
	, m_fnGenBufferViewId(std::move(fnGenBufferViewId))
	, m_fnGenAccessorId(std::move(fnGenAccessorId))
{ }

const Buffer& BufferBuilder2::AddBuffer(const char* bufferId)
{
	Buffer buffer;
	buffer.id = bufferId ? bufferId : std::to_string(m_fnGenBufferId(*this));
	buffer.byteLength = 0U; // The buffer's length is updated whenever an Accessor or BufferView is added (and data is written to the underlying buffer)
	buffer.uri = m_resourceWriter->GenerateBufferUri(buffer.id);

	m_buffers.push_back(std::move(buffer));
	return m_buffers.back();
}

const BufferView& BufferBuilder2::AddBufferView(BufferViewTarget target, size_t byteAlignment)
{
	Buffer& buffer = m_buffers.back();

	BufferView bufferView;
	bufferView.id = std::to_string(m_fnGenBufferViewId(*this));
	bufferView.bufferId = buffer.id;
	bufferView.byteOffset = buffer.byteLength + ::GetPadding(buffer.byteLength, byteAlignment);
	bufferView.byteLength = 0U; // The BufferView's length is updated whenever an Accessor is added (and data is written to the underlying buffer)
	bufferView.target = target;

	m_bufferViews.push_back(std::move(bufferView));
	return m_bufferViews.back();
}

const BufferView& BufferBuilder2::AddBufferView(const void* data, size_t byteLength, size_t byteStride, BufferViewTarget target, size_t byteAlignment)
{
	Buffer& buffer = m_buffers.back();

	BufferView bufferView;
	bufferView.id = std::to_string(m_fnGenBufferViewId(*this));
	bufferView.bufferId = buffer.id;
	bufferView.byteOffset = buffer.byteLength + ::GetPadding(buffer.byteLength, byteAlignment);
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

const Accessor& BufferBuilder2::AddAccessor(size_t count, size_t byteOffset, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues)
{
	Buffer& buffer = m_buffers.back();
	BufferView& bufferView = m_bufferViews.back();

	const auto componentCount = Accessor::GetTypeCount(accessorType);

	if (buffer.id != bufferView.bufferId)
	{
		throw InvalidGLTFException("bufferView.bufferId does not match buffer.id");
	}

	// Only check for a valid number of min and max values if they exist
	if ((!minValues.empty() || !maxValues.empty()) && ((minValues.size() != componentCount) || (maxValues.size() != componentCount)))
	{
		throw InvalidGLTFException("the number of min and max values must be equal to the number of elements to be stored in the accessor");
	}

	Accessor accessor;
	accessor.min = std::move(minValues);
	accessor.max = std::move(maxValues);
	accessor.id = std::to_string(m_fnGenAccessorId(*this));
	accessor.bufferViewId = bufferView.id;
	accessor.count = count;
	accessor.byteOffset = byteOffset;
	accessor.type = accessorType;
	accessor.componentType = componentType;

	size_t componentSize = Accessor::GetComponentTypeSize(componentType);
	if (byteOffset % componentSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer view must be a multiple of the component size");
	}

	if ((byteOffset + bufferView.byteOffset) % componentSize != 0)
	{
		throw InvalidGLTFException("accessor offset within buffer must be a multiple of the component size");
	}

	// Stride is determined implicitly by the accessor size if none is provided in the buffer view.
	const auto elementSize = componentCount * componentSize;
	const auto stride = bufferView.byteStride > 0 ? bufferView.byteStride : elementSize;

	// Start of last element, s = Stride * (count - 1) + byteOffset. 
	// End of last element, e = s + ElementSize.
	const auto accessorEnd = stride * (count - 1) + byteOffset + elementSize;

	// Ensure there is enough room in the BufferView for the accessor's data
	if (accessorEnd > bufferView.byteLength)
	{
		throw InvalidGLTFException("Position of last accessor element exceeds the buffer view's byte length");
	}

	m_accessors.push_back(std::move(accessor));
	return m_accessors.back();
}

const Accessor& BufferBuilder2::AddAccessor(const void* data, size_t count, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues)
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

	// TODO: make accessor min & max members be vectors of doubles
	Accessor accessor;
	accessor.min = std::move(minValues);
	accessor.max = std::move(maxValues);
	accessor.id = std::to_string(m_fnGenAccessorId(*this));
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

	for (auto& bufferView : m_bufferViews)
	{
		gltfDocument.bufferViews.Append(std::move(bufferView));
	}

	for (auto& accessor : m_accessors)
	{
		gltfDocument.accessors.Append(std::move(accessor));
	}

	m_buffers.clear();
	m_bufferViews.clear();
	m_accessors.clear();
}
