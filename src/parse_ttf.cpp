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

struct GlyphPoint
{
	i16 x{};
	i16 y{};
	bool onCurve{};
};

struct GlyphContours
{
	vector<vector<GlyphPoint>> contours{};
	bool isComposite{};
};

enum GlyphFlags : u8
{
	GLYPH_ON_CURVE_POINT      = 0x01,
	GLYPH_X_SHORT_SECTOR      = 0x02,
	GLYPH_Y_SHORT_SECTOR      = 0x04,
	GLYPH_REPEAT_FLAG         = 0x08,
	GLYPH_X_SAME_OR_POS_SHORT = 0x10,
	GLYPH_Y_SAME_OR_POS_SHORT = 0x20
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

static GlyphContours ParseSimpleGlyph(
	const vector<u8>& data,
	u32 glyfBase,
	u32 start,
	u32 end);

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

		//
		// SIMPLE GLYPH
		//

		const u32 glyfBase = glyfIt->offset;

		for (u32 gi = 0; gi < maxpTable.numGlyphs; ++gi)
		{
			u32 start = locaTable.glyphOffsets[gi];
			u32 end = locaTable.glyphOffsets[gi + 1];

			if (start == end) continue; //empty

			GlyphContours glyphTable = ParseSimpleGlyph(
				data,
				glyfBase,
				start,
				end);

			if (glyphTable.isComposite)
			{
				//TODO: handle composites here

				continue;
			}

			if (isVerbose
				&& !glyphTable.contours.empty())
			{
				const auto& c0 = glyphTable.contours[0];

				ostringstream simpleGlyfMsg;

				simpleGlyfMsg << "Glyph '" << gi << "' contours: " << glyphTable.contours.size()
					<< ", first contour points: " << c0.size();

				if (!c0.empty())
				{
					simpleGlyfMsg << "  p0: (" << c0[0].x << ", " << c0[0].y
						<< ") on: " << c0[0].onCurve;
				}

				Log::Print(simpleGlyfMsg.str());
			}
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

GlyphContours ParseSimpleGlyph(
	const vector<u8>& data,
	u32 glyfBase,
	u32 start,
	u32 end)
{
	GlyphContours contours{};

	if (start == end) return contours; //empty glyph

	size_t offset = size_t(glyfBase) + size_t(start);
	const size_t pend = size_t(glyfBase) + size_t(end);

	i16 numberOfContours = static_cast<i16>(Parse::ReadU16(data, offset)); offset += 2;
	i16 xMin             = static_cast<i16>(Parse::ReadU16(data, offset)); offset += 2;
	i16 yMin             = static_cast<i16>(Parse::ReadU16(data, offset)); offset += 2;
	i16 xMax             = static_cast<i16>(Parse::ReadU16(data, offset)); offset += 2;
	i16 yMax             = static_cast<i16>(Parse::ReadU16(data, offset)); offset += 2;

	//composite glyph
	if (numberOfContours < 0)
	{
		contours.isComposite = true;
		return contours;
	}

	//simple glyph with no contours
	if (numberOfContours == 0) return contours;

	vector<u16> endPtsOfContours(numberOfContours);
	for (int i = 0; i < numberOfContours; ++i)
	{
		endPtsOfContours[i] = Parse::ReadU16(data, offset);
		offset += 2;
	}

	u16 instructionsLength = Parse::ReadU16(data, offset); offset += 2;
	offset += instructionsLength; //ignore instructions

	const size_t pointCount = size_t(endPtsOfContours.back()) + 1;
	if (pointCount == 0) return contours;

	return contours;
}