//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"

#include "parser.hpp"
#include "core.hpp"
#include "command.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::ReadBinaryLinesFromFile;

using KalaFont::Core;
using KalaFont::CommandManager;

using std::vector;
using std::string;
using std::to_string;
using std::filesystem::exists;
using std::filesystem::is_regular_file;
using std::filesystem::current_path;
using std::filesystem::path;
using std::stoi;
using std::clamp;
using std::ostringstream;
using std::hex;
using std::dec;
using std::min;

constexpr u32 TTF_VERSION = 0x00010000;
constexpr u32 OTF_VERSION = 'OTTO';
static u32 thisVersion{};

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

static void Parse(
	const vector<string>& params,
	bool isVerbose = false);
static void ParseTTF(
	const vector<u8>& data,
	const OffsetTable& offsetTable,
	const HeadTable& headTable,
	const MaxpTable& maxpTable,
	bool isVerbose);
static void ParseOTF(
	const vector<u8>& data,
	const OffsetTable& offsetTable,
	const HeadTable& headTable,
	const MaxpTable& maxpTable,
	bool isVerbose);

static OffsetTable ReadOffsetTable(
	const string& path, 
	vector<u8>& outData,
	bool isVerbose);
static HeadTable ReadHeadTable(
	const vector<u8>& data, 
	u32 offset,
	bool isVerbose);
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

static void ParsePreCheck(const vector<string>& params);
static void GetPreCheck(const vector<string>& params);

namespace KalaFont
{
	void Parser::ParseFont(const vector<string>& params)
	{
		Parse(params, false);
	}
	void Parser::VerboseParseFont(const vector<string>& params)
	{
		Parse(params, true);
	}

	void Parser::GetKFontInfo(const vector<string>& params)
	{
		GetPreCheck(params);
	}
}

void Parse(
	const vector<string>& params,
	bool isVerbose)
{
	ParsePreCheck(params);

	//
	// OFFSET TABLE
	//

	vector<u8> data{};

	OffsetTable offsetTable = ReadOffsetTable(
		params[1], 
		data,
		isVerbose);

	if (offsetTable.tables.empty()) return;

	//
	// HEAD TABLE
	//

	auto headIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "head"; });

	HeadTable headTable = ReadHeadTable(
		data, 
		headIt->offset,
		isVerbose);

	//
	// MAXP TABLE
	//

	auto maxpIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "maxp"; });

	MaxpTable maxpTable = ReadMaxpTable(data, maxpIt->offset);

	if (isVerbose)
	{
		ostringstream maxpTableMsg{};

		maxpTableMsg << "Maxp table data:\n"
			<< "  version:   0x" << hex << maxpTable.version << dec << "\n"
			<< "  numGlyphs: " << maxpTable.numGlyphs << "\n";

		Log::Print(maxpTableMsg.str());
	}

	if (thisVersion == TTF_VERSION)
	{
		ParseTTF(
			data, 
			offsetTable, 
			headTable, 
			maxpTable,
			isVerbose);
	}
	else
	{
		ParseOTF(
			data,
			offsetTable,
			headTable,
			maxpTable,
			isVerbose);
	}

	string fontType = thisVersion == TTF_VERSION
		? "TTF"
		: "OTF";

	Log::Print(
		"Finished parsing " + fontType + " font!",
		"PARSE",
		LogType::LOG_SUCCESS);
}

void ParseTTF(
	const vector<u8>& data,
	const OffsetTable& offsetTable,
	const HeadTable& headTable,
	const MaxpTable& maxpTable,
	bool isVerbose)
{
	//
	// LOCA TABLE
	//

	auto locaIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "loca"; });

	LocaTable locaTable = ReadLocaTable(
		data,
		locaIt->offset,
		maxpTable.numGlyphs,
		headTable.indexToLocFormat);

	if (isVerbose)
	{
		ostringstream locaTableMsg{};

		locaTableMsg << "Loca table data:\n"
			<< "  glyph offsets count: " << locaTable.glyphOffsets.size() << "\n"
			<< "  First 10 offsets:\n";

		for (size_t i = 0; i < min<size_t>(locaTable.glyphOffsets.size(), 10); ++i)
		{
			locaTableMsg << "    [" << i << "]: " << locaTable.glyphOffsets[i] << "\n";
		}

		Log::Print(locaTableMsg.str());
	}

	//
	// GLYPH HEADER
	//

	auto glyfIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "glyf"; });

	if (isVerbose)
	{
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
}

