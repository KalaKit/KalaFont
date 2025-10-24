//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "KalaHeaders/core_utils.hpp"

namespace KalaFont
{
	using std::vector;
	using std::string;

	struct FontGlyph
	{
		u32 unicode{};
		u32 advance{};
		u32 bearingX{};
		u32 bearingY{};

		f32 u0{};
		f32 v0{};
		f32 u1{};
		f32 v1{};
	};

	struct FontMetrics
	{
		f32 unitsPerEm{};
		f32 ascent{};
		f32 descent{};
		f32 lineGap{};
	};

	struct FontData
	{
		string name{};
		FontMetrics metrics{};
		vector<FontGlyph> glyphs{};

		vector<u8> atlasData{}; //R8 SDF data
		u32 atlasWidth{};
		u32 atlasHeight{};
		const u8 atlasChannels = 1;
	};

	class Parser
	{
	public:
		//Parses an otf/ttf font into a kfont file
		static void ParseFont(const vector<string>& params);

		//Parses an otf/ttf font into a kfont file with detailed logs
		static void VerboseParseFont(const vector<string>& params);

		//Displays info about a parsed kfont file
		static void GetKFontInfo(const vector<string>& params);
	};
}