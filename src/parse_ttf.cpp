//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <sstream>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/math_utils.hpp"
#include "KalaHeaders/file_utils.hpp"

#include "parse.hpp"
#include "parse_ttf.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::vec2;
using KalaHeaders::mat2;
using KalaHeaders::ReadU8;
using KalaHeaders::ReadU16;
using KalaHeaders::ReadU32;

using KalaFont::Parse;
using KalaFont::OffsetTable;
using KalaFont::TableRecord;
using KalaFont::HeadTable;
using KalaFont::GlyphPoint;
using KalaFont::GlyphContours;

using std::vector;
using std::string;
using std::ostringstream;
using std::min;
using std::hex;
using std::dec;
using std::move;

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

static MaxpTable ReadMaxpTable(
	const vector<u8>& data,
	u32 offset,
	bool isVerbose);

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
	const GlyphInfo& header,
	u32 glyfBase,
	u32 start,
	u32 end);

static GlyphContours ParseCompositeGlyph(
	const vector<u8>& data,
	const LocaTable& locaTable,
	u32 glyfBase,
	u32 start,
	u32 end);

namespace KalaFont
{
	vector<GlyphResult> Parse_TTF::Parse(
		const vector<u8>& data,
		const OffsetTable& offsetTable,
		const HeadTable& headTable,
		const HheaTable& hheaTable,
		const vector<HmtxEntry>& hMetrics,
		bool isVerbose)
	{
		//
		// MAXP TABLE
		//

		auto maxpIt = find_if(offsetTable.tables.begin(), offsetTable.tables.end(),
			[](const TableRecord& t) { return string(t.tag, 4) == "maxp"; });

		MaxpTable maxpTable = ReadMaxpTable(
			data,
			maxpIt->offset,
			isVerbose);

		if (maxpTable.numGlyphs == 0)
		{
			Log::Print(
				"Failed to parse font because it had invalid maxp table data!",
				"PARSE_TTF",
				LogType::LOG_ERROR,
				2);

			return{};
		}

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

			return{};
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

		vector<GlyphResult> parsedData{};

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
					locaTable,
					glyfBase,
					start,
					end)
				: ParseSimpleGlyph(
					data,
					header,
					glyfBase,
					start,
					end);

			GlyphResult result{};
			result.glyphIndex = gi;
			result.contours = glyphData;

			if (gi < hMetrics.size())
			{
				result.advanceWidth = hMetrics[gi].advanceWidth;
				result.leftSideBearing = hMetrics[gi].leftSideBearing;
			}
			else
			{
				result.advanceWidth = hMetrics.back().advanceWidth;
				result.leftSideBearing = hMetrics.back().leftSideBearing;
			}

			result.anchor = { static_cast<f32>(result.leftSideBearing), 0.0f };

			parsedData.push_back(move(result));
		}

		return parsedData;
	}
}

MaxpTable ReadMaxpTable(
	const vector<u8>& data,
	u32 offset,
	bool isVerbose)
{
	//test if numglyphs is valid
	if (ReadU32(data, offset + 4) == 0) return {};

	MaxpTable table{};

	table.version = ReadU32(data, offset);
	table.numGlyphs = ReadU16(data, offset + 4);

	if (isVerbose)
	{
		ostringstream maxpTableMsg{};

		maxpTableMsg << "Maxp table data:\n"
			<< "  version:   0x" << hex << table.version << dec << "\n"
			<< "  numGlyphs: " << table.numGlyphs << "\n";

		Log::Print(maxpTableMsg.str());
	}

	return table;
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

	gi.numberOfContours = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart));
	gi.xMin = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 2));
	gi.yMin = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 4));
	gi.xMax = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 6));
	gi.yMax = static_cast<i16>(ReadU16(data, glyfOffset + glyfStart + 8));

	return gi;
}

