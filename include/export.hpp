//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

#include "KalaHeaders/import_ktf.hpp"

using KalaHeaders::GlyphBlock;

namespace KalaFont
{
	using std::vector;
	using std::string;
	using std::filesystem::path;
	
	using u8 = uint8_t;
	
	class Export
	{
	public:
		//Write the ktf data to the target path as a new binary
		static void ExportKTF(
			const path& targetPath,
			u8 type,
			u8 glyphHeight,
			u8 superSampleMultiplier,
			vector<GlyphBlock>& glyphBlocks);
	};
}