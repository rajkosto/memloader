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

#include "blz.h"

/*----------------------------------------------------------------------------*/
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
static void BLZ_Invert(u8 *buffer, int length)
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
ByteVector BLZ_Code(u8* raw_buffer, unsigned int raw_len, bool best)
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