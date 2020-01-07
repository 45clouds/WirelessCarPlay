/*
	File:    	ChecksumUtils.h
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
	
	Copyright (C) 2001-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__ChecksumUtils_h__
#define	__ChecksumUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Adler32
	@abstract	Generates a 32-bit Adler-32 checksum of the specified data.
	@discussion	Apple checksums (compatible with AirPort, Mac OS, Open Firmware, etc.) use a chunk size of 5000.
				Normal checksums, such as those compatible with zlib, use a chunk size of 5552.
*/
#define		AppleAdler32( ADLER, PTR, LEN )		Adler32Ex( (ADLER), 5000, (PTR), (LEN) )
#define		Adler32( ADLER, PTR, LEN )			Adler32Ex( (ADLER), 5552, (PTR), (LEN) )
uint32_t	Adler32Ex( uint32_t inAdler, size_t inChunkSize, const void *inData, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CRC8_CCITT
	@abstract	Updates a CRC-8-CCITT CRC with an input byte. Start with 0x00.
	@discussion	CRC-8-CCITT polynomial: x^8 + x^2 + x +1 = 0x07, 0xE0 (reversed), or 0x83 (reverse of reciprocal).
*/
uint8_t	CRC8_CCITT( uint8_t inCRC, uint8_t inData );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CRC16_CCITT
	@discussion
	
	This uses a polynomial of 0x1021 -> x^16 (implicit) + x^12 + x^5 + x^0 (same as CRC-16-CCITT).
	Most code uses an initial value of 0xFFFF.
*/
uint16_t	CRC16_CCITT( uint16_t inCRC, const void *inData, size_t inSize );
uint16_t	CRC16_Xmodem( uint16_t inCRC, const void *inData, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CRC32
	@abstract	Generates a 32-bit CRC-32 checksum of the specified data. Start with 0.
	@discussion	This uses the same polynomial as zlib: x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.
*/
uint32_t	CRC32( uint32_t inCRC, const void *inData, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Fletcher16
	@abstract	16-bit Fletcher checksum. See <http://en.wikipedia.org/wiki/Fletcher's_checksum>.
*/
uint16_t	Fletcher16( const void *inData, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	FNV1
	@abstract	32-bit Fowler/Noll/Vo (FNV-1) hash. See <http://www.isthe.com/chongo/tech/comp/fnv/>.
*/
uint32_t	FNV1( const void *inData, size_t inSize );
uint32_t	FNV1a( const void *inData, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	iAPChecksum8
	@abstract	8-bit checksum for iPod Accessory Protocol (iAP) packets.
*/
uint8_t	iAPChecksum8( const void *inData, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	iAPChecksum16
	@abstract	16-bit checksum for iPod Accessory Protocol (iAP) packets.
*/
uint16_t	iAPChecksum16( const void *inData, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NMEAChecksum
	@abstract	Checksum for NMEA 0183 messages. See <http://en.wikipedia.org/wiki/NMEA_0183>.
*/
uint8_t	NMEAChecksum( const void *inData, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Parity32
	@abstract	Computers parity for a 32-bit value.
	@result		1=odd parity. 0=even parity.
*/
uint32_t	Parity32( uint32_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PtrHash
	@abstract	Evenly distributes a pointer value so it's suitable as a hash function.
	@discussion	See <http://www.concentric.net/~Ttwang/tech/addrhash.htm>.
*/
#define PtrHash( PTR, ALIGNMENT )		( ( ( (uintptr_t)(PTR) ) >> (ALIGNMENT) ) * 2654435761 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SipHash
	@abstract	Fast, short-input PRF. See <https://131002.net/siphash/>.
*/
uint64_t	SipHash( const uint8_t inKey[ 16 ], const void *inSrc, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ThomasWangHash32/ThomasWangHash64to32
	@abstract	32-bit and 64-bit integer to 32-bit hash based on code from Thomas Wang. 
				See <http://www.cris.com/~Ttwang/tech/inthash.htm>.
*/
uint32_t	ThomasWangHash32( uint32_t key );
uint32_t	ThomasWangHash64to32( uint64_t inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MiscUtilsTest
	@abstract	Unit test.
*/
OSStatus	ChecksumUtils_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __ChecksumUtils_h__
