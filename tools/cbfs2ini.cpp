#include "Types.h"
#include "ScopeGuard.h"
#include "RelPath.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>

static int ReadFileToBuf(ByteVector& outBuf, const char* fileType, const char* inputFilename, bool silent)
{
	std::ifstream inputFile(inputFilename, std::ios::binary);
	if (!inputFile.is_open())
	{
		if (!silent)
			fprintf(stderr, "Couldn't open %s file '%s' for reading\n", fileType, inputFilename);

		return -2;
	}

	inputFile.seekg(0, std::ios::end);
	const auto inputSize = inputFile.tellg();
	inputFile.seekg(0, std::ios::beg);

	outBuf.resize((size_t)inputSize);
	if (outBuf.size() > 0)
	{
		inputFile.read((char*)&outBuf[0], outBuf.size());
		const auto bytesRead = inputFile.gcount();
		if (bytesRead < inputSize)
		{
			fprintf(stderr, "Error reading %s file '%s' (only %llu out of %llu bytes read)\n", fileType, inputFilename, (u64)bytesRead, (u64)inputSize);
			return -2;
		}
	}

	return 0;
};

static const size_t FMAP_NAMELEN = 32;
static const char FMAP_SIGNATURE[] = "__FMAP__";
static const size_t FMAP_SIGNATURE_SIZE = 8;
static const size_t FMAP_SEARCH_STRIDE = 4;
static const uint8_t FMAP_VER_MAJOR = 1;

#pragma pack(push, 1)
struct FmapHeader 
{
	char        fmap_signature[FMAP_SIGNATURE_SIZE];
	uint8_t     fmap_ver_major;
	uint8_t     fmap_ver_minor;
	uint64_t    fmap_base;
	uint32_t    fmap_size;
	char        fmap_name[FMAP_NAMELEN];
	uint16_t    fmap_nareas;

	struct FmapAreaHeader
	{
		uint32_t area_offset;
		uint32_t area_size;
		char     area_name[FMAP_NAMELEN];
		uint16_t area_flags;
	};

	static int is_fmap(const void* ptr)
	{
		auto fmap_header = (const FmapHeader *)ptr;

		if (0 != memcmp(ptr, FMAP_SIGNATURE, FMAP_SIGNATURE_SIZE))
			return 0;

		if (fmap_header->fmap_ver_major == FMAP_VER_MAJOR)
			return 1;

		fprintf(stderr, "Found FMAP, but major version is %u instead of %u\n", fmap_header->fmap_ver_major, FMAP_VER_MAJOR);
		return 0;
	}

	static const FmapHeader* fmap_find(const void* ptr, size_t size)
	{
		const size_t lim = size - sizeof(FmapHeader);
		if (lim >= 0 && is_fmap(ptr))
			return (const FmapHeader*)ptr;

		size_t align;
		for (align = FMAP_SEARCH_STRIDE; align <= lim; align *= 2);
		for (; align >= FMAP_SEARCH_STRIDE; align /= 2)
		{
			for (size_t offset=align; offset<=lim; offset+=align*2)
			{
				const uint8_t* bytePtr = (const uint8_t*)(ptr)+offset;
				if (is_fmap(bytePtr))
					return (const FmapHeader *)(bytePtr);
			}
		}

		return nullptr;
	}
};

static const char HEADER_MAGIC[] = "ORBC";

//in big endian
struct cbheader 
{
	u32 magic;
	u32 version;
	u32 romsize;
	u32 bootblocksize;
	u32 align;
	u32 offset;
	u32 architecture;
	u32 pad[1];

	cbheader& fromBigEndian(const cbheader* cbHeaderPtr)
	{
		magic = cbHeaderPtr->magic;
		version = cbHeaderPtr->version;
		romsize = _byteswap_ulong(cbHeaderPtr->romsize);
		bootblocksize = _byteswap_ulong(cbHeaderPtr->bootblocksize);
		align = _byteswap_ulong(cbHeaderPtr->align);
		offset = _byteswap_ulong(cbHeaderPtr->offset);
		architecture = _byteswap_ulong(cbHeaderPtr->architecture);

		return *this;
	}
};

