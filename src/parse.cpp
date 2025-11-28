//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>
#include <sstream>

#include "FreeType/include/ft2build.h"
#include FT_FREETYPE_H

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"
#include "KalaHeaders/import_ktf.hpp"

#include "KalaCLI/include/core.hpp"

#include "parse.hpp"
#include "export.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::HasAnyNonNumber;
using KalaHeaders::HasAnyWhiteSpace;
using KalaHeaders::GlyphBlock;
using KalaHeaders::MIN_GLYPH_HEIGHT;
using KalaHeaders::MAX_GLYPH_HEIGHT;

using KalaCLI::Core;

using KalaFont::Export;

using std::vector;
using std::string;
using std::to_string;
using std::filesystem::current_path;
using std::filesystem::path;
using std::filesystem::weakly_canonical;
using std::filesystem::is_regular_file;
using std::filesystem::status;
using std::filesystem::perms;
using std::ostringstream;
using std::hex;
using std::dec;
using std::move;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i16 = int16_t;

constexpr u8 MIN_SUPERSAMPLE = 1;    //multiplier
constexpr u8 MAX_SUPERSAMPLE = 3;    //multiplier

static void ParseAny(
	const vector<string>& params,
	bool isVerbose);
	
static void PrintError(const string& message)
{
	Log::Print(
		message,
		"FONT",
		LogType::LOG_ERROR,
		2);
}

namespace KalaFont
{
	void Parse::Command_Parse(const vector<string>& params)
	{
		ParseAny(params, false);
	}
	
	void Parse::Command_VerboseParse(const vector<string>& params)
	{
		ParseAny(params, true);
	}
}

