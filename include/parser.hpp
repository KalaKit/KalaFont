//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

namespace KalaFont
{
	using std::vector;
	using std::string;

	class Parser
	{
	public:
		//Parses an otf/ttf font into a kfont file
		static void ParseFont(const vector<string>& params);

		//Displays info about a parsed kfont file
		static void GetKFontInfo(const vector<string>& params);
	};
}