static const char LARCHIVE_MAGIC[] = "LARCHIVE";
enum ComponentType
{
	COMPONENT_DELETED = 0x00,
	COMPONENT_BOOTBLOCK = 0x01,
	COMPONENT_CBFSHEADER = 0x02,
	COMPONENT_STAGE = 0x10,
	COMPONENT_PAYLOAD = 0x20,
	COMPONENT_OPTIONROM = 0x30,
	COMPONENT_RAW = 0x50,
	COMPONENT_MICROCODE = 0x53,
	COMPONENT_CMOS_LAYOUT = 0x1aa,
	COMPONENT_NULL = -1
};

//in big endian
struct cbfile 
{
	u64 magic;
	u32 len;
	u32 type;
	u32 tagsoffset;
	u32 offset;
	char filename[0];

	cbfile& fromBigEndian(const cbfile* cbFilePtr)
	{
		magic = cbFilePtr->magic;
		len = _byteswap_ulong(cbFilePtr->len);
		type = _byteswap_ulong(cbFilePtr->type);
		tagsoffset = _byteswap_ulong(cbFilePtr->tagsoffset);
		offset = _byteswap_ulong(cbFilePtr->offset);

		return *this;
	}
};

struct cbfs_stage
{
	u32 compression;
	u64 entry;
	u64 load;
	u32 len;
	u32 memlen;
};

enum SegmentType
{
	PAYLOAD_SEGMENT_CODE = 0x45444F43,
	PAYLOAD_SEGMENT_DATA = 0x41544144,
	PAYLOAD_SEGMENT_BSS = 0x20535342,
	PAYLOAD_SEGMENT_PARAMS = 0x41524150,
	PAYLOAD_SEGMENT_ENTRY = 0x52544E45
};

// in big endian
struct cbfs_payload_segment 
{
	u32 type;
	u32 compression;
	u32 offset;
	u64 load_addr;
	u32 len;
	u32 mem_len;

	cbfs_payload_segment& fromBigEndian(const cbfs_payload_segment* payloadPtr)
	{
		type = payloadPtr->type;
		compression = _byteswap_ulong(payloadPtr->compression);
		offset = _byteswap_ulong(payloadPtr->offset);
		load_addr = _byteswap_uint64(payloadPtr->load_addr);
		len = _byteswap_ulong(payloadPtr->len);
		mem_len = _byteswap_ulong(payloadPtr->mem_len);

		return *this;
	}
};

#pragma pack(pop)

