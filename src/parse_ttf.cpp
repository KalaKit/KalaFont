//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"

#include "parse.hpp"
#include "parse_ttf.hpp"

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
using std::min;

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

static LocaTable ReadLocaTable(
	const vector<u8>& data,
	u32 offset,
	u16 numGlyphs,
	i16 indexToLocFormat,
	bool isVerbose);
static GlyphInfo ReadGlyphHeader(
	const vector<u8>& data,
	u32 glyfOffset,
	u32 glyfStart);

namespace KalaFont
{
	bool Parse_TTF::Parse(
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
			headTable.indexToLocFormat,
			isVerbose);

		if (locaTable.glyphOffsets.size() == 0)
		{
			Log::Print(
				"Failed to parse TTF font because it had no glyph offsets!",
				"PARSE_TTF",
				LogType::LOG_ERROR,
				2);

			return false;
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

		return true;
	}
}

LocaTable ReadLocaTable(
	const vector<u8>& data,
	u32 offset,
	u16 numGlyphs,
	i16 indexToLocFormat,
	bool isVerbose)
{
	LocaTable table{};

	table.glyphOffsets.resize(numGlyphs + 1);

	if (indexToLocFormat == 0)
	{
		for (u32 i = 0; i <= numGlyphs; ++i)
		{
			u16 val = Parse::ReadU16(data, offset + i * 2);
			table.glyphOffsets[i] = static_cast<u32>(val) * 2;
		}
	}
	else
	{
		for (u32 i = 0; i <= numGlyphs; ++i)
		{
			table.glyphOffsets[i] = Parse::ReadU32(data, offset + i * 4);
		}
	}

	if (isVerbose)
	{
		ostringstream locaTableMsg{};

		locaTableMsg << "Loca table data:\n"
			<< "  glyph offsets count: " << table.glyphOffsets.size() << "\n"
			<< "  First 10 offsets:\n";

		for (size_t i = 0; i < min<size_t>(table.glyphOffsets.size(), 10); ++i)
		{
			locaTableMsg << "    [" << i << "]: " << table.glyphOffsets[i] << "\n";
		}

		Log::Print(locaTableMsg.str());
	}

	return table;
}

GlyphInfo ReadGlyphHeader(
	const vector<u8>& data,
	u32 glyfOffset,
	u32 glyfStart)
{
	GlyphInfo gi{};

	gi.numberOfContours = static_cast<i16>(Parse::ReadU16(data, glyfOffset + glyfStart));
	gi.xMin = static_cast<i16>(Parse::ReadU16(data, glyfOffset + glyfStart + 2));
	gi.yMin = static_cast<i16>(Parse::ReadU16(data, glyfOffset + glyfStart + 4));
	gi.xMax = static_cast<i16>(Parse::ReadU16(data, glyfOffset + glyfStart + 6));
	gi.yMax = static_cast<i16>(Parse::ReadU16(data, glyfOffset + glyfStart + 8));

	return gi;
}