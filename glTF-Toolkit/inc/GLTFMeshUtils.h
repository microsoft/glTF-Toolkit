// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace Microsoft { namespace glTF 
{ 
	class GLTFDocument;
	class IStreamReader;
	class IStreamFactory;

	namespace Toolkit
	{
		extern const char* EXTENSION_MSFT_MESH_OPTIMIZER;

		enum class MeshOutputFormat
		{
			// Preserve the structure of each underlying stream.
			Preserve = 0,

			// Collapse mesh attributes into a single interleaved stream, if not already configured as such.
			Interleaved = 1,

			// Split mesh attributes into separate streams, if not already configured as such.
			Separate = 2,
		};
	
		struct MeshOptions
		{
			bool Clean;						// Perform an optimization pass on the mesh data.
			bool GenerateTangentSpace;		// Generate normals and/or tangents if non-existent.
			MeshOutputFormat OutputFormat;	// Specifies the format to output the mesh in.

			static MeshOptions Defaults(void)
			{
				MeshOptions Options;
				Options.Clean = true;
				Options.GenerateTangentSpace = true;
				Options.OutputFormat = MeshOutputFormat::Interleaved;
				return Options;
			}
		};

		class GLTFMeshUtils
		{
		public:
			static GLTFDocument OptimizeAllMeshes(	const IStreamReader& StreamReader, 
													std::unique_ptr<const IStreamFactory>&& StreamFactory,
													const GLTFDocument& Doc, 
													const MeshOptions& Options,
													const std::string& OutputDirectory);
		};
	}
}}
