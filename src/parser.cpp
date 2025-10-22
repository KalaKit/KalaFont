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

using KalaHeaders::Log;
using KalaHeaders::LogType;

using std::vector;
using std::string;
using std::to_string;
using std::filesystem::exists;
using std::filesystem::is_regular_file;
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
	if (!exists(params[1]))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + params[1] + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}
	if (exists(params[2]))
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + params[1] + "' already exists!",
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

	if (!is_regular_file(params[1]))
	{
		Log::Print(
			"Cannot parse to kfont because font origin path '" + params[1] + "' is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(params[1]).has_extension()
		|| path(params[1]).extension() != ".ttf"
		|| path(params[1]).extension() != ".otf")
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + params[2] + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (path(params[2]).extension() != ".kfont")
	{
		Log::Print(
			"Cannot parse to kfont because kfont target path '" + params[1] + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}

bool GetPreCheck(const vector<string>& params)
{
	if (!exists(params[1]))
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + params[1] + "' does not exist!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!is_regular_file(params[1]))
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + params[1] + "' is not a regular file!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (!path(params[1]).has_extension()
		|| path(params[1]).extension() != ".kfont")
	{
		Log::Print(
			"Cannot get kfont info because kfont target path '" + params[1] + "' does not have a valid extension!",
			"PARSE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}