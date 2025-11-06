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

#include "parse.hpp"
#include "core.hpp"
#include "export.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::HasAnyNonNumber;
using KalaHeaders::HasAnyWhiteSpace;

using KalaFont::Core;
using KalaFont::Export;
using KalaFont::GlyphBlock;

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

constexpr u32 MAX_SIZE_BYTES = 1073741824; //1024 MB
constexpr u16 MAX_GLYPH_COUNT = 1024;
constexpr u8 MIN_GLYPH_HEIGHT = 12;        //pixels
constexpr u8 MAX_GLYPH_HEIGHT = 255;       //pixels

constexpr u8 MIN_SUPERSAMPLE = 1;          //multiplier
constexpr u8 MAX_SUPERSAMPLE = 3;          //multiplier

static void ParseAny(
	const vector<string>& params,
	bool isVerbose);

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
		Log::Print(
			"Failed to initialize FreeType!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	Log::Print(
		"Initialized FreeType.",
		"COMPILE_FONT",
		LogType::LOG_DEBUG);
	
	if (Core::currentDir.empty()) Core::currentDir = current_path().string();
	path correctOrigin = weakly_canonical(path(Core::currentDir) / params[4]);
	path correctTarget = weakly_canonical(path(Core::currentDir) / params[5]);
	
	if (params[1] != "bitmap"
		&& params[1] != "glyph")
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because the compilation action was invalid!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	if (HasAnyNonNumber(params[2])
		|| HasAnyWhiteSpace(params[2]))
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because the glyph height was an invalid value!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	size_t glyphHeight = stoul(params[2]);
	if (glyphHeight < MIN_GLYPH_HEIGHT
		|| glyphHeight > MAX_GLYPH_HEIGHT)
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because the glyph height was out of allowed range!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	if (HasAnyNonNumber(params[3])
		|| HasAnyWhiteSpace(params[3]))
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because the supersample multiplier was an invalid value!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	size_t supersampleMultiplier = stoul(params[3]);
	if (supersampleMultiplier < MIN_SUPERSAMPLE
		|| supersampleMultiplier > MAX_SUPERSAMPLE)
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because the supersample multiplier was out of allowed range!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	if (!exists(correctOrigin))
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because it does not exist!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	if (!is_regular_file(correctOrigin)
		|| !correctOrigin.has_extension()
		|| (correctOrigin.extension() != ".ttf"
		&& correctOrigin.extension() != ".otf"))
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' because its extension is not valid!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
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
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "' because of insufficient read permissions for origin path!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	if (exists(correctTarget))
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "' because the target path already exists!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	if (!correctTarget.has_extension()
		|| correctTarget.extension() != ".ktf")
	{
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "' because the target does not have a valid extension!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
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
		Log::Print(
			"Failed to parse font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "' because of insufficient write permissions to target directory!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
		return;
	}
	
	Log::Print(
		"Starting to compile font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "'",
		"COMPILE_FONT",
		LogType::LOG_DEBUG);
		
	FT_Face face{};
	if (FT_New_Face(ft, correctOrigin.string().c_str(), 0, &face))
	{
		Log::Print(
			"FreeType failed to set new face for font '" + correctOrigin.string() + "'!",
			"COMPILE_FONT",
			LogType::LOG_ERROR,
			2);
		
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
				.charCode = charCode,
				.width = static_cast<u16>(bmp.width),
				.height = static_cast<u16>(bmp.rows),
				.pitch = static_cast<i16>(bmp.pitch),
				.bearingX = static_cast<i16>(slot->bitmap_left),
				.bearingY = static_cast<i16>(slot->bitmap_top),
				.advance = static_cast<u16>((slot->advance.x >> 6))
			};
			
			glyphBlock.glyphPixels.assign(
				bmp.buffer,
				bmp.buffer + (glyphBlock.height * abs(glyphBlock.pitch)));
				
			glyphBlock.glyphSize = static_cast<u32>(glyphBlock.glyphPixels.size());
			
			glyphs.push_back(move(glyphBlock));
			
			if (isVerbose)
			{
				oss.str(""); 
				oss.clear();
			
				oss << "Glyph info for 'U+" << hex << glyphBlock.charCode << dec << "'\n"
					<< "  width:    " << glyphBlock.width << "\n"
					<< "  height:   " << glyphBlock.height << "\n"
					<< "  pitch:    " << glyphBlock.pitch << "\n"
					<< "  bearingX: " << glyphBlock.bearingX << "\n"
					<< "  bearingY: " << glyphBlock.bearingY << "\n"
					<< "  advance:  " << glyphBlock.advance << "\n"
					<< "  size:     " << glyphBlock.glyphSize << "\n\n";
					
				oss << "Glyph bitmap for 'U+" << hex << glyphBlock.charCode << dec << "'\n\n";
				
				for (int y = 0; y < glyphBlock.height; ++y)
				{
					for (int x = 0; x < glyphBlock.width; ++x)
					{
						oss << ((bmp.buffer[y * abs(glyphBlock.pitch) + x] > 128) ? '#' : ' ');
					}
					oss << '\n';
				}
				
				oss << "--------------------\n";
					
				Log::Print(oss.str());
			}
		}
		else
		{
			Log::Print(
				"FreeType failed to load glyph '" + string(1, static_cast<char>(charCode)) + "'!",
				"COMPILE_FONT",
				LogType::LOG_ERROR,
				2);
		}
		
		charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
	}
	
	u8 type = params[1] == "bitmap" ? 1 : 2;
	
	Log::Print(
		"Finished compiling font!",
		"COMPILE_FONT",
		LogType::LOG_SUCCESS);
	
	Export::ExportKTF(
		correctTarget,
		type,
		static_cast<u8>(glyphHeight),
		static_cast<u8>(supersampleMultiplier),
		glyphs);
	
	FT_Done_Face(face);
	FT_Done_FreeType(ft);
}