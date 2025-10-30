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
#include <unordered_set>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"
#include "KalaHeaders/math_utils.hpp"

#include "parse.hpp"
#include "parse_ttf.hpp"
#include "parse_otf.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::vec2;
using KalaHeaders::WriteBinaryLinesToFile;
using KalaHeaders::ReadBinaryLinesFromFile;
using KalaHeaders::ReadU8;
using KalaHeaders::ReadU16;
using KalaHeaders::ReadU32;

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
using std::move;
using std::isfinite;
using std::unordered_set;

constexpr u32 TTF_VERSION = 0x00010000;
constexpr u32 OTF_VERSION = 'OTTO';
constexpr u32 MAGIC_NUMBER = 0x5F0F3CF5;

//Controls the curve resolution of the font,
//higher means smoother but more vertices
constexpr u8 CURVE_RESOLUTION = 16;

//Prevent absurdly high vertice counts per glyph
constexpr u16 MAX_VERTICES = 8192;

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

//Exports the data to a kfont file to target path
static bool ExportKFont(
	const path& targetPath,
	const vector<GlyphResult>& parsedData);

static bool ParsePreCheck(
	const path& fontPath, 
	const path& targetPath);

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
}

void ParseAny(
	const vector<string>& params,
	bool isVerbose)
{
	path fontPath = path(params[1]);
	path kfontPath = path(params[2]);

	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctFontPath = weakly_canonical(Core::currentDir / fontPath);
	path correctTargetPath = weakly_canonical(Core::currentDir / kfontPath);

	if (!ParsePreCheck(
		correctFontPath,
		correctTargetPath))
	{
		return;
	}

	//
	// OFFSET TABLE
	//

	vector<u8> data{};

	OffsetTable offsetTable = ReadOffsetTable(
		correctFontPath.string(),
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
			"Failed to parse " + correctFontPath.string() + " font!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	bool geometryResult = TriangulateGeometry(parsedData);

	if (!geometryResult)
	{
		Log::Print(
			"Failed to generate geometry for font " + correctFontPath.string() + "!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	//
	// REMOVE MALFORMED OR EMPTY DATA
	//

	parsedData.erase(
		remove_if(
			parsedData.begin(),
			parsedData.end(),
			[](const GlyphResult& glyph)
			{
				return glyph.vertices.empty()
					|| glyph.indices.empty()
					|| glyph.contours.contours.empty();
			}),
			parsedData.end());

	//
	// SCALE PARSED DATA
	//

	f32 scale = 1.0f / headTable.unitsPerEm;

	for (auto& glyph : parsedData)
	{
		if (glyph.vertices.size() > MAX_VERTICES)
		{
			Log::Print(
				"Font " + correctFontPath.string() + " exceeded 8192 vertices for glyph '" + to_string(glyph.glyphIndex) + "'!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return;
		}
		if (glyph.indices.size() > MAX_VERTICES)
		{
			Log::Print(
				"Font " + correctFontPath.string() + " exceeded 8192 indices for glyph '" + to_string(glyph.glyphIndex) + "'!",
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

	//
	// EXPORT PARSED DATA
	//

	if (!ExportKFont(
		correctTargetPath,
		parsedData))
	{
		Log::Print(
			"Failed to export font '" + correctFontPath.string() + "' parsed data to target path '" + correctTargetPath.string() + "'!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return;
	}

	Log::Print(
		"Parsed font '" + correctFontPath.string() + "' and exported it to target path '" + correctTargetPath.string() + "' !",
		"PARSE",
		LogType::LOG_SUCCESS);
}

OffsetTable ReadOffsetTable(
	const string& fontPath, 
	vector<u8>& outData,
	bool isVerbose)
{
	OffsetTable table{};

	vector<u8> data{};
	string result = ReadBinaryLinesFromFile(fontPath, data);

	if (!result.empty())
	{
		Log::Print(
			"Failed to parse the font file '" + fontPath + "'! Reason: " + result,
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return table;
	}

	size_t offset{};

	//test if numTables is valid
	if (ReadU16(data, offset + 6) == 0) return {};

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
	//test if magic number is valid
	if (ReadU32(data, offset + 12) != MAGIC_NUMBER) return {};

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
	table.ascender = ReadU16(data, offset + 4);
	table.descender = ReadU16(data, offset + 6);
	table.numberOfMetrics = ReadU16(data, offset + 34);

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
		v[i].advanceWidth = static_cast<f32>(ReadU16(data, p)); p += 2;
		v[i].leftSideBearing = static_cast<f32>(ReadU16(data, p)); p += 2;
	}

	return v;
}

bool TriangulateGeometry(vector<GlyphResult>& glyphs)
{
	constexpr float eps = 1e-6f;
	constexpr float eps_area = 1e-7f;
	
	auto almost_eq = [&](
		const vec2& a, 
		const vec2& b) -> bool
		{
			vec2 d = a - b;
			return fabsf(d.x) < eps
				&& fabsf(d.y) < eps;
		};
		
	//Cross of (b-a) x (c-b)
	auto cross2 = [&](
		const vec2& a, 
		const vec2& b,
		const vec2& c) -> float
		{
			vec2 u = b - a;
			vec2 v = c - b;
			
			return u.x * v.y - u.y * v.x;
		};
		
	//Shoelace - CCW > 0
	auto SignedArea = [&](const vector<vec2>& pts) -> float
		{
			if (pts.size() < 3) return 0.0f;
			double A = 0.0;
			
			for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++)
			{
				A += (double)pts[j].x
					* (double)pts[i].y
					- (double)pts[i].x
					* (double)pts[j].y;
			}
			
			return (float)(0.5 * A);
		};
		
	//Ray-cast with standard parity toggle
	auto PointInPolygon = [&](
		const vec2& p,
		const vector<vec2>& poly) -> bool
		{
			bool inside{};
			
			for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
			{
				const vec2& a = poly[i];
				const vec2& b = poly[j];
				bool condY = ((a.y > p.y) != (b.y > p.y));
				
				if (condY)
				{
					float dy = b.y - a.y;
					if (fabsf(dy) > eps)
					{
						float xInt = a.x + (b.x - a.x) * ((p.y - a.y) / dy);
						if (p.x < xInt) inside = !inside;
					}
				}
			}
			
			return inside;
		};
		
	auto polygon_centroid = [&](const vector<vec2>& p) -> vec2
		{
			DEBUG_ASSERT(p.size() >= 3);
			
			double A{};
			double cx{};
			double cy{};
			
			for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
			{
				double cross = 
					(double)p[j].x
					* (double)p[i].y
					- (double)p[i].x
					* (double)p[j].y;
					
			    A += cross;
					
				cx += ((double)p[j].x + (double)p[i].x) * cross;
				cy += ((double)p[j].y + (double)p[i].y) * cross;
			}
			
			A *= 0.5;
			if (fabs(A) < 1e-12) return p[0]; //fallback if degenerate
			return vec2{ (float)(cx / (6.0 * A)), (float)(cy / (6.0 * A)) };
		};
	
	auto dedupe_consecutive = [&](vector<vec2>& pts)
		{
			if (pts.empty()) return;
			
			vector<vec2> out{};
			out.reserve(pts.size());
			out.push_back(pts[0]);
			for (size_t i = 1; i < pts.size(); ++i)
			{
				if (!almost_eq(pts[i], out.back())) out.push_back(pts[i]);
			}
			
			//close ring duplicate
			if (out.size() > 1
				&& almost_eq(out.front(), out.back()))
			{
				out.pop_back();
			}
			
			pts.swap(out);
		};
		
	auto prune_colinear = [&](vector<vec2>& pts)
		{
			if (pts.size() < 3) return;
			
			vector<vec2> out{};
			out.reserve(pts.size());
			for (size_t i = 0; i < pts.size(); ++i)
			{
				const vec2& a = pts[(i + pts.size() - 1) % pts.size()];
				const vec2& b = pts[i];
				const vec2& c = pts[(i + 1) % pts.size()];
				
				if (fabsf(cross2(a, b, c)) > eps) out.push_back(b);
				else {} //drop colinear middle point????
			}
			
			//if everything collapsed (degenerate), keep original
			if (out.size() >= 3) pts.swap(out);
		};
		
	auto cleanup_polygon = [&](vector<vec2>& poly)
		{
			dedupe_consecutive(poly);
			prune_colinear(poly);
			
			//enforce ccw for triangulation
			if (SignedArea(poly) < 0.0f) reverse(poly.begin(), poly.end());
		};
		
	auto TriangulatePolygon = [&](const vector<vec2>& poly) -> vector<u32>
		{
			vector<u32> indices{};
			const size_t n = poly.size();
			if (n < 3) return indices;
			
			vector<u32> verts(n);
			iota(verts.begin(), verts.end(), 0);
			
			auto tri_area = [&](
				const vec2& a, 
				const vec2& b, 
				const vec2& c)
				{
					return (b.x - a.x)
						* (c.y - a.y)
						- (b.y - a.y)
						* (c.x - a.x);
				};
				
			//Allow points on edge with small epsilon
			auto inside = [&](
				const vec2& A,
				const vec2& B,
				const vec2& C,
				const vec2& P)
				{
					float a1 = tri_area(A, B, P);
					float a2 = tri_area(B, C, P);
					float a3 = tri_area(C, A, P);
					return (a1 >= -eps_area)
						&& (a2 >= -eps_area)
						&& (a3 >= -eps_area);
				};
				
			//conservative
			size_t watchdog = 3 * n;
			while (verts.size() > 2 && watchdog--)
			{
				bool earFound{};
				
				for (size_t i = 0; i < verts.size(); ++i)
				{
					u32 i0 = verts[(i + verts.size() - 1) % verts.size()];
					u32 i1 = verts[i];
					u32 i2 = verts[(i + 1) % verts.size()];
					
					const vec2& A = poly[i0];
					const vec2& B = poly[i1];
					const vec2& C = poly[i2];
					
					float Aarea = tri_area(A, B, C);
					if (Aarea <= eps_area) continue; //reflex or tiny
					
					bool anyInside{};
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
							anyInside = true;
							break;
						}
					}
					if (anyInside) continue;
					
					//ear
					indices.push_back(i0);
					indices.push_back(i1);
					indices.push_back(i2);
					verts.erase(verts.begin() + i);
					earFound = true;
					break;
				}
				
				if (!earFound)
				{
					//try a mild cleanup - if we got stuck,
					//drop the least significant point
					if (verts.size() > 3)
					{
						size_t kill{};
						float best = FLT_MAX;
						
						for (size_t k = 0; k < verts.size(); ++k)
						{
							const vec2& a = poly[verts[(k + verts.size() -1) % verts.size()]];
							const vec2& b = poly[verts[k]];
							const vec2& c = poly[verts[(k + 1) % verts.size()]];
							float area = fabsf(tri_area(a, b, c));
							if (area < best)
							{
								best = area;
								kill = k;
							}
						}
						
						verts.erase(verts.begin() + kill);
						continue;
					}
					
					Log::Print(
						"TriangulatePolygon: No ear found for " + to_string(verts.size()) + " verts", 
						"FONT", 
						LogType::LOG_ERROR,
						2);
					
					break;
				}
				
				if (watchdog % 500 == 0)
				{
					Log::Print(
						"TriangulatePolygon watchdog tick " + to_string(watchdog), 
						"FONT",
						LogType::LOG_DEBUG);	
				}

				if (watchdog == 0)
				{
					Log::Print(
						"TriangulatePolygon watchdog expired! Possible infinite loop.",
						"FONT",
						LogType::LOG_ERROR);
					break;
				}
			}
			
			DEBUG_ASSERT(indices.size() % 3 == 0);
			
			return indices;
		};
		
	//flatten a glyph contour (quadratic beziers) into a clean ccw polygon
	auto FlattenContour = [&](const vector<GlyphPoint>& contour) -> vector<vec2>
		{
			if (contour.empty()) return{};
			
			//normalize start to on-curve. if first is off-curve then prepend implied on-curve
			
			vector<GlyphPoint> norm = contour;
			if (!norm.front().onCurve)
			{
				const GlyphPoint& last = norm.back();
				GlyphPoint implied{};
				implied.onCurve = true;
				implied.size = (last.size + norm.front().size) * 0.5f;
				norm.insert(norm.begin(), implied);
			}
			
			//insert implied on-curve between consecutive off-curves
			
			vector<GlyphPoint> pts{};
			pts.reserve(norm.size() * 2);
			for (size_t i = 0; i < norm.size(); ++i)
			{
				const auto& a = norm[i];
				const auto& b = norm[(i + 1) % norm.size()];
				pts.push_back(a);
				
				if (!a.onCurve
					&& !b.onCurve)
				{
					GlyphPoint mid{};
					mid.onCurve = true;
					mid.size = (a.size + b.size) * 0.5f;
					pts.push_back(mid);
				}
			}
			
			//emit helper that avoids duplicates
			
			vector<vec2> out{};
			out.reserve(pts.size() * (CURVE_RESOLUTION + 1));
			auto emit = [&](const vec2& p)
				{
					if (out.empty()
						|| !almost_eq(out.back(), p))
					{
						out.push_back(p);	
					}
				};
				
			//avoid re-emitting A if it equals the last output point
			
			auto sampleQuad = [&](
				const vec2& A,
				const vec2& B,
				const vec2& C)
				{
					for (int s = 0; s <= CURVE_RESOLUTION; ++s)
					{
						float t = (float)s / (float)CURVE_RESOLUTION;
						float u = 1.0f - t;
						vec2 p = 
							(u * u) * A
							+ (2.0f * u * t) * B
							+ (t * t) * C;
							
						emit(p);
					}
				};
				
			size_t N = pts.size();
			vec2 prevOn = pts.back().onCurve
				? pts.back().size
				: (pts.back().size + pts[0].size) * 0.5f;
				
			for (size_t i = 0; i < N; ++i)
			{
				const GlyphPoint& a = pts[i];
				const GlyphPoint& b = pts[(i + 1) % N];
				
				if (a.onCurve
					&& b.onCurve)
				{
					emit(a.size);	
					prevOn = a.size;
					continue;
				}
				else if (a.onCurve
					&& !b.onCurve)
				{
					const GlyphPoint& cRaw = pts[(i + 2) % N];
					vec2 nextOn = cRaw.onCurve
						? cRaw.size
						: (b.size + cRaw.size) * 0.5f;
					
					sampleQuad(a.size, b.size, nextOn);
					prevOn = nextOn;
					++i;
					continue;
				}
				else if (!a.onCurve
					&& b.onCurve)
				{
					sampleQuad(prevOn, a.size, b.size);
					prevOn = b.size;
					continue;
				}
				else { Log::Print("Reached unexpected branch while flattening contour!"); }
			}
			
			//close ring by ensuring the start is not duplicated
			if (!out.empty()
				&& almost_eq(out.front(), out.back()))
			{
				out.pop_back();	
			}
			
			//clean up + enforce CCW
			cleanup_polygon(out);
			return out;
		};
		
		struct ContourInfo
		{
			vector<vec2> pts{}; //ccw for triangulation
			float area{};       //signed area before normalization (shoelace, CCW > 0)
			bool isHole{};      //set later from containment depth (odd depth = hole)
			int parent = -1;    //which outer this hole belongs to
		};
		
		for (auto& glyph : glyphs)
		{
			if (glyph.contours.contours.empty()) continue;
			
			glyph.vertices.clear();
			glyph.indices.clear();
			
			//flatten contours
			
			vector<ContourInfo> infos{};
			infos.reserve(glyph.contours.contours.size());
			
			for (const auto& contour : glyph.contours.contours)
			{
				auto poly = FlattenContour(contour);
				DEBUG_ASSERT(!poly.empty());
				DEBUG_ASSERT(poly.size() >= 3);
				if (poly.size() < 3) continue;
				
				float Ar = SignedArea(poly);
				DEBUG_ASSERT(isfinite(Ar));
				if (Ar < 0.0f)
				{
					reverse(poly.begin(), poly.end());
					Ar = -Ar;
				}
				
				ContourInfo ci{};
				ci.pts = move(poly);
				ci.area = Ar;
				ci.isHole = false;
				infos.push_back(move(ci));
			}
			
			//classify all contours purely by containment depth
			
			DEBUG_ASSERT(!infos.empty());
			for (size_t i = 0; i < infos.size(); ++i)
			{
				DEBUG_ASSERT(infos[i].pts.size() >= 3);
				DEBUG_ASSERT(isfinite(infos[i].area));
				
				infos[i].parent = -1;
				int bestParent = -1;
				float bestArea = FLT_MAX;
				
				for (size_t j = 0; j < infos.size(); ++j)
				{
					if (i == j) continue;
					if (infos[j].pts.size() < 3) continue;
					
					vec2 probe = polygon_centroid(infos[i].pts);
					if (PointInPolygon(probe, infos[j].pts))
					{
						float absArea = fabsf(infos[j].area);
						if (absArea < bestArea)
						{
							bestArea = absArea;
							bestParent = static_cast<int>(j);
						}
					}
				}
				
				infos[i].parent = bestParent;
			}
			
			for (size_t i = 0; i < infos.size(); ++i)
			{
				if (infos[i].parent != -1)
				{
					DEBUG_ASSERT(infos[i].parent < (int)infos.size());
					DEBUG_ASSERT(infos[i].parent != (int)i);
				}
			}
			
			for (size_t i = 0; i < infos.size(); ++i)
			{
				Log::Print(
					"Contour " + to_string(i) + "\n" +
					"  area:   " + to_string(infos[i].area) + "\n" +
					"  parent: " + to_string(infos[i].parent) + "\n" +
					"  type:   " + (infos[i].isHole ? "hole" : "outer"),
					"FONT",
					LogType::LOG_DEBUG);
			}
			
			//determine hole status based on nesting depth
			for (auto& ci : infos)
			{
				int depth{};
				int p = ci.parent;
				unordered_set<int> visited{};
				
				while (p != -1)
				{
					if (visited.count(p))
					{
						Log::Print(
							"Circular parent chain detected at contour '" + to_string(p) + "'!", 
							"FONT", 
							LogType::LOG_ERROR,
							2);
							
						p = -1;
						break;
					}
					visited.insert(p);
					
					++depth;
					
					DEBUG_ASSERT(depth < 32);
					
					if (p < 0
						|| p >= (int)infos.size())
					{
						Log::Print(
							"Invalid parent index '" + to_string(p) + "'!", 
							"FONT", 
							LogType::LOG_ERROR,
							2);
					}
					
					p = infos[p].parent;
				}
				ci.isHole = (depth % 2 == 1);
			}
			
			//triangulate each outer, discard triangles
			//whose centroid falls in any of its holes
			for (size_t oi = 0; oi < infos.size(); ++oi)
			{
				if (infos[oi].isHole) continue;
				auto& outer = infos[oi].pts;
				if (outer.size() < 3) continue;
				
				//triangulate the cleaned CCW outer
				vector<u32> tris = TriangulatePolygon(outer);
				
				DEBUG_ASSERT(!tris.empty() || outer.size() < 3);
				for (auto idx : tris)
				{
					DEBUG_ASSERT(idx < outer.size());
				}
				
				u32 vertexOffset = (u32)(glyph.vertices.size() / 2);
				
				DEBUG_ASSERT((glyph.vertices.size() % 2) == 0);
				DEBUG_ASSERT((glyph.indices.size() % 3) == 0);
				
				//emit vertices for this outer
				for (const auto& v : outer)
				{
					glyph.vertices.push_back(v.x);
					glyph.vertices.push_back(v.y);
				}
				
				//build list of holes belonging to this outer
				
				vector<const vector<vec2>*> holes{};
				holes.reserve(infos.size());
				for (auto& h : infos)
				{
					if (!h.isHole ||h.parent != (int)oi) continue;
					holes.push_back(&h.pts);
				}
				
				//keep triangles whose centroid is not inside any hole
				for (size_t t = 0; t + 2 < tris.size(); t += 3)
				{
					const vec2 a = outer[tris[t + 0]];
					const vec2 b = outer[tris[t + 1]];
					const vec2 c = outer[tris[t + 2]];
					const vec2 centroid = (a + b + c) * (1.0f / 3.0f);
					
					bool inHole{};
					for (auto hp : holes)
					{
						if (PointInPolygon(centroid, *hp))
						{
							inHole = true;
							break;
						}
					}
					if (inHole) continue;
					
					DEBUG_ASSERT(vertexOffset + tris[t + 0] < glyph.vertices.size() / 2);
					DEBUG_ASSERT(vertexOffset + tris[t + 1] < glyph.vertices.size() / 2);
					DEBUG_ASSERT(vertexOffset + tris[t + 2] < glyph.vertices.size() / 2);
					
					glyph.indices.push_back(vertexOffset + tris[t + 0]);
					glyph.indices.push_back(vertexOffset + tris[t + 1]);
					glyph.indices.push_back(vertexOffset + tris[t + 2]);
				}
			} 
		}

	return true;
}

bool ExportKFont(
	const path& targetPath,
	const vector<GlyphResult>& parsedData)
{
	if (parsedData.empty()) return false;

	vector<u8> data{};

	auto WriteU32 = [&](u32 v)
		{
			const u8* p = reinterpret_cast<const u8*>(&v);
			data.insert(data.end(), p, p + sizeof(u32));
		};
	auto WriteF32 = [&](f32 v)
		{
			const u8* p = reinterpret_cast<const u8*>(&v);
			data.insert(data.end(), p, p + sizeof(f32));
		};
	auto WriteStr = [&](const char* s, size_t len)
		{
			data.insert(data.end(), s, s + len);
		};

	//
	// HEADER
	//

	WriteStr("KFNT", 4);
	//version
	WriteU32(1);
	//glyph count
	WriteU32(static_cast<u32>(parsedData.size()));

	//
	// GLYPH BLOCKS
	//

	for (const auto& glyph : parsedData)
	{
		//tag
		WriteStr("GLYF", 4);

		//core
		WriteU32(glyph.glyphIndex);
		WriteF32(glyph.advanceWidth);
		WriteF32(glyph.leftSideBearing);

		//anchor
		WriteF32(glyph.anchor.x);
		WriteF32(glyph.anchor.y);

		//transform
		WriteF32(glyph.transform.m00);
		WriteF32(glyph.transform.m01);
		WriteF32(glyph.transform.m10);
		WriteF32(glyph.transform.m11);

		//vertices
		WriteStr("VERT", 4);
		u32 vertexCount = static_cast<u32>(glyph.vertices.size() / 2);
		WriteU32(vertexCount);
		for (u32 i = 0; i < vertexCount; ++i)
		{
			WriteF32(glyph.vertices[i * 2]);
			WriteF32(glyph.vertices[i * 2 + 1]);
		}

		//indices
		WriteStr("INDI", 4);
		u32 indiceCount = static_cast<u32>(glyph.indices.size());
		WriteU32(indiceCount);
		for (u32 i = 0; i < indiceCount; ++i)
		{
			WriteU32(glyph.indices[i]);
		}
	}

	//
	// WRITE TO FILE
	//

	string result = WriteBinaryLinesToFile(targetPath, data, false);
	if (!result.empty())
	{
		Log::Print(
			result,
			"EXPORT",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}

bool ParsePreCheck(
	const path& fontPath, 
	const path& targetPath)
{
	if (!exists(fontPath))
	{
		Log::Print(
			"Cannot parse to kfont because the font file '" + fontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (exists(targetPath))
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the kfont file already exists!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(fontPath))
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the font file is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(fontPath).has_extension()
		|| (path(fontPath).extension() != ".ttf"
		&& path(fontPath).extension() != ".otf"))
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the font file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(targetPath).has_extension()
		|| path(targetPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the kfont file does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	size_t offset{};
	vector<u8> versionData{};

	string result = ReadBinaryLinesFromFile(fontPath.string(), versionData, 0, 4);
	if (!result.empty())
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "'! Reason: " + result,
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	thisVersion = ReadU32(versionData, offset);

	if (path(fontPath).extension() == ".ttf"
		&& thisVersion != TTF_VERSION)
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the font file does not have a valid ttf/otf font version!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (path(fontPath).extension() == ".otf"
		&& thisVersion != TTF_VERSION
		&& thisVersion != OTF_VERSION)
	{
		Log::Print(
			"Cannot parse the font file '" + fontPath.string() + "' to kfont file '" + targetPath.string() + "' because the font file does not have a valid otf font version!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}