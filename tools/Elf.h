#pragma once
#include "Types.h"
#include <iostream>

struct Elf
{
	enum Machine : u16 // e_machine
	{
		EM_PPC = 20,		//PowerPC
		EM_ARM = 40,		//ARM
		EM_AARCH64 = 183	//ARM 64-bit architecture (AArch64)
	};

	enum Encoding : u8 // e_encoding
	{
		ELFDATANONE = 0,
		ELFDATA2LSB = 1,
		ELFDATA2MSB = 2
	};

	enum Class : u8 // e_class
	{
		ELFCLASSNONE = 0,
		ELFCLASS32 = 1,
		ELFCLASS64 = 2
	};

	enum Version : uint32_t // e_elf_version
	{
		EV_NONE = 0,
		EV_CURRENT = 1,
	};

	enum FileType : uint32_t // e_type
	{
		ET_NONE = 0,         // No file type
		ET_REL = 1,          // Relocatable file
		ET_EXEC = 2,         // Executable file
		ET_DYN = 3,          // Shared object file
		ET_CORE = 4,         // Core file
		ET_LOOS = 0xfe00,	 // Start of OS-specific codes
		ET_HIOS = 0xfeff,	 // End of OS-specific codes
		ET_LOPROC = 0xff00,  // Beginning of processor-specific codes
		ET_HIPROC = 0xffff   // End of Processor-specific
	};

	enum SectionFlags : uint32_t // sh_flags
	{
		SHF_WRITE = 0x1,
		SHF_ALLOC = 0x2,
		SHF_EXECINSTR = 0x4,
		SHF_DEFLATED = 0x08000000,
		SHF_MASKPROC = 0xF0000000,
	};

	enum SectionType : uint32_t // sh_type
	{
		SHT_NULL = 0,                 // No associated section (inactive entry).
		SHT_PROGBITS = 1,             // Program-defined contents.
		SHT_SYMTAB = 2,               // Symbol table.
		SHT_STRTAB = 3,               // String table.
		SHT_RELA = 4,                 // Relocation entries; explicit addends.
		SHT_HASH = 5,                 // Symbol hash table.
		SHT_DYNAMIC = 6,              // Information for dynamic linking.
		SHT_NOTE = 7,                 // Information about the file.
		SHT_NOBITS = 8,               // Data occupies no space in the file.
		SHT_REL = 9,                  // Relocation entries; no explicit addends.
		SHT_SHLIB = 10,               // Reserved.
		SHT_DYNSYM = 11,              // Symbol table.
		SHT_INIT_ARRAY = 14,          // Pointers to initialization functions.
		SHT_FINI_ARRAY = 15,          // Pointers to termination functions.
		SHT_PREINIT_ARRAY = 16,       // Pointers to pre-init functions.
		SHT_GROUP = 17,               // Section group.
		SHT_SYMTAB_SHNDX = 18,        // Indices for SHN_XINDEX entries.
		SHT_GNU_HASH = 0x6ffffff6,
		SHT_LOPROC = 0x70000000,      // Lowest processor arch-specific type.
		SHT_HIPROC = 0x7fffffff,      // Highest processor arch-specific type.
		SHT_LOUSER = 0x80000000,      // Lowest type reserved for applications.
		SHT_HIUSER = 0xffffffff       // Highest type reserved for applications.
	};

	enum SegmentType : uint32_t // p_flags
	{
		PT_NULL = 0,
		PT_LOAD = 1,
		PT_DYNAMIC = 2,
		PT_INTERP = 3,
		PT_NOTE = 4,
		PT_SHLIB = 5,
		PT_PHDR = 6,
		PT_TLS = 7,
		PT_LOOS = 0x60000000,
		PT_HIOS = 0x6fffffff,
		PT_LOPROC = 0x70000000,
		PT_HIPROC = 0x7fffffff,
	};

	enum SegmentFlags : uint32_t // p_flags
	{
		PF_EXEC = 1,
		PF_WRITE = 2,
		PF_READ = 4
	};

	template<size_t bitness>
	struct Types {};

	template<>
	struct Types<32>
	{
		typedef u32	Addr;
		typedef u16	Half;
		typedef s16	SHalf;
		typedef u32	Off;
		typedef s32	SWord;
		typedef u32 Word;
		typedef u32 Size;
		typedef s32 SLong;
	};

