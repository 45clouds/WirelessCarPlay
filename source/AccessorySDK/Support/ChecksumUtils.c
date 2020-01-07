/*
	File:    	ChecksumUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public 
	Software”, and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive 
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use, 
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and 
	redistribute the Apple Software, with or without modifications, in binary form. While you may not 
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses, 
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be 
	incorporated.  
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug 
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use, 
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products 
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you 
	acknowledge and agree that Apple may exercise the license granted above without the payment of 
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR 
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES 
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE 
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2001-2015 Apple Inc. All Rights Reserved.
*/

#include "ChecksumUtils.h"

//===========================================================================================================================
//	Adler32
//===========================================================================================================================

uint32_t	Adler32Ex( uint32_t inAdler, size_t inChunkSize, const void *inData, size_t inSize )
{
	uint32_t			lo;
	uint32_t			hi;
	const uint8_t *		src;
	const uint8_t *		end;
	const uint8_t *		end2;
	size_t				len;
	
	lo  = inAdler & 0xFFFFU;
	hi  = inAdler >> 16;
	src = (const uint8_t *) inData;
	end = src + inSize;
	while( src != end )
	{
		len = (size_t)( end - src );
		if( len > inChunkSize ) len = inChunkSize;
		end2 = src + len;
		while( src != end2 )
		{
			lo += *src++;
			hi += lo;
		}
		lo %= 65521;
		hi %= 65521;
	}
	return( ( hi << 16 ) | lo );
}

//===========================================================================================================================
//	CRC8_CCITT
//===========================================================================================================================

#define kCRC8_Polynomial	0x07 // CRC-8-CCITT: x^8 + x^2 + x +1 = 0x07, 0xE0 (reversed), or 0x83 (reverse of reciprocal).

uint8_t	CRC8_CCITT( uint8_t inCRC, uint8_t inData )
{
	uint8_t		i;
	uint8_t		crc;
	
	crc = inCRC ^ inData;
	for( i = 0; i < 8; ++i )
	{
		if( crc & 0x80 ) crc = (uint8_t)( ( crc << 1 ) ^ kCRC8_Polynomial );
		else			 crc <<= 1;
	}
	return( crc );
}

//===========================================================================================================================
//	CRC16_CCITT
//
//	This uses a polynomial of 0x1021 -> x^16 (implicit) + x^12 + x^5 + x^0 (same as CRC-16-CCITT).
//	Most code uses an initial value of 0xFFFF.
//===========================================================================================================================

uint16_t	CRC16_CCITT( uint16_t inCRC, const void *inData, size_t inSize )
{
	const uint8_t *		src;
	const uint8_t *		end;
	
	src = (const uint8_t *) inData;
	end = src + inSize;
	while( src != end )
	{
		inCRC  = (uint16_t)( ( inCRC >> 8 ) | ( inCRC << 8 ) );
		inCRC ^= *src++;
		inCRC ^= ( ( inCRC & 0xFF ) >> 4 );
		inCRC ^= (   inCRC << 12  );
		inCRC ^= ( ( inCRC & 0xFF ) << 5 );
	}
	return( inCRC );
}

//===========================================================================================================================
//	CRC16_Xmodem
//
//	This uses a polynomial of 0x1021 -> x^16 (implicit) + x^12 + x^5 + x^0 (same as CRC-16-CCITT, but bit order is different).
//	Most code uses an initial value of 0xFFFF.
//===========================================================================================================================

uint16_t	CRC16_Xmodem( uint16_t inCRC, const void *inData, size_t inSize )
{
	const uint8_t *		src;
	const uint8_t *		end;
	int					i;
	
	src = (const uint8_t *) inData;
	end = src + inSize;
	for( ; src != end; ++src )
	{
		inCRC ^= ( ( (uint16_t) *src ) << 8 );
		for( i = 0; i < 8; ++i )
		{
			if( inCRC & 0x8000 ) inCRC = (uint16_t)( ( inCRC << 1 ) ^ 0x1021 );
			else				 inCRC <<= 1;
		}
	}
	return( inCRC );
}

//===========================================================================================================================
//	CRC32
//===========================================================================================================================

// Note: This uses the same polynomial as zlib: x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

