//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"

#include "parser.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::ReadBinaryLinesFromFile;

using KalaFont::Core;

using std::vector;
using std::string;
using std::to_string;
using std::filesystem::exists;
using std::filesystem::is_regular_file;
using std::filesystem::current_path;
using std::filesystem::path;
using std::from_chars;
using std::errc;
using std::stoi;
using std::clamp;
using std::ostringstream;
using std::hex;
using std::dec;
using std::min;

static bool IsInteger(const string& s)
{
	int value{};
	auto [ptr, ec] = from_chars(s.data(), s.data() + s.size(), value);
	return ec == errc{}
	&& ptr == s.data() + s.size();
}

static u16 ReadU16(
	const vector<u8>& data, 
	size_t offset)
{
	return (data[offset] << 8)
		| data[offset + 1];
}
static u32 ReadU32(
	const vector<u8>& data, 
	size_t offset)
{
	return (data[offset] << 24)
		| (data[offset + 1] << 16)
		| (data[offset + 2] << 8)
		| (data[offset + 3]);
}

struct TableRecord
{
	char tag[4]{};
	u32 checkSum{};
	u32 offset{};
	u32 length{};
};

struct OffsetTable
{
	u32 scalerType{};
	u16 numTables{};
	u16 searchRange{};
	u16 entrySelector{};
	u16 rangeShift{};
	vector<TableRecord> tables{};
};

struct HeadTable
{
	i16 majorVersion{};
	i16 minorVersion{};
	f32 fontRevision{}; //fixed 16.16 value
	u32 checkSumAdjustment{};
	u32 magicNumber{};
	u16 flags{};
	u16 unitsPerEm{};
	i64 created{}; //longDateTime (64-bit)
	i64 modified{}; //longDateTime (64-bit)
	i16 xMin{};
	i16 yMin{};
	i16 xMax{};
	i16 yMax{};
	u16 macStyle{};
	u16 lowestRecPPEM{};
	i16 fontDirectionHint{};
	i16 indexToLocFormat{}; //0 = short, 1 = long
	i16 glyphDataFormat{};
};

struct MaxpTable
{
	u32 version{};
	u16 numGlyphs{};
};

struct LocaTable
{
	vector<u32> glyphOffsets{};
};

struct GlyphInfo
{
	i16 numberOfContours{};
	i16 xMin{};
	i16 yMin{};
	i16 xMax{};
	i16 yMax{};
};

static OffsetTable ReadOffsetTable(
	const string& path, 
	vector<u8>& outData);
static HeadTable ReadHeadTable(
	const vector<u8>& data, 
	u32 offset);
static MaxpTable ReadMaxpTable(
	const vector<u8>& data, 
	u32 offset);
static LocaTable ReadLocaTable(
	const vector<u8>& data, 
	u32 offset, 
	u16 numGlyphs, 
	i16 indexToLocFormat);
static GlyphInfo ReadGlyphHeader(
	const vector<u8>& data,
	u32 glyfOffset,
	u32 glyfStart);

static bool ParsePreCheck(const vector<string>& params);
static bool GetPreCheck(const vector<string>& params);

