//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <sstream>

#include "KalaCLI/include/core.hpp"
#include "KalaCLI/include/command.hpp"

#include "parse.hpp"

using KalaCLI::Core;
using KalaCLI::Command;
using KalaCLI::CommandManager;

using KalaFont::Parse;

using std::ostringstream;

static void AddExternalCommands()
{
	ostringstream msgParse{};
	
	msgParse << "Compiles ttf and otf fonts to ktf for runtime use with the help of FreeType.\n"
		<< "    Second parameter must be compile type (bitmap or glyph)\n"
		<< "    Third parameter must be glyph height - how tall each glyph will be, their width is adjusted according to height\n"
		<< "    Fourth parameter must be compression quality (1 to 3, higher is better quality but bigger size)\n"
		<< "    Fifth parameter must be origin font path (.ttf or .otf)\n"
		<< "    Sixth parameter must be target path (.ktf)";
	
	ostringstream msgVerboseParse{};
	
	msgVerboseParse << "Compiles ttf and otf fonts to ktf for runtime use with the help of FreeType with additional verbose logging.\n"
		<< "    Second parameter must be compile type (bitmap or glyph)\n"
		<< "    Third parameter must be glyph height - how tall each glyph will be, their width is adjusted according to height\n"
		<< "    Fourth parameter must be compression quality (1 to 3, higher is better quality but bigger size)\n"
		<< "    Fifth parameter must be origin font path (.ttf or .otf)\n"
		<< "    Sixth parameter must be target path (.ktf)";
	
	Command cmd_parse
	{
		.primary = { "parse", "p" },
		.description = msgParse.str(),
		.paramCount = 6,
		.targetFunction = Parse::Command_Parse
	};
	Command cmd_verboseparse
	{
		.primary = { "vp" },
		.description = msgVerboseParse.str(),
		.paramCount = 6,
		.targetFunction = Parse::Command_VerboseParse
	};

	CommandManager::AddCommand(cmd_parse);
	CommandManager::AddCommand(cmd_verboseparse);
}

int main(int argc, char* argv[])
{
	Core::Run(argc, argv, AddExternalCommands);

	return 0;
}