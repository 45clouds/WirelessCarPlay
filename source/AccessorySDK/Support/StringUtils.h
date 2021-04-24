/*
	File:    	StringUtils.h
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
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
	
	Copyright (C) 2001-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef	__StringUtils_h__
#define	__StringUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_OS_BSD )
	#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Conversions ==
#endif

#define kStringMaxIPv4	27
#define kStringMaxIPv6	81
#define kStringMaxMAC	18 	// 6 byte MAC address * 3 

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	StringToIPAddressFlags
	@abstract	Flags to control how IP address strings are parsed.
*/
typedef uint32_t	StringToIPAddressFlags;

#define	kStringToIPAddressFlagsNone				0			//! No flags.
#define	kStringToIPAddressFlagsNoPort			( 1 << 0 )	//! Don't allow a port number to be specified.
#define	kStringToIPAddressFlagsNoPrefix			( 1 << 1 )	//! Don't allow a prefix length to be specified.
#define	kStringToIPAddressFlagsNoScope			( 1 << 2 )	//! Don't allow a scope ID to be specified.
#define	kStringToIPAddressFlagsNoIPv4Mapped		( 1 << 3 )	//! Don't allow IPv4-mapped/compatible IPv6 addresses.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	StringToIPv6Address
	@abstract	Converts an IPv6 address string.
	
	@param		inStr		String to convert.
	@param		inFlags		Flags to control parsing.
	@param		outAddr		Receives 128-bit (16 byte) IPv6 address in network byte order. May be NULL.
	@param		outScope	Receives scope ID if found. Suitable for the sin6_scope_id field of sockaddr_in6. May be NULL.
	@param		outPort		Receives port number if found. May be NULL.
	@param		outPrefix	Receives prefix length if found. May be NULL.
	@param		outStr		Receives ptr to the end of the parsed input string. May be NULL.
	
	@discussion:
	
	Examples:
		
		[fe80::5445:5245:444f%5]:80/64		IPv6 with scope ID 5, port 80, and prefix length of 64 bits.
		[fe80::5445:5245:444f%5]:80			IPv6 with scope ID 5 and port 80.
		[fe80::5445:5245:444f%en0]:80		IPv6 with scope ID en0 and port 80. Note: textual scopes only on Unix systems.
		fe80::5445:5245:444f%5				IPv6 with scope ID 5 and no port.
		fe80::5445:5245:444f				IPv6 with no scope ID and no port.
		::									IPv6 unspecified address.
		::1									IPv6 loopback address.
