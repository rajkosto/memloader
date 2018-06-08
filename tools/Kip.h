#pragma once
#include "Types.h"
#include <iostream>
#include <stdexcept>

struct Kip
{
	struct Segment
	{
		u32 DstOff;
		u32 DecompSz;
		u32 CompSz;
		u32 Attribute;
	};

	struct Header
	{
		static const size_t KIP_MAGIC_SIZE = sizeof(u32);
		static const u8 KIP_MAGIC_0 = 'K';
		static const u8 KIP_MAGIC_1 = 'I';
		static const u8 KIP_MAGIC_2 = 'P';
		static const u8 KIP_MAGIC_3 = '1';

		u8  Magic[KIP_MAGIC_SIZE];
		u8  Name[0xC];
		u64 TitleId;
		u32 ProcessCategory;
		u8  MainThreadPriority;
		u8  DefaultCpuId;
		u8  Unk;
		u8  Flags;
		Segment Segments[6];
		u32 Capabilities[0x20];

		bool validMagic() const 
		{ 
			return (Magic[0] == KIP_MAGIC_0 &&
					Magic[1] == KIP_MAGIC_1 &&
					Magic[2] == KIP_MAGIC_2 &&
					Magic[3] == KIP_MAGIC_3);
		}

		Header& deserialize(std::istream& src)
		{
			src.read((char*)Magic, KIP_MAGIC_SIZE);
			if (validMagic())
			{
				if (src.eof())
					throw std::runtime_error("KIP1 unexpected end after magic");

				src.read((char*)Name, sizeof(Name));
				if (src.fail()) throw std::runtime_error("KIP1 too small for Name");
					
				src.read((char*)&TitleId, sizeof(TitleId));
				if (src.fail()) throw std::runtime_error("KIP1 too small for TitleId");

				src.read((char*)&ProcessCategory, sizeof(ProcessCategory));
				if (src.fail()) throw std::runtime_error("KIP1 too small for ProcessCategory");

				src.read((char*)&MainThreadPriority, sizeof(MainThreadPriority));
				if (src.fail()) throw std::runtime_error("KIP1 too small for MainThreadPriority");

				src.read((char*)&DefaultCpuId, sizeof(DefaultCpuId));
				if (src.fail()) throw std::runtime_error("KIP1 too small for DefaultCpuId");

				src.read((char*)&Unk, sizeof(Unk));
				if (src.fail()) throw std::runtime_error("KIP1 too small for Unk");

				src.read((char*)&Flags, sizeof(Flags));
				if (src.fail()) throw std::runtime_error("KIP1 too small for Flags");

				for (int i=0; i<6; i++)
				{
					src.read((char*)&Segments[i], sizeof(Segment));
					if (src.fail()) throw std::runtime_error("KIP1 too small for Segment");
				}

				src.read((char*)Capabilities, sizeof(Capabilities));
				if (src.fail()) throw std::runtime_error("KIP1 too small for Capabilities");

				if (src.eof())
					throw std::runtime_error("KIP1 unexpected end after header");
			}
			return *this;
		}
	};
	static_assert(sizeof(Header) == 256, "Kip::Header wrong size");
};