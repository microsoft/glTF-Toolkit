// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft::glTF::exp
{
	template<typename T>
	const BufferView& BufferBuilder2::AddBufferView(const std::vector<T>& data, size_t byteStride, BufferViewTarget target, size_t byteAlignment)
	{
		return AddBufferView(data.data(), data.size() * sizeof(T), byteStride, target, byteAlignment);
	}

	template<typename T>
	const Accessor& BufferBuilder2::AddAccessor(const std::vector<T>& data, ComponentType componentType, AccessorType accessorType, std::vector<float>& minValues, std::vector<float>& maxValues)
	{
		const auto accessorTypeSize = Accessor::GetTypeCount(accessorType);
		const auto componentTypeSize = Accessor::GetComponentTypeSize(componentType);

		if (data.size() % accessorTypeSize)
		{
			throw InvalidGLTFException("vector size is not a multiple of accessor type size");
		}

		return AddAccessor(data.data(), data.size() / (accessorTypeSize * componentTypeSize), componentType, accessorType, minValues, maxValues);
	}
}
