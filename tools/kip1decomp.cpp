#include "Types.h"
#include "ScopeGuard.h"
#include "Kip.h"
#include <cassert>
#include <cstdio>
#include <fstream>

ByteVector kip1_blz_decompress(const unsigned char* compData, unsigned int compDataLen)
{
	unsigned int compressed_size, init_index, uncompressed_addl_size;
	{
		struct BlzFooter
		{
			unsigned int compressed_size;
			unsigned int init_index;
			unsigned int uncompressed_addl_size;
		} footer;

		if (compDataLen < sizeof(BlzFooter))
			throw std::runtime_error("Compressed data not large enough for BLZ footer!");

		memcpy(&footer, compData+(compDataLen-sizeof(BlzFooter)), sizeof(BlzFooter));
		compressed_size = footer.compressed_size;
		init_index = footer.init_index;
		uncompressed_addl_size = footer.uncompressed_addl_size;
	}

	ByteVector compressed(compDataLen);
	if (compressed.size() > 0)
		memcpy(&compressed[0], compData, compDataLen);

	ByteVector decompressed(compDataLen + uncompressed_addl_size, 0);
	if (compDataLen > 0)
		memcpy(&decompressed[0], compData, compDataLen);

	unsigned int decompressed_size = (u32)decompressed.size();
    if (compDataLen != compressed_size)
	{
		if (compDataLen < compressed_size)
			throw std::logic_error("Not enough BLZ compressed bytes supplied");

		auto numSkipBytes = compDataLen - compressed_size;
		memmove(&compressed[0], &compressed[numSkipBytes], compressed.size()-numSkipBytes);
		compressed.resize(compressed.size() - numSkipBytes);
	}
	if ((compressed_size + uncompressed_addl_size) == 0)
		return ByteVector();

	unsigned int index = compressed_size - init_index;
	unsigned int outindex = decompressed_size;
    while (outindex > 0)
	{
		index -= 1;
		unsigned char control = compressed[index];
		for (unsigned int i=0; i<8; i++)
		{
            if ((control & 0x80) != 0)
			{
				if (index < 2)
					throw std::runtime_error("Compression out of bounds1 !");

				index -= 2;
				unsigned int segmentoffset = (unsigned int)(compressed[index]) | ((unsigned int)(compressed[index+1]) << 8);
				unsigned int segmentsize = ((segmentoffset >> 12) & 0xF) + 3;
				segmentoffset &= 0x0FFF;
				segmentoffset += 2;
				if (outindex < segmentsize)
					throw std::runtime_error("Compression out of bounds2 !");

                for (unsigned int j=0; j<segmentsize; j++)
				{
					if ((outindex + segmentoffset) >= decompressed_size)
						throw std::runtime_error("Compression out of bounds3 !");

					unsigned char data = decompressed[outindex+segmentoffset];
					outindex -= 1;
					decompressed[outindex] = data;
				}
			}
            else
			{
				if (outindex < 1)
					throw std::runtime_error("Compression out of bounds4 !");

				outindex -= 1;
				index -= 1;
				decompressed[outindex] = compressed[index];
			}

			control <<= 1;
			control &= 0xFF;
			if (outindex == 0)
				break;
		}
	}

	return decompressed;
}

/*----------------------------------------------------------------------------*/
/*--  blz.c - Bottom LZ coding for Nintendo GBA/DS                          --*/
/*--  Copyright (C) 2011 CUE                                                --*/
/*--                                                                        --*/
/*--  This program is free software: you can redistribute it and/or modify  --*/
/*--  it under the terms of the GNU General Public License as published by  --*/
/*--  the Free Software Foundation, either version 3 of the License, or     --*/
/*--  (at your option) any later version.                                   --*/
/*--                                                                        --*/
/*--  This program is distributed in the hope that it will be useful,       --*/
/*--  but WITHOUT ANY WARRANTY; without even the implied warranty of        --*/
/*--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          --*/
/*--  GNU General Public License for more details.                          --*/
/*--                                                                        --*/
/*--  You should have received a copy of the GNU General Public License     --*/
/*--  along with this program. If not, see <http://www.gnu.org/licenses/>.  --*/
/*----------------------------------------------------------------------------*/

#define BLZ_NORMAL    0          // normal mode
#define BLZ_BEST	  1			 // best mode

/*----------------------------------------------------------------------------*/
#define CMD_DECODE    0x00       // decode
#define CMD_ENCODE    0x01       // encode

