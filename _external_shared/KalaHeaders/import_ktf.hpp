//------------------------------------------------------------------------------
// import_ktf.hpp
//
// Copyright (C) 2025 Lost Empire Entertainment
//
// This is free source code, and you are welcome to redistribute it under certain conditions.
// Read LICENSE.md for more information.
//
// Provides:
//   - Helpers for streaming individual font glyphs or loading the full kalafont type binary into memory
//------------------------------------------------------------------------------

/*------------------------------------------------------------------------------

# KTF binary top header for glyph export-import

Note: Always 34 bytes

Offset | Size | Field
-------|------|--------------------------------------------
0      | 4    | KTF magic word, always 'K', 'T', 'F', '\0' aka '0x0046544B'
4      | 1    | version, always '1'
5      | 1    | type, '1' for bitmap, '2' for glyph
6      | 2    | height of all glyphs in pixels
8      | 4    | number of glyphs, max is 1024 glyphs
12     | 1    | first indice, always '0'
13     | 1    | second indice, always '1'
14     | 1    | third indice, always '2'
15     | 1    | fourth indice, always '2'
16     | 1    | fifth indice, always '3'
17     | 1    | sixth indice, always '0'
18     | 2    | top-left uv position (x, y)
20     | 2    | top-right uv position (x, y)
22     | 2    | bottom-right uv position (x, y)
24     | 2    | bottom-left uv position (x, y)
26     | 4    | glyph table size in bytes
30     | 4    | glyph block size in bytes, max is 1024MB

# KTF binary glyph table for glyph export-import

Note: Always 12 bytes

Offset | Size | Field
-------|------|--------------------------------------------
??     | 4    | character code in unicode
??+4   | 4    | absolutre offset from start of file relative to its glyph block start
??+8   | 4    | size of the glyh block (info + payload)

# KTF binary glyph block for glyph export-import

Note: Always atleast 25 bytes, bearings and vertices can be negative

Offset | Size | Field
-------|------|--------------------------------------------
??     | 4    | character code in unicode
??+4   | 2    | width
??+6   | 2    | height
??+8   | 2    | left bearing (X)
??+10  | 2    | top bearing (Y)
??+12  | 2    | advance

??+14  | 2    | top-left vertice position (x, y)
??+16  | 2    | top-right vertice position (x, y)
??+18  | 2    | bottom-right vertice position (x, y)
??+20  | 2    | bottom-left vertice position (x, y)

??+22  | 4    | raw pixels size
??+24  | 1    | each raw pixel value
...

------------------------------------------------------------------------------*/

#pragma once

#include <vector>
#include <array>

namespace KalaHeaders
{
	using std::vector;
	using std::array;
	
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using i8 = int8_t;
	using i16 = int16_t;
	
	//The true top header size that is always required
	constexpr u8 CORRECT_TOP_HEADER_SIZE = 26u;
	
	//The true per-glyph table size that is always required
	constexpr u8 CORRECT_GLYPH_TABLE_SIZE = 12u;
	
	//The offset where pixel data must always start relative to each glyph block
	constexpr u8 RAW_PIXEL_DATA_OFFSET = 24u;
	
	//At the top of the ktf binary
	struct TopHeader
	{
		u32 magic = 0x0046544B; //'K', 'T', 'F', '\0'
		u8 version = 1;         //version of this ktf binary
		u8 type;                //1 = bitmap, 2 = glyph
		u16 glyphHeight;        //height of all glyphs in pixels
		u32 glyphCount;         //number of glyphs
		array<u8, 6> indices = { 0, 1, 2, 2, 3, 0 };
		//uv of this glyph
		array<array<u8, 2>, 4> uvs = 
		{{
			{ 0,   255 },
			{ 255, 0   },
			{ 255, 255 },
			{ 0,   255 }
		}};       
		u32 glyphTableSize;     //glyph search table size in bytes
		u32 glyphBlockSize;     //glyph payload block size in bytes
	};

	//Helps find glyphs fast
	struct GlyphTable
	{
		u32 charCode;    //glyph character code in unicode
		u32 blockOffset; //absolute offset from start of file
		u32 blockSize;   //size of the glyph block (info + payload)
	};
		
	//Info + payload of each glyph
	struct GlyphBlock
	{
		u32 charCode;                    //glyph character code in unicode
		u16 width;                       //glyph width
		u16 height;                      //glyph height
		i16 bearingX;                    //glyph left bearing
		i16 bearingY;                    //glyph top bearing
		u16 advance;                     //glyph advance
		array<array<i8, 2>, 4> vertices; //vertices of this glyph, can be negative
		u32 rawPixelSize;                //size of this glyph's pixels
		vector<u8> rawPixels;            //8-bit raw pixels of this glyph (0 - 255, 0 is transparent, 255 is white)
	};
}