GlyphContours ParseSimpleGlyph(
	const vector<u8>& data,
	const GlyphInfo& header,
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
		endPts[i] = ReadU16(data, p);
		p += 2;
	}

	//
	// IGNORE INSTRUCTIONS
	//

	u16 instructionsLength = ReadU16(data, p); p += 2;
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
		u8 f = ReadU8(data, p++);
		flags.push_back(f);

		if (f & GLYPH_REPEAT_FLAG)
		{
			u8 count = ReadU8(data, p++);
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
				u8 b = ReadU8(data, p++);
				dx = (f & GLYPH_X_SAME_OR_POS_SHORT) ? int(b) : -int(b);
			}
			else
			{
				if (f & GLYPH_X_SAME_OR_POS_SHORT) dx = 0;
				else
				{
					dx = static_cast<i16>(ReadU16(data, p));
					p += 2;
				}
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
				u8 b = ReadU8(data, p++);
				dy = (f & GLYPH_Y_SAME_OR_POS_SHORT) ? int(b) : -int(b);
			}
			else
			{
				if (f & GLYPH_Y_SAME_OR_POS_SHORT) dy = 0;
				else
				{
					dy = static_cast<i16>(ReadU16(data, p));
					p += 2;
				}
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
	const LocaTable& locaTable,
	u32 glyfBase,
	u32 start,
	u32 end)
{
	GlyphContours contours{};

	size_t p = size_t(glyfBase) + size_t(start) + 10;
	const size_t pend = size_t(glyfBase) + size_t(end);

	struct Component
	{
		u16 glyphIndex{};
		vec2 args{};
		mat2 transform
		{
			1.0f, 0.0f,
			0.0f, 1.0f
		};
	};

	vector<Component> components{};

	u16 lastFlags{};
	bool more = true;

	while (more && p + 4 <= pend)
	{
		u16 flags = ReadU16(data, p); p += 2;
		u16 glyphIndex = ReadU16(data, p); p += 2;
		lastFlags = flags;

		Component comp{};
		comp.glyphIndex = glyphIndex;

		//read arguments
		if (flags & GLYPH_ARG_1_AND_2_ARE_WORDS)
		{
			comp.args.x = ReadU16(data, p); p += 2;
			comp.args.y = ReadU16(data, p); p += 2;
		}
		else
		{
			comp.args.x = ReadU16(data, p); p ++;
			comp.args.y = ReadU16(data, p); p ++;
		}

		//interpret args as XY if flagged
		if (!(flags & GLYPH_ARGS_ARE_XY_VALUES))
		{
			//these are point indices - skip for now
		}

		//handle transform

		if (flags & GLYPH_WE_HAVE_A_SCALE)
		{
			i16 val = static_cast<i16>(ReadU16(data, p)); p += 2;
			comp.transform.m00 = comp.transform.m11 = val / 16384.0f;
		}
		else if (flags & GLYPH_WE_HAVE_AN_X_AND_Y_SCALE)
		{
			i16 xScale = static_cast<i16>(ReadU16(data, p)); p += 2;
			i16 yScale = static_cast<i16>(ReadU16(data, p)); p += 2;
			comp.transform.m00 = xScale / 16384.0f;
			comp.transform.m11 = yScale / 16384.0f;
		}
		else if (flags & GLYPH_WE_HAVE_A_TWO_BY_TWO)
		{
			i16 m00 = static_cast<i16>(ReadU16(data, p)); p += 2;
			i16 m01 = static_cast<i16>(ReadU16(data, p)); p += 2;
			i16 m10 = static_cast<i16>(ReadU16(data, p)); p += 2;
			i16 m11 = static_cast<i16>(ReadU16(data, p)); p += 2;
			comp.transform.m00 = m00 / 16384.0f;
			comp.transform.m01 = m01 / 16384.0f;
			comp.transform.m10 = m10 / 16384.0f;
			comp.transform.m11 = m11 / 16384.0f;
		}

		components.push_back(comp);
		more = (flags & GLYPH_MORE_COMPONENTS);
	}

	//skip instructions
	if (lastFlags & GLYPH_WE_HAVE_INSTRUCTIONS)
	{
		u16 len = ReadU16(data, p);
		p += 2 + len;
	}

	//load and transform each components contours
	for (const Component& comp : components)
	{
		//get this components start/end from loca
		if (comp.glyphIndex + 1 >= locaTable.glyphOffsets.size()) continue;

		u32 compStart = locaTable.glyphOffsets[comp.glyphIndex];
		u32 compEnd = locaTable.glyphOffsets[comp.glyphIndex + 1];

		//empty
		if (compStart == compEnd) continue;

		GlyphInfo subHeader = ReadGlyphHeader(
			data,
			glyfBase,
			compStart);

		GlyphContours subContours{};

		subContours = subHeader.numberOfContours >= 0
			? ParseSimpleGlyph(
				data,
				subHeader,
				glyfBase,
				compStart,
				compEnd)
			: ParseCompositeGlyph(
				data,
				locaTable,
				glyfBase,
				compStart,
				compEnd);

		//apply transform + offset
		for (auto& contour : subContours.contours)
		{
			for (auto& pt : contour)
			{
				vec2 pos = pt.size;
				pt.size = (comp.transform * pos) + comp.args;
			}
		}

		//merge into master
		contours.contours.insert(
			contours.contours.end(),
			subContours.contours.begin(),
			subContours.contours.end());
	}

	return contours;
}