#define BLZ_SHIFT     1          // bits to shift
#define BLZ_MASK      0x80       // bits to check:
// ((((1 << BLZ_SHIFT) - 1) << (8 - BLZ_SHIFT)

#define BLZ_THRESHOLD 2          // max number of bytes to not encode
#define BLZ_N         0x1002     // max offset ((1 << 12) + 2)
#define BLZ_F         0x12       // max coded ((1 << 4) + BLZ_THRESHOLD)

#define RAW_MINIM     0x00000000 // empty file, 0 bytes
#define RAW_MAXIM     0x00FFFFFF // 3-bytes length, 16MB - 1

#define BLZ_MINIM     0x00000004 // header only (empty RAW file)
#define BLZ_MAXIM     0x01400000 // 0x0120000A, padded to 20MB:
// * length, RAW_MAXIM
// * flags, (RAW_MAXIM + 7) / 8
// * header, 11
// 0x00FFFFFF + 0x00200000 + 12 + padding

/*----------------------------------------------------------------------------*/
#define BREAK(text)   { printf(text); return; }
#define EXIT(text)    { printf(text); exit(-1); }

/*----------------------------------------------------------------------------*/
void BLZ_Invert(u8 *buffer, int length)
{
	u8 *bottom, ch;

	bottom = buffer + length - 1;

	while (buffer < bottom)
	{
		ch = *buffer;
		*buffer++ = *bottom;
		*bottom-- = ch;
	}
}

/*----------------------------------------------------------------------------*/
ByteVector BLZ_Code(u8* raw_buffer, unsigned int raw_len, int best) 
{
	u8 *pak_buffer, *pak, *raw, *raw_end, *flg = nullptr;
	u32   pak_len, inc_len, hdr_len, enc_len, len, pos, max;
	u32   len_best, pos_best = 0, len_next, pos_next, len_post, pos_post;
	u32   pak_tmp, raw_tmp;
	u8  mask;

#define SEARCH(l,p) { \
  l = BLZ_THRESHOLD;                                             \
                                                                 \
  max = (raw-raw_buffer >= BLZ_N) ? BLZ_N : u32(raw-raw_buffer); \
  for (pos = 3; pos <= max; pos++) {                             \
    for (len = 0; len < BLZ_F; len++) {                          \
      if (raw + len == raw_end) break;                           \
      if (len >= pos) break;                                     \
      if (*(raw + len) != *(raw + len - pos)) break;             \
    }                                                            \
                                                                 \
    if (len > l) {                                               \
      p = pos;                                                   \
      if ((l = len) == BLZ_F) break;                             \
    }                                                            \
  }                                                              \
}

	pak_tmp = 0;
	raw_tmp = raw_len;

	pak_len = raw_len + ((raw_len + 7) / 8) + 15;
	ByteVector outBuf(pak_len, 0);
	if (outBuf.size() > 0)
		pak_buffer = &outBuf[0];
	else
		pak_buffer = nullptr;

	BLZ_Invert(raw_buffer, raw_len);

	pak = pak_buffer;
	raw = raw_buffer;
	raw_end = raw_buffer + raw_len;

	mask = 0;

	while (raw < raw_end) 
	{
		if (!(mask >>= BLZ_SHIFT)) 
		{
			*(flg = pak++) = 0;
			mask = BLZ_MASK;
		}

		SEARCH(len_best, pos_best);

		// LZ-CUE optimization start
		if (best) 
		{
			if (len_best > BLZ_THRESHOLD) 
			{
				if (raw + len_best < raw_end) 
				{
					raw += len_best;
					SEARCH(len_next, pos_next);
					raw -= len_best - 1;
					SEARCH(len_post, pos_post);
					raw--;

					if (len_next <= BLZ_THRESHOLD) len_next = 1;
					if (len_post <= BLZ_THRESHOLD) len_post = 1;

					if (len_best + len_next <= 1 + len_post) len_best = 1;
				}
			}
		}
		// LZ-CUE optimization end

		*flg <<= 1;
		if (len_best > BLZ_THRESHOLD) 
		{
			raw += len_best;
			*flg |= 1;
			*pak++ = ((len_best - (BLZ_THRESHOLD+1)) << 4) | ((pos_best - 3) >> 8);
			*pak++ = (pos_best - 3) & 0xFF;
		}
		else 
		{
			*pak++ = *raw++;
		}

		if ((pak - pak_buffer + raw_len - (raw - raw_buffer)) < (pak_tmp + raw_tmp)) 
		{
			pak_tmp = u32(pak - pak_buffer);
			raw_tmp = raw_len - u32(raw - raw_buffer);
		}
	}

