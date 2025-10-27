//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <numeric>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"
#include "KalaHeaders/math_utils.hpp"

#include "parse.hpp"
#include "parse_ttf.hpp"
#include "parse_otf.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::kvec2;
using KalaHeaders::ReadBinaryLinesFromFile;

using KalaFont::Core;
using KalaFont::Parse;
using KalaFont::OffsetTable;
using KalaFont::TableRecord;
using KalaFont::HeadTable;
using KalaFont::HheaTable;
using KalaFont::HmtxEntry;
using KalaFont::GlyphPoint;
using KalaFont::GlyphResult;
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
using std::iota;

constexpr u32 TTF_VERSION = 0x00010000;
constexpr u32 OTF_VERSION = 'OTTO';
constexpr u32 MAGIC_NUMBER = 0x5F0F3CF5;

//Controls the curve resolution of the font,
//higher means smoother but more vertices
constexpr u8 CURVE_RESOLUTION = 1;

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

//Generate triangulated meshes for each glyph
static bool TriangulateGeometry(vector<GlyphResult>& parsedData);

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
	if (!ParsePreCheck(params)) return;

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

	vector<GlyphResult> parsedData{};

	if (thisVersion == TTF_VERSION)
	{
		parsedData = Parse_TTF::Parse(
			data, 
			offsetTable, 
			headTable,
			hheaTable,
			hMetrics,
			isVerbose);
	}
	else
	{
		parsedData = Parse_OTF::Parse(
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

	if (parsedData.empty())
	{
		Log::Print(
			"Failed to parse " + fontType + " font!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	bool geometryResult = TriangulateGeometry(parsedData);

	if (!geometryResult)
	{
		Log::Print(
			"Failed to generate geometry for font " + fontType + "!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	f32 scale = 1.0f / headTable.unitsPerEm;

	for (auto& glyph : parsedData)
	{
		if (glyph.vertices.size() > 512)
		{
			Log::Print(
				"Font " + fontType + " exceeded 512 vertices for glyph '" + to_string(glyph.glyphIndex) + "'!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}
		if (glyph.indices.size() > 512)
		{
			Log::Print(
				"Font " + fontType + " exceeded 512 indices for glyph '" + to_string(glyph.glyphIndex) + "'!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		for (size_t i = 0; i < glyph.vertices.size(); i += 2)
		{
			glyph.vertices[i + 0] *= scale;
			glyph.vertices[i + 1] *= scale;
		}

		glyph.anchor *= scale;
		glyph.leftSideBearing = static_cast<f32>(glyph.leftSideBearing) * scale;
		glyph.advanceWidth = static_cast<f32>(glyph.advanceWidth) * scale;
	}

	if (isVerbose)
	{
		ostringstream resultMsg{};
		resultMsg << "First couple of glyphs:\n";
		Log::Print(resultMsg.str());

		u8 count = 0;

		for (const auto& v : parsedData)
		{
			ostringstream vmsg{};

			if (v.glyphIndex > 10
				|| v.contours.contours.empty()
				|| v.vertices.empty()
				|| v.indices.empty())
			{
				continue;
			}

			vmsg << "Glyph[" << v.glyphIndex << "]\n"
				<< "  advance width:     " << v.advanceWidth << "\n"
				<< "  left side bearing: " << v.leftSideBearing << "\n"
				<< "  anchor:            (" << v.anchor.x << ", " << v.anchor.y << ")\n"
				<< "  transform:\n"
				<< "    [" << v.transform.m00 << ", " << v.transform.m01 << "]\n"
				<< "    [" << v.transform.m10 << ", " << v.transform.m11 << "]\n"
				<< "  vertices:\n";

			for (const auto& vert : v.vertices)
			{
				vmsg << "    " << vert << "\n";
			}

			vmsg << "  indices:\n";

			for (const auto& ind : v.indices)
			{
				vmsg << "    " << ind << "\n";
			}

			Log::Print(vmsg.str());
		}
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
		v[i].advanceWidth = static_cast<f32>(Parse::ReadU16(data, p)); p += 2;
		v[i].leftSideBearing = static_cast<f32>(Parse::ReadU16(data, p)); p += 2;
	}

	return v;
}

bool TriangulateGeometry(vector<GlyphResult>& glyphs)
{
	//TODO: winding correction + hole detection for complex glyphs

	for (auto& glyph : glyphs)
	{
		if (glyph.contours.contours.empty()) continue;

		glyph.vertices.clear();
		glyph.indices.clear();

		//ear-clipping lambda
		auto TriangulatePolygon = [](const vector<kvec2>& poly) -> vector<u32>
			{
				vector<u32> indices{};
				const size_t n = poly.size();
				if (n < 3) return indices;

				vector<u32> verts(n);
				iota(verts.begin(), verts.end(), 0);

				auto area = [&](const kvec2& a, const kvec2& b, const kvec2& c)
					{
						return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
					};

				auto inside = [&](const kvec2& A, const kvec2& B, const kvec2& C, const kvec2& P)
					{
						return 
							area(A, B, P) > 0 
							&& area(B, C, P) > 0 
							&& area(C, A, P) > 0;
					};

				size_t count = 2 * n;
				while (verts.size() > 2 && count--)
				{
					bool earFound{};

					for (size_t i = 0; i < verts.size(); ++i)
					{
						u32 i0 = verts[(i + verts.size() - 1) % verts.size()];
						u32 i1 = verts[i];
						u32 i2 = verts[(i + 1) % verts.size()];

						const kvec2& A = poly[i0];
						const kvec2& B = poly[i1];
						const kvec2& C = poly[i2];

						if (area(A, B, C) <= 0) continue; //reflex

						bool anySide{};

						for (u32 j : verts)
						{
							if (j == i0
								|| j == i1
								|| j == i2)
							{
								continue;
							}

							if (inside(A, B, C, poly[j]))
							{
								anySide = true;
								break;
							}
						}

						if (!anySide)
						{
							indices.push_back(i0);
							indices.push_back(i1);
							indices.push_back(i2);
							verts.erase(verts.begin() + i);
							earFound = true;
							break;
						}
					}
					if (!earFound) break;
				}

				return indices;
			};

		//flatten quadratic beziers into polygons

		vector<vector<kvec2>> polygons{};

		for (const auto& contour : glyph.contours.contours)
		{
			vector<kvec2> flattened{};
			if (contour.empty()) continue;

			size_t count = contour.size();
			auto GetPoint = [&](size_t idx) -> const GlyphPoint&
				{
					return contour[idx % count];
				};

			for (size_t i = 0; i < count; ++i)
			{
				const GlyphPoint& p0 = GetPoint(i);
				const GlyphPoint& p1 = GetPoint(i + 1);

				if (p0.onCurve
					&& p1.onCurve)
				{
					flattened.push_back(p0.size);
				}
				else if (p0.onCurve
					&& !p1.onCurve)
				{
					const GlyphPoint& p2 = GetPoint(i + 2);
					kvec2 nextOn = p2.onCurve ? p2.size : (p1.size + p2.size) * 0.5f;

					for (int s = 0; s <= CURVE_RESOLUTION; ++s)
					{
						f32 t = static_cast<f32>(s) / CURVE_RESOLUTION;
						f32 u = 1.0f - t;

						kvec2 pt =
							(u * u) * p0.size
							+ (2.0f * u * t) * p1.size
							+ (t * t) * nextOn;
						flattened.push_back(pt);
					}
				}
			}

			polygons.push_back(move(flattened));
		}

		//triangulate each contour
		for (const auto& poly : polygons)
		{
			if (poly.size() < 3) continue;

			vector<u32> tris = TriangulatePolygon(poly);
			u32 vertexOffset = static_cast<u32>(glyph.vertices.size() / 2);

			for (const auto& v : poly)
			{
				glyph.vertices.push_back(v.x);
				glyph.vertices.push_back(v.y);
			}

			for (u32 i : tris) glyph.indices.push_back(vertexOffset + i);
		}
	}

	return true;
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