	template<>
	struct Types<64>
	{
		typedef u64	Addr;
		typedef u16	Half;
		typedef s16	SHalf;
		typedef u64	Off;
		typedef s32	SWord;
		typedef u32 Word;
		typedef u64 Size;
		typedef s64 SLong;
	};

	struct HeaderBase
	{
		static const size_t ELF_NIDENT = 16;
		enum ElfIdent
		{
			EI_MAG0			= 0, // 0x7F
			EI_MAG1			= 1, // 'E'
			EI_MAG2			= 2, // 'L'
			EI_MAG3			= 3, // 'F'
			EI_CLASS		= 4, // Architecture (32/64)
			EI_DATA			= 5, // Byte Order
			EI_VERSION		= 6, // ELF Version
			EI_OSABI		= 7, // OS Specific
			EI_ABIVERSION	= 8, // OS Specific
			EI_PAD			= 9	 // Padding
		};

		static const u8 ELF_MAGIC_0 = 0x7F;
		static const u8 ELF_MAGIC_1 = 'E';
		static const u8 ELF_MAGIC_2 = 'L';
		static const u8 ELF_MAGIC_3 = 'F';

		u8 ident[ELF_NIDENT]; // File identification, class, encoding, version, ABI
		bool validMagic() const 
		{ 
			return (ident[EI_MAG0] == ELF_MAGIC_0 &&
					ident[EI_MAG1] == ELF_MAGIC_1 &&
					ident[EI_MAG2] == ELF_MAGIC_2 &&
					ident[EI_MAG3] == ELF_MAGIC_3);
		}

		HeaderBase& deserialize(std::istream& src)
		{
			src.read((char*)this->ident, ELF_NIDENT);
			return *this;
		}
	};
	static_assert(sizeof(HeaderBase) == HeaderBase::ELF_NIDENT, "Elf::HeaderBase wrong size");

	template<size_t bitness>
	struct Header : public HeaderBase
	{
		using T = Types<bitness>;

		typename T::Half type;        // Type of file (ET_*)
		typename T::Half machine;     // Required architecture for this file (EM_*)
		typename T::Word version;     // Must be equal to 1
		typename T::Addr entry;       // Address to jump to in order to start program
		typename T::Off phoff;       // Program header table's file offset, in bytes
		typename T::Off shoff;       // Section header table's file offset, in bytes
		typename T::Word flags;       // Processor-specific flags
		typename T::Half ehsize;      // Size of ELF header, in bytes
		typename T::Half phentsize;   // Size of an entry in the program header table
		typename T::Half phnum;       // Number of entries in the program header table
		typename T::Half shentsize;   // Size of an entry in the section header table
		typename T::Half shnum;       // Number of entries in the section header table
		typename T::Half shstrndx;    // Sect hdr table index of sect name string table

		Header& deserialize(std::istream& src)
		{
			HeaderBase::deserialize(src);
			src.read((char*)&this->type, sizeof(this->type));
			src.read((char*)&this->machine, sizeof(this->machine));
			src.read((char*)&this->version, sizeof(this->version));
			src.read((char*)&this->entry, sizeof(this->entry));
			src.read((char*)&this->phoff, sizeof(this->phoff));
			src.read((char*)&this->shoff, sizeof(this->shoff));
			src.read((char*)&this->flags, sizeof(this->flags));
			src.read((char*)&this->ehsize, sizeof(this->ehsize));
			src.read((char*)&this->phentsize, sizeof(this->phentsize));
			src.read((char*)&this->phnum, sizeof(this->phnum));
			src.read((char*)&this->shentsize, sizeof(this->shentsize));
			src.read((char*)&this->shnum, sizeof(this->shnum));
			src.read((char*)&this->shstrndx, sizeof(this->shstrndx));
			return *this;
		}
	};
	static_assert(sizeof(Header<32>) == 0x34, "Elf::Header32 wrong size");
	static_assert(sizeof(Header<64>) == 0x40, "Elf::Header64 wrong size");

	template<size_t bitness>
	struct SectionHeader
	{
		using T = Types<bitness>;

		typename T::Word name;      // Section name (index into string table)
		typename T::Word type;      // Section type (SHT_*)
		typename T::Size flags;     // Section flags (SHF_*)
		typename T::Addr addr;      // Address where section is to be loaded
		typename T::Off offset;    // File offset of section data, in bytes
		typename T::Size size;      // Size of section, in bytes
		typename T::Word link;      // Section type-specific header table index link
		typename T::Word info;      // Section type-specific extra information
		typename T::Size addralign; // Section address alignment
		typename T::Size entsize;   // Size of records contained within the section

