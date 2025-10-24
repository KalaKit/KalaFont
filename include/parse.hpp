//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "KalaHeaders/core_utils.hpp"

namespace KalaFont
{
	using std::vector;
	using std::string;

	struct TableRecord
	{
		char tag[4]{};
		u32 checkSum{};
		u32 offset{};
		u32 length{};
	};

	struct OffsetTable
	{
		u32 scalerType{};
		u16 numTables{};
		u16 searchRange{};
		u16 entrySelector{};
		u16 rangeShift{};
		vector<TableRecord> tables{};
	};

	struct HeadTable
	{
		i16 majorVersion{};
		i16 minorVersion{};
		f32 fontRevision{}; //fixed 16.16 value
		u32 checkSumAdjustment{};
		u32 magicNumber{};
		u16 flags{};
		u16 unitsPerEm{};
		i64 created{}; //longDateTime (64-bit)
		i64 modified{}; //longDateTime (64-bit)
		i16 xMin{};
		i16 yMin{};
		i16 xMax{};
		i16 yMax{};
		u16 macStyle{};
		u16 lowestRecPPEM{};
		i16 fontDirectionHint{};
		i16 indexToLocFormat{}; //0 = short, 1 = long
		i16 glyphDataFormat{};
	};

	struct MaxpTable
	{
		u32 version{};
		u16 numGlyphs{};
	};

	class Parse
	{
	public:
		static u8 ReadU8(
			const vector<u8>& data,
			size_t offset)
		{
			return data[offset];
		}
		static u16 ReadU16(
			const vector<u8>& data,
			size_t offset)
		{
			return (data[offset] << 8)
				| data[offset + 1];
		}
		static u32 ReadU32(
			const vector<u8>& data,
			size_t offset)
		{
			return (data[offset] << 24)
				| (data[offset + 1] << 16)
				| (data[offset + 2] << 8)
				| (data[offset + 3]);
		}

		//Parses an otf/ttf font into a kfont file
		static void ParseFont(const vector<string>& params);

		//Parses an otf/ttf font into a kfont file with detailed logs
		static void VerboseParseFont(const vector<string>& params);

		//Displays info about a parsed kfont file
		static void GetKFontInfo(const vector<string>& params);
	};
}