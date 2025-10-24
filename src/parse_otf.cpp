//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"

#include "parse.hpp"
#include "parse_otf.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaFont::Parse;
using KalaFont::OffsetTable;
using KalaFont::TableRecord;
using KalaFont::HeadTable;
using KalaFont::MaxpTable;

using std::vector;
using std::string;
using std::ostringstream;

namespace KalaFont
{
	bool Parse_OTF::Parse(
		const vector<u8>& data,
		const OffsetTable& offsetTable,
		const HeadTable& headTable,
		const MaxpTable& maxpTable,
		bool isVerbose)
	{
		return false;
	}
}