#undef SEARCH

	while (mask && (mask != 1)) 
	{
		mask >>= BLZ_SHIFT;
		*flg <<= 1;
	}

	pak_len = u32(pak - pak_buffer);

	BLZ_Invert(raw_buffer, raw_len);
	BLZ_Invert(pak_buffer, pak_len);

	if (!pak_tmp || (raw_len + 4 < ((pak_tmp + raw_tmp + 3) & -4) + 8)) 
	{
		pak = pak_buffer;
		raw = raw_buffer;
		raw_end = raw_buffer + raw_len;

		while (raw < raw_end) *pak++ = *raw++;

		while ((pak - pak_buffer) & 3) *pak++ = 0;

		*(u32 *)pak = 0; pak += 4;
	}
	else 
	{
		//scope for tmpBuf
		{
			ByteVector tmpBuf(raw_tmp + pak_tmp + 15, 0);
			u8* tmp = &tmpBuf[0];

			for (len = 0; len < raw_tmp; len++)
				tmp[len] = raw_buffer[len];

			for (len = 0; len < pak_tmp; len++)
				tmp[raw_tmp + len] = pak_buffer[len + pak_len - pak_tmp];

			outBuf = std::move(tmpBuf);
		}		
		pak_buffer = &outBuf[0];
		pak = pak_buffer + raw_tmp + pak_tmp;

		enc_len = pak_tmp;
		hdr_len = 12;
		inc_len = raw_len - pak_tmp - raw_tmp;

		while ((pak - pak_buffer) & 3) 
		{
			*pak++ = 0xFF;
			hdr_len++;
		}

		*(u32 *)pak = enc_len + hdr_len; pak += 4;
		*(u32 *)pak = hdr_len;           pak += 4;
		*(u32 *)pak = inc_len - hdr_len; pak += 4;
	}

	const size_t new_len = pak - pak_buffer;
	outBuf.resize(new_len, 0);

	return outBuf;
}

