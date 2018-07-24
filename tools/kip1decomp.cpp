#include "Types.h"
#include "Kip.h"
#include "blz.h"
#include <cstdio>
#include <fstream>

//from https://github.com/SciresM/hactool/blob/master/kip.c which is exactly how kernel does it, thanks SciresM!
bool kip1_blz_uncompress(unsigned char* dataBuf, unsigned int compSize)
{
	u32* hdr_end = (u32*)&dataBuf[compSize];

	u32 addl_size = hdr_end[-1];
	u32 header_size = hdr_end[-2];
	u32 cmp_and_hdr_size = hdr_end[-3];

	unsigned char* cmp_start = (unsigned char *)(hdr_end) - cmp_and_hdr_size;
	u32 cmp_ofs = cmp_and_hdr_size - header_size;
	u32 out_ofs = cmp_and_hdr_size + addl_size;

	while (out_ofs)
	{
		unsigned char control = cmp_start[--cmp_ofs];
		for (unsigned int i=0; i<8; i++)
		{
			if (control & 0x80)
			{
				if (cmp_ofs < 2)
					return false; //out of bounds

				cmp_ofs -= 2;
				u16 seg_val = ((unsigned int)(cmp_start[cmp_ofs+1]) << 8) | cmp_start[cmp_ofs];
				u32 seg_size = ((seg_val >> 12) & 0xF) + 3;
				u32 seg_ofs = (seg_val & 0x0FFF) + 3;
				if (out_ofs < seg_size) // Kernel restricts segment copy to stay in bounds.
					seg_size = out_ofs;

				out_ofs -= seg_size;

				for (unsigned int j=0; j<seg_size; j++)
					cmp_start[out_ofs + j] = cmp_start[out_ofs + j + seg_ofs];
			}
			else
			{
				// Copy directly.
				if (cmp_ofs < 1)
					return false; //out of bounds

				cmp_start[--out_ofs] = cmp_start[--cmp_ofs];
			}
			control <<= 1;
			if (out_ofs == 0) // blz works backwards, so if it reaches byte 0, it's done
				return true;
		}
	}

	return true;
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
			segmentData.resize(info.DecompSz, 0);
			inFile.read((char*)&segmentData[0], info.CompSz);
			if (inFile.fail())
			{
				printf("FAIL!\n");
				fprintf(stderr, "Error reading comp data for section %u from input file '%s', pos: %llu\n", i, inputFilename, (u64)inFile.tellg());
				return -3;
			}

			if (!kip1_blz_uncompress(&segmentData[0], info.CompSz))
			{
				printf("FAIL!\n");
				fprintf(stderr, "Error decompressing BLZ data for section %u from input file '%s'\n", i, inputFilename);
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
			ByteVector compData = BLZ_Code(&srcData[0], (int)srcData.size(), true);
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