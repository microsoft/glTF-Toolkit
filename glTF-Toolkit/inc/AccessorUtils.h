// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "GLTFSDK.h"
#include <functional>
#include <vector>

namespace Microsoft::glTF::Toolkit
{
    /// <summary>
    /// Utilities to manipulate accessors in a glTF asset.
    /// </summary>
    class AccessorUtils
    {
    public:
        // Note: XML Documentation cannot be applied to templated types per https://docs.microsoft.com/en-us/cpp/ide/xml-documentation-visual-cpp
        // Calculates the min and max values for an accessor according to the glTF 2.0 specification.
        // accessor is: The accessor definition for which the min and max values will be calculated.</param>
        // accessorContents is: The raw data contained in the accessor.
        // returns: A pair containing the min and max vectors for the accessor, in that order.
        template <typename T>
        static std::pair<std::vector<float>, std::vector<float>> CalculateMinMax(const Accessor& accessor, const std::vector<T>& accessorContents)
        {
            auto typeCount = Accessor::GetTypeCount(accessor.type);
            auto min = std::vector<float>(typeCount);
            auto max = std::vector<float>(typeCount);

            if (accessorContents.size() < typeCount)
            {
                throw std::invalid_argument("The accessor must contain data in order to calculate min and max.");
            }

            // Initialize min and max with the first elements of the array
            for (size_t j = 0; j < typeCount; j++)
            {
                auto current = static_cast<float>(accessorContents[j]);
                min[j] = current;
                max[j] = current;
            }

            for (size_t i = 1; i < accessor.count; i++)
            {
                for (size_t j = 0; j < typeCount; j++)
                {
                    auto current = static_cast<float>(accessorContents[i * typeCount + j]);
                    min[j] = std::min(min[j], current);
                    max[j] = std::max(max[j], current);
                }
            }

            return std::make_pair(min, max);
        }
    };
}