int main(int argc, char* argv[])
{
	auto PrintUsage = []() -> int
	{
		fprintf(stderr, "Usage: kip1decomp.exe d/c inputfile.kip1 outputfile.kip1\n");
		return -1;
	};

	if (argc < 4)
		return PrintUsage();

	enum OperationType
	{
		OP_DECOMPRESS = 0,
		OP_COMPRESS = 1,
		OP_COUNT = 2
	} opType = OP_COUNT;

	const char* opTypeStr = argv[1];
	if (strlen(opTypeStr) == 1 && opTypeStr[0] == 'd')
		opType = OP_DECOMPRESS;
	else if (strlen(opTypeStr) == 1 && opTypeStr[0] == 'c')
		opType = OP_COMPRESS;
	else
	{
		fprintf(stderr, "Invalid operation specified as first argument\n");
		return PrintUsage();
	}

	const char* inputFilename = argv[2];
	const char* outputFilename = argv[3];

	//check all arguments
	if (inputFilename == nullptr || strlen(inputFilename) == 0)
	{
		fprintf(stderr, "No input filename specified\n");
		return PrintUsage();
	}

	if (outputFilename == nullptr || strlen(outputFilename) == 0)
	{
		fprintf(stderr, "No output filename specified\n");
		return PrintUsage();
	}

	std::ifstream inFile(inputFilename, std::ios::binary);
	if (!inFile.is_open())
	{
		fprintf(stderr, "Error opening input filename '%s' for reading\n", inputFilename);
		return -2;
	}

	Kip::Header kipHeader;
	try { kipHeader.deserialize(inFile); }
	catch (std::exception& err)
	{
		fprintf(stderr, "Error parsing KIP1 header in file '%s': %s\n", inputFilename, err.what());
		return -2;
	}

	if (!kipHeader.validMagic())
	{
		fprintf(stderr, "Input file '%s' is not a KIP1 file!\n", inputFilename);
		return -2;
	}

	ByteVector segments[array_countof(kipHeader.Segments)];
	for (unsigned int i=0; i<array_countof(segments); i++)
	{
		bool compressed = false;
		if (i < 3)
			compressed = (kipHeader.Flags & (1u << i)) != 0;

		ByteVector& segmentData = segments[i];
		const Kip::Segment& info = kipHeader.Segments[i];
		printf("Reading segment %u compSize: %u decompSize: %u BLZ: %s...", i, info.CompSz, info.DecompSz, compressed ? "true" : "false");

		if (info.CompSz == 0)
		{
			printf("EMPTY!\n");
			continue;
		}
		else if (!compressed)
		{
			segmentData.resize(info.CompSz);
			inFile.read((char*)&segmentData[0], segmentData.size());
			if (inFile.fail())
			{
				printf("FAIL!\n");
				fprintf(stderr, "Error reading decomp data for section %u from input file '%s', pos: %llu\n", i, inputFilename, (u64)inFile.tellg());
				return -3;
			}
			else
				printf("OK!\n");
		}
		else
		{
			ByteVector compData(info.CompSz);
			inFile.read((char*)&compData[0], compData.size());
			if (inFile.fail())
			{
				printf("FAIL!\n");
				fprintf(stderr, "Error reading comp data for section %u from input file '%s', pos: %llu\n", i, inputFilename, (u64)inFile.tellg());
				return -3;
			}

			try { segmentData = kip1_blz_decompress(&compData[0], (u32)compData.size()); }
			catch (std::exception& err)
			{
				printf("FAIL!\n");
				fprintf(stderr, "Error decompressing BLZ data for section %u from input file '%s', err: %s\n", i, inputFilename, err.what());
				return -3;
			}
			printf("OK!\n");
		}
	}

	ByteVector trailingData;
	if (!inFile.eof())
	{
		const auto startPos = inFile.tellg();
		inFile.seekg(0, std::ios::end);
		const auto endPos = inFile.tellg();

		const auto trailingSize = endPos - startPos;
		if (trailingSize > 0)
		{
			trailingData.resize((size_t)trailingSize);
			inFile.seekg(startPos, std::ios::beg);
			inFile.read((char*)&trailingData[0], trailingData.size());
		}
	}
	inFile.close();

	//nothing is compressed anymore
	kipHeader.Flags &= 0xF8; 
	//unless we compress it again
	if (opType == OP_COMPRESS)
	{
		for (unsigned int i=0; i<3; i++)
		{
			ByteVector srcData = segments[i]; //compressor modifies this
			if (srcData.size() == 0)
				continue;

			printf("Compressing segment %u (size: %u)...", i, (u32)srcData.size());
			ByteVector compData = BLZ_Code(&srcData[0], (int)srcData.size(), BLZ_BEST);
			printf("%u bytes.", (u32)compData.size());
			if (compData.size() < srcData.size())
			{
				segments[i] = compData;
				kipHeader.Flags |= (1u << i) & 0x7;
				printf("Using COMPRESSED.\n");
			}
			else
				printf("Using DECOMPRESSED.\n");
		}
	}

	//update the file size of all the sections
	for (unsigned int i=0; i<array_countof(segments); i++)
		kipHeader.Segments[i].CompSz = (unsigned int)segments[i].size();

	std::ofstream outFile(outputFilename, std::ios::binary);
	if (!outFile.is_open())
	{
		fprintf(stderr, "Error opening output filename '%s' for writing\n", outputFilename);
		return -4;
	}

	printf("Writing KIP1 header to file @%llu...", (u64)outFile.tellp());
	outFile.write((const char*)&kipHeader, sizeof(kipHeader));
	if (outFile.fail()) { printf("FAIL!\n"); return -4; } else { printf("OK!\n"); }

	for (unsigned int i=0; i<array_countof(segments); i++)
	{
		const ByteVector& segmentData = segments[i];
		if (segmentData.size() == 0)
			continue;

		printf("Writing segment %u data (size: %u) to file @%llu...", i, (u32)segmentData.size(), (u64)outFile.tellp());
		outFile.write((const char*)&segmentData[0], segmentData.size());
		if (outFile.fail()) { printf("FAIL!\n"); return -4; } else { printf("OK!\n"); }
	}

	if (trailingData.size() > 0)
	{
		printf("Writing trailing data (size: %u) to file @%llu...", (u32)trailingData.size(), (u64)outFile.tellp());
		outFile.write((const char*)&trailingData[0], trailingData.size());
		if (outFile.fail()) { printf("FAIL!\n"); return -4; }
		else { printf("OK!\n"); }
	}

	outFile.close();
	return 0;
}