//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <string>
#include <filesystem>
#include <system_error>
#include <algorithm>

#include "KalaHeaders/log_utils.hpp"

#include "parser.hpp"
#include "core.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaFont::Core;

using std::vector;
using std::string;
using std::to_string;
using std::filesystem::exists;
using std::filesystem::is_regular_file;
using std::filesystem::current_path;
using std::filesystem::path;
using std::from_chars;
using std::errc;
using std::stoi;
using std::clamp;

static bool IsInteger(const string& s)
{
	int value{};
	auto [ptr, ec] = from_chars(s.data(), s.data() + s.size(), value);
	return ec == errc{}
	&& ptr == s.data() + s.size();
}

static bool ParsePreCheck(const vector<string>& params);

static bool GetPreCheck(const vector<string>& params);

namespace KalaFont
{
	void Parser::ParseFont(const vector<string>& params)
	{
		if (!ParsePreCheck(params)) return;

		int val = stoi(params[3]);
		if (val < 1
			|| val > 255)
		{
			val = clamp(val, 1, 255);
			string clampedVal = to_string(val);

			Log::Print(
				"Font size '" + params[3] + "' was out of range! It was clamped to a safe value '" + clampedVal + "'.",
				"PARSE",
				LogType::LOG_WARNING);
		}
	}

	void Parser::GetKFontInfo(const vector<string>& params)
	{
		if (!GetPreCheck(params)) return;
	}
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
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (exists(correctKFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + correctKFontPath.string() + "' already exists!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (!IsInteger(params[3]))
	{
		Log::Print(
			"Cannot parse to kfont because font size '" + params[3] + "' is not an integer!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(correctFontPath))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' is not a regular file!",
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
			"Cannot parse to kfont because font origin path '" + correctFontPath.string() + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(correctKFontPath).has_extension()
		|| path(correctKFontPath).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + correctKFontPath.string() + "' does not have a valid extension!",
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