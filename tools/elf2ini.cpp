#include "Types.h"
#include "ScopeGuard.h"
#include "RelPath.h"
#include "Elf.h"
#include <cassert>
#include <cstdio>
#include <fstream>

int main(int argc, char* argv[])
{
	auto PrintUsage = []() -> int
	{
		fprintf(stderr, "Usage: elf2ini.exe inputFile.elf [outputFile.ini]\n");
		return -1;
	};

	if (argc < 2)
		return PrintUsage();

	const char* inputFilename = argv[1];
	const char* outputFilename = (argc>2) ? argv[2] : nullptr;

	//check all arguments
	if (inputFilename == nullptr || strlen(inputFilename) == 0)
	{
		fprintf(stderr, "No input filename specified\n");
		return PrintUsage();
	}

	std::ifstream inFile(inputFilename, std::ios::binary);
	if (!inFile.is_open())
	{
		fprintf(stderr, "Error opening input filename '%s' for reading\n", inputFilename);
		return -2;
	}

	//check if file is valid elf of the type we want
	{
		Elf::HeaderBase base;
		memset(&base, 0, sizeof(base));
		base.deserialize(inFile);

		if (!base.validMagic())
		{
			fprintf(stderr, "Invalid ELF magic in '%s'\n", inputFilename);
			return -3;
		}

		if (base.ident[Elf::HeaderBase::EI_CLASS] != Elf::ELFCLASS64)
		{
			fprintf(stderr, "Invalid ELF class (needs to be 64-bit) in '%s'\n", inputFilename);
			return -3;
		}

		if (base.ident[Elf::HeaderBase::EI_DATA] != Elf::ELFDATA2LSB)
		{
			fprintf(stderr, "ELF header not in little-endian in '%s'\n", inputFilename);
			return -3;
		}

		if (base.ident[Elf::HeaderBase::EI_VERSION] != Elf::EV_CURRENT)
		{
			fprintf(stderr, "Invalid ELF header version in '%s'\n", inputFilename);
			return -3;
		}

		inFile.seekg(0, std::ios::beg);
	}

	Elf::Header<64> hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.deserialize(inFile);

	if (hdr.type != Elf::ET_EXEC)
	{
		fprintf(stderr, "Invalid ELF type (must be EXEC) in '%s'\n", inputFilename);
		return -3;
	}

	if (hdr.machine != Elf::EM_AARCH64)
	{
		fprintf(stderr, "Invalid ELF machine (must be AArch64) in '%s'\n", inputFilename);
		return -3;
	}

	if (hdr.version != Elf::EV_CURRENT)
	{
		fprintf(stderr, "Invalid ELF version (must be 1) in '%s'\n", inputFilename);
		return -3;
	}

	if (hdr.ehsize != sizeof(hdr))
	{
		fprintf(stderr, "Invalid ELF header size in '%s'\n", inputFilename);
		return -3;
	}

	if (hdr.phentsize != sizeof(Elf::ProgramHeader<64>))
	{
		fprintf(stderr, "Invalid ELF program header size in '%s'\n", inputFilename);
		return -3;
	}

	if (hdr.shentsize != 0 && hdr.shentsize != sizeof(Elf::SectionHeader<64>))
	{
		fprintf(stderr, "Invalid ELF section header size in '%s'\n", inputFilename);
		return -3;
	}
	
	FILE* outputFile = nullptr;
	if (outputFilename != nullptr && strlen(outputFilename) > 0)
	{
		outputFile = fopen(outputFilename, "w");
		if (outputFile == nullptr)
		{
			fprintf(stderr, "Error opening output filename '%s' for writing\n", outputFilename);
			return -2;
		}
	}
	auto outFileGuard = MakeScopeGuard([&outputFile]() {
		if (outputFile != nullptr)
		{
			fclose(outputFile);
			outputFile = nullptr;
		}
	});
	
	std::string newRelativeFilename;
	if (outputFile == nullptr)
	{
		outputFile = stdout;
		outFileGuard.reset();
	}
	else
	{
		newRelativeFilename = GetRelativePath(inputFilename, outputFilename);
		inputFilename = newRelativeFilename.c_str();
	}

	inFile.seekg(hdr.phoff, std::ios::beg);
	for (size_t i=0; i<hdr.phnum; i++)
	{
		Elf::ProgramHeader<64> phdr;
		memset(&phdr, 0, sizeof(phdr));
		phdr.deserialize(inFile);

		if (phdr.type != Elf::PT_LOAD)
			continue;

		fprintf(outputFile, "[load:PH_%u]\n", (u32)i);
		fprintf(outputFile, "if=%s\n", inputFilename);
		fprintf(outputFile, "skip=0x%08llx\n", (u64)phdr.offset);
		fprintf(outputFile, "count=0x%08llx\n", (u64)phdr.filesz);
		fprintf(outputFile, "dst=0x%08llx\n", (u64)phdr.vaddr);
		fprintf(outputFile, "\n");
		fflush(outputFile);
	}

	if (hdr.entry != 0)
	{
		fprintf(outputFile, "[boot:ENTRY]\n");
		fprintf(outputFile, "pc=0x%08llx\n", hdr.entry);
		fprintf(outputFile, "\n");
		fflush(outputFile);
	}

	return 0;
}