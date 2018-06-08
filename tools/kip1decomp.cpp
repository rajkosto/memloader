#include "Types.h"
#include "ScopeGuard.h"
#include "Kip.h"
#include <cassert>
#include <cstdio>
#include <fstream>

ByteVector kip1_blz_decompress(const unsigned char* compData, size_t compDataLen)
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

	size_t decompressed_size = decompressed.size();
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
	size_t outindex = decompressed_size;
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

int main(int argc, char* argv[])
{
	auto PrintUsage = []() -> int
	{
		fprintf(stderr, "Usage: kip1decomp.exe inputfile.kip1 outputfile.kip1\n");
		return -1;
	};

	if (argc < 3)
		return PrintUsage();

	const char* inputFilename = argv[1];
	const char* outputFilename = argv[2];

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

			try { segmentData = kip1_blz_decompress(&compData[0], compData.size()); }
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