int main(int argc, char* argv[])
{
	auto PrintUsage = []() -> int
	{
		fprintf(stderr, "Usage: cbfs2ini.exe (--add-section=FMAP)* (--skip-section=BIOS)* (--add-archive=*)* (--skip-archive=fallback/romstage)* --load-addr=0x80000000 [--boot=fallback/ramstage] coreboot.rom\n");
		return -1;
	};

	auto FindInVectByName = [](const vector<const char*>& vect, const char* name) -> vector<const char*>::const_iterator {
		return std::find_if(vect.cbegin(), vect.cend(), [name](const char* txt) { return !stricmp(txt, name); });
	};

	vector<const char*> addSections;
	vector<const char*> skipSections;
	vector<const char*> addArchives;
	vector<const char*> skipArchives;
	u32 loadStartAddress = 0;
	const char* bootArchiveName = nullptr;
	const char* cbfsFilename = nullptr;
	const char* outputFilename = nullptr;

	const char HEXA_PREFIX[] = "0x";
	for (int argIdx=1; argIdx<argc; argIdx++)
	{
		char* currArg = argv[argIdx];

		enum ArgType
		{
			ARG_ADDSECTION,
			ARG_SKIPSECTION,
			ARG_ADDARCHIVE,
			ARG_SKIPARCHIVE,
			ARG_BOOT,
			ARG_OUTPUT,
			ARG_LOADADDRESS,
			ARGTYPE_COUNT
		};
		const char* TEXT_ARGUMENTS[] ={ "--add-section", "--skip-section", "--add-archive", "--skip-archive", "--boot", "--out", "--load-addr" };
		static_assert(array_countof(TEXT_ARGUMENTS) == ARGTYPE_COUNT, "TEXT_ARGUMENTS size mismatch vs enum");

		size_t argType;
		for (argType=0; argType<array_countof(TEXT_ARGUMENTS); argType++)
		{
			const char* matchedStr = TEXT_ARGUMENTS[argType];
			const size_t matchedLen = strlen(matchedStr);

			if (strnicmp(currArg, matchedStr, matchedLen))
				continue;

			const char* theValueStr = nullptr;
			if (currArg[matchedLen] == '=')
				theValueStr = &currArg[matchedLen+1];
			else if (currArg[matchedLen] == 0)
			{
				if (argIdx==argc-1)
					return PrintUsage();

				theValueStr = argv[++argIdx];
			}
			else
				return PrintUsage();

			if (argType == ARG_ADDSECTION)
				addSections.push_back(theValueStr);
			else if (argType == ARG_SKIPSECTION)
				skipSections.push_back(theValueStr);
			else if (argType == ARG_ADDARCHIVE)
				addArchives.push_back(theValueStr);
			else if (argType == ARG_SKIPARCHIVE)
				skipArchives.push_back(theValueStr);
			else if (argType == ARG_BOOT)
				bootArchiveName = theValueStr;
			else if (argType == ARG_OUTPUT)
				outputFilename = theValueStr;
			else if (argType == ARG_LOADADDRESS)
			{
				if (strlen(theValueStr) >= array_countof(HEXA_PREFIX) &&
					strnicmp(theValueStr, HEXA_PREFIX, array_countof(HEXA_PREFIX)-1) == 0)
					theValueStr += array_countof(HEXA_PREFIX)-1;

				char* matchedEnd = nullptr;
				loadStartAddress = strtoul(theValueStr, &matchedEnd, 0x10);
				if (matchedEnd == nullptr || matchedEnd == theValueStr)
				{
					fprintf(stderr, "Invalid load start address '%s'\n", theValueStr);
					return PrintUsage();
				}
			}

			break;
		}
		if (argType != array_countof(TEXT_ARGUMENTS))
			continue; //already parsed
		else if (currArg[0] == '-') //unknown option
		{
			fprintf(stderr, "Unknown option %s\n", currArg);
			return PrintUsage();
		}
		else //payload/data filename
		{
			cbfsFilename = currArg;
		}
	}

	//check all arguments
	if (cbfsFilename == nullptr || strlen(cbfsFilename) == 0)
	{
		fprintf(stderr, "No input filename specified\n");
		return PrintUsage();
	}
	if (loadStartAddress == 0)
	{
		fprintf(stderr, "No load start address specified\n");
		return PrintUsage();
	}
	if (addSections.size() == 0 && addArchives.size() == 0)
	{
		fprintf(stderr, "No sections or archives specified for load\n");
		return PrintUsage();
	}	

	ByteVector fileBuf;
	int retVal = ReadFileToBuf(fileBuf, "cbrom", cbfsFilename, false);
	if (retVal != 0 || fileBuf.size() == 0)
		return retVal;

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
		newRelativeFilename = GetRelativePath(cbfsFilename, outputFilename);
		cbfsFilename = newRelativeFilename.c_str();
	}

	auto fmapPtr = FmapHeader::fmap_find(&fileBuf[0], fileBuf.size());
	if (fmapPtr == nullptr && addSections.size() > 0)
	{
		fprintf(stderr, "No FMAP found but we need to load sections, exiting\n");
		return -3;
	}
	else if (fmapPtr != nullptr)
	{
		fprintf(stderr, "Found FMAP ver %u.%u at 0x%08llx, base: 0x%08llx size: 0x%08x\n",
			(u32)fmapPtr->fmap_ver_major, (u32)fmapPtr->fmap_ver_minor, (u64)fmapPtr, fmapPtr->fmap_base, fmapPtr->fmap_size);

		auto areas = reinterpret_cast<const FmapHeader::FmapAreaHeader*>((const u8*)(fmapPtr)+sizeof(FmapHeader));
		const bool wildcardEnabled = FindInVectByName(addSections, "*") != addSections.cend();
		for (decltype(fmapPtr->fmap_nareas) i=0; i<fmapPtr->fmap_nareas; i++)
		{
			const FmapHeader::FmapAreaHeader* currArea = &areas[i];
			fprintf(stderr, "\t%s @0x%08x size 0x%08x bytes: ", currArea->area_name, currArea->area_offset, currArea->area_size);
			if (!wildcardEnabled && FindInVectByName(addSections, currArea->area_name) == addSections.cend())
			{
				fprintf(stderr, "Skipped due to not being in addSections.\n");
				continue;
			}
			if (FindInVectByName(skipSections, currArea->area_name) != skipSections.cend() && FindInVectByName(addSections, currArea->area_name) == addSections.cend())
			{
				fprintf(stderr, "Skipped due to being in skipSections (and not in addSections).\n");
				continue;
			}
			fprintf(stderr, "Added!\n");

			fprintf(outputFile, "[load:%s_%s]\n", fmapPtr->fmap_name, currArea->area_name);
			fprintf(outputFile, "if=%s\n", cbfsFilename);
			fprintf(outputFile, "skip=0x%08llx\n", fmapPtr->fmap_base + currArea->area_offset);
			fprintf(outputFile, "count=0x%08x\n", currArea->area_size);
			fprintf(outputFile, "dst=0x%08llx\n", loadStartAddress + fmapPtr->fmap_base + currArea->area_offset);
			fprintf(outputFile, "\n");
			fflush(outputFile);
		}
	}

	if (fileBuf.size() < sizeof(s32))
		return -3;

	cbheader cbHeader;
	memset(&cbHeader, 0, sizeof(cbHeader));
	const cbheader* cbHeaderPtr = nullptr;
	{
		const s64 masterBlockOffset = *reinterpret_cast<const s32*>(&fileBuf[fileBuf.size()-sizeof(s32)]);
		if (masterBlockOffset < 0)
			cbHeaderPtr = reinterpret_cast<const cbheader*>(&fileBuf[u64(fileBuf.size())+masterBlockOffset]);
		else
			cbHeaderPtr = reinterpret_cast<const cbheader*>(&fileBuf[masterBlockOffset]);

		if (uptr(cbHeaderPtr) < uptr(&fileBuf[0]) || uptr(cbHeaderPtr) > uptr(&fileBuf[0] + fileBuf.size()))
		{
			fprintf(stderr, "Invalid master header offset: %lld\n", masterBlockOffset);
			return -3;
		}
		if (memcmp(&cbHeaderPtr->magic, HEADER_MAGIC, sizeof(cbHeaderPtr->magic)) != 0)
		{
			fprintf(stderr, "Invalid master header magic at offset: %lld\n", masterBlockOffset);
			return -3;
		}
		cbHeader.fromBigEndian(cbHeaderPtr);
	}

	if (cbHeader.offset >= fileBuf.size() || cbHeader.romsize > fileBuf.size())
	{
		fprintf(stderr, "Invalid master header extents (offset: 0x%08x romsize: 0x%08x)\n", cbHeader.offset, cbHeader.romsize);
		return -3;
	}
	else
	{
		fprintf(stderr, "CB master header version %c.%c.%c.%c at 0x%08llx has offset: 0x%08x romsize: 0x%08x\n",
			cbHeader.version & 0xFF, (cbHeader.version >> 8) & 0xFF, (cbHeader.version >> 16) & 0xFF, (cbHeader.version >> 24) & 0xFF,
			u64(cbHeaderPtr), cbHeader.offset, cbHeader.romsize);
	}

	const bool wildcardEnabled = FindInVectByName(addArchives, "*") != addArchives.cend();
	const cbfile* bootArchivePtr = nullptr;
	for (u32 i=cbHeader.offset; i<cbHeader.romsize;)
	{
		const auto cbFilePtr = reinterpret_cast<const cbfile*>(&fileBuf[i]);
		if (memcmp(&cbFilePtr->magic, LARCHIVE_MAGIC, sizeof(cbFilePtr->magic)) != 0)
		{
			fprintf(stderr, "Not a LARCHIVE at offset: 0x%08x of %s, are we done ?\n", i, cbfsFilename);
			return 0;
		}

		auto cbFile = cbfile().fromBigEndian(cbFilePtr);
		const auto areaStart = i;
		const auto areaEnd = i+cbFile.offset+cbFile.len;
		fprintf(stderr, "\t%s @0x%08x data: 0x%08x size 0x%08x bytes: ", cbFilePtr->filename, areaStart, areaEnd-cbFile.len, cbFile.len);		
		
		i = align_up(areaEnd, cbHeader.align);
		if (!wildcardEnabled && FindInVectByName(addArchives, cbFilePtr->filename) == addArchives.cend())
		{
			fprintf(stderr, "Skipped due to not being in addArchives.\n");
			continue;
		}
		if (FindInVectByName(skipArchives, cbFilePtr->filename) != skipArchives.cend() && FindInVectByName(addArchives, cbFilePtr->filename) == addArchives.cend())
		{
			fprintf(stderr, "Skipped due to being in skipArchives (and not in addArchives).\n");
			continue;
		}
		if (strlen(cbFilePtr->filename) == 0 && cbFile.type == COMPONENT_NULL) //padding section, can be skipped
		{
			fprintf(stderr, "Skipped due to being an empty padding section.\n");
			continue;
		}

		fprintf(stderr, "Added!\n");

		fprintf(outputFile, "[load:%s]\n", cbFilePtr->filename);
		fprintf(outputFile, "if=%s\n", cbfsFilename);
		fprintf(outputFile, "skip=0x%08x\n", areaStart);
		fprintf(outputFile, "count=0x%08x\n", areaEnd-areaStart);
		fprintf(outputFile, "dst=0x%08x\n", loadStartAddress + areaStart);
		fprintf(outputFile, "\n");
		fflush(outputFile);

		if (bootArchiveName != nullptr && stricmp(cbFilePtr->filename, bootArchiveName) == 0)
			bootArchivePtr = cbFilePtr;
	}

	if (bootArchiveName != nullptr && bootArchivePtr == nullptr)
	{
		fprintf(stderr, "Specified archive for booting '%s' not found or skipped\n", bootArchiveName);
		return -4;
	}
	if (bootArchivePtr != nullptr)
	{
		const u8* bootArchiveBytes = (const u8*)(bootArchivePtr);
		auto cbFile = cbfile().fromBigEndian(bootArchivePtr);
		if (cbFile.type == COMPONENT_STAGE)
		{
			const auto stagePtr = reinterpret_cast<const cbfs_stage*>(bootArchiveBytes + cbFile.offset);
			const auto dataPtr = bootArchiveBytes + cbFile.offset + sizeof(cbfs_stage);

			fprintf(outputFile, "[copy:%s]\n", bootArchivePtr->filename);
			fprintf(outputFile, "type=%d\n", stagePtr->compression);
			fprintf(outputFile, "src=0x%08llx\n", u64(loadStartAddress) + u64(dataPtr)-u64(&fileBuf[0]));
			fprintf(outputFile, "srclen=0x%08x\n", stagePtr->len);
			fprintf(outputFile, "dst=0x%08llx\n", stagePtr->load);
			fprintf(outputFile, "dstlen=0x%08x\n", stagePtr->memlen);
			fprintf(outputFile, "\n");

			fprintf(outputFile, "[boot:%s]\n", bootArchivePtr->filename);
			fprintf(outputFile, "pc=0x%08llx\n", stagePtr->entry);
			fprintf(outputFile, "\n");
			fflush(outputFile);
		}
		else if (cbFile.type == COMPONENT_PAYLOAD)
		{
			const auto startPtr = bootArchiveBytes + cbFile.offset;
			auto stagePtr = reinterpret_cast<const cbfs_payload_segment*>(startPtr);
			while (uptr(stagePtr) < uptr(&fileBuf[0]+fileBuf.size()))
			{
				if (stagePtr->type == PAYLOAD_SEGMENT_CODE ||
					stagePtr->type == PAYLOAD_SEGMENT_DATA ||
					stagePtr->type == PAYLOAD_SEGMENT_BSS ||
					stagePtr->type == PAYLOAD_SEGMENT_PARAMS ||
					stagePtr->type == PAYLOAD_SEGMENT_ENTRY)
				{
					const auto stage = cbfs_payload_segment().fromBigEndian(stagePtr);
					if (stage.type == PAYLOAD_SEGMENT_ENTRY)
					{
						fprintf(outputFile, "[boot:%s]\n", bootArchivePtr->filename);
						fprintf(outputFile, "pc=0x%08llx\n", stage.load_addr);
						fprintf(outputFile, "\n");
						fflush(outputFile);
					}
					else
					{
						fprintf(outputFile, "[copy:%s_%s]\n", bootArchivePtr->filename, (const char*)&stagePtr->type);
						fprintf(outputFile, "type=%d\n", stage.compression);
						fprintf(outputFile, "src=0x%08llx\n", u64(loadStartAddress) + u64(startPtr+stage.offset)-u64(&fileBuf[0]));
						fprintf(outputFile, "srclen=0x%08x\n", stage.len);
						fprintf(outputFile, "dst=0x%08llx\n", stage.load_addr);
						fprintf(outputFile, "dstlen=0x%08x\n", stage.mem_len);
						fprintf(outputFile, "\n");
						fflush(outputFile);
					}

					stagePtr++;
				}
				else
					break;
			}
		}		
	}

	return 0;
}