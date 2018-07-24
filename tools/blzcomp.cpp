#include "Types.h"
#include "blz.h"
#include <cstdio>
#include <fstream>

int main(int argc, char* argv[])
{
	auto PrintUsage = []() -> int
	{
		fprintf(stderr, "Usage: blzcomp.exe [--fast] inputfile.bin outputfile.blz\n");
		return -1;
	};

	bool best = true;
	std::vector<const char*> args;
	args.reserve(2);

	for (int i=1; i<argc; i++)
	{
		const char* currArg = argv[i];
		if (stricmp(currArg, "--fast") == 0)
			best = false;
		else if (strncmp(currArg, "--", 2) == 0)
		{
			fprintf(stderr, "Unknown option '%s'\n", currArg);
			return PrintUsage();
		}
		else
			args.push_back(currArg);
	}

	if (args.size() != 2)
	{
		fprintf(stderr, "You must specify both input and output filename, and no more\n");
		return PrintUsage();
	}

	const char* inputFilename = args[0];
	const char* outputFilename = args[1];

	std::ifstream inFile(inputFilename, std::ios::binary);
	if (!inFile.is_open())
	{
		fprintf(stderr, "Error opening input filename '%s' for reading\n", inputFilename);
		return -2;
	}

	ByteVector inputData;
	inFile.seekg(0, std::ios::end);
	inputData.resize((size_t)inFile.tellg());
	inFile.seekg(0, std::ios::beg);

	if (inputData.size() == 0)
	{
		fprintf(stderr, "Zero sized input file '%s'!\n", inputFilename);
		return -2;
	}

	inFile.read((char*)&inputData[0], inputData.size());
	if (inFile.fail())
	{
		fprintf(stderr, "Error reading input file '%s'!\n", inputFilename);
		return -2;
	}
	inFile.close();

	ByteVector outputData = BLZ_Code(&inputData[0], (unsigned int)inputData.size(), best);
	if (outputData.size() > inputData.size())
	{
		fprintf(stderr, "Compression inflated input from %zu to %zu bytes!\n", inputData.size(), outputData.size());
		return -3;
	}

	printf("Compressed input from %zu to %zu bytes.\n", inputData.size(), outputData.size());

	std::ofstream outFile(outputFilename, std::ios::binary);
	if (!outFile.is_open())
	{
		fprintf(stderr, "Error opening output filename '%s' for writing\n", outputFilename);
		return -4;
	}

	outFile.write((const char*)&outputData[0], outputData.size());
	if (outFile.fail())
	{
		fprintf(stderr, "Error writing to output file '%s'!\n", outputFilename);
		return -4;
	}
	outFile.close();

	return 0;
}