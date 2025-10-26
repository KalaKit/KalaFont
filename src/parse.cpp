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

#include "parse.hpp"
#include "parse_ttf.hpp"
#include "parse_otf.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::ReadBinaryLinesFromFile;

using KalaFont::Core;
using KalaFont::Parse;
using KalaFont::OffsetTable;
using KalaFont::TableRecord;
using KalaFont::HeadTable;
using KalaFont::HheaTable;
using KalaFont::HmtxEntry;
using KalaFont::Parse_TTF;
using KalaFont::Parse_OTF;

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
constexpr u32 MAGIC_NUMBER = 0x5F0F3CF5;

static u32 thisVersion{};

static void ParseAny(
	const vector<string>& params,
	bool isVerbose = false);

static OffsetTable ReadOffsetTable(
	const string& path, 
	vector<u8>& outData,
	bool isVerbose);
static HeadTable ReadHeadTable(
	const vector<u8>& data, 
	u32 offset,
	bool isVerbose);
static HheaTable ReadHhea(
	const vector<u8>& data,
	u32 offset);
static vector<HmtxEntry> ReadHmtx(
	const vector<u8>& data,
	u32 offset,
	u16 count);

static bool ParsePreCheck(const vector<string>& params);
static bool GetPreCheck(const vector<string>& params);

namespace KalaFont
{
	void Parse::ParseFont(const vector<string>& params)
	{
		ParseAny(params, false);
	}
	void Parse::VerboseParseFont(const vector<string>& params)
	{
		ParseAny(params, true);
	}

	void Parse::GetKFontInfo(const vector<string>& params)
	{
		GetPreCheck(params);
	}
}

