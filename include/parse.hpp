//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

namespace KalaFont
{
	using std::vector;
	using std::string;
	
	class Parse
	{
	public:
		//Compiles ttf and otf fonts to ktf for runtime use with the help of FreeType.
		static void Command_Parse(const vector<string>& params);
		
		//Compiles ttf and otf fonts to ktf for runtime use
		//with the help of FreeType with additional verbose logging.
		static void Command_VerboseParse(const vector<string>& params);
	};
}