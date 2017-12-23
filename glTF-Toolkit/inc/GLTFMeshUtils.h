// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace Microsoft::glTF
{ 
	class GLTFDocument;
	class IStreamReader;
	class IStreamFactory;

	namespace Toolkit
	{
		extern const char* EXTENSION_MSFT_MESH_OPTIMIZER;


		// Specifies the format of how each primitive is stored.
		enum class PrimitiveFormat : uint8_t
		{
			// Detect and preserve the existing structure of each underlying format.
			Preserved = 0,

			// Primitive index & vertex data are combined into a globalized set over the entire mesh. 
			// Note: Creates the least number of API objects when rendering.
			Combine = 1,

			// Primitives are partitioned into their own localized segments.
			// Note: Allows for additional per-primitive compression on index & vertex data.
			Separate = 2,
		};


		// Specifies the format of each primitive's vertex attributes.
		enum class AttributeFormat : uint8_t
		{
			// Vertex attribute data is integrated into a single, interleaved stream.
			// Note: Fastest performance during draw calls with all attributes bound.
			Interleave = 0,

			// Vertex attribute data is split into separate lists of contiguous streams. 
			// Note: Worse performance, but provides flexibility of minimizing attribute selection when specifying input layouts.
			Separate = 1,
		};
	

		struct MeshOptions
		{
			bool Optimize;						// Perform an optimization pass on the mesh data (requires indices.)
			bool GenerateTangentSpace;			// Generate normals and/or tangents if non-existent (requires indices.)
			PrimitiveFormat PrimitiveFormat;	// Specifies the format in which to output the mesh primitives.
			AttributeFormat AttributeFormat;	// Specifies the format in which to output the attributes.

			static MeshOptions Defaults(void)
			{
				MeshOptions Options;
				Options.Optimize = true;
				Options.GenerateTangentSpace = true;
				Options.PrimitiveFormat = PrimitiveFormat::Separate;
				Options.AttributeFormat = AttributeFormat::Separate;
				return Options;
			}
		};


		//-----------------------------------------------
		// GLTFMeshUtils

		class GLTFMeshUtils
		{
		public:
			static GLTFDocument ProcessMeshes(const std::string& GLTFPath, const GLTFDocument& Doc, const IStreamReader& StreamReader, const MeshOptions& Options, const std::string& OutputDirectory);

			static const char* s_DataUriRegex;
		};
	}
}