namespace KalaFont
{
	void Parser::ParseFont(const vector<string>& params)
	{
		if (!ParsePreCheck(params)) return;

		int val = stoi(params[3]);
		if (val < 1
			|| val > 255)
		{
			val = clamp(val, 1, 255);
			string clampedVal = to_string(val);

			Log::Print(
				"Font size '" + params[3] + "' was out of range! It was clamped to a safe value '" + clampedVal + "'.",
				"PARSE",
				LogType::LOG_WARNING);
		}

		//
		// OFFSET TABLE
		//

		vector<u8> data{};
		OffsetTable oTable = ReadOffsetTable(params[1], data);

		if (data.empty())
		{
			Log::Print(
				"Failed to parse offset table for font '" + params[1] + "'! No data was found.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		//
		// HEAD TABLE
		//

		auto headIt = find_if(oTable.tables.begin(), oTable.tables.end(),
			[](const TableRecord& t) { return string(t.tag, 4) == "head"; });

		if (headIt == oTable.tables.end())
		{
			Log::Print(
				"Failed to parse offset table for font '" + params[1] + "'! Head tag was not found.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		HeadTable headTable = ReadHeadTable(data, headIt->offset);

		if (headTable.magicNumber == 0
			|| headTable.magicNumber != 0x5F0F3CF5)
		{
			Log::Print(
				"Failed to parse head table for font '" + params[1] + "'! Magic number is incorrect.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		ostringstream headTableMsg{};

		headTableMsg << "Head table data:\n"
			<< "  majorVersion:       " << headTable.majorVersion << "\n"
			<< "  minorVersion:       " << headTable.minorVersion << "\n"
			<< "  fontRevision:       " << headTable.fontRevision << "\n"
			<< "  checkSumAdjustment: " << headTable.checkSumAdjustment << "\n"
			<< "  magicNumber:        0x" << hex << headTable.magicNumber << dec << "\n"
			<< "  flags:              " << headTable.flags << "\n"
			<< "  unitsPerEm:         " << headTable.unitsPerEm << "\n"
			<< "  xMin:               " << headTable.xMin << "\n"
			<< "  yMin:               " << headTable.yMin << "\n"
			<< "  xMax:               " << headTable.xMax << "\n"
			<< "  yMax:               " << headTable.yMax << "\n"
			<< "  macStyle:           " << headTable.macStyle << "\n"
			<< "  lowestRecPPEM:      " << headTable.lowestRecPPEM << "\n"
			<< "  fontDirectionHint:  " << headTable.fontDirectionHint << "\n"
			<< "  indexToLocFormat:   " << headTable.indexToLocFormat << "\n"
			<< "  glyphDataFormat:    " << headTable.glyphDataFormat << "\n";

		Log::Print(headTableMsg.str());

		//
		// MAXP TABLE
		//

		auto maxpIt = find_if(oTable.tables.begin(), oTable.tables.end(),
			[](const TableRecord& t) { return string(t.tag, 4) == "maxp"; });

		if (maxpIt == oTable.tables.end())
		{
			Log::Print(
				"Failed to parse offset table for font '" + params[1] + "'! Maxp tag was not found.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		MaxpTable maxpTable = ReadMaxpTable(data, maxpIt->offset);

		ostringstream maxpTableMsg{};

		maxpTableMsg << "Maxp table data:\n"
			<< "  version:   0x" << hex << maxpTable.version << dec << "\n"
			<< "  numGlyphs: " << maxpTable.numGlyphs << "\n";

		Log::Print(maxpTableMsg.str());

		//
		// LOCA TABLE
		//

		auto locaIt = find_if(oTable.tables.begin(), oTable.tables.end(),
			[](const TableRecord& t) { return string(t.tag, 4) == "loca"; });

		if (maxpIt == oTable.tables.end())
		{
			Log::Print(
				"Failed to parse offset table for font '" + params[1] + "'! Loca tag was not found.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		LocaTable locaTable = ReadLocaTable(
			data,
			locaIt->offset,
			maxpTable.numGlyphs,
			headTable.indexToLocFormat);

		ostringstream locaTableMsg{};

		locaTableMsg << "Loca table data:\n"
			<< "  glyph offsets count: " << locaTable.glyphOffsets.size() << "\n"
			<< "  First 10 offsets:\n";

		for (size_t i = 0; i < min<size_t>(locaTable.glyphOffsets.size(), 10); ++i)
		{
			locaTableMsg << "    [" << i << "]: " << locaTable.glyphOffsets[i] << "\n";
		}

		Log::Print(locaTableMsg.str());

		//
		// GLYPH HEADER
		//

		auto glyfIt = find_if(oTable.tables.begin(), oTable.tables.end(),
			[](const TableRecord& t) { return string(t.tag, 4) == "glyf"; });

		if (glyfIt == oTable.tables.end())
		{
			Log::Print(
				"Failed to parse offset table for font '" + params[1] + "'! Glyf tag was not found.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		ostringstream glyphHeaderMsg{};

		glyphHeaderMsg << "First 10 glyphs:\n";

		const u32 glyphBase = glyfIt->offset;
		for (u32 i = 0; i < min<u32>(10, maxpTable.numGlyphs); ++i)
		{
			u32 start = locaTable.glyphOffsets[i];
			u32 end = locaTable.glyphOffsets[i + 1];

			if (start == end)
			{
				glyphHeaderMsg << "  [" << i << "] empty glyph\n";
				continue;
			}

			GlyphInfo gi = ReadGlyphHeader(
				data, 
				glyphBase, 
				start);

			glyphHeaderMsg << "  [" << i << "] contours: " << gi.numberOfContours
				<< " bounds: (" << gi.xMin << ", " << gi.yMin << ", " << gi.xMax << ", " << gi.yMax << ")\n";
		}

		Log::Print(glyphHeaderMsg.str());
	}

	void Parser::GetKFontInfo(const vector<string>& params)
	{
		if (!GetPreCheck(params)) return;
	}
}

OffsetTable ReadOffsetTable(
	const string& fontPath, 
	vector<u8>& outData)
{
	OffsetTable table{};

	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctFontPath = weakly_canonical(Core::currentDir / path(fontPath));

	vector<u8> data{};
	string result = ReadBinaryLinesFromFile(correctFontPath, data);

	if (!result.empty())
	{
		Log::Print(
			"Failed to parse the font file '" + correctFontPath.string() + "'! Reason: " + result,
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return table;
	}

	size_t offset{};
	table.scalerType    = ReadU32(data, offset); offset += 4;
	table.numTables     = ReadU16(data, offset); offset += 2;
	table.searchRange   = ReadU16(data, offset); offset += 2;
	table.entrySelector = ReadU16(data, offset); offset += 2;
	table.rangeShift    = ReadU16(data, offset); offset += 2;

	table.tables.reserve(table.numTables);

	for (u16 i = 0; i < table.numTables; ++i)
	{
		TableRecord rec{};
		memcpy(rec.tag, &data[offset], 4);
		rec.checkSum = ReadU32(data, offset + 4);
		rec.offset   = ReadU32(data, offset + 8);
		rec.length   = ReadU32(data, offset + 12);

		table.tables.push_back(rec);
		offset += 16;
	}

	ostringstream offsetTableMsg{};

	offsetTableMsg << "\nOffset table data:\n";

	for (const auto& rec : table.tables)
	{
		string tag(rec.tag, 4);
		offsetTableMsg << "  Table '" << tag << "' found at offset '"
			<< rec.offset << "' with length '" 
			<< rec.length << "'\n";
	}

	Log::Print(offsetTableMsg.str());

	outData = data;

	return table;
}

HeadTable ReadHeadTable(
	const vector<u8>& data, 
	u32 offset)
{
	HeadTable table{};
	
	table.majorVersion       = static_cast<i16>(ReadU16(data, offset));
	table.minorVersion       = static_cast<i16>(ReadU16(data, offset + 2));
	table.fontRevision       = static_cast<i32>(ReadU32(data, offset + 4));
	table.checkSumAdjustment = ReadU32(data, offset + 8);
	table.magicNumber        = ReadU32(data, offset + 12);
	table.flags              = ReadU16(data, offset + 16);
	table.unitsPerEm         = ReadU16(data, offset + 18);
	table.created            = (static_cast<i64>(ReadU32(data, offset + 20)) << 32
	                         | ReadU32(data, offset + 24));
	table.modified           = (static_cast<i64>(ReadU32(data, offset + 28)) << 32
	                         | ReadU32(data, offset + 32));
	table.xMin               = static_cast<i16>(ReadU16(data, offset + 36));
	table.yMin               = static_cast<i16>(ReadU16(data, offset + 38));
	table.xMax               = static_cast<i16>(ReadU16(data, offset + 40));
	table.yMax               = static_cast<i16>(ReadU16(data, offset + 42));
	table.macStyle           = ReadU16(data, offset + 44);
	table.lowestRecPPEM      = ReadU16(data, offset + 46);
	table.fontDirectionHint  = static_cast<i16>(ReadU16(data, offset + 48));
	table.indexToLocFormat   = static_cast<i16>(ReadU16(data, offset + 50));
	table.glyphDataFormat    = static_cast<i16>(ReadU16(data, offset + 52));

	return table;
}

MaxpTable ReadMaxpTable(
	const vector<u8>& data, 
	u32 offset)
{
	MaxpTable table{};

	table.version = ReadU32(data, offset);
	table.numGlyphs = ReadU16(data, offset + 4);

	return table;
}

LocaTable ReadLocaTable(
	const vector<u8>& data,
	u32 offset,
	u16 numGlyphs,
	i16 indexToLocFormat)
{
	LocaTable table{};

	table.glyphOffsets.resize(numGlyphs + 1);

	if (indexToLocFormat == 0)
	{
		for (u32 i = 0; i <= numGlyphs; ++i)
		{
			u16 val = ReadU16(data, offset + i * 2);
			table.glyphOffsets[i] = static_cast<u32>(val) * 2;
		}
	}
	else
	{
		for (u32 i = 0; i <= numGlyphs; ++i)
		{
			table.glyphOffsets[i] = ReadU32(data, offset + i * 4);
		}
	}

	return table;
}

GlyphInfo ReadGlyphHeader(
	const vector<u8>& data,
	u32 glyfOffset,
	u32 glyfStart)
{
	GlyphInfo gi{};

	gi.numberOfContours = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart));
	gi.xMin             = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 2));
	gi.yMin             = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 4));
	gi.xMax             = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 6));
	gi.yMax             = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 8));

	return gi;
}

bool ParsePreCheck(const vector<string>& params)
{
	path fontPath = path(params[1]);
	path kfontPath = path(params[2]);

	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctFontPath = weakly_canonical(Core::currentDir / fontPath);
	path correctKFontPath = weakly_canonical(Core::currentDir / kfontPath);

	if (!exists(correctFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (exists(correctKFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + correctKFontPath.string() + "' already exists!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (!IsInteger(params[3]))
	{
		Log::Print(
			"Cannot parse to kfont because font size '" + params[3] + "' is not an integer!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(correctFontPath).has_extension()
		|| (path(correctFontPath).extension() != ".ttf"
		&& path(correctFontPath).extension() != ".otf"))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(correctKFontPath).has_extension()
		|| path(correctKFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + correctKFontPath.string() + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}

bool GetPreCheck(const vector<string>& params)
{
	path fontPath = path(params[1]);

	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctFontPath = weakly_canonical(Core::currentDir / fontPath);

	if (!exists(correctFontPath))
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + correctFontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + correctFontPath.string() + "' is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(correctFontPath).has_extension()
		|| path(correctFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + correctFontPath.string() + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}