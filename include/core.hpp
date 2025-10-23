//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

namespace KalaFont
{
	using std::string;

	class Core
	{
	public:
		static inline string currentDir{};

		static void Run(int argc, char* argv[]);
	};
}