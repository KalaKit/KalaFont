//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "KalaHeaders/log_utils.hpp"

#include "export.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

namespace KalaFont
{
	void Export::ExportKTF(
		const path& targetPath,
		u8 type,
		u8 glyphHeight,
		u8 superSampleMultiplier,
		vector<GlyphBlock>& glyphBlocks)
	{
		Log::Print(
			"Starting to export font to path '" + targetPath.string() + "'.",
			"EXPORT_FONT",
			LogType::LOG_DEBUG);
			
		
			
		Log::Print(
			"Finished exporting font!",
			"EXPORT_FONT",
			LogType::LOG_SUCCESS);
	}
}