void ParseAny(
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

	if (offsetTable.tables.empty())
	{
		Log::Print(
			"Failed to parse font because it had invalid offset table data!",
			"PARSE_TTF",
			LogType::LOG_ERROR,
			2);

		return;
	}

	//
	// HEAD TABLE
	//

	auto headIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "head"; });

	HeadTable headTable = ReadHeadTable(
		data, 
		headIt->offset,
		isVerbose);

	if (headTable.magicNumber == 0)
	{
		Log::Print(
			"Failed to parse font because it had invalid head table data!",
			"PARSE_TTF",
			LogType::LOG_ERROR,
			2);

		return;
	}

	//
	// PARSE HHEA AND HMTX
	//

	auto hheaIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "hhea"; });

	HheaTable hheaTable = ReadHhea(
		data, 
		hheaIt->offset);

	auto hmtxIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
		[](const TableRecord& t) { return string(t.tag, 4) == "hmtx"; });

	vector<HmtxEntry> hMetrics = ReadHmtx(
		data, 
		hmtxIt->offset, 
		hheaTable.numberOfMetrics);

	//
	// PARSE OTF/TTF
	//

	bool success{};
	if (thisVersion == TTF_VERSION)
	{
		success = Parse_TTF::Parse(
			data, 
			offsetTable, 
			headTable,
			hheaTable,
			hMetrics,
			isVerbose);
	}
	else
	{
		success = Parse_OTF::Parse(
			data,
			offsetTable,
			headTable,
			hheaTable,
			hMetrics,
			isVerbose);
	}

	string fontType = thisVersion == TTF_VERSION
		? "TTF"
		: "OTF";

	if (!success)
	{
		Log::Print(
			"Failed to parse " + fontType + " font!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	Log::Print(
		"Finished parsing " + fontType + " font!",
		"PARSE",
		LogType::LOG_SUCCESS);
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

		return table;
	}

	size_t offset{};

	//test if numTables is valid
	if (Parse::ReadU16(data, offset + 6) == 0) return {};

	table.scalerType    = Parse::ReadU32(data, offset); offset += 4;
	table.numTables     = Parse::ReadU16(data, offset); offset += 2;
	table.searchRange   = Parse::ReadU16(data, offset); offset += 2;
	table.entrySelector = Parse::ReadU16(data, offset); offset += 2;
	table.rangeShift    = Parse::ReadU16(data, offset); offset += 2;

	table.tables.reserve(table.numTables);

	for (u16 i = 0; i < table.numTables; ++i)
	{
		TableRecord rec{};
		memcpy(rec.tag, &data[offset], 4);
		rec.checkSum = Parse::ReadU32(data, offset + 4);
		rec.offset   = Parse::ReadU32(data, offset + 8);
		rec.length   = Parse::ReadU32(data, offset + 12);

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
	//test if magic number is valid
	if (Parse::ReadU32(data, offset + 12) != MAGIC_NUMBER) return {};

	HeadTable table{};
	
	table.majorVersion       = static_cast<i16>(Parse::ReadU16(data, offset));
	table.minorVersion       = static_cast<i16>(Parse::ReadU16(data, offset + 2));
	table.fontRevision       = static_cast<i32>(Parse::ReadU32(data, offset + 4));
	table.checkSumAdjustment = Parse::ReadU32(data, offset + 8);
	table.magicNumber        = Parse::ReadU32(data, offset + 12);
	table.flags              = Parse::ReadU16(data, offset + 16);
	table.unitsPerEm         = Parse::ReadU16(data, offset + 18);
	table.created            = (static_cast<i64>(Parse::ReadU32(data, offset + 20)) << 32
	                         | Parse::ReadU32(data, offset + 24));
	table.modified           = (static_cast<i64>(Parse::ReadU32(data, offset + 28)) << 32
	                         | Parse::ReadU32(data, offset + 32));
	table.xMin               = static_cast<i16>(Parse::ReadU16(data, offset + 36));
	table.yMin               = static_cast<i16>(Parse::ReadU16(data, offset + 38));
	table.xMax               = static_cast<i16>(Parse::ReadU16(data, offset + 40));
	table.yMax               = static_cast<i16>(Parse::ReadU16(data, offset + 42));
	table.macStyle           = Parse::ReadU16(data, offset + 44);
	table.lowestRecPPEM      = Parse::ReadU16(data, offset + 46);
	table.fontDirectionHint  = static_cast<i16>(Parse::ReadU16(data, offset + 48));
	table.indexToLocFormat   = static_cast<i16>(Parse::ReadU16(data, offset + 50));
	table.glyphDataFormat    = static_cast<i16>(Parse::ReadU16(data, offset + 52));

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

HheaTable ReadHhea(
	const vector<u8>& data,
	u32 offset)
{
	HheaTable table{};
	table.ascender = Parse::ReadU16(data, offset + 4);
	table.descender = Parse::ReadU16(data, offset + 6);
	table.numberOfMetrics = Parse::ReadU16(data, offset + 34);

	return table;
}

vector<HmtxEntry> ReadHmtx(
	const vector<u8>& data,
	u32 offset,
	u16 count)
{
	vector<HmtxEntry> v(count);
	size_t p = offset;

	for (u16 i = 0; i < count; ++i)
	{
		v[i].advanceWidth = Parse::ReadU16(data, p); p += 2;
		v[i].leftSideBearing = static_cast<i16>(Parse::ReadU16(data, p)); p += 2;
	}

	return v;
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
			"Cannot parse to kfont because the font file '" + correctFontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (exists(correctKFontPath))
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the kfont file already exists!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file is not a regular file!",
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
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(correctKFontPath).has_extension()
		|| path(correctKFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the kfont file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
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

		return false;
	}

	thisVersion = Parse::ReadU32(versionData, offset);

	if (path(correctFontPath).extension() == ".ttf"
		&& thisVersion != TTF_VERSION)
	{
		Log::Print(
			"Cannot parse the font file '" + correctFontPath.string() + "' to kfont file '" + correctKFontPath.string() + "' because the font file does not have a valid ttf/otf font version!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
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