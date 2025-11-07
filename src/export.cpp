//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <fstream>
#include <string>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"
#include "KalaHeaders/import_ktf.hpp"

#include "export.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::WriteU8;
using KalaHeaders::WriteU16;
using KalaHeaders::WriteU32;
using KalaHeaders::WriteI8;
using KalaHeaders::WriteI16;
using KalaHeaders::WriteI32;
using KalaHeaders::GlyphHeader;
using KalaHeaders::CORRECT_GLYPH_HEADER_SIZE;
using KalaHeaders::CORRECT_GLYPH_TABLE_SIZE;
using KalaHeaders::RAW_PIXEL_DATA_OFFSET;
using KalaHeaders::MAX_GLYPH_COUNT;
using KalaHeaders::MAX_GLYPH_TABLE_SIZE;

using std::ofstream;
using std::ios;
using std::to_string;

using i8 = int8_t;
using u32 = uint32_t;

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
			Log::Print(
				"Failed to export because glyph count exceeded max allowed count '" + to_string(MAX_GLYPH_COUNT) + "'!",
				"EXPORT_BITMAP",
				LogType::LOG_ERROR,
				2);
		
			return;
		}
		
		if (CORRECT_GLYPH_TABLE_SIZE * glyphBlocks.size() > MAX_GLYPH_TABLE_SIZE)
		{
			Log::Print(
				"Failed to export because glyph data size exceeded max allowed size '" + to_string(MAX_GLYPH_TABLE_SIZE) + "'!",
				"EXPORT_BITMAP",
				LogType::LOG_ERROR,
				2);
		
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
			Log::Print(
				"Failed to export because glyph data size exceeded max allowed size '" + to_string(MAX_GLYPH_TABLE_SIZE) + "'!",
				"EXPORT_GLYPH",
				LogType::LOG_ERROR,
				2);
		
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
		glyphHeader.type = type;
		glyphHeader.glyphHeight = glyphHeight;
		glyphHeader.glyphCount = glyphBlocks.size();
		
		u32 offset{};
		
		output.reserve(CORRECT_GLYPH_HEADER_SIZE);
		
		WriteU32(output, offset, glyphHeader.magic);       offset += 4;
		WriteU8(output, offset, glyphHeader.version);      offset++;
		WriteU8(output, offset, glyphHeader.type);         offset++;
		WriteU16(output, offset, glyphHeader.glyphHeight); offset += 2;
		WriteU32(output, offset, glyphHeader.glyphCount);  offset += 4;
		
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
		// THEN STORE THE GLYPH TABLE
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
			
			auto CreateVertices = [&]() -> vector<i8>
				{
					i8 x0 = static_cast<i8>(g.bearingX);
					i8 y0 = static_cast<i8>(-g.bearingY);
					i8 x1 = static_cast<i8>(g.bearingX + g.width);
					i8 y1 = static_cast<i8>(-g.bearingY + g.height);
					
					return 
					{
						x0, y0, //top-left
						x1, y0, //top-right
						x1, y1, //bottom-right
						x0, y1  //bottom-left
					};
				};
				
			vector<i8> vertices = CreateVertices();
				
			WriteI8(glyphBlockOutput, gOffset, vertices[0]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[1]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[2]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[3]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[4]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[5]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[6]); gOffset++;
			WriteI8(glyphBlockOutput, gOffset, vertices[7]); gOffset++;
				
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