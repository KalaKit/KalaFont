//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"

#include "command.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::ContainsString;
using KalaHeaders::RemoveAllFromString;

namespace KalaFont
{
	bool CommandManager::ParseCommand(const vector<string>& params)
	{
		if (params.empty()) return false;

		if (!COMMAND_PREFIX.empty()
			&& !ContainsString(params[0], COMMAND_PREFIX.data()))
		{
			Log::Print(
				"Target command '" + params[0] + "' is missing required prefix '" + COMMAND_PREFIX.data() + "'!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

				return false;
		}

		vector<string> cleanedParams = params;

		Command foundCommand{};

		if (!COMMAND_PREFIX.empty()) cleanedParams[0] = RemoveAllFromString(cleanedParams[0], COMMAND_PREFIX.data());

		for (const auto& c : commands)
		{
			if (find(c.primary.begin(), c.primary.end(), cleanedParams[0]) != c.primary.end())
			{
				if (cleanedParams.size() == c.paramCount)
				{
					foundCommand = c;
					break;
				}
				
				Log::Print(
					"Failed to run command '" + cleanedParams[0] + "'! Incorrect amount of parameters were passed for the command.",
					"PARSE",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		if (foundCommand.primary.empty())
		{
			Log::Print(
				"Failed to run command '" + cleanedParams[0] + "'! The command does not exist.",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (foundCommand.paramCount == 0)
		{
			Log::Print(
				"Target command '" + cleanedParams[0] + "' has an invalid param count!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!foundCommand.targetFunction)
		{
			Log::Print(
				"Target command '" + cleanedParams[0] + "' has no attached function!",
				"PARSE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		foundCommand.targetFunction(cleanedParams);

		return true;
	}

	bool CommandManager::AddCommand(Command newValue)
	{
		//skip empty commands
		if (newValue.primary.size() == 0
			|| newValue.paramCount == 0
			|| !newValue.targetFunction)
		{
			Log::Print(
				"Failed to add a command because it has no primary parameter, parameter count or target function!",
				"COMMAND",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//skip existing primary variants
		for (const auto& c : commands)
		{
			for (const auto& p : newValue.primary)
			{
				if (find(c.primary.begin(), c.primary.end(), p) != c.primary.end())
				{
					Log::Print(
						"Failed to add a command because its primary parameter '" + p + "' is already in use by another command!",
						"COMMAND",
						LogType::LOG_ERROR,
						2);

					return false;
				}
			}
		}

		commands.push_back(newValue);

		return true;
	}
}