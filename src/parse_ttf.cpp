//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/math_utils.hpp"

#include "parse.hpp"
#include "parse_ttf.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::kvec2;

using KalaFont::Parse;
using KalaFont::OffsetTable;
using KalaFont::TableRecord;
using KalaFont::HeadTable;
using KalaFont::MaxpTable;
using KalaFont::GlyphInfo;
using KalaFont::GlyphPoint;
using KalaFont::GlyphContours;

using std::vector;
using std::string;
using std::ostringstream;
using std::min;

struct LocaTable
{
	vector<u32> glyphOffsets{};
};

enum SimpleGlyphFlags : u8
{
	GLYPH_ON_CURVE_POINT      = 0x01,
	GLYPH_X_SHORT_SECTOR      = 0x02,
	GLYPH_Y_SHORT_SECTOR      = 0x04,
	GLYPH_REPEAT_FLAG         = 0x08,
	GLYPH_X_SAME_OR_POS_SHORT = 0x10,
	GLYPH_Y_SAME_OR_POS_SHORT = 0x20
};
enum CompositeGlyphFlags : u16
{
	GLYPH_ARG_1_AND_2_ARE_WORDS    = 0x0001, //else bytes
	GLYPH_ARGS_ARE_XY_VALUES       = 0x0002, //else point indices
	GLYPH_ROUND_XY_TO_GRID         = 0x0004,
	GLYPH_WE_HAVE_A_SCALE          = 0x0008,
	GLYPH_MORE_COMPONENTS          = 0x0020,
	GLYPH_WE_HAVE_AN_X_AND_Y_SCALE = 0x0040,
	GLYPH_WE_HAVE_A_TWO_BY_TWO     = 0x0080,
	GLYPH_WE_HAVE_INSTRUCTIONS     = 0x0100
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
	GlyphInfo header,
	u32 glyfBase,
	u32 start,
	u32 end);

static GlyphContours ParseCompositeGlyph(
	const vector<u8>& data,
	GlyphInfo header,
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
		// SIMPLE AND COMPOSITE GLYPH
		//

		const u32 glyfBase = glyfIt->offset;

		for (u32 gi = 0; gi < maxpTable.numGlyphs; ++gi)
		{
			u32 start = locaTable.glyphOffsets[gi];
			u32 end = locaTable.glyphOffsets[gi + 1];

			if (start == end) continue; //empty

			GlyphInfo header = ReadGlyphHeader(
				data, 
				glyfBase, 
				start);

			GlyphContours glyphData =
				(header.numberOfContours < 0)
				? ParseCompositeGlyph(
					data,
					header,
					glyfBase,
					start,
					end)
				: ParseSimpleGlyph(
					data,
					header,
					glyfBase,
					start,
					end);

			if (isVerbose
				&& !glyphData.contours.empty())
			{
				const auto& c0 = glyphData.contours[0];

				ostringstream glyfMsg;

				glyfMsg << "Glyph '" << gi << "' contours: " << glyphData.contours.size()
					<< ", first contour points: " << c0.size();

				if (!c0.empty())
				{
					glyfMsg << "  p0: (" << c0[0].size.x << ", " << c0[0].size.y
						<< ") on: " << c0[0].onCurve;
				}

				Log::Print(glyfMsg.str());
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
	GlyphInfo header,
	u32 glyfBase,
	u32 start,
	u32 end)
{
	GlyphContours contours{};

	size_t p = size_t(glyfBase) + size_t(start) + 10;
	const size_t pend = size_t(glyfBase) + size_t(end);

	//endPtsOfContours
	vector<u16> endPts(header.numberOfContours);
	for (int i = 0; i < header.numberOfContours; ++i)
	{
		endPts[i] = Parse::ReadU16(data, p);
		p += 2;
	}

	//
	// IGNORE INSTRUCTIONS
	//

	u16 instructionsLength = Parse::ReadU16(data, p); p += 2;
	p += instructionsLength;

	//
	// TOTAL POINTS
	//

	const size_t pointCount = size_t(endPts.back()) + 1;
	if (pointCount == 0) return contours;

	//
	// READ FLAGS
	//

	vector<u8> flags{};
	flags.reserve(pointCount);

	while (flags.size() < pointCount)
	{
		u8 f = Parse::ReadU8(data, p++);
		flags.push_back(f);

		if (f & GLYPH_REPEAT_FLAG)
		{
			u8 count = Parse::ReadU8(data, p++);
			flags.insert(flags.end(), count, f);
		}
	}

	//
	// READ X COORDINATES
	//

	vector<i16> xs(pointCount);
	{
		int x{};
		for (size_t i = 0; i < pointCount; ++i)
		{
			u8 f = flags[i];
			int dx{};

			if (f & GLYPH_X_SHORT_SECTOR)
			{
				u8 b = Parse::ReadU8(data, p++);
				dx = (f & GLYPH_X_SAME_OR_POS_SHORT) ? int(b) : -int(b);
			}
			else
			{
				if (f & GLYPH_X_SAME_OR_POS_SHORT) dx = 0;
				else dx = static_cast<i16>(Parse::ReadU16(data, p)); p += 2;
			}

			x += dx;
			xs[i] = static_cast<i16>(x);
		}
	}

	//
	// READ Y COORDINATES
	//

	vector<i16> ys(pointCount);
	{
		int y{};
		for (size_t i = 0; i < pointCount; ++i)
		{
			u8 f = flags[i];
			int dy{};

			if (f & GLYPH_Y_SHORT_SECTOR)
			{
				u8 b = Parse::ReadU8(data, p++);
				dy = (f & GLYPH_Y_SAME_OR_POS_SHORT) ? int(b) : -int(b);
			}
			else
			{
				if (f & GLYPH_Y_SAME_OR_POS_SHORT) dy = 0;
				else dy = static_cast<i16>(Parse::ReadU16(data, p)); p += 2;
			}

			y += dy;
			ys[i] = static_cast<i16>(y);
		}
	}

	//
	// BUILD POINT LIST
	//

	vector<GlyphPoint> pts(pointCount);
	for (size_t i = 0; i < pointCount; ++i)
	{
		pts[i] = GlyphPoint
		{
			{ static_cast<f32>(xs[i]), static_cast<f32>(ys[i]) },
			(flags[i] & GLYPH_ON_CURVE_POINT) != 0
		};
	}

	//
	// SPLIT INTO CONTOURS USING END PTS
	//

	contours.contours.reserve(header.numberOfContours);
	size_t startIndex{};

	for (int c = 0; c < header.numberOfContours; ++c)
	{
		size_t endIndex = size_t(endPts[c]);
		vector<GlyphPoint> contour{};
		contour.reserve(endIndex + 1 - startIndex);

		for (size_t i = startIndex; i <= endIndex; ++i)
		{
			contour.push_back(pts[i]);
		}

		contours.contours.push_back(move(contour));
		startIndex = endIndex + 1;
	}

	return contours;
}

GlyphContours ParseCompositeGlyph(
	const vector<u8>& data,
	GlyphInfo header,
	u32 glyfBase,
	u32 start,
	u32 end)
{
	GlyphContours contours{};

	size_t p = size_t(glyfBase) + size_t(start) + 10;
	const size_t pend = size_t(glyfBase) + size_t(end);

	return contours;
}