void ParseOTF(
	const vector<u8>& data,
	const OffsetTable& offsetTable,
	const HeadTable& headTable,
	const MaxpTable& maxpTable,
	bool isVerbose)
{

}

OffsetTable ReadOffsetTable(
	const string& fontPath, 
	vector<u8>& outData,
	bool isVerbose)
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

		CommandManager::ParseCommand({ "--e" });
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

	if (isVerbose)
	{
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
	}

	outData = data;

	return table;
}

HeadTable ReadHeadTable(
	const vector<u8>& data, 
	u32 offset,
	bool isVerbose)
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

	if (table.magicNumber != 0x5F0F3CF5)
	{
		Log::Print(
			"Failed to parse head table! Magic number is incorrect.",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (isVerbose)
	{
		ostringstream headTableMsg{};

		headTableMsg << "Head table data:\n"
			<< "  majorVersion:       " << table.majorVersion << "\n"
			<< "  minorVersion:       " << table.minorVersion << "\n"
			<< "  fontRevision:       " << table.fontRevision << "\n"
			<< "  checkSumAdjustment: " << table.checkSumAdjustment << "\n"
			<< "  magicNumber:        0x" << hex << table.magicNumber << dec << "\n"
			<< "  flags:              " << table.flags << "\n"
			<< "  unitsPerEm:         " << table.unitsPerEm << "\n"
			<< "  xMin:               " << table.xMin << "\n"
			<< "  yMin:               " << table.yMin << "\n"
			<< "  xMax:               " << table.xMax << "\n"
			<< "  yMax:               " << table.yMax << "\n"
			<< "  macStyle:           " << table.macStyle << "\n"
			<< "  lowestRecPPEM:      " << table.lowestRecPPEM << "\n"
			<< "  fontDirectionHint:  " << table.fontDirectionHint << "\n"
			<< "  indexToLocFormat:   " << table.indexToLocFormat << "\n"
			<< "  glyphDataFormat:    " << table.glyphDataFormat << "\n";

		Log::Print(headTableMsg.str());
	}

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

void ParsePreCheck(const vector<string>& params)
{
	path fontPath = path(params[1]);
	path kfontPath = path(params[2]);

	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctFontPath = weakly_canonical(Core::currentDir / fontPath);
	path correctKFontPath = weakly_canonical(Core::currentDir / kfontPath);

	if (!exists(correctFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because the font file '" + correctFontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}
	if (exists(correctKFontPath))
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the kfont file already exists!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (!path(correctFontPath).has_extension()
		|| (path(correctFontPath).extension() != ".ttf"
		&& path(correctFontPath).extension() != ".otf"))
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (!path(correctKFontPath).has_extension()
		|| path(correctKFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the kfont file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	size_t offset{};
	vector<u8> versionData{};

	string result = ReadBinaryLinesFromFile(correctFontPath.string(), versionData, 0, 4);
	if (!result.empty())
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "'! Reason: " + result,
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	thisVersion = ReadU32(versionData, offset);

	if (path(correctFontPath).extension() == ".ttf"
		&& thisVersion != TTF_VERSION)
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file does not have a valid ttf/otf font version!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (path(correctFontPath).extension() == ".otf"
		&& thisVersion != TTF_VERSION
		&& thisVersion != OTF_VERSION)
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file does not have a valid otf font version!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}
}

void GetPreCheck(const vector<string>& params)
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

		CommandManager::ParseCommand({ "--e" });
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + correctFontPath.string() + "' is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}

	if (!path(correctFontPath).has_extension()
		|| path(correctFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + correctFontPath.string() + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		CommandManager::ParseCommand({ "--e" });
	}
}