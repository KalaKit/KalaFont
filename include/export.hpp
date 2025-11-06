//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace KalaFont
{
	using std::vector;
	using std::string;
	using std::filesystem::path;
	
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using i16 = int16_t;
	
	//At the top of the kgm binary
	struct TopHeader
	{
		char magic[4] = { 'K', 'T', 'F', '\0' };
		u8 version = 1;
		u8 type;            //1 = bitmap, 2 = glyph
		u16 glyphHeight;    //height of all glyphs in pixels
		u32 glyphCount;     //number of glyphs
		u32 glyphTableSize; //glyph search table size in bytes
		u32 payloadSize;    //glyph payload block size in bytes
	};

	//Helps find glyphs fast
	struct GlyphTable
	{
		u32 charCode;    //unicode codepoint
		u32 blockOffset; //absolute offset from start of file
		u32 blockSize;   //size of the glyph block (info + payload)
	};
		
	//Info + payload of each glyph
	struct GlyphBlock
	{
		u32 charCode;           //unicode codepoint
		u16 width;              //bmp.width
		u16 height;             //bmp.rows
		i16 pitch;              //bmp.pitch (can be negative for some formats)
		i16 bearingX;           //slot->bitmap_left
		i16 bearingY;           //slot->bitmap_top
		u16 advance;            //slot->advance.x >> 6
		u32 glyphSize;          //size of this glyph's bitmap
		vector<u8> glyphPixels; //pixels of this glyph bitmap
	};
	
	class Export
	{
	public:
		//Write the ktf data to the target path as a new binary
		static void ExportKTF(
			const path& targetPath,
			u8 type,
			u8 glyphHeight,
			u8 superSampleMultiplier,
			vector<GlyphBlock>& glyphBlocks);
	};
}