		SectionHeader& setType(SectionType arg) { type = arg; return *this; }
		SectionHeader& setFlags(typename T::Size arg) { flags = arg; return *this; }
		SectionHeader& setAddr(typename T::Addr arg) { addr = arg; return *this; }
		SectionHeader& setSize(typename T::Size arg) { size = arg; return *this; }
		SectionHeader& setLink(typename T::Word arg) { link = arg; return *this; }
		SectionHeader& setInfo(typename T::Word arg) { info = arg; return *this; }
		SectionHeader& setAlign(typename T::Size arg) { addralign = arg; return *this; }
		SectionHeader& setEntSize(typename T::Size arg) { entsize = arg; return *this; }

		template<size_t otherbitness>
		SectionHeader& operator=(const SectionHeader<otherbitness>& rhs)
		{
			this->name = rhs.name;
			this->type = rhs.type;
			this->flags = rhs.flags;
			this->addr = rhs.addr;
			this->offset = rhs.offset;
			this->size = rhs.size;
			this->link = rhs.link;
			this->info = rhs.info;
			this->addralign = rhs.addralign;
			this->entsize = rhs.entsize;

			return *this;
		}

		SectionHeader& deserialize(std::istream& src)
		{
			src.read((char*)&this->name, sizeof(this->name));
			src.read((char*)&this->type, sizeof(this->type));
			src.read((char*)&this->flags, sizeof(this->flags));
			src.read((char*)&this->addr, sizeof(this->addr));
			src.read((char*)&this->offset, sizeof(this->offset));
			src.read((char*)&this->size, sizeof(this->size));
			src.read((char*)&this->link, sizeof(this->link));
			src.read((char*)&this->info, sizeof(this->info));
			src.read((char*)&this->addralign, sizeof(this->addralign));
			src.read((char*)&this->entsize, sizeof(this->entsize));
			return *this;
		}
	};
	static_assert(sizeof(SectionHeader<32>) == 0x28, "Elf::SectionHeader32 wrong size");
	static_assert(sizeof(SectionHeader<64>) == 0x40, "Elf::SectionHeader64 wrong size");

	template<size_t bitness>
	struct ProgramHeader
	{
		using T = Types<bitness>;

		typename T::Word type;
		typename T::Word flags;
		typename T::Off offset;
		typename T::Addr vaddr;
		typename T::Addr paddr;
		typename T::Size filesz;
		typename T::Size memsz;
		typename T::Size align;

		bool readable() const { return (flags & Elf::SegmentFlags::PF_READ) != 0; }
		bool writable() const { return (flags & Elf::SegmentFlags::PF_WRITE) != 0; }
		bool executable() const { return (flags & Elf::SegmentFlags::PF_EXEC) != 0; }

		template<size_t otherbitness>
		ProgramHeader& operator=(const ProgramHeader<otherbitness>& rhs)
		{
			this->type = rhs.type;
			this->flags = rhs.flags;
			this->offset = rhs.offset;
			this->vaddr = rhs.vaddr;
			this->paddr = rhs.paddr;
			this->filesz = rhs.filesz;
			this->memsz = rhs.memsz;
			this->align = rhs.align;

			return *this;
		}

		ProgramHeader& deserialize(std::istream& src)
		{
			src.read((char*)&this->type, sizeof(this->type));

			if (bitness == 64)
				src.read((char*)&this->flags, sizeof(this->flags));

			src.read((char*)&this->offset, sizeof(this->offset));
			src.read((char*)&this->vaddr, sizeof(this->vaddr));
			src.read((char*)&this->paddr, sizeof(this->paddr));
			src.read((char*)&this->filesz, sizeof(this->filesz));
			src.read((char*)&this->memsz, sizeof(this->memsz));

			if (bitness == 32)
				src.read((char*)&this->flags, sizeof(this->flags));

			src.read((char*)&this->align, sizeof(this->align));

			return *this;
		}
	};
	static_assert(sizeof(ProgramHeader<32>) == 0x20, "Elf::ProgramHeader32 wrong size");
	static_assert(sizeof(ProgramHeader<64>) == 0x38, "Elf::ProgramHeader64 wrong size");
};