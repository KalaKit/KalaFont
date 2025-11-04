//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"

#include "parse.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::HasAnyNonNumber;
using KalaHeaders::HasAnyWhiteSpace;

using KalaFont::Core;

using std::vector;
using std::string;
using std::filesystem::current_path;
using std::filesystem::path;
using std::filesystem::weakly_canonical;
using std::filesystem::is_regular_file;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

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
	
	Log::Print(
		"Starting to compile font '" + correctOrigin.string() + "' to target path '" + correctTarget.string() + "'",
		"COMPILE_FONT",
		LogType::LOG_INFO);
}