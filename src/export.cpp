//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <fstream>
#include <string>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"
#include "KalaHeaders/import_kfd.hpp"

#include "export.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaFile::WriteU8;
using KalaHeaders::KalaFile::WriteU16;
using KalaHeaders::KalaFile::WriteU32;
using KalaHeaders::KalaFile::WriteI8;
using KalaHeaders::KalaFile::WriteI16;
using KalaHeaders::KalaFile::WriteI32;
using KalaHeaders::KalaFontData::GlyphHeader;
using KalaHeaders::KalaFontData::CORRECT_GLYPH_HEADER_SIZE;
using KalaHeaders::KalaFontData::CORRECT_GLYPH_TABLE_SIZE;
using KalaHeaders::KalaFontData::RAW_PIXEL_DATA_OFFSET;
using KalaHeaders::KalaFontData::MAX_GLYPH_COUNT;
using KalaHeaders::KalaFontData::MAX_GLYPH_TABLE_SIZE;

using std::ofstream;
using std::ios;
using std::string;
using std::to_string;

using i8 = int8_t;
using i16 = int16_t;
using u32 = uint32_t;

static void PrintError(const string& message, bool isBitMap)
{
	string type = isBitMap ? "EXPORT_BITMAP" : "EXPORT_GLYPH";
	
	Log::Print(
		message,
		type,
		LogType::LOG_ERROR,
		2);
}

namespace KalaFont
{
	void Export::ExportBitmap(
		const path& targetPath,
		u8 type,
		u8 glyphHeight,
		u8 superSampleMultiplier,
		vector<GlyphBlock>& glyphBlocks)
	{
		if (glyphBlocks.size() > MAX_GLYPH_COUNT)
		{
			PrintError(
				"Failed to export because glyph count exceeded max allowed count '" + to_string(MAX_GLYPH_COUNT) + "'!",
				true);
		
			return;
		}
		
		if (CORRECT_GLYPH_TABLE_SIZE * glyphBlocks.size() > MAX_GLYPH_TABLE_SIZE)
		{
			PrintError(
				"Failed to export because glyph data size exceeded max allowed size '" + to_string(MAX_GLYPH_TABLE_SIZE) + "'!",
				true);
		
			return;
		}
		
		Log::Print(
			"Starting to export bitmap to path '" + targetPath.string() + "'.",
			"EXPORT_BITMAP",
			LogType::LOG_DEBUG);
			
		Log::Print(
			"Finished exporting bitmap!",
			"EXPORT_BITMAP",
			LogType::LOG_SUCCESS);
	}
	
