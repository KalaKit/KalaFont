//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <fstream>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/file_utils.hpp"

#include "export.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;
using KalaHeaders::WriteU8;
using KalaHeaders::WriteU16;
using KalaHeaders::WriteU32;
using KalaHeaders::WriteI8;
using KalaHeaders::WriteI16;
using KalaHeaders::WriteI32;

using std::ofstream;
using std::ios;

namespace KalaFont
{
	void Export::ExportKTF(
		const path& targetPath,
		u8 type,
		u8 glyphHeight,
		u8 superSampleMultiplier,
		vector<GlyphBlock>& glyphBlocks)
	{
		Log::Print(
			"Starting to export font to path '" + targetPath.string() + "'.",
			"EXPORT_FONT",
			LogType::LOG_DEBUG);
			
		vector<u8> output{};
		vector<u8> glyphTableOutput{};
		vector<u8> glyphBlockOutput{};
		
		//
		// FIRST STORE THE TOP HEADER
		//
			
		TopHeader topHeader{};
		topHeader.type = type;
		topHeader.glyphHeight = glyphHeight;
		topHeader.glyphCount = glyphBlocks.size();
		
		u32 offset{};
		
		output.reserve(20);
		
		WriteU32(output, offset, topHeader.magic);       offset += 4;
		WriteU8(output, offset, topHeader.version);      offset++;
		WriteU8(output, offset, topHeader.type);         offset++;
		WriteU16(output, offset, topHeader.glyphHeight); offset += 2;
		WriteU32(output, offset, topHeader.glyphCount);  offset += 4;
		
		//
		// THEN STORE THE GLYPH TABLE
		//
		
		size_t totalGTBytes = 12 * glyphBlocks.size();
		glyphTableOutput.reserve(totalGTBytes);
		
		u32 baseOffset = 20 + totalGTBytes;
		u32 tableOffset{};
		
		for (const auto& g : glyphBlocks)
		{
			u32 blockSize = 20 + g.glyphPixels.size();
			
			WriteU32(glyphTableOutput, tableOffset + 0, g.charCode);
			WriteU32(glyphTableOutput, tableOffset + 4, baseOffset);
			WriteU32(glyphTableOutput, tableOffset + 8, blockSize);
			
			tableOffset += 12;        //next table entry (internal buffer)
			baseOffset  += blockSize; //next glyph block (absolute in final file)
		}
		
		//
		// THEN STORE THE GLYPH BLOCKS
		//
		
		size_t totalGBBytes{};
		for (const auto& g : glyphBlocks) totalGBBytes += 20 + g.glyphPixels.size();
		
		glyphBlockOutput.reserve(totalGBBytes);
		
		u32 gOffset = 0;
		
		for (const auto& g : glyphBlocks)
		{
			WriteU32(glyphBlockOutput, gOffset, g.charCode);  gOffset += 4;
			WriteU16(glyphBlockOutput, gOffset, g.width);     gOffset += 2;
			WriteU16(glyphBlockOutput, gOffset, g.height);    gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, g.pitch);     gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, g.bearingX);  gOffset += 2;
			WriteI16(glyphBlockOutput, gOffset, g.bearingY);  gOffset += 2;
			WriteU16(glyphBlockOutput, gOffset, g.advance);   gOffset += 2;
			WriteU32(glyphBlockOutput, gOffset, g.glyphSize); gOffset += 4;
			
			for (const auto& p : g.glyphPixels)
			{
				WriteU8(glyphBlockOutput, gOffset, p); gOffset++;
			}
		}
		
		//
		// AND PASS THE FINAL DATA
		//
		
		WriteU32(output, offset, totalGTBytes);  offset += 4;
		WriteU32(output, offset, totalGBBytes);  offset += 4;
		
		output.reserve(20 + totalGTBytes + totalGBBytes);
		
		output.insert(output.end(), glyphTableOutput.begin(), glyphTableOutput.end());
		output.insert(output.end(), glyphBlockOutput.begin(), glyphBlockOutput.end());
			
		ofstream file(
			targetPath,
			ios::binary);
			
		file.write(
			reinterpret_cast<const char*>(output.data()), output.size());
			
		file.close();
			
		Log::Print(
			"Finished exporting font!",
			"EXPORT_FONT",
			LogType::LOG_SUCCESS);
	}
}