static const uint32_t	kCRC32Table[ 256 ] =
{
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 
	0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 
	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 
	0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 
	0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 
	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 
	0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 
	0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 
	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 
	0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 
	0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 
	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 
	0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 
	0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t	CRC32( uint32_t inCRC, const void *inData, size_t inSize )
{
	const uint8_t *		src;
	const uint8_t *		end;
	
	inCRC ^= 0xFFFFFFFFU;
	src = (const uint8_t *) inData;
	end = src + inSize;
	while( src != end )
	{
		inCRC = kCRC32Table[ ( inCRC ^ ( *src++ ) ) & 0xFF ] ^ ( inCRC >> 8 );
	}
	inCRC ^= 0xFFFFFFFFU;
	return( inCRC );
}

//===========================================================================================================================
//	Fletcher16
//
//	Based on code from <http://en.wikipedia.org/wiki/Fletcher's_checksum>.
//===========================================================================================================================

uint16_t	Fletcher16( const void *inData, size_t inLen )
{
	const uint8_t *		src  = (const uint8_t *) inData;
	uint16_t			sum1 = 0xFF;
	uint16_t			sum2 = 0xFF;
	size_t				len;
	
	while( inLen > 0 )
	{
		len = ( inLen > 20 ) ? 20 : inLen;
		inLen -= len;
		do
		{
			sum1 += *src++;
			sum2 += sum1;
		
		}	while( --len );
		sum1 = ( sum1 & 0xFF ) + ( sum1 >> 8 );
		sum2 = ( sum2 & 0xFF ) + ( sum2 >> 8 );
	}
	sum1 = ( sum1 & 0xFF ) + ( sum1 >> 8 );
	sum2 = ( sum2 & 0xFF ) + ( sum2 >> 8 );
	return( (uint16_t)( ( sum2 << 8 ) | sum1 ) );
}

//===========================================================================================================================
//	FNV1
//===========================================================================================================================

uint32_t	FNV1( const void *inData, size_t inSize )
{
	uint32_t			hash;
	const uint8_t *		p;
	const uint8_t *		q;
	
	// 32-bit Fowler/Noll/Vo (FNV-1) hash. See <http://www.isthe.com/chongo/tech/comp/fnv/>.
	
	hash = 0x811c9dc5U;
	p = (const uint8_t *) inData;
	q = p + inSize;
	while( p != q )
	{
		hash *= 0x01000193;
		hash ^= *p++;
	}
	return( hash );
}

//===========================================================================================================================
//	FNV1a
//===========================================================================================================================

uint32_t	FNV1a( const void *inData, size_t inSize )
{
	uint32_t			hash;
	const uint8_t *		p;
	const uint8_t *		q;
	
	// 32-bit Fowler/Noll/Vo (FNV-1a) hash. See <http://www.isthe.com/chongo/tech/comp/fnv/>.
	
	hash = 0x811c9dc5U;
	p = (const uint8_t *) inData;
	q = p + inSize;
	while( p != q )
	{
		hash ^= *p++;
		hash *= 0x01000193;
	}
	return( hash );
}

//===========================================================================================================================
//	iAPChecksum8
//===========================================================================================================================

uint8_t	iAPChecksum8( const void *inData, size_t inLen )
{
	const int8_t *				src = (const int8_t *) inData;
	const int8_t * const		end = src + inLen;
	int8_t						c = 0;
	
	while( src != end ) c += *src++;
	return( (uint8_t)( -c ) );
}

//===========================================================================================================================
//	iAPChecksum16
//===========================================================================================================================

uint16_t	iAPChecksum16( const void *inData, size_t inLen )
{
	return( (uint16_t)( 0 - Fletcher16( inData, inLen ) ) );
}

//===========================================================================================================================
//	NMEAChecksum
//===========================================================================================================================

uint8_t	NMEAChecksum( const void *inData, size_t inLen )
{
	const uint8_t *				src = (const uint8_t *) inData;
	const uint8_t * const		end = src + ( ( inLen == kSizeCString ) ? strlen( (const char *) inData ) : inLen );
	uint8_t						c = 0;
	
	while( src != end ) c ^= *src++;
	return( c );
}

//===========================================================================================================================
//	Parity32
//
//	Based on code from the book Hacker's Delight section 5-2.
//===========================================================================================================================

uint32_t	Parity32( uint32_t x )
{
	x ^= ( x >>  1 );
	x ^= ( x >>  2 );
	x ^= ( x >>  4 );
	x ^= ( x >>  8 );
	x ^= ( x >> 16 );
	return( x & 1 );
}

//===========================================================================================================================
//	SipHash
//
//	Based on <https://131002.net/siphash/>.
//===========================================================================================================================

#define SipRound() \
	do \
	{ \
		v0 += v1; v1 = ROTL64( v1, 13 ); v1 ^= v0; v0 = ROTL64( v0, 32 ); \
		v2 += v3; v3 = ROTL64( v3, 16 ); v3 ^= v2; \
		v0 += v3; v3 = ROTL64( v3, 21 ); v3 ^= v0; \
		v2 += v1; v1 = ROTL64( v1, 17 ); v1 ^= v2; v2 = ROTL64( v2, 32 ); \
		\
	}	while( 0 )

uint64_t	SipHash( const uint8_t inKey[ 16 ], const void *inSrc, size_t inLen )
{
	const uint8_t *				src  = (const uint8_t *) inSrc;
	size_t const				left = inLen % 8;
	const uint8_t * const		end  = src + ( inLen - left );
	uint64_t					k0, k1, v0, v1, v2, v3, tmp;
	
	k0 = ReadLittle64( &inKey[ 0 ] );
	k1 = ReadLittle64( &inKey[ 8 ] );
	v0 = k0 ^ UINT64_C( 0x736f6d6570736575 ); // 'somepseu'
	v1 = k1 ^ UINT64_C( 0x646f72616e646f6d ); // 'dorandom'
	v2 = k0 ^ UINT64_C( 0x6c7967656e657261 ); // 'lygenera'
	v3 = k1 ^ UINT64_C( 0x7465646279746573 ); // 'tedbytes'
	
	for( ; src != end; src += 8 )
	{
		tmp = ReadLittle64( src );
		v3 ^= tmp;
		SipRound();
		SipRound();
		v0 ^= tmp;
	}
	
	tmp = ( (uint64_t)( inLen & 0xFF ) ) << 56;
	switch( left )
	{
		case 7: tmp |= ( ( (uint64_t) src[ 6 ] ) << 48 );
		case 6: tmp |= ( ( (uint64_t) src[ 5 ] ) << 40 );
		case 5: tmp |= ( ( (uint64_t) src[ 4 ] ) << 32 );
		case 4: tmp |= ( ( (uint64_t) src[ 3 ] ) << 24 );
		case 3: tmp |= ( ( (uint64_t) src[ 2 ] ) << 16 );
		case 2: tmp |= ( ( (uint64_t) src[ 1 ] ) <<  8 );
		case 1: tmp |=   ( (uint64_t) src[ 0 ] );
		default: break;
	}
	v3 ^= tmp;
	SipRound();
	SipRound();
	v0 ^= tmp;
	v2 ^= 0xFF;
	SipRound();
	SipRound();
	SipRound();
	SipRound();
	return( v0 ^ v1 ^ v2 ^ v3 );
}

//===========================================================================================================================
//	ThomasWangHash32
//===========================================================================================================================

uint32_t	ThomasWangHash32( uint32_t key )
{
	// 32-bit integer hash from Thomas Wang. See <http://www.cris.com/~Ttwang/tech/inthash.htm>.
	
	key = ( key << 15 ) - key - 1;
	key =   key ^ ( key >> 12 );
	key =   key + ( key <<  2 );
	key =   key ^ ( key >>  4 );
	key = ( key + ( key <<  3 ) ) + ( key << 11 );
	key =   key ^ ( key >> 16 );
	return( key );
}

//===========================================================================================================================
//	ThomasWangHash64to32
//===========================================================================================================================

uint32_t	ThomasWangHash64to32( uint64_t key )
{
	// 64-bit integer to 32-bit hash based on code from Thomas Wang. See <http://www.cris.com/~Ttwang/tech/inthash.htm>.
	
	key = ( key << 18 ) - key - 1;
	key =   key ^ ( key >> 31 );
	key = ( key + ( key <<  2 ) ) + ( key << 4 );
	key =   key ^ ( key >> 11 );
	key =   key + ( key <<  6 );
	key =   key ^ ( key >> 22 );
	return( (uint32_t)( key & UINT64_C( 0xFFFFFFFF ) ) );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )

/*
	Test vectors from C reference implementation linked from <https://131002.net/siphash/>.
	
	SipHash-2-4 output with
	k = 00 01 02 ...
	and
	in = (empty string)
	in = 00 (1 byte)
	in = 00 01 (2 bytes)
	in = 00 01 02 (3 bytes)
	...
	in = 00 01 02 ... 3e (63 bytes)
*/
static const uint64_t		kSipHashTestVectors[ 64 ] = 
{
	0x310e0edd47db6f72, 0xfd67dc93c539f874, 0x5a4fa9d909806c0d, 0x2d7efbd796666785, 
	0xb7877127e09427cf, 0x8da699cd64557618, 0xcee3fe586e46c9cb, 0x37d1018bf50002ab, 
	0x6224939a79f5f593, 0xb0e4a90bdf82009e, 0xf3b9dd94c5bb5d7a, 0xa7ad6b22462fb3f4, 
	0xfbe50e86bc8f1e75, 0x903d84c02756ea14, 0xeef27a8e90ca23f7, 0xe545be4961ca29a1, 
	0xdb9bc2577fcc2a3f, 0x9447be2cf5e99a69, 0x9cd38d96f0b3c14b, 0xbd6179a71dc96dbb, 
	0x98eea21af25cd6be, 0xc7673b2eb0cbf2d0, 0x883ea3e395675393, 0xc8ce5ccd8c030ca8, 
	0x94af49f6c650adb8, 0xeab8858ade92e1bc, 0xf315bb5bb835d817, 0xadcf6b0763612e2f, 
	0xa5c91da7acaa4dde, 0x716595876650a2a6, 0x28ef495c53a387ad, 0x42c341d8fa92d832, 
	0xce7cf2722f512771, 0xe37859f94623f3a7, 0x381205bb1ab0e012, 0xae97a10fd434e015, 
	0xb4a31508beff4d31, 0x81396229f0907902, 0x4d0cf49ee5d4dcca, 0x5c73336a76d8bf9a, 
	0xd0a704536ba93e0e, 0x925958fcd6420cad, 0xa915c29bc8067318, 0x952b79f3bc0aa6d4, 
	0xf21df2e41d4535f9, 0x87577519048f53a9, 0x10a56cf5dfcd9adb, 0xeb75095ccd986cd0, 
	0x51a9cb9ecba312e6, 0x96afadfc2ce666c7, 0x72fe52975a4364ee, 0x5a1645b276d592a1, 
	0xb274cb8ebf87870a, 0x6f9bb4203de7b381, 0xeaecb2a30b22a87f, 0x9924a43cc1315724, 
	0xbd838d3aafbf8db7, 0x0b1a2a3265d51aea, 0x135079a3231ce660, 0x932b2846e4d70666, 
	0xe1915f5cb1eca46c, 0xf325965ca16d629f, 0x575ff28e60381be5, 0x724506eb4c328a95
};

//===========================================================================================================================
//	ChecksumUtils_Test
//===========================================================================================================================

OSStatus	ChecksumUtils_Test( void )
{
	OSStatus			err;
	uint32_t			u32;
	uint64_t			u64;
	uint8_t				buf[ 256 ];
	uint8_t				key[ 16 ];
	size_t				i;
	const char *		cptr;
	
	// Adler32
	
	u32 = Adler32( 1, NULL, 0 );
	require_action( u32 == 1, exit, err = kResponseErr );
	
	u32 = Adler32( 1, "A", 1 );
	require_action( u32 == UINT32_C( 0x00420042 ), exit, err = kResponseErr );
	
	for( i = 0; i < sizeof( buf ); ++i ) buf[ i ] = 'A';
	u32 = Adler32( 1, buf, i );
	require_action( u32 == UINT32_C( 0xA3604101 ), exit, err = kResponseErr );
	
	for( i = 0; i < 67; ++i ) buf[ i ] = (uint8_t) i;
	u32 = Adler32( 1, buf, i );
	require_action( u32 == UINT32_C( 0xC40708A4 ), exit, err = kResponseErr );
	
	for( i = 0; i < sizeof( buf ); ++i ) buf[ i ] = (uint8_t) i;
	u32 = Adler32( 1, buf, i );
	require_action( u32 == UINT32_C( 0xADF67F81 ), exit, err = kResponseErr );
	
	for( i = 0; i < sizeof( buf ); ++i ) buf[ i ] = 'A';
	u32 = AppleAdler32( 1, buf, i );
	require_action( u32 == UINT32_C( 0xA3604101 ), exit, err = kResponseErr );
	
	// CRC8_CCITT
	
	require_action( CRC8_CCITT( 0x00, 0x16 ) == 0x62, exit, err = -1 );
	require_action( CRC8_CCITT( 0x62, 0x12 ) == 0x57, exit, err = -1 );
	require_action( CRC8_CCITT( 0x57, 0xe4 ) == 0x10, exit, err = -1 );
	require_action( CRC8_CCITT( 0x10, 0x00 ) == 0x70, exit, err = -1 );
	require_action( CRC8_CCITT( 0x70, 0xff ) == 0xa4, exit, err = -1 );
	require_action( CRC8_CCITT( 0xa4, 0xa4 ) == 0x00, exit, err = -1 );
	
	// CRC16_CCITT
	
	require_action( CRC16_CCITT( 0xFFFFU, "A", 1 ) == 0xB915, exit, err = -1 );
	require_action( CRC16_CCITT( 0xFFFFU, "123456789", 9 ) == 0x29B1, exit, err = -1 );
	require_action( CRC16_CCITT( 0xFFFFU, "abcdefghijklmnopqrstuvwxyz0123456789", 36 ) == 0x4AEB, exit, err = -1 );
	check_compile_time_code( sizeof( buf ) >= 256 );
	for( i = 0; i < 256; ++i ) buf[ i ] = (uint8_t) i;
	require_action( CRC16_CCITT( 0xFFFFU, buf, 256 ) == 0x3FBD, exit, err = -1 );
	
	// CRC32
	
	u32 = CRC32( 0, NULL, 0 );
	require_action( u32 == 0, exit, err = kResponseErr );
	
	u32 = CRC32( 0, "A", 1 );
	require_action( u32 == UINT32_C( 0xD3D99E8B ), exit, err = kResponseErr );
	
	for( i = 0; i < sizeof( buf ); ++i ) buf[ i ] = 'A';
	u32 = CRC32( 0, buf, i );
	require_action( u32 == UINT32_C( 0x49975B13 ), exit, err = kResponseErr );
	
	for( i = 0; i < 67; ++i ) buf[ i ] = (uint8_t) i;
	u32 = CRC32( 0, buf, i );
	require_action( u32 == UINT32_C( 0xA4853F19 ), exit, err = kResponseErr );
	
	for( i = 0; i < sizeof( buf ); ++i ) buf[ i ] = (uint8_t) i;
	u32 = CRC32( 0, buf, i );
	require_action( u32 == UINT32_C( 0x29058C73 ), exit, err = kResponseErr );
	
	// Fletcher16
	
	cptr = "\x01\x02";
	require_action( Fletcher16( cptr, 2 ) == 0x0403, exit, err = -1 );
	
	// FNV
	
	require_action( FNV1( "", 0 ) == 0x811c9dc5U, exit, err = -1 );
	require_action( FNV1( "hello", 5 ) == 0xb6fa7167U, exit, err = -1 );
	require_action( FNV1( "\xff\x00\x00\x04", 4 ) == 0xb78320a4U, exit, err = -1 );
	require_action( FNV1( "http://www.nature.nps.gov/air/webcams/parks/havoso2alert/timelines_24.cfm", 73 ) == 0x878c0ec9U, 
		exit, err = -1 );
	
	require_action( FNV1a( "", 0 ) == 0x811c9dc5U, exit, err = -1 );
	require_action( FNV1a( "hello", 5 ) == 0x4f9f2cabU, exit, err = -1 );
	require_action( FNV1a( "\xff\x00\x00\x04", 4 ) == 0xbf8fb08eU, exit, err = -1 );
	require_action( FNV1a( "http://www.nature.nps.gov/air/webcams/parks/havoso2alert/timelines_24.cfm", 73 ) == 0x9a8b6805U, 
		exit, err = -1 );
	
	// iAPChecksum8
	
	cptr = "\xFF\x5A\x00\x1A\x80\x2B\x00\x00";
	require_action( iAPChecksum8( cptr, 8 ) == 0xE2, exit, err = -1 );
	
	cptr = "\x01\x05\x10\x00\x04\x0B\x00\x17\x03\x03\x0A\x00\x01\x0B\x02\x01";
	require_action( iAPChecksum8( cptr, 16 ) == 0xA5, exit, err = -1 );
	
	// NMEAChecksum
	
	cptr = "GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,";
	require_action( NMEAChecksum( cptr, kSizeCString ) == 0x76, exit, err = -1 );
	
	cptr = "GPRMC,092751.000,A,5321.6802,N,00630.3371,W,0.06,31.66,280511,,,A";
	require_action( NMEAChecksum( cptr, kSizeCString ) == 0x45, exit, err = -1 );
	
	// Parity32
	
	require_action( Parity32( 0x87ee2d0b ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87ee2d08 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87ee2d07 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87ee2d0d ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87ee2d04 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87ee2d02 ) == 1, exit, err = -1 );
	
	require_action( Parity32( 0x87eec70a ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec709 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec706 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec70c ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec75c ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec705 ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec75f ) == 1, exit, err = -1 );
	require_action( Parity32( 0x87eec703 ) == 1, exit, err = -1 );
	
	// SipHash
	
	for( i = 0; i < countof( key ); ++i ) key[ i ] = (uint8_t) i;
	for( i = 0; i < 64; ++i )
	{
		buf[ i ] = (uint8_t) i;
		u64 = SipHash( key, buf, i );
		u64 = hton64( u64 ); // Swap so we can use the official test vectors (little endian).
		require_action( u64 == kSipHashTestVectors[ i ], exit, err = -1 );
	}
	err = kNoErr;
	
exit:
	printf( "ChecksumUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