void ParseAny(
	const vector<string>& params,
	bool isVerbose)
{
	FT_Library ft{};
	if (FT_Init_FreeType(&ft))
	{
		PrintError("Failed to initialize FreeType!");
		
		return;
	}
	
	Log::Print(
		"Initialized FreeType.",
		"FONT",
		LogType::LOG_DEBUG);
	
	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctOrigin = weakly_canonical(path(Core::currentDir) / params[4]);
	path correctTarget = weakly_canonical(path(Core::currentDir) / params[5]);
	
	//
	// VERIFY PARAMS
	//
	
	if (params[1] != "bitmap"
		&& params[1] != "glyph")
	{
		PrintError("Failed to load font '" + correctOrigin.string() + "' because the load action was invalid!");
		
		return;
	}
	
	if (HasAnyNonNumber(params[2])
		|| HasAnyWhiteSpace(params[2]))
	{
		PrintError("Failed to load font '" + correctOrigin.string() + "' because the glyph height was an invalid value!");
		
		return;
	}
	size_t glyphHeight = stoul(params[2]);
	if (glyphHeight < MIN_GLYPH_HEIGHT
		|| glyphHeight > MAX_GLYPH_HEIGHT)
	{
		PrintError("Failed to load font '" + correctOrigin.string() + "' because the glyph height was out of allowed range!");
		
		return;
	}
	
	if (HasAnyNonNumber(params[3])
		|| HasAnyWhiteSpace(params[3]))
	{
		PrintError("Failed to load font '" + correctOrigin.string() + "' because the supersample multiplier was an invalid value!");
		
		return;
	}
	size_t supersampleMultiplier = stoul(params[3]);
	if (supersampleMultiplier < MIN_SUPERSAMPLE
		|| supersampleMultiplier > MAX_SUPERSAMPLE)
	{
		PrintError("Failed to load font '" + correctOrigin.string() + "' because the supersample multiplier was out of allowed range!");
		
		return;
	}
	
	//
	// VERIFY ORIGIN
	//
	
	if (!exists(correctOrigin))
	{
		PrintError("Failed to load font because input path '" + correctOrigin.string() + "' does not exist!");
		
		return;
	}
	
	if (!is_regular_file(correctOrigin)
		|| !correctOrigin.has_extension())
	{
		PrintError("Failed to load font because input path '" + correctOrigin.string() + "' is not a regular file!");
		
		return;
	}
	
	if (correctOrigin.extension() != ".ttf"
		&& correctOrigin.extension() != ".otf")
	{
		PrintError("Failed to load font because input path '" + correctOrigin.string() + "' extension '" + correctOrigin.extension().string() + "' is not allowed!");
		
		return;
	}
	
	auto fileStatusOrigin = status(correctOrigin);
	auto filePermsOrigin = fileStatusOrigin.permissions();
		
	bool canReadOrigin = (filePermsOrigin & (
		perms::owner_read
		| perms::group_read
		| perms::others_read))  
		!= perms::none;
		
	if (!canReadOrigin)
	{
		PrintError("Failed to load font because you have insufficient read permissions for input path '" + correctOrigin.string() + "'!");
		
		return;
	}
	
	//
	// VERIFY TARGET
	//
	
	if (exists(correctTarget))
	{
		PrintError("Failed to load font because output path '" + correctTarget.string() + "' already exists!");
		
		return;
	}
	if (!correctTarget.has_extension()
		|| correctTarget.extension() != ".ktf")
	{
		PrintError("Failed to load font because output path '" + correctTarget.string() + "' extension '" + correctTarget.extension().string() + "' is not allowed!");
		
		return;
	}
	
	auto fileStatusTarget = status(correctTarget.parent_path());
	auto filePermsTarget = fileStatusTarget.permissions();
		
	bool canWriteTarget = (filePermsTarget & (
		perms::owner_write
		| perms::group_write
		| perms::others_write))  
		!= perms::none;
			
	if (!canWriteTarget)
	{
		PrintError("Failed to load font because you have insufficient write permissions for output parent path '" + correctTarget.string() + "'!");
		
		return;
	}
	
	//
	// LOAD FONT
	//
	
	Log::Print(
		"Starting to load font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "'",
		"FONT",
		LogType::LOG_DEBUG);
		
	FT_Face face{};
	if (FT_New_Face(ft, correctOrigin.string().c_str(), 0, &face))
	{
		PrintError("FreeType failed to set new face for font '" + correctOrigin.string() + "'!");
		
		return;
	}
	
	FT_Set_Pixel_Sizes(face, 0, glyphHeight);
	
	FT_ULong charCode{};
	FT_UInt glyphIndex{};
	
	vector<GlyphBlock> glyphs{};
	
	ostringstream oss{};
	
	charCode = FT_Get_First_Char(face, &glyphIndex);
	while (glyphIndex != 0)
	{
		if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) == 0
			&& FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0)
		{
			FT_GlyphSlot slot = face->glyph;
			FT_Bitmap& bmp = slot->bitmap;
			
			GlyphBlock glyphBlock = 
			{
				.charCode = static_cast<u32>(charCode),
				.width = static_cast<u16>(bmp.width),
				.height = static_cast<u16>(bmp.rows),
				.bearingX = static_cast<i16>(slot->bitmap_left),
				.bearingY = static_cast<i16>(slot->bitmap_top),
				.advance = static_cast<u16>((slot->advance.x >> 6))
			};
			
			glyphBlock.rawPixels.assign(
				bmp.buffer,
				bmp.buffer + (bmp.rows * abs(bmp.pitch)));
				
			glyphBlock.rawPixelSize = static_cast<u32>(glyphBlock.rawPixels.size());
			
			glyphs.push_back(move(glyphBlock));
			
			if (isVerbose)
			{
				oss.str(""); 
				oss.clear();
			
				oss << "Glyph info for 'U+" << hex << glyphBlock.charCode << dec << "'\n"
					<< "  width:    " << glyphBlock.width << "\n"
					<< "  height:   " << glyphBlock.height << "\n"
					<< "  bearingX: " << glyphBlock.bearingX << "\n"
					<< "  bearingY: " << glyphBlock.bearingY << "\n"
					<< "  advance:  " << glyphBlock.advance << "\n"
					<< "  size:     " << glyphBlock.rawPixelSize << "\n\n";
					
				oss << "Glyph bitmap for 'U+" << hex << glyphBlock.charCode << dec << "'\n\n";
				
				for (int y = 0; y < glyphBlock.height; ++y)
				{
					for (int x = 0; x < glyphBlock.width; ++x)
					{
						oss << ((bmp.buffer[y * abs(bmp.pitch) + x] > 128) ? '#' : ' ');
					}
					oss << '\n';
				}
				
				oss << "--------------------\n";
					
				Log::Print(oss.str());
			}
		}
		else
		{
			PrintError("FreeType failed to load glyph '" + string(1, static_cast<char>(charCode)) + "'!");
		}
		
		charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
	}
	
	u8 type = params[1] == "bitmap" ? 1 : 2;
	
	Log::Print(
		"Finished loading font!",
		"FONT",
		LogType::LOG_SUCCESS);
	
	if (type == 1)
	{
		Export::ExportBitmap(
			correctTarget,
			type,
			static_cast<u8>(glyphHeight),
			static_cast<u8>(supersampleMultiplier),
			glyphs);	
	}
	else
	{
		Export::ExportGlyph(
			correctTarget,
			type,
			static_cast<u8>(glyphHeight),
			static_cast<u8>(supersampleMultiplier),
			glyphs);	
	}
	
	FT_Done_Face(face);
	FT_Done_FreeType(ft);
}