	void Export::ExportGlyph(
		const path& targetPath,
		u8 type,
		u8 glyphHeight,
		u8 superSampleMultiplier,
		vector<GlyphBlock>& glyphBlocks)
	{
		if (glyphBlocks.size() > MAX_GLYPH_COUNT)
		{
			Log::Print(
				"Failed to export because glyph count exceeded max allowed count '" + to_string(MAX_GLYPH_COUNT) + "'!",
				"EXPORT_GLYPH",
				LogType::LOG_ERROR,
				2);
		
			return;
		}
		
		if (CORRECT_GLYPH_TABLE_SIZE * glyphBlocks.size() > MAX_GLYPH_TABLE_SIZE)
		{
			PrintError(
				"Failed to export because glyph data size exceeded max allowed size '" + to_string(MAX_GLYPH_TABLE_SIZE) + "'!",
				false);
		
			return;
		}
		
		Log::Print(
			"Starting to export glyphs to path '" + targetPath.string() + "'.",
			"EXPORT_GLYPH",
			LogType::LOG_DEBUG);
			
		vector<u8> output{};
		vector<u8> glyphTableOutput{};
		vector<u8> glyphBlockOutput{};
		
		//
		// FIRST STORE THE TOP HEADER
		//
			
		GlyphHeader glyphHeader{};
		
		u32 offset{};
		
		output.reserve(CORRECT_GLYPH_HEADER_SIZE);
		
		WriteU32(output, offset, glyphHeader.magic);  offset += 4;
		WriteU8(output, offset, glyphHeader.version); offset++;
		WriteU8(output, offset, type);                offset++;
		WriteU16(output, offset, glyphHeight);        offset += 2;
		WriteU32(output, offset, glyphBlocks.size()); offset += 4;
		
		//indices
		
		WriteU8(output, offset, glyphHeader.indices[0]); offset++;
		WriteU8(output, offset, glyphHeader.indices[1]); offset++;
		WriteU8(output, offset, glyphHeader.indices[2]); offset++;
		WriteU8(output, offset, glyphHeader.indices[3]); offset++;
		WriteU8(output, offset, glyphHeader.indices[4]); offset++;
		WriteU8(output, offset, glyphHeader.indices[5]); offset++;
		
		//uvs
		
		WriteU8(output, offset, glyphHeader.uvs[0][0]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[0][1]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[1][0]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[1][1]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[2][0]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[2][1]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[3][0]); offset++;
		WriteU8(output, offset, glyphHeader.uvs[3][1]); offset++;
		
		//
		// THEN STORE THE GLYPH TABLES
		//
		
		size_t totalGTBytes = CORRECT_GLYPH_TABLE_SIZE * glyphBlocks.size();
		glyphTableOutput.reserve(totalGTBytes);
		
		u32 baseOffset = CORRECT_GLYPH_HEADER_SIZE + totalGTBytes;
		u32 tableOffset{};
		
		for (const auto& g : glyphBlocks)
		{
			u32 blockSize = RAW_PIXEL_DATA_OFFSET + g.rawPixels.size();
			
			WriteU32(glyphTableOutput, tableOffset + 0, g.charCode);
			WriteU32(glyphTableOutput, tableOffset + 4, baseOffset);
			WriteU32(glyphTableOutput, tableOffset + 8, blockSize);
			
			//next table entry (internal buffer)
			tableOffset += CORRECT_GLYPH_TABLE_SIZE;
			
			//next glyph block (absolute in final file)
			baseOffset += blockSize;
		}
		
		//
		// THEN STORE THE GLYPH BLOCKS
		//
		
		size_t totalGBBytes{};
		for (const auto& g : glyphBlocks) totalGBBytes += RAW_PIXEL_DATA_OFFSET + g.rawPixels.size();
		
		glyphBlockOutput.reserve(totalGBBytes);
		
		u32 gOffset{};
		
		for (const auto& g : glyphBlocks)
		{
			WriteU32(glyphBlockOutput, gOffset, g.charCode); gOffset += 4;
			WriteU16(glyphBlockOutput, gOffset, g.width);    gOffset += 2;
			WriteU16(glyphBlockOutput, gOffset, g.height);   gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, g.bearingX); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, g.bearingY); gOffset += 2;
			WriteU16(glyphBlockOutput, gOffset, g.advance);  gOffset += 2;
			
			//vertices
			
			auto CreateVertices = [&]() -> vector<i16>
				{
					i16 x0 = static_cast<i16>(g.bearingX);
					i16 y0 = static_cast<i16>(g.bearingY - g.height); //bottom
					i16 x1 = static_cast<i16>(g.bearingX + g.width);
					i16 y1 = static_cast<i16>(g.bearingY);            //top
					
					return 
					{
						x0, y1,  //top-left
						x1, y1,  //top-right
						x1, y0,  //bottom-right
						x0, y0   //bottom-left
					};
				};
				
			vector<i16> vertices = CreateVertices();
				
			WriteI16(glyphBlockOutput, gOffset, vertices[0]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[1]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[2]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[3]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[4]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[5]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[6]); gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, vertices[7]); gOffset += 2;
				
			//raw pixel data
			
			WriteU32(glyphBlockOutput, gOffset, g.rawPixels.size()); gOffset += 4;
			
			for (const auto& p : g.rawPixels)
			{
				WriteU8(glyphBlockOutput, gOffset, p); gOffset++;
			}
		}
		
		//
		// AND PASS THE FINAL DATA
		//
		
		WriteU32(output, offset, totalGTBytes); offset += 4;
		WriteU32(output, offset, totalGBBytes); offset += 4;
		
		output.reserve(CORRECT_GLYPH_HEADER_SIZE + totalGTBytes + totalGBBytes);
		
		output.insert(output.end(), glyphTableOutput.begin(), glyphTableOutput.end());
		output.insert(output.end(), glyphBlockOutput.begin(), glyphBlockOutput.end());
			
		ofstream file(
			targetPath,
			ios::binary);
			
		file.write(
			reinterpret_cast<const char*>(output.data()), output.size());
			
		file.close();
			
		Log::Print(
			"Finished exporting glyphs!",
			"EXPORT_GLYPH",
			LogType::LOG_SUCCESS);
	}
}