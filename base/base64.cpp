/*****************************************************************************
 * Author:   Valient Gough <vgough@pobox.com>
 *
 *****************************************************************************
 * Copyright (c) 2002-2004, Valient Gough
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.  
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base/base64.h"

#include <ctype.h>
#include <glog/logging.h>

namespace encfs {

// change between two powers of two, stored as the low bits of the bytes in the
// arrays.
// It is the caller's responsibility to make sure the output array is large
// enough.
void changeBase2(byte *src, int srcLen, int src2Pow,
                 byte *dst, int dstLen, int dst2Pow)
{
    unsigned long work = 0;
    int workBits = 0; // number of bits left in the work buffer
    byte *end = src + srcLen;
    byte *origDst = dst;
    const int mask = (1 << dst2Pow) -1;

    // copy the new bits onto the high bits of the stream.
    // The bits that fall off the low end are the output bits.
    while(src != end)
    {
	work |= ((unsigned long)(*src++)) << workBits;
	workBits += src2Pow;

	while(workBits >= dst2Pow)
	{
	    *dst++ = work & mask;
	    work >>= dst2Pow;
	    workBits -= dst2Pow;
	}
    }

    // now, we could have a partial value left in the work buffer..
    if(workBits && ((dst - origDst) < dstLen))
	*dst++ = work & mask;
}

/*
    Same as changeBase2, except the output is written over the input data.  The
    output is assumed to be large enough to accept the data.

    Uses the stack to store output values.  Recurse every time a new value is
    to be written, then write the value at the tail end of the recursion.
*/
static
void changeBase2Inline(byte *src, int srcLen, 
	               int src2Pow, int dst2Pow,
		       bool outputPartialLastByte,
		       unsigned long work,
		       int workBits,
		       byte *outLoc)
{
    const int mask = (1 << dst2Pow) -1;
    if(!outLoc)
	outLoc = src;

    // copy the new bits onto the high bits of the stream.
    // The bits that fall off the low end are the output bits.
    while(srcLen && workBits < dst2Pow)
    {
	work |= ((unsigned long)(*src++)) << workBits;
	workBits += src2Pow;
	--srcLen;
    }

    // we have at least one value that can be output
    byte outVal = work & mask;
    work >>= dst2Pow;
    workBits -= dst2Pow;

    if(srcLen)
    {
	// more input left, so recurse
	changeBase2Inline( src, srcLen, src2Pow, dst2Pow,
		           outputPartialLastByte, work, workBits, outLoc+1);
	*outLoc = outVal;
    } else
    {
	// no input left, we can write remaining values directly
	*outLoc++ = outVal;

	// we could have a partial value left in the work buffer..
        if(outputPartialLastByte)
        {
            while(workBits > 0)
            {
                *outLoc++ = work & mask;
                work >>= dst2Pow;
                workBits -= dst2Pow;
            }
        }
    }
}

void changeBase2Inline(byte *src, int srcLen, 
	               int src2Pow, int dst2Pow,
		       bool outputPartialLastByte)
{
    changeBase2Inline(src, srcLen, src2Pow, dst2Pow, 
	    outputPartialLastByte, 0, 0, 0);
}


// character set for ascii b64:
// ",-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
// a standard base64 (eg a64l doesn't use ',-' but uses './'.  We don't
// do that because '/' is a reserved character, and it is useful not to have
// '.' included in the encrypted names, so that it can be reserved for files
// with special meaning.
static const char B642AsciiTable[] = ",-0123456789";
void B64ToAscii(byte *in, int length)
{
  for(int offset=0; offset<length; ++offset)
  {
    int ch = in[offset];
    if(ch > 11)
    {
      if(ch > 37)
        ch += 'a' - 38;
      else
        ch += 'A' - 12;
    } else
      ch = B642AsciiTable[ ch ];

    in[offset] = ch;
  }
}

static const byte Ascii2B64Table[] = 
       "                                            01  23456789:;       ";
    //  0123456789 123456789 123456789 123456789 123456789 123456789 1234
    //  0         1         2         3         4         5         6
void AsciiToB64(byte *in, int length)
{
    return AsciiToB64(in, in, length);
}

void AsciiToB64(byte *out, const byte *in, int length)
{
  while(length--)
  {
    byte ch = *in++;
    if(ch >= 'A')
    {
      if(ch >= 'a')
        ch += 38 - 'a';
      else
        ch += 12 - 'A';
    } else
      ch = Ascii2B64Table[ ch ] - '0';

    *out++ = ch;
  }
}


void B32ToAscii(byte *buf, int len)
{
  for(int offset=0; offset<len; ++offset)
  {
    int ch = buf[offset];
    if (ch >= 0 && ch < 26)
      ch += 'A';
    else
      ch += '2' - 26;

    buf[offset] = ch;
  }
}

void AsciiToB32(byte *in, int length)
{
  return AsciiToB32(in, in, length);
}

void AsciiToB32(byte *out, const byte *in, int length)
{
  while(length--)
  {
    byte ch = *in++;
    int lch = toupper(ch);
    if (lch >= 'A')
      lch -= 'A';
    else
      lch += 26 - '2';

    *out++ = (byte)lch;
  }
}


#define WHITESPACE 64
#define EQUALS     65
#define INVALID    66
 
static const byte d[] = {
    66,66,66,66,66,66,66,66,66,64,
    66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,
    66,66,66,62,66,66,66,63,52,53,

    54,55,56,57,58,59,60,61,66,66, // 50-59
    66,65,66,66,66, 0, 1, 2, 3, 4,
     5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,
    25,66,66,66,66,66,66,26,27,28, 

    29,30,31,32,33,34,35,36,37,38, // 100-109
    39,40,41,42,43,44,45,46,47,48,
    49,50,51
};
 
bool B64StandardDecode(byte *out, const byte *in, int inLen) { 
  const byte *end = in + inLen;
  size_t buf = 1;
 
  while (in < end) {
    byte v = *in++;
    if (v > 'z') {
      LOG(ERROR) << "Invalid character: " << (unsigned int)v;
      return false;
    }
    byte c = d[v];

    switch (c) {
    case WHITESPACE: continue;   /* skip whitespace */
    case INVALID:
      LOG(ERROR) << "Invalid character: " << (unsigned int)v;
      return false; /* invalid input, return error */
    case EQUALS:                 /* pad character, end of data */
      in = end;
      continue;
    default:
      buf = buf << 6 | c;
 
      /* If the buffer is full, split it into bytes */
      if (buf & 0x1000000) {
        *out++ = buf >> 16;
        *out++ = buf >> 8;
        *out++ = buf;
        buf = 1;
      }   
    }
  }

  if (buf & 0x40000) {
    *out++ = buf >> 10;
    *out++ = buf >> 2;
  }
  else if (buf & 0x1000) {
    *out++ = buf >> 4;
  }

  return true;
}

}  // namespace encfs