*/
OSStatus
	StringToIPv6Address( 
		const char *			inStr, 
		StringToIPAddressFlags	inFlags, 
		uint8_t					outAddr[ 16 ], 
		uint32_t *				outScope, 
		int *					outPort, 
		int *					outPrefix, 
		const char **			outStr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	StringToIPv4Address
	@abstract	Converts an IPv4 address string.
	
	@param		inStr		String to convert.
	@param		inFlags		Flags to control parsing.
	@param		outIP		Receives 32-bit IPv4 address in host byte order. May be NULL.
	@param		outPort		Receives port number if found. May be NULL.
	@param		outSubnet	Receives 32-bit subnet mask if a prefix length was found. May be NULL.
	@param		outRouter	Receives 32-bit default router IPv4 address if a prefix length was found. May be NULL.
	@param		outPtr		Receives ptr to the end of the parsed input string. May be NULL.
	
	Examples:
		
		127.0.0.1:80/24		IPv4 with port 80 and prefix length of 24 bits.
		127.0.0.1:80		IPv4 with port 80.
		127.0.0.1			IPv4 with no port.
*/
OSStatus
	StringToIPv4Address( 
		const char *			inStr, 
		StringToIPAddressFlags	inFlags, 
		uint32_t *				outIP, 
		int *					outPort, 
		uint32_t *				outSubnet, 
		uint32_t *				outRouter, 
		const char **			outPtr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	IPv6AddressToCString
	@abstract	Converts an IPv6 address to a C string.
	
	@param	inAddr		128-bit/16-byte IPv6 address to convert.
	@param	outAddr		Receives 128-bit (16 byte) IPv6 address in network byte order. May be NULL.
	@param	inScope		Scope ID to embed in the string. May be 0 to not include a scope ID.
	@param	inPort		Port number to embed in the string. May be <= 0 to not include a port number.
	@param	inPrefix	Prefix length to embed in the string. May be <= 0 to not include a prefix length.
	@param	inBuffer	Receives resulting string. Must be at least 128 bytes (max string + some margin).
	
	@result	Ptr to beginning of textual string (same as input buffer).
	
	@discussion:
	
	Note: The following is the longest string this will produce (81 bytes including in the null terminator) assuming a
	maximum scope ID of 16 characters (common maximum on POSIX systems):
	
		[0000:0000:0000:0000:0000:0000:0000:0000%abcdefghijklmnop]:4294967296/4294967296
*/
#define kIPv6AddressToCStringForceIPv6Brackets		-2			//! Pass as the port to always include brackets around IPv6 addresses.
#define kIPv6AddressToCStringEscapeScopeID			( 1 << 0 )	//! Pass as a flag to percent-escape scope IDs (e.g. %25en0 instead of %en0).

char *	IPv6AddressToCString( const uint8_t inAddr[ 16 ], uint32_t inScope, int inPort, int inPrefix, char *inBuffer, uint32_t inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPv4AddressToCString
	@abstract	Converts an IPv4 (32-bit) address in host byte order to a dotted-decimal C string (e.g. "127.0.0.1").
	
	@param	inIP		32-bit IPv4 addess in host byte order.
	@param	inPort		Port number to embed in the string. May be <= 0 to not include a port number.
	@param	inBuffer	Receives resulting string. Must be at least 32 bytes (max string + some margin).
	
	@result	Ptr to beginning of textual string (same as input buffer).
	
	Note: The following is the longest string this will produce (27 bytes including in the null terminator):
	
		255.255.255.255:4294967296
*/
char *	IPv4AddressToCString( uint32_t inIP, int inPort, char *inBuffer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	BitListString_Parse
	@abstract	Parses a comma-separated string of bit numbers (e.g. "0,2" -> 0x5 since bit 0 and bit 2 are set).
*/
OSStatus	BitListString_Parse( const void *inStr, size_t inLen, uint32_t *outBits );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	FourCharCodeToCString / TextToFourCharCode
	@abstract	Convert to and from 32-bit FourCharCode/OSType values and text/C-strings.
*/
char *		FourCharCodeToCString( uint32_t inCode, char outString[ 5 ] );
uint32_t	TextToFourCharCode( const void *inText, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextToHardwareAddress
	@abstract	Parses hardware address text (e.g. AA:BB:CC:00:11:22 for a Mac address) to a byte array.

	@param		inText		Text to convert.
	@param		inSize		Number of bytes of text. May be kSizeCString if inText is null terminated.
	@param		inTextSize	Number of bytes expected in the hardware address (e.g. 6 for an Ethernet MAC address).
	@param		outAddr		Receives the parsed hardware address byte array. May be null. If non-null, it must be at 
							least inSize bytes.
	
	@discussion
	
	Segments can be separated by a colon ':', dash '-', or a space. A separator is not required if each segment uses 2
	digits. Segments with separators do not need zero padding (e.g. "0:1:2:3:4:5" is equivalent to "00:01:02:03:04:05".
	
	Examples: 
	
		"AA:BB:CC:00:11:22"			-> AA:BB:CC:00:11:22
		"AA-BB-CC-00-11-22"			-> AA:BB:CC:00:11:22
		"AA BB CC 00 11 22"			-> AA:BB:CC:00:11:22
		"AABBCCDDEEFF"				-> AA:BB:CC:00:11:22
		"0:1:2:3:4:5"				-> 00:01:02:03:04:05
		"0-1-2-3-4-5"				-> 00:01:02:03:04:05
		"0 1 2 3 4 5"				-> 00:01:02:03:04:05
		"AA:1:CC:2:11:3"			-> AA:01:CC:02:11:03
		"1:BB:2:00:3:22"			-> 01:BB:02:00:03:22
		"AA:BB:CC:00:11:22:33:44"	-> AA:BB:CC:00:11:22:33:44 (Fibre Channel MAC Address)
*/
OSStatus	TextToHardwareAddress( const void *inText, size_t inTextSize, size_t inSize, void *outAddr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextToMACAddress
	@abstract	Parses MAC address text (e.g. AA:BB:CC:00:11:22) to a 48-bit/6-byte array. See TextToHardwareAddress.
*/
#define	TextToMACAddress( TEXT, SIZE, ADDR )	TextToHardwareAddress( TEXT, SIZE, 6, ADDR )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HardwareAddressToCString
	@abstract	Converts a byte array containing a hardware address to a C string (e.g. 00:11:22:33:44:55).
	
	@param		inAddr	Ptr to hardware address byte array.
	@param		inSize	Number of bytes in inAddr.
	@param		outStr	Receives resulting string. Must be at least inSize * 3 bytes for the text and null terminator.
	
	@result		Ptr to beginning of textual string (same as input buffer).
*/
char *	HardwareAddressToCString( const void *inAddr, size_t inSize, char *outStr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MACAddressToCString
	@abstract	Converts a 48-bit (6-byte) IEEE 802-style MAC address to a C string (e.g. 00:11:22:33:44:55).
	
	@param		STR		Receives resulting string. Must be at least 18 bytes.
	
	@result		Ptr to beginning of textual string (same as input buffer).
*/
#define	MACAddressToCString( ADDR, STR )	HardwareAddressToCString( ADDR, 6, STR )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextToNumVersion
	@abstract	Converts a textual version such as "1.2.3b4" into a 32-bit NumVersion-style value.

	@param		inText		Text to parse.
	@param		inSize		Number of bytes of text. May be kSizeCString if inText is null terminated.
	@param		outVersion	Receives parsed 32-bit NumVersion-style version.
*/
OSStatus	TextToNumVersion( const void *inText, size_t inSize, uint32_t *outVersion );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextToSourceVersion
				SourceVersionToCString
	@abstract	Converts between a source version string (e.g. 112.5.3) and a source version number (e.g. 1120503).
	@discussion	SourceVersionToCString requires a string buffer of at least 16 bytes (214747.99.99 and '\0').
*/
uint32_t	TextToSourceVersion( const void *inText, size_t inSize );
char *		SourceVersionToCString( uint32_t inVersion, char *inString );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HexToData
	@abstract	Parses a hex string (e.g. "01AB23") to a byte array.

	@param		inStr				Text to convert.
	@param		inLen				Number of bytes of text. May be kSizeCString if inStr is null terminated.
	@param		inFlags				Flags to control behavior.
	@param		inBuf				Buffer to store bytes into. May be NULL to determine how big the buffer should be.
	@param		inMaxBytes			Maximum number of bytes to write into the output buffer. The hex text will be 
									parsed to the end, but only up to inMaxBytes bytes will be written.
	@param		outWrittenBytes		Receives the number of bytes written to inBuf (not including padding). May be NULL.
	@param		outTotalBytes		Receives the total number of bytes that could be converted. May be NULL.
	@param		outNext				Receives a pointer to end of the parsed string. May be NULL.
	
	@discussion
	
	This function always writes fills in non-NULL output parameters, even if an error is returned.
	The error code is purely informational in case the caller wants extra validity checking.
*/
typedef uint32_t	HexToDataFlags;

#define kHexToData_NoFlags				0			//! No flags.
#define kHexToData_DefaultFlags			( kHexToData_IgnoreWhitespace | \
										  kHexToData_IgnoreDelimiters | \
										  kHexToData_IgnorePrefixes )
#define kHexToData_ZeroPad				( 1 << 0 )	//! Pad any unwritten bytes in the destination buffer with zeros.
#define kHexToData_IgnoreWhitespace		( 1 << 1 )	//! Ignore whitespace in the middle of hex instead of stopping.
#define kHexToData_IgnoreDelimiters		( 1 << 2 )	//! Ignore ":-_," in the middle of hex instead of stopping.
#define kHexToData_WholeBytes			( 1 << 3 )	//! Require a full 2-digit hex value (i.e. don't allow just the upper nibble).
#define kHexToData_IgnorePrefixes		( 1 << 4 )	//! Ignore 0x prefixes on non-leading bytes. Leading 0x is always ignored.

OSStatus
	HexToData( 
		const void *	inStr, 
		size_t			inLen, 				// May be kSizeCString.
		HexToDataFlags	inFlags, 
		void *			inBuf, 				// May be NULL.
		size_t			inMaxBytes, 		// May be 0.
		size_t *		outWrittenBytes,	// May be NULL.
		size_t *		outTotalBytes, 		// May be NULL.
		const char **	outNext );			// May be NULL.
OSStatus
	HexToDataCopy( 
		const void *	inStr, 
		size_t			inLen, 
		HexToDataFlags	inFlags, 
		void *			outBytePtr, 
		size_t *		outByteCount, 
		const char **	outNext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DataToHexCString
	@abstract	Converts a byte array to a hex-encoded C string (e.g. "\x01\xAB\x23" -> "01ab23").
	
	@param		inData	Data to convert.
	@param		inSize	Number of bytes in inData.
	@param		outStr	Receives resulting string. Must be at least ( inSize * 2 ) + 1 bytes for the text and null terminator.
	
	@result		Ptr to beginning of textual string (same as input buffer).
*/
#define DataToHexCString( DATA, SIZE, OUT_STR )		DataToHexCStringEx( (DATA), (SIZE), (OUT_STR), kHexDigitsLowercase )
char *	DataToHexCStringEx( const void *inData, size_t inSize, char *outStr, const char * const inHexDigits );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextToInt32
	@abstract	Converts text into a 32-bit integer, taking into consideration potential format prefixes (e.g. "0x").

	@param		inText			Text to convert.
	@param		inSize			Number of bytes of text. May be kSizeCString if inText is null terminated.
	@param		inDefaultBase	Default base to use if text does not contain a format prefix (e.g. "0x").
	
	@result		Integer converted from the text.
	
	@discussion
	
	The following format prefixes are supported:
	
		"-"	 Negative decimal number.
		"+"  Positive decimal number.
		"0x" Hexidecimal number.
		"0"  Octal number
		"0b" Binary number.
*/
int32_t	TextToInt32( const void *inText, size_t inSize, int inDefaultBase );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NormalizeUUIDString
	@abstract	Normalizes a UUID string to convert to short/full form, etc.
	
	@param		inUUIDStr		UUID string to normalize in short or full form. Doesn't need to be NUL-terminated.
	@param		inLen			Number of bytes in inUUIDStr. If inUUIDStr is NUL-terminated, kSizeCString can be used.
	@param		inBaseUUID		Optional 16-byte base UUID. May be NULL if there's no base UUID.
	@param		inFlags			Flags to control how the normalized string is output. See kUUIDFlags_*.
	@param		outUUIDStr		Receives normalized string. Must be at least 38 bytes.
*/
OSStatus
	NormalizeUUIDString( 
		const char *	inUUIDStr, 
		size_t			inLen, 
		const void *	inBaseUUID, 
		uint32_t		inFlags, 
		char *			outUUIDStr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UUIDtoCString/StringToUUID
	@abstract	Converts between a 16-byte UUID to a UUID string.
	
	@param		inUUID			16-byte UUID in a byte order determined by the inLittleEndian parameter.
	@param		inLittleEndian	1 if the UUID is in little endian byte order. 0 if the UUID is in big endian.
								If 1: 10 b8 a7 6b ad 9d d1 11 80 b4 00 c0 4f d4 30 c8 -> "6ba7b810-9dad-11d1-80b4-00c04fd430c8".
								If 0: 6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8 -> "6ba7b810-9dad-11d1-80b4-00c04fd430c8".
	@param		inBuffer		Receives resulting string. Must be at least 38 bytes to hold the string and null terminator.
	
	@result		Ptr to beginning of string (same as input buffer).
*/
#define kUUIDFlags_None				0			// No flags.
#define kUUIDFlag_LittleEndian		( 1 << 0 )	// Interpret each section of the UUID as a little endian number (e.g. Microsoft UUIDs).
#define kUUIDFlag_ShortForm			( 1 << 1 )	// Output a short-form UUID if it's derived from the specified base UUID.

char *		UUIDtoCString( const void *inUUID, int inLittleEndian, void *inBuffer );
char *		UUIDtoCStringEx( const void *inUUID, size_t inSize, int inLittleEndian, const uint8_t *inBaseUUID, void *inBuffer );
char *
	UUIDtoCStringFlags( 
		const void *	inUUID, 
		size_t			inSize, 
		const void *	inBaseUUID, 
		uint32_t		inFlags, 
		void *			inBuffer, 
		OSStatus *		outErr );
OSStatus	StringToUUID( const char *inStr, size_t inSize, int inLittleEndian, void *outUUID );
OSStatus	StringToUUIDEx( const char *inStr, size_t inSize, int inLittleEndian, const uint8_t *inBaseUUID, void *outUUID );

#if 0
#pragma mark == ANSI C Extensions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	memicmp
	@abstract	Like stricmp, but for non-null terminated blocks of text.
*/
#if( TARGET_OS_WINDOWS )
	#define memicmp		_memicmp
#else
	int	memicmp( const void *inP1, const void *inP2, size_t inLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	snprintf_add / vsnprintf_add
	@abstract	Adds formatted text to a buffer with snprintf and updates the current pointer.
*/
OSStatus	snprintf_add( char **ioPtr, char *inEnd, const char *inFormat, ... );
OSStatus	vsnprintf_add( char **ioPtr, char *inEnd, const char *inFormat, va_list inArgs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strdup
				strndup
	@abstract	Copies a string into a newly malloc'd and null-terminated buffer.
	
	@param		inString	String to duplicate.
	
	@result		malloc'd and null-terminated copy of the input string. The caller must free if non-NULL.
*/
#if( !TARGET_OS_WINDOWS && !defined( strdup ) )
	char *	strdup( const char *inString );
#endif
#if( !TARGET_OS_LINUX )
	char *	strndup( const char *inStr, size_t inN );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	stricmp
	@abstract	Compares two C strings in a case-insensitive manner.
	
	@param		inS1	String 1.
	@param		inS1	String 2.
	
	@result		-1 if s1 <  s2; or
				 0 if s1 == s2; or 
				 1 if s1 >  s2.
	
	@discussion
	
	Like the ANSI C strcmp routine, but performs a case-insensitive compare.
*/
#if( TARGET_OS_POSIX )
	#define	stricmp( S1, S2 )		strcasecmp( ( S1 ), ( S2 ) )
#elif( !TARGET_OS_WINDOWS )
	int	stricmp( const char *inS1, const char *inS2 );
#endif
#if( !TARGET_OS_POSIX )
	#define strcasecmp( S1, S2 )	stricmp( (S1), (S2) )		
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strnicmp
	@abstract	Compares two C strings in a case-insensitive manner up to a maximum size.
	
	@param		inS1	String 1.
	@param		inS1	String 2.
	@param		inMax	Maximum number of bytes to compare.
	
	@result		-1 if s1 <  s2; or
				 0 if s1 == s2; or 
				 1 if s1 >  s2.
	
	@discussion
	
	Like the ANSI C strncmp routine, but performs a case-insensitive compare.
*/
#if( TARGET_OS_POSIX )
	#define	strnicmp( S1, S2, SIZE )	strncasecmp( ( S1 ), ( S2 ), SIZE )
#elif( !TARGET_OS_WINDOWS )
	int	strnicmp( const char *inS1, const char *inS2, size_t inMax );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		strcmp prefix functions
	@abstract	Like strncmp/strnicmp routine, but only returns 0 if the entire prefix/suffix matches.
	
	param		inStr/inS1		String 1.
	param		inN				Max number of bytes in String 1. Note: String 1 may be shorter if null-terminated.
	param		inPrefix		Null-terminated prefix/suffix string to check for.
	
	@result		< 0 if s1 <  s2; or
				  0 if s1 == s2; or 
				> 0 if s1 >  s2.
*/
int	strcmp_prefix( const char *inStr, const char *inPrefix );
int	stricmp_prefix( const char *inStr, const char *inPrefix );
int	strncmp_prefix( const void *inS1, size_t inN, const char *inPrefix );
int	strnicmp_prefix( const void *inS1, size_t inN, const char *inPrefix );

#define strcmp_suffix( STR, SUFFIX )	strncmp_suffix( ( STR ), SIZE_MAX, ( SUFFIX ) )
int		strncmp_suffix( const void *inStr, size_t inMaxLen, const char *inSuffix );
#define stricmp_suffix( STR, SUFFIX )	strnicmp_suffix( ( STR ), SIZE_MAX, ( SUFFIX ) )
int		strnicmp_suffix( const void *inS1, size_t inN, const char *inSuffix );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strncmpx
	@abstract	Compares text to a C string and requires that all characters in s1 match all characters in s2.
	@discussion	Like the ANSI C strncmp routine, but requires that all the characters in s1 match all the characters in s2.
	
	@param		inS1	String 1.
	@param		inS2	String 2.
	@param		inN		Number of characters to compare.
					
	@result		-1 if s1 <  s2; or
				 0 if s1 == s2; or 
				 1 if s1 >  s2.
*/
int	strncmpx( const void *inS1, size_t inN, const char *inS2 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strnicmpx
	@abstract	Compares text to a C string and requires that all characters in s1 match all characters in s2.
	@discussion	Like the strnicmp routine, but requires that all the characters in s1 match all the characters in s2.
	
	@param		inS1	String 1.
	@param		inS2	String 2.
	@param		inN		Number of characters to compare.
					
	@result		-1 if s1 <  s2; or
				 0 if s1 == s2; or 
				 1 if s1 >  s2.
*/
int	strnicmpx( const void *inS1, size_t inN, const char *inS2 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strnlen
	@abstract	Determines the length of a null terminated string up to a maximum size.

	@param		inString	String to determine the length of.
	@param		inMax		Maximum length of the string. The string will not be accessed beyond this length.

	@result		Length of string (if null terminator found) or the maximum size (if no null terminator found).
	
	@discussion
	
	Like the ANSI C strlen routine, but allows you to specify a maximum size.
	
	This routine works whether or not the string is null terminated. It is best suited for situations where
	the string may be null terminated, but is not required to be null terminated.
*/
#if( !TARGET_OS_DARWIN && !TARGET_VISUAL_STUDIO_2005_OR_LATER && !TARGET_OS_LINUX && ( QNX_VERSION < 660 ) )
	size_t	strnlen( const char *inStr, size_t inMaxLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		strstr variants.
	@discussion
	
	strnstr:		Find the first occurrence of find in s, where the search is limited to the first slen characters of s.
	strncasestr:	Case-insensitive version of strnstr.
*/
char *	strnstr(const char *s, const char *find, size_t slen);
char *	strncasestr(const char *s, const char *find, size_t slen);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strlcat
	
	@abstract	Appends src to string dst of size siz (unlike strncat, siz is the full size of dst, not space left). 
				At most siz-1 characters will be copied. Always NUL terminates (unless siz <= strlen(dst)). Returns 
				strlen(src) + MIN(siz, strlen(initial dst)). If retval >= siz, truncation occurred.
*/
#if( !TARGET_OS_BSD )
	size_t	strlcat( char *inDst, const char *inSrc, size_t inLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strlcpy
	@abstract	Copy src to string dst of size siz. At most siz-1 characters will be copied. Always NUL terminates 
				(unless siz == 0). Returns strlen(src); if retval >= siz, truncation occurred.
*/
#if( !TARGET_OS_BSD )
	size_t	strlcpy( char *inDst, const char *inSrc, size_t inLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strnspn
	@abstract	Like strspn, but supports non-null-terminated strings.
*/
size_t	strnspn( const void *inStr, size_t inLen, const char *inCharSet );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strnspnx
	@abstract	Returns true if the string contains only characters from the specified set.
*/
Boolean	strnspnx( const char *inStr, size_t inLen, const char *inCharSet );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strsep_compat
	@abstract	Emulation of the pseudo-POSIX strsep.
*/
char *	strsep_compat( char **ioStr, const char *inDelimiters );
#if( !TARGET_OS_POSIX )
	#define strsep( IO_STR, DELIMITERS )	strsep_compat( (IO_STR), (DELIMITERS) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	strtoi
	@abstract	Similar to strtol, but returns an error result instead of errno and it works with int's instead of long's.
	@result		Returns 0 on success or an errno value on errors (suitable for use as process exit codes).
*/
int	strtoi( const char *inString, char **outEnd, int *outValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	memrchr
	@abstract	Like the ANSI C strrchr routine, but works on with a non-null terminated buffer.
	
	@param		inSrc	Ptr to memory to search.
	@param		inChar	Character to search for.
	@param		inSize	Maximum number of bytes to search.
	
	@result		Ptr to last occurance of character or NULL if the character is not found.
*/
#if( !TARGET_OS_LINUX )
	void *	memrchr( const void *inSrc, int inChar, size_t inSize );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	memrlen
	@abstract	Returns the number of bytes until the last 0 in the buffer.
*/
size_t	memrlen( const void *inSrc, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	tolowerstr / toupperstr
	@abstract	Converts a C string to lowercase / uppercase. Note: inSrc and inDst may point to the same string.
	@result		Returns the beginning of the destination string.
*/
char *	tolowerstr( const void *inSrc, void *inDst, size_t inMaxLen );
char *	toupperstr( const void *inSrc, void *inDst, size_t inMaxLen );

#if 0
#pragma mark == Mac OS X Kernel ctype.h support ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Mac OS X Kernel ctype.h support
	@abstract	Like the ANSI C strrchr routine, but works on with a non-null terminated buffer.
*/
#if( TARGET_OS_DARWIN_KERNEL )
	static inline int	isspace( int c )	{ return( ( c == ' ' ) || ( ( c >= 0x09 ) && ( c <= 0x0D ) ) ); }
	static inline int	isdigit( int c )	{ return( ( c >= '0' ) && ( c <= '9' ) ); }
	static inline int	isxdigit( int c )	{ return( ( ( c >= '0' ) && ( c <= '9' ) ) || 
													  ( ( c >= 'a' ) && ( c <= 'f' ) ) ||
													  ( ( c >= 'A' ) && ( c <= 'F' ) ) ); }
	static inline int	tolower( int c )	{ return( ( ( c >= 'A' ) && ( c <= 'Z' ) ) ? ( c + ( 'a' - 'A' ) ) : c ); }
#endif

#if 0
#pragma mark == Misc ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetLastFilePathSegment
	@abstract	Finds the last segment of a file path.
	@discussion	For example, if the path is "a/b/c/d.txt", "d.txt" is returned.
*/
const char *	GetLastFilePathSegment( const char *inPtr, size_t inLen, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetParentPath
	@abstract	Gets the path of the parent of an path.
	@discussion	For example, if the path is "a/b/c/d.txt", "a/b/c" is returned.
*/
OSStatus	GetParentPath( const char *inPathPtr, size_t inPathLen, char *inBuffer, size_t inMaxLen, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	INIGetNext
	@abstract	Parses a line from INI file-formatted text.
	
	@param		inSrc			Current pointer within the INI data.
	@param		inEnd			Pointer to the end of the INI data.
	@param		outFlags		Receives flags indicated if the parsed line is a section header or property.
	@param		outNamePtr		Receives pointer to name of section or property.
	@param		outNameLen		Receives length of name of section or property.
	@param		outValuePtr		Receives pointer to value of section or property. NULL if there is no value.
	@param		outValueLen		Receives length of value of section or property. 0 if there is value or it's empty.
	@param		outSrc			Receives pointer after the parsed data (suitable for calling this function in a loop).
	
	@result		True if the line was parsed. False if there is nothing left to parse.
*/
#define kINIFlag_Section			( 1 << 0 )
#define kINIFlag_Property			( 1 << 1 )
#define kINISectionType_Global		"" // Virtual section for global properties (i.e. properties before any sections).

Boolean
	INIGetNext( 
		const char *	inSrc, 
		const char *	inEnd, 
		uint32_t *		outFlags, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsTrueString
	@abstract	Returns true if the string is true/yes/y/on/1.
*/
#define IsTrueString( PTR, LEN )		( (Boolean)( \
	( strnicmpx( (PTR), (LEN), "true" )	== 0 ) || \
	( strnicmpx( (PTR), (LEN), "yes" )	== 0 ) || \
	( strnicmpx( (PTR), (LEN), "y" )	== 0 ) || \
	( strnicmpx( (PTR), (LEN), "on" )	== 0 ) || \
	( strnicmpx( (PTR), (LEN), "1" )	== 0 ) ) )

#define kAnyTrueMessage		"true|yes|y|on|1"

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsFalseString
	@abstract	Returns true if the string is false/no/n/off/0.
*/
#define IsFalseString( PTR, LEN )		( (Boolean)( \
	( strnicmpx( (PTR), (LEN), "false" )	== 0 ) || \
	( strnicmpx( (PTR), (LEN), "no" )		== 0 ) || \
	( strnicmpx( (PTR), (LEN), "n" )		== 0 ) || \
	( strnicmpx( (PTR), (LEN), "off" )		== 0 ) || \
	( strnicmpx( (PTR), (LEN), "0" )		== 0 ) ) )

#define kAnyFalseMessage			"false|no|n|off|0"

#define kAnyTrueOrFalseMessage		"true, false, yes, no, on, off, 0, or 1"

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MapCountToOrSeparator
	@abstract	Maps a count and totalCount to a separator for "a" vs "a or b" vs "a, b, or c".
*/
#define MapCountToOrSeparator( COUNT, TOTAL_COUNT )	( \
	( (COUNT) > 0 ) ? \
		( (TOTAL_COUNT) > 2 ) ? \
			( ( (COUNT) + 1 ) == (TOTAL_COUNT) )	? ", or " \
													: ", " \
													: " or " \
													: "" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MapStringToValue
	@abstract	Maps a string to a value using a variable argument list of string/int pairs, terminated by a single NULL.
	@discussion
	
	Example usage:
	int	i;
	
	i = MapStringToValue( "blue", -1, 
		"red",   1, 
		"green", 2, 
		"blue",  3, 
		NULL );
	Result: i == 3
*/
int	MapStringToValue( const char *inString, int inDefaultValue, ... );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ParseCommandLineIntoArgV/FreeCommandLineArgs
	@abstract	Parses a command line string into an argv array.
*/
OSStatus	ParseCommandLineIntoArgV( const char *inCmdLine, int *outArgC, char ***outArgV );
void		FreeCommandLineArgV( int inArgC, char **inArgV );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ParseCommaSeparatedNameValuePair
	@abstract	Parses a comma-delimited pair of escaped name/value strings.
	@discussion
	
	This routine is typically used in a loop to process each item. For example:
	
	char		name[ 256 ];
	char		value[ 256 ];
	
	while( ParseCommaSeparatedNameValuePair( src, end, name, sizeof( name ), NULL, NULL, 
		value, sizeof( value ), NULL, NULL, &src ) == kNoErr )
	{
		printf( "%s = %s\n", name, value );
	}
*/
OSStatus
	ParseCommaSeparatedNameValuePair( 
		const char *	inSrc, 
		const char *	inEnd, 
		char *			inNameBuf, 
		size_t			inNameMaxLen, 
		size_t *		outNameCopiedLen, 
		size_t *		outNameTotalLen, 
		char *			inValueBuf, 
		size_t			inValueMaxLen, 
		size_t *		outValueCopiedLen, 
		size_t *		outValueTotalLen, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ParseEscapedString
	@abstract	Parses a backslash-escaped string.

	@param		inSrc			Ptr to comma-limited text to parse.
	@param		inEnd			End of comma-limited text to parse.
	@param		inDelimiter		Delimiter to end parsing (e.g. ',' for comma-separated strings).
	@param		inBuf			Receives unescaped and null terminated string. May be NULL.
	@param		inMaxLen		Max number of bytes to write to inBuf. Size includes the null terminator.
	@param		outCopiedLen	Receives the number of bytes copied to inBuf (excluding the null terminator). May be NULL.
								This won't be more than inMaxLen bytes. This is filled in even if inBuf is NULL.
	@param		outTotalLen		Receives the total number of unescaped bytes in the string. May be NULL.
	@param		outSrc			Receives a ptr to data immediately after the string (for iterating). May be NULL.
*/
OSStatus
	ParseEscapedString( 
		const char *	inSrc, 
		const char *	inEnd, 
		char			inDelimiter, 
		char *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outCopiedLen, 
		size_t *		outTotalLen, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ParseQuotedEscapedString
	@abstract	Parses a string that may contain single or double quoted strings, escaped characters, etc.

	@param		inSrc			Ptr to text to parse.
	@param		inEnd			End of text to parse.
	@param		inDelimiters	Delimiters to end parsing (e.g. "," for comma-separated strings).
	@param		inBuf			Receives unescaped string. May be NULL.
	@param		inMaxLen		Max number of bytes to write to inBuf.
	@param		outCopiedLen	Receives the number of bytes copied to inBuf. May be NULL.
								This won't be more than inMaxLen bytes. This is filled in even if inBuf is NULL.
	@param		outTotalLen		Receives the total number of unescaped bytes in the string. May be NULL.
	@param		outSrc			Receives a ptr to data immediately after the string (for iterating). May be NULL.
	
	@result		true if there is more text to parse.
	
	@discussion
	
	See <http://www.mpi-inf.mpg.de/~uwe/lehre/unixffb/quoting-guide.html> for details on quoting rules.
	Additionally, the following escape sequences are supported:
	
		\x<hex><hex>			-- Escape a hex byte, such as "\x1f" to put the byte 0x1f in place of \x1f.
		\<octal><octal><octal>	-- Escape an octal byte, such as "\x377" to put the byte 0377 (hex 0xFF) in place of \x377.
*/
Boolean
	ParseQuotedEscapedString( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inDelimiters, 
		char *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outCopiedLen, 
		size_t *		outTotalLen, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RegexMatch
	@abstract	Simple regular expression searching.
	@discussion
	
	C	Matches any literal character C.
	.	Matches any single character.
	^	Matches the beginning of the input string.
	$	Matches the end of the input string
	*	Matches zero or more occurrences of the previous character.
*/
int	RegexMatch( const char *inPattern, const char *inText );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ReplaceDifferentString
	@abstract	Replaces a string if it's different with NULL being equivalent to an empty string.
*/
OSStatus	ReplaceDifferentString( char **ioString, const char *inNewString );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ReplaceString
	@abstract	Replaces a string with another string.
	
	@param		ioStr	Ptr to string ptr to hold string. Must pointer to either NULL or a malloc'd pointer.
						If this points to a malloc'd pointer on entry, it will be free'd.
	@param		ioLen	Optional ptr to store the length of the new string. May be NULL.
	@param		inStr	New string to replace the old string with. Doesn't need to be NUL-terminated. May be NULL.
	@param		inLen	New string length. May be kSizeCString to determine the string length if it's NUL-terminated.
*/
OSStatus	ReplaceString( char **ioStr, size_t *ioLen, const void *inStr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		StringArray
	@abstract	Maintains a dynamically-sized array of strings.
*/
OSStatus	StringArray_Append( char ***ioArray, size_t *ioCount, const char *inStr );
void		StringArray_Free( char **inArray, size_t inCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TextCompareNatural
	@abstract	Compares two blocks of text using a natural order comparision (e.g. "file 10" sorts after "file 1").
	
	@result		< 0 if s1 <  s2
				  0 if s1 == s2
				> 0 if s1 >  s2
*/
int
	TextCompareNatural( 
		const void *	inLeftPtr, 
		size_t			inLeftLen, 
		const char *	inRightPtr, 
		size_t			inRightLen, 
		Boolean			inCaseSensitive );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TruncateUTF8

	@abstract	Truncates UTF-8 text to no more than "max" bytes.
	
	@param		inSrcPtr			UTF-8 text to truncate.
	@param		inSrcLen			Number of bytes in UTF-8 text.
	@param		inDstPtr			Ptr to receive truncated text. May be the same as inSrcPtr.
	@param		inMaxLen			Max bytes in truncated text.
	@param		inNullTerminate		True if the result should be null terminated.
	
	@result		Number of bytes in truncated text.
	
	@discussion
	
	When a string needs to be truncated, care is taken to avoid truncating in the middle of a multi-byte UTF-8 character 
	sequence or between the UTF-8 equivalent of a UTF-16 surrogate pair (not legal in UTF-8, but still possible). 
*/
size_t	TruncateUTF8( const void *inSrcPtr, size_t inSrcLen, void *inDstPtr, size_t inMaxLen, Boolean inNullTerminate );
size_t	TruncatedUTF8Length(const void *str, size_t length, size_t max);

#if 0
#pragma mark -
#pragma mark == DNS ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DNSServiceConstructFullNameEx
	@abstract	Extension of DNSServiceConstructFullName to also escape '%' and ':' for scope IDs and port numbers.
*/
OSStatus
	DNSServiceConstructFullNameEx( 
		char * const		outFullName, 
		const char * const	inService, // May be NULL
		const char * const	inType, 
		const char * const	inDomain );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MakeDomainNameFromDNSNameString
	@abstract	Makes a DNS name (1 or more length-prefixed labels) from a C string.
	@result		Returns the end of the written data or NULL if there's a failure.
*/
uint8_t *	MakeDomainNameFromDNSNameString( uint8_t * const outName, const char *inStr );

#if 0
#pragma mark == SNScanF ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SNScanF
	@abstract	Parses a sized block of text using a format string. Like the ANSI C sscanf for non-null-terminated strings.

	@param		inString	String to parse. Does not need to be null terminated.
	@param		inSize		Number of characters in the string to parse. May be kSizeCString if inString is null terminated.
	@param		inFormat	sscanf-style format string.
	param		VA_ARGS		Variable argument list.
	
	@result		The number of matches that occurred or -1 if an error occurred.
	
	@discussion

	Extra features over the ISO C99 sscanf:

	- Supports non-null-terminated buffers. A ptr and size must be specified.
	- %. for variable field widths (e.g. %.s). Arg is an int specifying how many characters to read.
	- %#s for a string that matches even if it is empty.
	- %#[ for a scan set that matches even if it is empty.
	- %b for binary integers (e.g. "1111011", "%b" -> decimal 123).
	- %i support for "0b" and "0B" prefixes for binary integers (e.g. "0b1111011", "%i" -> decimal 123).
	- %&s, %&[, and %&c for returning a ptr and size instead of copying the data into the buffer.
	- %#n for returning a ptr to the current position in the string.
	
	Features missing from the ISO C99 sscanf:
	
	- wchar_t support (e.g. %ls).
*/
int	SNScanF( const void *inString, size_t inSize, const char *inFormat, ... ) SCANF_STYLE_FUNCTION( 3, 4 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	VSNScanF
	@abstract	va_list version of SNScanF. Like the ANSI C vsnscanf for non-null-terminated strings. See SNScanF for details.
*/
int	VSNScanF( const void *inString, size_t inSize, const char *inFormat, va_list inArgs );

#if( !EXCLUDE_UNIT_TESTS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SNScanFTest
	@abstract	Unit test.
*/
OSStatus	SNScanFTest( void );
#endif

#if( !EXCLUDE_UNIT_TESTS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	StringUtilsTest
	@abstract	Unit test.
*/
OSStatus	StringUtilsTest( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __StringUtils_h__
