//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "KalaHeaders/core_utils.hpp"

#include "parse.hpp"

namespace KalaFont
{
	using std::vector;
	using std::string;

	class Parse_OTF
	{
	public:
		static bool Parse(
			const vector<u8>& data,
			const OffsetTable& offsetTable,
			const HeadTable& headTable,
			const MaxpTable& maxpTable,
			bool isVerbose);
	};
}