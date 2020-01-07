/*
	File:    	StringUtils.c
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

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "StringUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "MathUtils.h"
#include "PrintFUtils.h"
#include "TimeUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <ctype.h>
	#include <limits.h>
	#include <math.h>
	#include <stddef.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#if( defined( __MWERKS__ ) )
	#include <extras.h>
#endif

#if( TARGET_OS_DARWIN_KERNEL )
	#include <netinet/in.h>
#endif

#if( TARGET_OS_POSIX )
	#include <sys/types.h>
	#include <net/if.h>
#endif

#if 0
#pragma mark == Conversions ==
#endif

//===========================================================================================================================
//	Conversions
//===========================================================================================================================

static OSStatus	ParseIPv6Address( const char *inStr, int inAllowV4Mapped, uint8_t inBuffer[ 16 ], const char **outStr );
static OSStatus	ParseIPv6Scope( const char *inStr, uint32_t *outScope, const char **outStr );
static OSStatus	ParseIPv4Address( const char *inStr, uint8_t inBuffer[ 4 ], const char **outStr );

//===========================================================================================================================
//	StringToIPv6Address
//===========================================================================================================================

OSStatus
	StringToIPv6Address( 
		const char *			inStr, 
		StringToIPAddressFlags	inFlags, 
		uint8_t					outIPv6[ 16 ], 
		uint32_t *				outScope, 
		int *					outPort, 
		int *					outPrefix, 
		const char **			outStr )
{
	OSStatus		err;
	uint8_t			ipv6[ 16 ];
	int				c;
	int				hasScope;
	uint32_t		scope;
	int				hasPort;
	int				port;
	int				hasPrefix;
	int				prefix;
	int				hasBracket;
	int				i;
	
	require_action( inStr, exit, err = kParamErr );
	
	if( *inStr == '[' ) ++inStr; // Skip a leading bracket for []-wrapped addresses (e.g. "[::1]:80").
	
	// Parse the address-only part of the address (e.g. "1::1").
	
	err = ParseIPv6Address( inStr, !( inFlags & kStringToIPAddressFlagsNoIPv4Mapped ), ipv6, &inStr );
	require_noerr_quiet( err, exit );
	c = *inStr;
	
	// Parse the scope, port, or prefix length.
	
	hasScope	= 0;
	scope		= 0;
	hasPort		= 0;
	port		= 0;
	hasPrefix	= 0;
	prefix		= 0;
	hasBracket	= 0;
	for( ;; )
	{
		if( c == '%' )		// Scope (e.g. "%en0" or "%5")
		{
			require_action_quiet( !hasScope, exit, err = kMalformedErr );
			require_action_quiet( !( inFlags & kStringToIPAddressFlagsNoScope ), exit, err = kUnexpectedErr );
			++inStr;
			err = ParseIPv6Scope( inStr, &scope, &inStr );
			require_noerr_quiet( err, exit );
			hasScope = 1;
			c = *inStr;
		}
		else if( c == ':' )	// Port (e.g. ":80")
		{
			require_action_quiet( !hasPort, exit, err = kMalformedErr );
			require_action_quiet( !( inFlags & kStringToIPAddressFlagsNoPort ), exit, err = kUnexpectedErr );
			while( ( ( c = *( ++inStr ) ) != '\0' ) && ( ( c >= '0' ) && ( c <= '9' ) ) ) port = ( port * 10 ) + ( c - '0' );
			require_action_quiet( port <= 65535, exit, err = kRangeErr );
			hasPort = 1;
		}
		else if( c == '/' )	// Prefix Length (e.g. "/64")
		{
			require_action_quiet( !hasPrefix, exit, err = kMalformedErr );
			require_action_quiet( !( inFlags & kStringToIPAddressFlagsNoPrefix ), exit, err = kUnexpectedErr );
			while( ( ( c = *( ++inStr ) ) != '\0' ) && ( ( c >= '0' ) && ( c <= '9' ) ) ) prefix = ( prefix * 10 ) + ( c - '0' );
			require_action_quiet( ( prefix >= 0 ) && ( prefix <= 128 ), exit, err = kRangeErr );
			hasPrefix = 1;
		}
		else if( c == ']' )
		{
			require_action_quiet( !hasBracket, exit, err = kMalformedErr );
			hasBracket = 1;
			c = *( ++inStr );
		}
		else
		{
			break;
		}
	}
	
	// Return the results. Only fill in scope/port/prefix results if the info was found to allow for defaults.
	
	if( outIPv6 )				 for( i = 0; i < 16; ++i ) outIPv6[ i ] = ipv6[ i ];
	if( outScope  && hasScope )  *outScope	= scope;
	if( outPort   && hasPort )   *outPort	= port;
	if( outPrefix && hasPrefix ) *outPrefix	= prefix;
	if( outStr )				 *outStr	= inStr;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringToIPv4Address
//===========================================================================================================================

OSStatus
	StringToIPv4Address( 
		const char *			inStr, 
		StringToIPAddressFlags	inFlags, 
		uint32_t *				outIP, 
		int *					outPort, 
		uint32_t *				outSubnet, 
		uint32_t *				outRouter, 
		const char **			outStr )
{
	OSStatus			err;
	uint8_t				buf[ 4 ];
	int					c;
	uint32_t			ip;
	int					hasPort;
	int					port;
	int					hasPrefix;
	int					prefix;
	uint32_t			subnetMask;
	uint32_t			router;
	
	require_action( inStr, exit, err = kParamErr );
	
	// Parse the address-only part of the address (e.g. "1.2.3.4").
	
	err = ParseIPv4Address( inStr, buf, &inStr );
	require_noerr_quiet( err, exit );
	ip = (uint32_t)( ( buf[ 0 ] << 24 ) | ( buf[ 1 ] << 16 ) | ( buf[ 2 ] << 8 ) | buf[ 3 ] );
	c = *inStr;
	
	// Parse the port (if any).
	
	hasPort = 0;
	port    = 0;
	if( c == ':' )
	{
		require_action_quiet( !( inFlags & kStringToIPAddressFlagsNoPort ), exit, err = kUnexpectedErr );
		while( ( ( c = *( ++inStr ) ) != '\0' ) && ( ( c >= '0' ) && ( c <= '9' ) ) ) port = ( port * 10 ) + ( c - '0' );
		require_action_quiet( port <= 65535, exit, err = kRangeErr );
		hasPort = 1;
	}
	
	// Parse the prefix length (if any).
	
	hasPrefix  = 0;
	prefix     = 0;
	subnetMask = 0;
	router     = 0;
	if( c == '/' )
	{
		require_action_quiet( !( inFlags & kStringToIPAddressFlagsNoPrefix ), exit, err = kUnexpectedErr );
		while( ( ( c = *( ++inStr ) ) != '\0' ) && ( ( c >= '0' ) && ( c <= '9' ) ) ) prefix = ( prefix * 10 ) + ( c - '0' );
		require_action_quiet( ( prefix >= 0 ) && ( prefix <= 32 ), exit, err = kRangeErr );
		hasPrefix = 1;
		
		subnetMask = ( prefix > 0 ) ? ( UINT32_C( 0xFFFFFFFF ) << ( 32 - prefix ) ) : 0;
		router	   = ( ip & subnetMask ) | 1;
	}
	
	// Return the results. Only fill in port/prefix/router results if the info was found to allow for defaults.
	
	if( outIP )					 *outIP		= ip;
	if( outPort   && hasPort )	 *outPort	= port;
	if( outSubnet && hasPrefix ) *outSubnet	= subnetMask;
	if( outRouter && hasPrefix ) *outRouter	= router;
	if( outStr )				 *outStr	= inStr;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ParseIPv6Address
//
//	Note: Parsed according to the rules specified in RFC 3513.
//	Warning: "inBuffer" may be modified even in error cases.
//===========================================================================================================================

static OSStatus	ParseIPv6Address( const char *inStr, int inAllowV4Mapped, uint8_t inBuffer[ 16 ], const char **outStr )
{
													// Table to map uppercase hex characters - '0' to their numeric values.
													// 0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?  @  A   B   C   D   E   F
	static const uint8_t		kASCIItoHexTable[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15 };
	OSStatus					err;
	const char *				ptr;
	uint8_t *					dst;
	uint8_t *					lim;
	uint8_t *					colonPtr;
	int							c;
	int							sawDigit;
	unsigned int				v;
	int							i;
	int							n;
	
	// Pre-zero the address to simplify handling of compressed addresses (e.g. "::1").
	
	for( i = 0; i < 16; ++i ) inBuffer[ i ] = 0;
	
	// Special case leading :: (e.g. "::1") to simplify processing later.
	
	if( *inStr == ':' )
	{
		++inStr;
		require_action_quiet( *inStr == ':', exit, err = kMalformedErr );
	}
	
	// Parse the address.
	
	ptr		 = inStr;
	dst		 = inBuffer;
	lim		 = dst + 16;
	colonPtr = NULL;
	sawDigit = 0;
	v		 = 0;
	while( ( ( c = *inStr++ ) != '\0' ) && ( c != '%' ) && ( c != '/' ) && ( c != ']' ) )
	{
		if(   ( c >= 'a' ) && ( c <= 'f' ) ) c -= ( 'a' - 'A' );
		if( ( ( c >= '0' ) && ( c <= '9' ) ) || ( ( c >= 'A' ) && ( c <= 'F' ) ) )
		{
			c -= '0';
			check( c < (int) countof( kASCIItoHexTable ) );
			v = ( v << 4 ) | kASCIItoHexTable[ c ];
			require_action_quiet( v <= 0xFFFF, exit, err = kRangeErr );
			sawDigit = 1;
			continue;
		}
		if( c == ':' )
		{
			ptr = inStr;
			if( !sawDigit )
			{
				require_action_quiet( !colonPtr, exit, err = kMalformedErr );
				colonPtr = dst;
				continue;
			}
			require_action_quiet( *inStr != '\0', exit, err = kUnderrunErr );
			require_action_quiet( ( dst + 2 ) <= lim, exit, err = kOverrunErr );
			*dst++ = (uint8_t)( ( v >> 8 ) & 0xFF );
			*dst++ = (uint8_t)(   v        & 0xFF );
			sawDigit = 0;
			v = 0;
			continue;
		}
		
		// Handle IPv4-mapped/compatible addresses (e.g. ::FFFF:1.2.3.4).
		
		if( inAllowV4Mapped && ( c == '.' ) && ( ( dst + 4 ) <= lim ) )
		{
			err = ParseIPv4Address( ptr, dst, &inStr );
			require_noerr_quiet( err, exit );
			dst += 4;
			sawDigit = 0;
			++inStr; // Increment because the code below expects the end to be at "inStr - 1".
		}
		break;
	}
	if( sawDigit )
	{
		require_action_quiet( ( dst + 2 ) <= lim, exit, err = kOverrunErr );
		*dst++ = (uint8_t)( ( v >> 8 ) & 0xFF );
		*dst++ = (uint8_t)(   v        & 0xFF );
	}
	check( dst <= lim );
	if( colonPtr )
	{
		require_action_quiet( dst < lim, exit, err = kOverrunErr );
		n = (int)( dst - colonPtr );
		for( i = 1; i <= n; ++i )
		{
			lim[ -i ] = colonPtr[ n - i ];
			colonPtr[ n - i ] = 0;
		}
		dst = lim;
	}
	require_action_quiet( dst == lim, exit, err = kUnderrunErr );
	
	*outStr = inStr - 1;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ParseIPv6Scope
//===========================================================================================================================

static OSStatus	ParseIPv6Scope( const char *inStr, uint32_t *outScope, const char **outStr )
{
#if( TARGET_OS_POSIX )
	OSStatus			err;
	char				scopeStr[ 64 ];
	char *				dst;
	char *				lim;
	int					c;
	uint32_t			scope;
	const char *		ptr;
	
	// Copy into a local NULL-terminated string since that is what if_nametoindex expects.
	
	dst = scopeStr;
	lim = dst + ( countof( scopeStr ) - 1 );
	while( ( ( c = *inStr ) != '\0' ) && ( c != ':' ) && ( c != '/' ) && ( c != ']' ) && ( dst < lim ) )
	{
		*dst++ = *inStr++;
	}
	*dst = '\0';
	check( dst <= lim );
	
	// First try to map as a name and if that fails, treat it as a numeric scope.
	
	scope = if_nametoindex( scopeStr );
	if( scope == 0 )
	{
		for( ptr = scopeStr; ( ( c = *ptr ) >= '0' ) && ( c <= '9' ); ++ptr )
		{
			scope = ( scope * 10 ) + ( c - '0' );
		}
		require_action_quiet( c == '\0', exit, err = kMalformedErr );
		require_action_quiet( ( ptr != scopeStr ) && ( ( (int)( ptr - scopeStr ) ) <= 10 ), exit, err = kMalformedErr );
	}
	
	*outScope = scope;
	*outStr   = inStr;
	err = kNoErr;
	
exit:
	return( err );
#else
	OSStatus			err;
	uint32_t			scope;
	const char *		start;
	int					c;
	
	scope = 0;
	for( start = inStr; ( ( c = *inStr ) >= '0' ) && ( c <= '9' ); ++inStr )
	{
		scope = ( scope * 10 ) + ( c - '0' );
	}
	require_action_quiet( ( inStr != start ) && ( ( (int)( inStr - start ) ) <= 10 ), exit, err = kMalformedErr );
	
	*outScope = scope;
	*outStr   = inStr;
	err = kNoErr;
	
exit:
	return( err );
#endif
}

//===========================================================================================================================
//	ParseIPv4Address
//
//	Warning: "inBuffer" may be modified even in error cases.
//===========================================================================================================================

static OSStatus	ParseIPv4Address( const char *inStr, uint8_t inBuffer[ 4 ], const char **outStr )
{
	OSStatus		err;
	uint8_t *		dst;
	int				segments;
	int				sawDigit;
	int				c;
	int				v;
	
	check( inBuffer );
	check( outStr );
	
	dst		 = inBuffer;
	*dst	 = 0;
	sawDigit = 0;
	segments = 0;
	for( ; ( c = *inStr ) != '\0'; ++inStr )
	{
		if( isdigit_safe( c ) )
		{
			v = ( *dst * 10 ) + ( c - '0' );
			require_action_quiet( v <= 255, exit, err = kRangeErr );
			*dst = (uint8_t) v;
			if( !sawDigit )
			{
				++segments;
				require_action_quiet( segments <= 4, exit, err = kOverrunErr );
				sawDigit = 1;
			}
		}
		else if( ( c == '.' ) && sawDigit )
		{
			require_action_quiet( segments < 4, exit, err = kMalformedErr );
			*++dst = 0;
			sawDigit = 0;
		}
		else
		{
			break;
		}
	}
	require_action_quiet( segments == 4, exit, err = kUnderrunErr );
	
	*outStr = inStr;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	IPv6AddressToCString
//===========================================================================================================================

char *	IPv6AddressToCString( const uint8_t inAddr[ 16 ], uint32_t inScope, int inPort, int inPrefix, char *inBuffer, uint32_t inFlags )
{
	int			v4Mapped;
	int			v4Compat;
	char *		dst = inBuffer;
	int			i;
	int			j;
	int			k;
	int			x;
	int			skip;
	uint8_t		u8;
	uint8_t		u4;
	char		segs[ 8 ][ 5 ];
	char *		seg;
	int			runs[ 8 ];
	char		c;
	
	check( inPort <= 65535 );
	check( inPrefix <= 128 );
	
	// Address must be []-wrapped if there is a port number or the caller always wants it.
	
	if( ( inPort > 0 ) || ( inPort == kIPv6AddressToCStringForceIPv6Brackets )  )
	{
		*dst++ = '[';
	}
	
	// Treat IPv4-mapped/compatible addresses specially.
	
	v4Mapped = IsIPv4MappedIPv6Address( inAddr );
	v4Compat = IsIPv4CompatibleIPv6Address( inAddr );
	if( v4Mapped || v4Compat )
	{
		*dst++ = ':';
		*dst++ = ':';
		if( v4Mapped )
		{
			*dst++ = 'f';
			*dst++ = 'f';
			*dst++ = 'f';
			*dst++ = 'f';
			*dst++ = ':';
		}
		for( i = 12; i < 16; ++i )
		{
			x = inAddr[ i ];
			append_decimal_string( x, dst );
			if( i != 15 ) *dst++ = '.';
		}
		goto trailer;
	}
	
	// Build an array of 16-bit string segments from the 16-byte IPv6 address.
	
	k = 0;
	for( i = 0; i < 16; )
	{
		seg  = &segs[ k ][ 0 ];
		skip = 1;
		j    = 0;
		
		u8 = inAddr[ i++ ];
		u4 = (uint8_t)( u8 >> 4 );
		if( u4 != 0 )
		{
			skip = 0;
			seg[ j++ ] = kHexDigitsLowercase[ u4 ];
		}
		u4 = (uint8_t)( u8 & 0x0F );
		if( ( skip == 0 ) || ( ( skip == 1 ) && ( u4 != 0 ) ) )
		{
			skip = 0;
			seg[ j++ ] = kHexDigitsLowercase[ u4 ];
		}
		
		u8 = inAddr[ i++ ];
		u4 = (uint8_t)( u8 >> 4 );
		if( ( skip == 0 ) || ( ( skip == 1 ) && ( u4 != 0 ) ) )
		{
			seg[ j++ ] = kHexDigitsLowercase[ u4 ];
		}
		
		u4 = (uint8_t)( u8 & 0x0F );
		seg[ j++ ] = kHexDigitsLowercase[ u4 ];
		seg[ j ] = '\0';
		
		++k;
	}
	
	// Find runs of zeros to collapse into :: notation.
	
	j = 0;
	for( i = 7; i >= 0; --i )
	{
		x = i * 2;
		if( ( inAddr[ x ] == 0 ) && ( inAddr[ x + 1  ] == 0 ) ) ++j;
		else													j = 0;
		runs[ i ] = j;
	}
	
	// Find the longest run of zeros.
	
	k = -1;
	j = 0;
	for( i = 0; i < 8; ++i )
	{
		x = runs[ i ];
		if( x > j )
		{
			k = i;
			j = x;
		}
	}
	
	// Build the string.
	
	for( i = 0; i < 8; ++i )
	{
		if( i == k )
		{
			if( i == 0 ) *dst++ = ':';
			*dst++ = ':';
			i += ( runs[ i ] - 1 );
			continue;
		}
		seg = &segs[ i ][ 0 ];
		for( j = 0; ( c = seg[ j ] ) != '\0'; ++j ) *dst++ = c;
		if( i != 7 ) *dst++ = ':';
	}
		
trailer:
	if( inScope > 0 )	// Scope
	{
		*dst++ = '%';
		if( inFlags & kIPv6AddressToCStringEscapeScopeID )
		{
			*dst++ = '2';
			*dst++ = '5';
		}
		#if( TARGET_OS_POSIX )
		{
			char		buf[ 64 ];
			char *		p;
			
			check_compile_time_code( countof( buf ) >= IFNAMSIZ );
			
			p = if_indextoname( (unsigned int) inScope, buf );
			if( p && ( *p != '\0' ) )	while( ( c = *p++ ) != '\0' ) *dst++ = c;
			else						append_decimal_string( inScope, dst );
		}
		#else
			append_decimal_string( inScope, dst );
		#endif
	}
	if( inPort > 0 )	// Port
	{
		*dst++ = ']';
		*dst++ = ':';
		append_decimal_string( inPort, dst );
	}
	else if( inPort == kIPv6AddressToCStringForceIPv6Brackets )
	{
		*dst++ = ']';
	}
	if( inPrefix >= 0 )	// Prefix
	{
		*dst++ = '/';
		append_decimal_string( inPrefix, dst );
	}
	*dst = '\0';
	return( inBuffer );
}

//===========================================================================================================================
//	IPv4AddressToCString
//===========================================================================================================================

char *	IPv4AddressToCString( uint32_t inIP, int inPort, char *inBuffer )
{
	char *		dst;
	int			i;
	int			x;
	
	dst = inBuffer;
	for( i = 1; i <= 4; ++i )
	{
		x = ( inIP >> ( 32 - ( 8 * i ) ) ) & 0xFF;
		append_decimal_string( x, dst );
		if( i < 4 ) *dst++ = '.';
	}
	if( inPort > 0 )
	{
		*dst++ = ':';
		append_decimal_string( inPort, dst );
	}
	*dst = '\0';
	return( inBuffer );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BCDTextToInt
//===========================================================================================================================

uint64_t	BCDTextToInt( const char *inSrc, size_t inLen, const char **outSrc )
{
	uint64_t			bcd;
	uint64_t			dec;
	const char *		end;
	int					i;
	
	if( inLen == kSizeCString ) inLen = strlen( inSrc );
	bcd = 0;
	for( end = inSrc + inLen; ( inSrc < end ) && isdigit_safe( *inSrc ); ++inSrc )
	{
		bcd = ( bcd * 10 ) + ( *inSrc - '0' );
	}
	
	dec = 0;
	for( i = (int)( ( sizeof( bcd ) * 8 ) - 4 ); i >= 0; i -= 4 )
	{
		dec = ( dec * 10 ) + ( ( bcd >> i ) & 0x0F );
	}
	
	if( outSrc ) *outSrc = inSrc;
	return( dec );
}

//===========================================================================================================================
//	BCDTextFromInt
//===========================================================================================================================

char *	BCDTextFromInt( uint64_t inValue, char *inBuf, size_t inLen )
{
	uint64_t		bcd;
	int				i;
	
	bcd = 0;
	for( i = 0; inValue != 0; ++i )
	{
		bcd |= ( ( inValue % 10 ) << ( i * 4 ) );
		inValue /= 10;
	}
	SNPrintF( inBuf, inLen, "%llu", bcd );
	return( inBuf );
}

//===========================================================================================================================
//	BitListString_Make
//===========================================================================================================================

char *	BitListString_Make( uint32_t inBits, char *inBuffer, size_t *outSize )
{
	char *		p;
	uint32_t	bit;
	uint32_t	tmp;
	
	p = inBuffer;
	for( bit = 0; inBits != 0; ++bit, inBits >>= 1 )
	{
		if( inBits & 1 )
		{
			if( p != inBuffer ) *p++ = ',';
			
			tmp = bit;
			append_decimal_string( tmp, p );
		}
	}
	*p = '\0';
	if( outSize ) *outSize = (size_t)( p - inBuffer );
	return( inBuffer );
}

//===========================================================================================================================
//	BitListString_Parse
//===========================================================================================================================

OSStatus	BitListString_Parse( const void *inStr, size_t inLen, uint32_t *outBits )
{
	OSStatus		err;
	const char *	src;
	const char *	end;
	const char *	ptr;
	uint32_t		bits;
	uint32_t		x;
	char			c;
	
	src = (const char *) inStr;
	if( inLen == kSizeCString ) inLen = strlen( src );
	end = src + inLen;
	
	bits = 0;
	c = 0;
	for( ; src < end; ++src )
	{
		x = 0;
		for( ptr = src; ( src < end ) && ( ( c = *src ) >= '0' ) && ( c <= '9' ); ++src )
		{
			x = ( x * 10 ) + ( c - '0' );
		}
		require_action( src != ptr, exit, err = kMalformedErr );
		require_action( ( c == ',' ) || ( src == end ), exit, err = kMalformedErr );
		require_action( x < 32, exit, err = kRangeErr );
		
		bits |= ( 1 << x );
	}
	
	*outBits = bits;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	FourCharCodeToCString
//===========================================================================================================================

char *	FourCharCodeToCString( uint32_t inCode, char outString[ 5 ] )
{
	char *		p;
	char		c;
	
	p    = outString;
	*p++ = ( ( c = (char)( ( inCode >> 24 ) & 0xFF ) ) == '\0' ) ? ' ' : c;
	*p++ = ( ( c = (char)( ( inCode >> 16 ) & 0xFF ) ) == '\0' ) ? ' ' : c;
	*p++ = ( ( c = (char)( ( inCode >>  8 ) & 0xFF ) ) == '\0' ) ? ' ' : c;
	*p++ = ( ( c = (char)(   inCode         & 0xFF ) ) == '\0' ) ? ' ' : c;
	*p 	 = '\0';
	return( outString );
}

//===========================================================================================================================
//	TextToFourCharCode
//===========================================================================================================================

uint32_t	TextToFourCharCode( const void *inText, size_t inSize )
{
	uint32_t			code;
	const uint8_t *		src;
	uint8_t				c;
	
	if( inSize == kSizeCString ) inSize = strlen( (const char *) inText );
	src = (const uint8_t *) inText;
	code = (uint32_t)(                   ( 0 < inSize ) ? ( ( c = src[ 0 ] ) != 0 ) ? c : ' ' : ' ' );
	code = (uint32_t)( ( code << 8 ) | ( ( 1 < inSize ) ? ( ( c = src[ 1 ] ) != 0 ) ? c : ' ' : ' ' ) );
	code = (uint32_t)( ( code << 8 ) | ( ( 2 < inSize ) ? ( ( c = src[ 2 ] ) != 0 ) ? c : ' ' : ' ' ) );
	code = (uint32_t)( ( code << 8 ) | ( ( 3 < inSize ) ? ( ( c = src[ 3 ] ) != 0 ) ? c : ' ' : ' ' ) );
	return( code );
}

//===========================================================================================================================
//	TextToHardwareAddress
//
//	Parses hardware address text (e.g. AA:BB:CC:00:11:22:33:44 for Fibre Channel) into an n-byte array.
//	Segments can be separated by a colon ':', dash '-', or a space ' '. Segments do not need zero padding 
//	(e.g. "0:1:2:3:4:5:6:7" is equivalent to "00:01:02:03:04:05:06:07").
//===========================================================================================================================

OSStatus	TextToHardwareAddress( const void *inText, size_t inTextSize, size_t inAddrSize, void *outAddr )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	int					i;
	int					x;
	char				c;
	uint8_t *			dst;
	
	if( inTextSize == kSizeCString ) inTextSize = strlen( (const char *) inText );
	src = (const char *) inText;
	end = src + inTextSize;
	dst = (uint8_t *) outAddr;
	
	while( inAddrSize-- > 0 )
	{
		x = 0;
		i = 0;
		while( ( i < 2 ) && ( src < end ) )
		{
			c = *src++;
			if(      isdigit_safe(  c ) ) { x = ( x * 16 )      +               ( c   - '0' ); ++i; }
			else if( isxdigit_safe( c ) ) { x = ( x * 16 ) + 10 + ( tolower_safe( c ) - 'a' ); ++i; }
			else if( ( i != 0 ) || ( ( c != ':' ) && ( c != '-' ) && ( c != ' ' ) ) ) break;
		}
		if( i == 0 )
		{
			err = kMalformedErr;
			goto exit;
		}
		check( ( x >= 0x00 ) && ( x <= 0xFF ) );
		if( dst ) *dst++ = (uint8_t) x;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TextToHardwareAddressScalar
//===========================================================================================================================

uint64_t	TextToHardwareAddressScalar( const void *inText, size_t inTextSize, size_t inAddrSize )
{
	uint8_t			addr[ 8 ];
	OSStatus		err;
	
	require( inAddrSize <= 8, error );
	err = TextToHardwareAddress( inText, inTextSize, inAddrSize, addr );
	require_noerr_quiet( err, error );
	
	switch( inAddrSize )
	{
		case 6:  return( ReadBig48( addr ) );
		case 8:  return( ReadBig64( addr ) );
		default: dlogassert( "Bad hardware address size: %zu", inAddrSize ); break;
	}
	
error:
	return( 0 );
}

//===========================================================================================================================
//	HardwareAddressToCString
//===========================================================================================================================

char *	HardwareAddressToCString( const void *inAddr, size_t inSize, char *outStr )
{
	const uint8_t *		src;
	const uint8_t *		end;
	char *				dst;
	uint8_t				b;
	
	src = (const uint8_t *) inAddr;
	end = src + inSize;
	dst = outStr;
	while( src < end )
	{
		if( dst != outStr ) *dst++ = ':';
		b = *src++;
		*dst++ = kHexDigitsUppercase[ b >> 4 ];
		*dst++ = kHexDigitsUppercase[ b & 0xF ];
	}
	*dst = '\0';
	return( outStr );
}

//===========================================================================================================================
//	TextToNumVersion
//===========================================================================================================================

OSStatus	TextToNumVersion( const void *inText, size_t inSize, uint32_t *outVersion )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	const uint8_t *		tokenStart;
	const uint8_t *		tokenEnd;
	uint32_t			major;
	uint32_t			minor;
	uint32_t			bugFix;
	uint32_t			stage;
	uint32_t			revision;
	uint32_t			version;
	int					c;
	
	if( inSize == kSizeCString ) inSize = strlen( (const char *) inText );
	
	// Default to 0.0.0f0.
	
	bugFix 		= 0;
	stage 		= kVersionStageFinal;
	revision 	= 0;
	
	src = (const uint8_t *) inText;
	end = src + inSize;
	
	// Skip leading white space.
	
	while( ( src < end ) && isspace( *src ) ) ++src;
	
	// Parse the major version and convert to a number.
	
	tokenStart = src;
	while( ( src < end ) && isdigit( *src ) ) ++src;
	tokenEnd = src;
	require_action_quiet( tokenStart != tokenEnd, exit, err = kMalformedErr );
	major = (uint32_t) TextToInt32( tokenStart, (size_t)( tokenEnd - tokenStart ), 10 );
	require_action_quiet( major <= 255, exit, err = kRangeErr );
	if( src < end ) ++src;
	
	// Parse the minor version and convert to a number.
	
	tokenStart = src;
	while( ( src < end ) && isdigit( *src ) ) ++src;
	tokenEnd = src;
	minor = (uint32_t) TextToInt32( tokenStart, (size_t)( tokenEnd - tokenStart ), 10 );
	require_action_quiet( minor <= 15, exit, err = kRangeErr );
	if( ( tokenStart != tokenEnd ) && ( src < end ) )
	{
		c = tolower( *src );
		++src;
		if( c == '.' )
		{
			// Parse the bug fix number and convert to a number.
			
			tokenStart = src;
			while( ( src < end ) && isdigit( *src ) ) ++src;
			tokenEnd = src;
			bugFix = (uint32_t) TextToInt32( tokenStart, (size_t)( tokenEnd - tokenStart ), 10 );
			require_action_quiet( bugFix <= 15, exit, err = kRangeErr );
			if( ( tokenStart != tokenEnd ) && ( src < end ) )
			{
				c = tolower( *src );
				++src;
			}
		}
		switch( c )
		{
			case 'd': stage = kVersionStageDevelopment; break;
			case 'a': stage = kVersionStageAlpha;		break;
			case 'b': stage = kVersionStageBeta;		break;
			default: break; // It's okay for this to be something else (e.g. ".", "f". etc.).
		}
		
		// Parse the non-release revision number.
		
		tokenStart = src;
		while( ( src < end ) && isdigit( *src ) ) ++src;
		tokenEnd = src;
		if( tokenStart < tokenEnd )
		{
			revision = (uint32_t) TextToInt32( tokenStart, (size_t)( tokenEnd - tokenStart ), 10 );
		}
		if( ( tokenStart == tokenEnd ) || ( revision > 255 ) )
		{
			// Invalid stage or revision so assume the text beyond the minor version is not part of the version.
			
			stage		= kVersionStageFinal;
			revision	= 0;
		}
	}
	
	version  = (uint32_t)( major  << 24 );
	version |= (uint32_t)( minor  << 20 );
	version |= (uint32_t)( bugFix << 16 );
	version |= (uint32_t)( stage  <<  8 );
	version |= (uint32_t)  revision;
	
	if( outVersion ) *outVersion = version;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NumVersionToCString
//===========================================================================================================================

char *	NumVersionToCString( uint32_t inVersion, char *inString )
{
	char *		dst;
	char *		lim;
	uint8_t		majorRev;
	uint8_t		minor;
	uint8_t		bugFix;
	uint8_t		stage;
	uint8_t		revision;
		
	majorRev 	= (uint8_t)( ( inVersion >> 24 ) & 0xFF );
	minor		= (uint8_t)( ( inVersion >> 20 ) & 0x0F );
	bugFix		= (uint8_t)( ( inVersion >> 16 ) & 0x0F );
	stage 		= (uint8_t)( ( inVersion >>  8 ) & 0xFF );
	revision 	= (uint8_t)(   inVersion         & 0xFF );
	
	// Convert the major, minor, and bugfix numbers. Bugfix only added if it is non-zero (e.g. 6.2 and not 6.2.0).
	
	dst = inString;
	lim = dst + 14; // Largest possible result is "255.15.15d255", which is 14 bytes, including the null terminator.
	SNPrintF_Add( &dst, lim, "%u.%u", majorRev, minor );
	if( bugFix != 0 ) SNPrintF_Add( &dst, lim, ".%u", bugFix );
	
	// Convert the version stage and non-release revision number.
	
	switch( stage )
	{
		case kVersionStageDevelopment:	SNPrintF_Add( &dst, lim, "d%u", revision ); break;
		case kVersionStageAlpha:		SNPrintF_Add( &dst, lim, "a%u", revision ); break;
		case kVersionStageBeta:			SNPrintF_Add( &dst, lim, "b%u", revision ); break;
		case kVersionStageFinal:
			
			// A non-release revision of zero is a special case indicating the software is GM (at the golden master 
			// stage) and therefore, the non-release revision should not be added to the string.
			
			if( revision != 0 ) SNPrintF_Add( &dst, lim, "f%u", revision );
			break;
		
		default:
			// Don't assert on a version stage of 0 to handle code that masks the low bits...assume GM.
			
			if( stage != 0 ) dlog( kLogLevelAssert, "### invalid NumVersion stage: 0x%02X\n", stage );
			break;
	}
	return( inString );
}

//===========================================================================================================================
//	TextToSourceVersion
//===========================================================================================================================

uint32_t	TextToSourceVersion( const void *inText, size_t inSize )
{
	uint32_t			version;
	const uint8_t *		src;
	const uint8_t *		end;
	uint8_t				c;
	uint32_t			x;
	uint32_t			y;
	uint32_t			z;
	
	version = 0;
	
	if( inSize == kSizeCString ) inSize = strlen( (const char *) inText );
	src = (const uint8_t *) inText;
	end = src + inSize;
	while( ( src < end ) && isspace( *src ) ) ++src;
	
	c = 0;
	for( x = 0; ( src < end ) && isdigit( c = *src ) && ( x <= 214747 ); ++src ) x = ( x * 10 ) + ( c - '0' );
	if( x > 214747 ) goto exit;
	if( c == '.' ) ++src;
		
	for( y = 0; ( src < end ) && isdigit( c = *src ) && ( y <= 99 ); ++src ) y = ( y * 10 ) + ( c - '0' );
	if( y > 99 ) goto exit;
	if( c == '.' ) ++src;
	
	for( z = 0; ( src < end ) && isdigit( c = *src ) && ( z <= 99 ); ++src ) z = ( z * 10 ) + ( c - '0' );
	if( z > 99 ) goto exit;
	
	version = ( x * 10000 ) + ( y * 100 ) + z;
	
exit:
	return( version );
}

//===========================================================================================================================
//	SourceVersionToCString
//===========================================================================================================================

char *	SourceVersionToCString( uint32_t inVersion, char *inString )
{
	unsigned int		x;
	unsigned int		y;
	unsigned int		z;
	
	x =   inVersion / 10000;
	y = ( inVersion /   100 ) % 100;
	z =   inVersion %   100;
	
	if( z )			snprintf( inString, 16, "%u.%u.%u", x, y, z );
	else if( y )	snprintf( inString, 16, "%u.%u", x, y );
	else			snprintf( inString, 16, "%u", x );
	
	return( inString );
}

//===========================================================================================================================
//	HexToData
//===========================================================================================================================

OSStatus
	HexToData( 
		const void *	inStr, 
		size_t			inLen, 
		HexToDataFlags	inFlags, 
		void *			inBuf, 
		size_t			inMaxBytes, 
		size_t *		outWrittenBytes, 
		size_t *		outTotalBytes, 
		const char **	outNext )
{
	Boolean const		ignoreDelims	= ( inFlags & kHexToData_IgnoreDelimiters ) ? true : false;
	Boolean const		ignorePrefixes	= ( inFlags & kHexToData_IgnorePrefixes )   ? true : false;
	Boolean const		ignoreSpaces	= ( inFlags & kHexToData_IgnoreWhitespace ) ? true : false;
	const char *		src;
	const char *		end;
	uint8_t *			buf;
	size_t				writtenBytes;
	size_t				totalBytes;
	int					state;
	char				c;
	uint8_t				nibble;
	uint8_t				byte;
	
	src = (const char *) inStr;
	if( inLen == kSizeCString ) inLen = strlen( src );
	end = src + inLen;
	buf = (uint8_t *) inBuf;
	writtenBytes = 0;
	totalBytes   = 0;
	
	// Skip leading whitespace.
	
	while( ( src < end ) && isspace_safe( *src ) )
		++src;
	
	// Skip optional leading "0x".
	
	if( ( ( end - src ) >= 2 ) && ( src[ 0 ] == '0' ) && ( ( src[ 1 ] == 'x' ) || ( src[ 1 ] == 'X' ) ) )
		src += 2;
	
	// Parse each nibble character and convert to bytes.
	
	byte  = 0;
	state = 0;
	for( ; src < end; ++src )
	{
		if( ignorePrefixes && ( state == 0 ) && ( ( end - src ) >= 2 ) && 
			( src[ 0 ] == '0' ) && ( ( src[ 1 ] == 'x' ) || ( src[ 1 ] == 'X' ) ) )
		{
			src += 1;
			continue;
		}
		
		c = *src;
		if(      ( c >= '0' ) && ( c <= '9' ) ) nibble = c - '0';
		else if( ( c >= 'A' ) && ( c <= 'F' ) ) nibble = 10 + ( c - 'A' );
		else if( ( c >= 'a' ) && ( c <= 'f' ) ) nibble = 10 + ( c - 'a' );
		else if( ignoreSpaces && isspace_safe( c ) ) continue;
		else if( ignoreDelims && ( ( c == ':' ) || ( c == '-' ) || ( c == '_' ) || ( c == ',' ) ) ) continue;
		else break;
		
		if( state == 0 )
		{
			byte = (uint8_t)( nibble << 4 );
			state = 1;
		}
		else
		{
			byte |= nibble;
			if( buf && ( writtenBytes < inMaxBytes ) )
				buf[ writtenBytes++ ] = byte;
			++totalBytes;
			state = 0;
		}
	}
	
	// If we wrote partial data to the current byte, optionally write it to the buffer.
	
	if( ( state != 0 ) && !( inFlags & kHexToData_WholeBytes ) )
	{
		if( buf && ( writtenBytes < inMaxBytes ) )
			buf[ writtenBytes++ ] = byte;
		++totalBytes;
	}
	
	// Optionally set any unwritten bytes to 0 (but don't count them as written bytes).
	
	if( inBuf && ( inFlags & kHexToData_ZeroPad ) )
	{
		size_t		n;
		
		for( n = writtenBytes; n < inMaxBytes; ++n )
			buf[ n ] = 0;
	}
	
	if( outWrittenBytes )	*outWrittenBytes	= writtenBytes;
	if( outTotalBytes )		*outTotalBytes		= totalBytes;
	if( outNext )			*outNext			= src;
	
	// If the only thing remaining is insignificant whitespace then we parsed everything.
	
	while( ( src < end ) && isspace_safe( *src ) ) ++src;
	if( src == end )
		return( kNoErr );
	
	// If there's still hex digits left in the source then we either ran out of buffer space or 
	// there's whitespace in between hex characters and we're not ignoring in-between whitespace.
	
	if( isxdigit_safe( *src ) )
	{
		if( writtenBytes < inMaxBytes )
			return( kFormatErr );
		else
			return( kOverrunErr );
	}
	
	// If there are non-hex characters left in the source and we didn't parse anything then it's not a hex string.
	
	if( totalBytes == 0 )
		return( kMalformedErr );
	
	return( kNoErr );
}

//===========================================================================================================================
//	HexToDataCopy
//===========================================================================================================================

OSStatus
	HexToDataCopy( 
		const void *	inStr, 
		size_t			inLen, 
		HexToDataFlags	inFlags, 
		void *			outBytePtr, 
		size_t *		outByteCount, 
		const char **	outNext )
{
	OSStatus		err;
	size_t			totalBytes;
	uint8_t *		bytes;
	
	totalBytes = 0;
	HexToData( inStr, inLen, inFlags, NULL, 0, NULL, &totalBytes, NULL );
	
	bytes = (uint8_t *) malloc( totalBytes + 1 );
	require_action( bytes, exit, err = kNoMemoryErr );
	
	HexToData( inStr, inLen, inFlags, bytes, totalBytes, outByteCount, NULL, outNext );
	bytes[ totalBytes ] = 0; // Null terminate for convenience.
	*( (void **) outBytePtr ) = bytes;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	DataToHexCString
//===========================================================================================================================

char *	DataToHexCStringEx( const void *inData, size_t inSize, char *outStr, const char * const inHexDigits )
{
	const uint8_t *		src;
	const uint8_t *		end;
	char *				dst;
	uint8_t				b;
	
	src = (const uint8_t *) inData;
	end = src + inSize;
	dst = outStr;
	while( src < end )
	{
		b = *src++;
		*dst++ = inHexDigits[ b >> 4 ];
		*dst++ = inHexDigits[ b & 0xF ];
	}
	*dst = '\0';
	return( outStr );	
}

//===========================================================================================================================
//	TextToInt32
//===========================================================================================================================

int32_t	TextToInt32( const void *inText, size_t inSize, int inBase )
{
	int32_t						v;
	const unsigned char *		src;
	const unsigned char *		end;
	int							c;
	int							negative;
	
	check( ( inBase == 0 ) || ( inBase == 16 ) || ( inBase == 10 ) || ( inBase == 8 ) || ( inBase == 2 ) );
	if( inSize == kSizeCString ) inSize = strlen( (const char *) inText );
	src = (const unsigned char *) inText;
	end = src + inSize;
	
	// Skip leading whitespace.
	
	while( ( src < end ) && isspace( *src ) ) ++src;
	
	// Use prefix (if present) to determine base. Otherwise, default to base 10.
	
	if( ( end - src ) >= 2 )
	{
		if( ( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'x' ) )			// Hex
		{
			src   += 2;
			inBase = 16;
		}
		else if( ( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'b' ) )	// Binary
		{
			src   += 2;
			inBase = 2;
		}
		else if( ( inBase == 0 )     && ( src[ 0 ] == '0' ) && 
				 ( src[ 1 ] >= '0' ) && ( src[ 1 ] <= '7' ) )				// Octal
		{
			src   += 1;
			inBase = 8;
		}
	}
	if( inBase == 0 ) inBase = 10;
	
	// Convert using the specified base.
	
	v = 0;
	switch( inBase )
	{
		case 16:	// Hex
		
			while( src < end )
			{
				c = *src++;				
				if(      isdigit(  c ) ) v = ( v * 16 ) + ( c - '0' );						// 0-9 digit
				else if( isxdigit( c ) ) v = ( v * 16 ) + ( 10 + ( tolower( c ) - 'a' ) );	// A-F digit
				else break;
			}
			break;
		
		case 10:	// Decimal
		
			c = *src;
			negative = ( c == '-' );
			if( negative || ( c == '+' ) ) ++src;
			while( src < end )
			{
				c = *src++;
				if( c == ',' ) continue;	// Skip commas so 1,000 is treated as 1000.
				if( !isdigit( c ) ) break;
				
				v = ( v * 10 ) + ( c - '0' );
			}
			if( negative ) v = -v;
			break;
		
		case 8:		// Octal
		
			while( src < end )
			{
				c = *src++;
				if( ( c < '0' ) || ( c > '7' ) ) break;
				
				v = ( v * 8 ) + ( c - '0' );
			}
			break;
		
		case 2:		// Binary
		
			while( src < end )
			{
				c = *src++;
				if( ( c != '0' ) && ( c != '1' ) ) break;
				
				v = ( v * 2 ) + ( c - '0' );
			}
			break;
		
		default:
			dlog( kLogLevelError, "%s: unsupported base: %d\n", __ROUTINE__, inBase );
			break;
	}
	return( v );
}

//===========================================================================================================================
//	NormalizeUUIDString
//===========================================================================================================================

OSStatus
	NormalizeUUIDString( 
		const char *	inUUIDStr, 
		size_t			inLen, 
		const void *	inBaseUUID, 
		uint32_t		inFlags, 
		char *			outUUIDStr )
{
	OSStatus		err;
	uint8_t			uuid[ 16 ];
	
	err = StringToUUIDEx( inUUIDStr, inLen, ( inFlags & kUUIDFlag_LittleEndian ) ? true : false, inBaseUUID, uuid );
	require_noerr_quiet( err, exit );
	
	UUIDtoCStringFlags( uuid, 16, inBaseUUID, inFlags, outUUIDStr, &err );
	
exit:
	return( err );
}

//===========================================================================================================================
//	UUIDtoCString
//===========================================================================================================================

char *	UUIDtoCString( const void *inUUID, int inLittleEndian, void *inBuffer )
{
	return( UUIDtoCStringEx( inUUID, 16, inLittleEndian, NULL, inBuffer ) );
}

char *	UUIDtoCStringEx( const void *inUUID, size_t inSize, int inLittleEndian, const uint8_t *inBaseUUID, void *inBuffer )
{
	return( UUIDtoCStringFlags( inUUID, inSize, inBaseUUID, inLittleEndian ? kUUIDFlag_LittleEndian : kUUIDFlags_None, 
		inBuffer, NULL ) );
}

char *
	UUIDtoCStringFlags( 
		const void *	inUUID, 
		size_t			inSize, 
		const void *	inBaseUUID, 
		uint32_t		inFlags, 
		void *			inBuffer, 
		OSStatus *		outErr )
{
	const uint8_t * const		baseUUID		= (const uint8_t *) inBaseUUID;
	Boolean const				littleEndian	= ( inFlags & kUUIDFlag_LittleEndian ) ? true : false;
	Boolean const				shortForm		= ( inFlags & kUUIDFlag_ShortForm )    ? true : false;
	const uint8_t *				a				= (const uint8_t *) inUUID;
	char * const				buf				= (char *) inBuffer;
	OSStatus					err				= kNoErr;
	uint8_t						tempUUID[ 16 ];
	
	if( ( inSize == 1 ) && baseUUID )
	{
		memcpy( tempUUID, baseUUID, 16 );
		tempUUID[ littleEndian ? 0 : 3 ] = a[ 0 ];
		a = tempUUID;
	}
	else if( ( inSize == 2 ) && baseUUID )
	{
		memcpy( tempUUID, baseUUID, 16 );
		if( littleEndian )
		{
			tempUUID[ 0 ] = a[ 1 ];
			tempUUID[ 1 ] = a[ 0 ];
		}
		else
		{
			tempUUID[ 2 ] = a[ 0 ];
			tempUUID[ 3 ] = a[ 1 ];
		}
		a = tempUUID;
	}
	else if( ( inSize == 4 ) && baseUUID )
	{
		memcpy( tempUUID, baseUUID, 16 );
		if( littleEndian )
		{
			tempUUID[ 3 ] = a[ 0 ];
			tempUUID[ 2 ] = a[ 1 ];
			tempUUID[ 1 ] = a[ 2 ];
			tempUUID[ 0 ] = a[ 3 ];
		}
		else
		{
			tempUUID[ 0 ] = a[ 0 ];
			tempUUID[ 1 ] = a[ 1 ];
			tempUUID[ 2 ] = a[ 2 ];
			tempUUID[ 3 ] = a[ 3 ];
		}
		a = tempUUID;
	}
	else if( inSize != 16 )
	{
		a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
		err = kSizeErr;
	}
	if( shortForm && baseUUID && ( memcmp( &a[ 4 ], &baseUUID[ 4 ], 12 ) == 0 ) )
	{
		snprintf( buf, 37, "%x", (unsigned int)( littleEndian ? ReadLittle32( a ) : ReadBig32( a ) ) );
	}
	else
	{
		if( littleEndian )
		{
			snprintf( buf, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
				a[  3 ], a[  2 ], a[  1 ], a[  0 ], a[  5 ], a[  4 ], a[  7 ], a[  6 ], 
				a[  8 ], a[  9 ], a[ 10 ], a[ 11 ], a[ 12 ], a[ 13 ], a[ 14 ], a[ 15 ] );
		}
		else
		{
			snprintf( buf, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
				a[  0 ], a[  1 ], a[  2 ], a[  3 ], a[  4 ], a[  5 ], a[  6 ], a[  7 ], 
				a[  8 ], a[  9 ], a[ 10 ], a[ 11 ], a[ 12 ], a[ 13 ], a[ 14 ], a[ 15 ] );
		}
	}
	if( outErr ) *outErr = err;
	return( buf );
}

//===========================================================================================================================
//	StringToUUID
//===========================================================================================================================

OSStatus	StringToUUID( const char *inStr, size_t inSize, int inLittleEndian, void *outUUID )
{
	return( StringToUUIDEx( inStr, inSize, inLittleEndian, NULL, outUUID ) );
}

//===========================================================================================================================
//	StringToUUIDEx
//===========================================================================================================================

OSStatus	StringToUUIDEx( const char *inStr, size_t inSize, int inLittleEndian, const uint8_t *inBaseUUID, void *outUUID )
{
	uint8_t * const		uuidPtr = (uint8_t *) outUUID;
	OSStatus			err;
	uint8_t				a[ 16 ];
	int					n;
	int					i, i2;
	int64_t				s64;
	
	if( inSize == kSizeCString ) inSize = strlen( inStr );
	
	i = 0;
	if( inLittleEndian )
	{
		n = SNScanF( inStr, inSize, "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%n", 
			&a[ 3 ], &a[ 2 ], &a[  1 ], &a[  0 ], &a[  5 ], &a[  4 ], &a[  7 ], &a[  6 ], 
			&a[ 8 ], &a[ 9 ], &a[ 10 ], &a[ 11 ], &a[ 12 ], &a[ 13 ], &a[ 14 ], &a[ 15 ], 
			&i );
	}
	else
	{
		n = SNScanF( inStr, inSize, "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%n", 
			&a[ 0 ], &a[ 1 ], &a[  2 ], &a[  3 ], &a[  4 ], &a[  5 ], &a[  6 ], &a[  7 ], 
			&a[ 8 ], &a[ 9 ], &a[ 10 ], &a[ 11 ], &a[ 12 ], &a[ 13 ], &a[ 14 ], &a[ 15 ], 
			&i );
	}
	if( ( n != 16 ) && inBaseUUID )
	{
		if( SNScanF( inStr, inSize, "%llx%n", &s64, &i2 ) == 1 )
		{
			if( ( s64 >= 0 ) && ( s64 <= UINT32_MAX ) && ( i2 == ( (int) inSize ) ) )
			{
				if( uuidPtr )
				{
					memcpy( uuidPtr, inBaseUUID, 16 );
					if( inLittleEndian )	WriteLittle32( &uuidPtr[ 0 ], (uint32_t) s64 );
					else					WriteBig32(    &uuidPtr[ 0 ], (uint32_t) s64 );
				}
				err = kNoErr;
				goto exit;
			}
		}
	}
	require_action_quiet( n == 16, exit, err = kMalformedErr );
	require_action_quiet( i == 36, exit, err = kMalformedErr );
	
	if( uuidPtr ) memcpy( uuidPtr, a, 16 );
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == ANSI C Extensions ==
#endif




#if( !TARGET_OS_WINDOWS )
//===========================================================================================================================
//	memicmp
//===========================================================================================================================

int	memicmp( const void *inP1, const void *inP2, size_t inLen )
{
	const unsigned char *		p1;
	const unsigned char *		e1;
	const unsigned char *		p2;
	int							c1;
	int							c2;
	
	p1 = (const unsigned char *) inP1;
	e1 = p1 + inLen;
	p2 = (const unsigned char *) inP2;
	while( p1 < e1 )
	{
		c1 = *p1++;
		c2 = *p2++;
		c1 = tolower( c1 );
		c2 = tolower( c2 );
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
	}
	return( 0 );
}
#endif

//===========================================================================================================================
//	snprintf_add / vsnprintf_add
//===========================================================================================================================

OSStatus	snprintf_add( char **ioPtr, char *inEnd, const char *inFormat, ... )
{
	OSStatus	err;
	va_list		args;
	
	va_start( args, inFormat );
	err = vsnprintf_add( ioPtr, inEnd, inFormat, args );
	va_end( args );
	return( err );
}

OSStatus	vsnprintf_add( char **ioPtr, char *inEnd, const char *inFormat, va_list inArgs )
{
	char * const		ptr = *ioPtr;
	size_t				len;
	int					n;
	
	len = (size_t)( inEnd - ptr );
	require_action_quiet( len > 0, exit, n = kNoSpaceErr );
	
	n = vsnprintf( ptr, len, inFormat, inArgs );
	require( n >= 0, exit );
	if( n >= ( (int) len ) )
	{
		*ioPtr = inEnd;
		n = kOverrunErr;
		goto exit;
	}
	*ioPtr = ptr + n;
	n = kNoErr;
	
exit:
	return( n );
}


#if( TARGET_OS_DARWIN_KERNEL || TARGET_OS_VXWORKS )
//===========================================================================================================================
//	strdup
//===========================================================================================================================

char *	strdup( const char *inString )
{
	size_t		size;
	char *		s;
	
	size = strlen( inString ) + 1;
	s = (char *) malloc( size );
	require( s, exit );
	
	memcpy( s, inString, size );
	
exit:
	return( s );
}
#endif

#if( !TARGET_OS_LINUX )
//===========================================================================================================================
//	strndup
//===========================================================================================================================

char *	strndup( const char *inStr, size_t inN )
{
	size_t		size;
	char *		s;
	
	size = strnlen( inStr, inN );
	s = (char *) malloc( size + 1 );
	require( s, exit );
	
	memcpy( s, inStr, size );
	s[ size ] = '\0';
	
exit:
	return( s );
}
#endif


#if( !defined( _MSL_EXTRAS_H ) && !TARGET_OS_POSIX && !TARGET_OS_WINDOWS && !TARGET_PLATFORM_WICED )
//===========================================================================================================================
//	stricmp
//
//	Like the ANSI C strcmp routine, but performs a case-insensitive compare.
//===========================================================================================================================

int	stricmp( const char *inS1, const char *inS2 )
{
	int		c1;
	int		c2;
	
	for( ;; )
	{
		c1 = tolower( *( (const unsigned char *) inS1 ) );
		c2 = tolower( *( (const unsigned char *) inS2 ) );
		if( c1 < c2 )    return( -1 );
		if( c1 > c2 )    return(  1 );
		if( c1 == '\0' ) break;
		
		++inS1;
		++inS2;
	}
	return( 0 );
}
#endif

#if( !defined( _MSL_EXTRAS_H ) && !TARGET_OS_POSIX && !TARGET_OS_WINDOWS && !TARGET_PLATFORM_WICED )
//===========================================================================================================================
//	strnicmp
//
//	Like the ANSI C strncmp routine, but performs a case-insensitive compare.
//===========================================================================================================================

int	strnicmp( const char *inS1, const char *inS2, size_t inMax )
{
	const char *		end;
	int					c1;
	int					c2;
	
	end = inS1 + inMax;
	while( inS1 < end )
	{
		c1 = tolower( *( (const unsigned char *) inS1 ) );
		c2 = tolower( *( (const unsigned char *) inS2 ) );
		if( c1 < c2 )    return( -1 );
		if( c1 > c2 )    return(  1 );
		if( c1 == '\0' ) break;
		
		++inS1;
		++inS2;
	}
	return( 0 );
}
#endif

//===========================================================================================================================
//	strcmp_prefix
//
//	Like strcmp, but only compares to the end of the prefix string.
//===========================================================================================================================

int	strcmp_prefix( const char *inStr, const char *inPrefix )
{
	int		c1;
	int		c2;
	
	for( ;; )
	{
		c1 = *( (const unsigned char *) inStr );
		c2 = *( (const unsigned char *) inPrefix );
		if( c2 == '\0' ) break;
		if( c1 < c2 )    return( -1 );
		if( c1 > c2 )    return(  1 );
		
		++inStr;
		++inPrefix;
	}
	return( 0 );
}

//===========================================================================================================================
//	stricmp_prefix
//
//	Like stricmp, but only compares to the end of the prefix string.
//===========================================================================================================================

int	stricmp_prefix( const char *inStr, const char *inPrefix )
{
	int		c1;
	int		c2;
	
	for( ;; )
	{
		c1 = tolower( *( (const unsigned char *) inStr ) );
		c2 = tolower( *( (const unsigned char *) inPrefix ) );
		if( c2 == '\0' ) break;
		if( c1 < c2 )    return( -1 );
		if( c1 > c2 )    return(  1 );
		
		++inStr;
		++inPrefix;
	}
	return( 0 );
}

//===========================================================================================================================
//	strncmp_prefix
//
//	Like strncmp routine, but only returns 0 if the entire prefix matches.
//===========================================================================================================================

int	strncmp_prefix( const void *inS1, size_t inN, const char *inPrefix )
{
	const unsigned char *		s1;
	const unsigned char *		s2;
	int							c1;
	int							c2;

	s1 = (const unsigned char *) inS1;
	s2 = (const unsigned char *) inPrefix;
	while( inN-- > 0 )
	{
		c1 = *s1++;
		c2 = *s2++;
		if( c2 == 0 ) return(  0 );
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
	}
	if( *s2 != 0 ) return( -1 );
	return( 0 );
}

//===========================================================================================================================
//	strnicmp_prefix
//
//	Like strnicmp routine, but only returns 0 if the entire prefix matches.
//===========================================================================================================================

int	strnicmp_prefix( const void *inS1, size_t inN, const char *inPrefix )
{
	const unsigned char *		s1;
	const unsigned char *		s2;
	int							c1;
	int							c2;

	s1 = (const unsigned char *) inS1;
	s2 = (const unsigned char *) inPrefix;
	while( inN-- > 0 )
	{
		c1 = tolower( *s1 );
		c2 = tolower( *s2 );
		if( c2 == 0 ) return(  0 );
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
		
		++s1;
		++s2;
	}
	if( *s2 != 0 ) return( -1 );
	return( 0 );
}

//===========================================================================================================================
//	strncmp_suffix
//
//	Like strncmp routine, but only returns 0 if the entire suffix matches.
//===========================================================================================================================

int	strncmp_suffix( const void *inStr, size_t inMaxLen, const char *inSuffix )
{
	const char *		stringPtr;
	size_t				stringLen;
	size_t				suffixLen;
	
	stringPtr = (const char *) inStr;
	stringLen = strnlen( stringPtr, inMaxLen );
	suffixLen = strlen( inSuffix );
	if( suffixLen <= stringLen )
	{
		return( strncmpx( stringPtr + ( stringLen - suffixLen ), suffixLen, inSuffix ) );
	}
	return( -1 );
}

//===========================================================================================================================
//	strnicmp_suffix
//
//	Like strnicmp routine, but only returns 0 if the entire suffix matches.
//===========================================================================================================================

int	strnicmp_suffix( const void *inStr, size_t inMaxLen, const char *inSuffix )
{
	const char *		stringPtr;
	size_t				stringLen;
	size_t				suffixLen;
	
	stringPtr = (const char *) inStr;
	stringLen = strnlen( stringPtr, inMaxLen );
	suffixLen = strlen( inSuffix );
	if( suffixLen <= stringLen )
	{
		return( strnicmpx( stringPtr + ( stringLen - suffixLen ), suffixLen, inSuffix ) );
	}
	return( -1 );
}

//===========================================================================================================================
//	strncmpx
//
//	Like the ANSI C strncmp routine, but requires that all the characters in s1 match all the characters in s2.
//===========================================================================================================================

int	strncmpx( const void *inS1, size_t inN, const char *inS2 )
{
	const unsigned char *		s1;
	const unsigned char *		s2;
	int							c1;
	int							c2;

	s1 = (const unsigned char *) inS1;
	s2 = (const unsigned char *) inS2;
	while( inN-- > 0 )
	{
		c1 = *s1++;
		c2 = *s2++;
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
		if( c2 == 0 ) return(  0 );
	}
	if( *s2 != 0 ) return( -1 );
	return( 0 );
}

//===========================================================================================================================
//	strnicmpx
//
//	Like the ANSI C strncmp routine, but case-insensitive and requires all characters in s1 match all characters in s2.
//===========================================================================================================================

int	strnicmpx( const void *inS1, size_t inN, const char *inS2 )
{
	const unsigned char *		s1;
	const unsigned char *		s2;
	int							c1;
	int							c2;

	s1 = (const unsigned char *) inS1;
	s2 = (const unsigned char *) inS2;
	while( inN-- > 0 )
	{
		c1 = tolower( *s1 );
		c2 = tolower( *s2 );
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
		if( c2 == 0 ) return(  0 );
		
		++s1;
		++s2;
	}
	if( *s2 != 0 ) return( -1 );
	return( 0 );
}

#if( !TARGET_VISUAL_STUDIO_2005_OR_LATER && !TARGET_OS_LINUX && ( QNX_VERSION < 660 ) )
//===========================================================================================================================
//	strnlen
//
//	Like the ANSI C strlen routine, but allows you to specify a maximum size.
//===========================================================================================================================

size_t	strnlen( const char *inStr, size_t inMaxLen )
{
	size_t		len;
	
	for( len = 0; ( len < inMaxLen ) && ( inStr[ len ] != '\0' ); ++len ) {}
	return( len );
}
#endif


#if( !TARGET_OS_VXWORKS )
//===========================================================================================================================
//	stristr
//
//	Case-insensitive version of strstr. Based on Metrowerks strstr.
//===========================================================================================================================

char *	stristr( const char *str, const char *pat )
{
	unsigned char * s1 = (unsigned char *) str;
	unsigned char * p1 = (unsigned char *) pat;
	int firstc, c1, c2;
	
	if ((pat == NULL) || ((firstc = *p1++) == '\0'))
										/*- mm 980424 -*/
										/*- beb 971017 -*/
		return((char *) str);
	
	firstc = tolower(firstc);
	while((c1 = *s1++) != '\0')
	{
		c1 = tolower(c1);
		if (c1 == firstc)
		{
			const unsigned char * s2 = s1;
			const unsigned char * p2 = p1;
			
			for(;;)
			{
				c1 = *s2++;
				c1 = tolower( c1 );
				c2 = *p2++;
				c2 = tolower( c2 );
				if( ( c1 != c2 ) || !c1 ) break;
			}
			if (!c2)
				return((char *) s1 - 1);
		}
	}
	return(NULL);
}
#endif

#if( !TARGET_OS_DARWIN )
//===========================================================================================================================
//	strnstr
//
//	From FreeBSD: Find the first occurrence of find in s, where the search is limited to the first slen characters of s.
//===========================================================================================================================

char * strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif // !TARGET_OS_DARWIN

//===========================================================================================================================
//	strncasestr
//
//	Case-insensitive variant of strnstr. Based on FreeBSD version of strnstr.
//===========================================================================================================================

char * strncasestr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	c = *find++;
	c = (char) tolower((unsigned char) c);
	if (c != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1)
					return (NULL);
				sc = *s++;
				sc = (char) tolower((unsigned char) sc);
				if (sc == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strnicmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

#if( !TARGET_OS_BSD )
//===========================================================================================================================
//	strlcat from Darwin
//
//	Appends src to string dst of size siz (unlike strncat, siz is the
//	full size of dst, not space left). At most siz-1 characters
//	will be copied. Always NUL terminates (unless siz <= strlen(dst)).
//	Returns strlen(src) + MIN(siz, strlen(initial dst)).
//	If retval >= siz, truncation occurred.
//===========================================================================================================================

size_t	strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif

#if( !TARGET_OS_BSD )
//===========================================================================================================================
//	strlcpy from Darwin
//
//	Copy src to string dst of size siz. At most siz-1 characters
//	will be copied. Always NUL terminates (unless siz == 0).
//	Returns strlen(src); if retval >= siz, truncation occurred.
//===========================================================================================================================

size_t	strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif

//===========================================================================================================================
//	strnspn
//===========================================================================================================================

size_t	strnspn( const void *inStr, size_t inLen, const char *inCharSet )
{
#if( TARGET_RT_64_BIT )
	#define BITS_PER_CHUNK		64
	
	static const uint64_t		kBit0 = 1;
	uint64_t					t[ 4 ];
#else
	#define BITS_PER_CHUNK		32
	
	static const uint32_t		kBit0 = 1;
	uint32_t					t[ 8 ];
#endif
	size_t						i;
	unsigned int				b;
	const unsigned char *		ptr;
	const unsigned char *		end;
	unsigned char				c;
	
#if( TARGET_RT_64_BIT )
	t[ 0 ] = t[ 1 ] = t[ 2 ] = t[ 3 ] = 0;
#else
	t[ 0 ] = t[ 1 ] = t[ 2 ] = t[ 3 ] = t[ 4 ] = t[ 5 ] = t[ 6 ] = t[ 7 ] = 0;
#endif
	
	// Populate the table with a bit for each unique character.
	
	for( ptr = (const unsigned char *) inCharSet; ( c = *ptr ) != '\0'; ++ptr )
	{
		i = c / BITS_PER_CHUNK;
		b = c % BITS_PER_CHUNK;
		t[ i ] |= ( kBit0 << b );
	}
	
	// Scan the string to find the first character that's not in the set. Note: '\0' is never set.
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inStr );
	ptr = (const unsigned char *) inStr;
	end = ptr + inLen;
	for( ; ptr < end; ++ptr )
	{
		c = *ptr;
		i = c / BITS_PER_CHUNK;
		b = c % BITS_PER_CHUNK;
		if( !( t[ i ] & ( kBit0 << b ) ) )
		{
			break;
		}
	}
	return( (size_t)( ptr - ( (const unsigned char *) inStr ) ) );
}

//===========================================================================================================================
//	strnspnx
//===========================================================================================================================

Boolean	strnspnx( const char *inStr, size_t inLen, const char *inCharSet )
{
	if( inLen == kSizeCString ) inLen = strlen( inStr );
	return( (Boolean)( strnspn( inStr, inLen, inCharSet ) == inLen ) );
}


#if( TARGET_OS_DARWIN_KERNEL )
//===========================================================================================================================
//	strrchr
//===========================================================================================================================

char *	strrchr( const char *inStr, int inC )
{
	char *		last;
	char		c;
	
	last = NULL;
	c = (char) inC;
	for( ;; )
	{
		if( *inStr == c )    last = (char *) inStr;
		if( *inStr == '\0' ) break;
		++inStr;
	}
	return( last );
}
#endif


//===========================================================================================================================
//	strsep_compat
//===========================================================================================================================

char *	strsep_compat( char **ioStr, const char *inDelimiters )
{
	char * const		ptr = *ioStr;
	char *				end;
	
	if( !ptr ) return( NULL );
	end = strpbrk( ptr, inDelimiters );
	if( end ) *end++ = '\0';
	*ioStr = end;
	return( ptr );
}

#if( TARGET_HAS_STD_C_LIB )
//===========================================================================================================================
//	strtoi
//
//	Warning: This must only return errno-compatible error codes (i.e. those suitable as process exit codes).
//===========================================================================================================================

int	strtoi( const char *inString, char **outEnd, int *outValue )
{
	long		x;
	int			err;
	
	errno = 0;
	x = strtol( inString, outEnd, 0 );
	err = errno;
	if( err ) return( err );
	if( ( x < INT_MIN ) || ( x > INT_MAX ) ) return( EINVAL );
	*outValue = (int) x;
	return( 0 );
}
#endif

#if( TARGET_OS_DARWIN_KERNEL || !TARGET_OS_LINUX )
//===========================================================================================================================
//	memrchr
//
//	Similiar to the ANSI strrchr routine, but works with data that may not be null-terminated.
//===========================================================================================================================

void *	memrchr( const void *inSrc, int inChar, size_t inSize )
{
	const unsigned char *		p;
	const unsigned char *		q;
	
	p = (const unsigned char *) inSrc;
	q = p + inSize;
	while( p < q )
	{
		--q;
		if( *q == inChar )
		{
			return( (void *) q );
		}
	}
	return( NULL );
}
#endif

//===========================================================================================================================
//	memrlen
//
//	Returns the number of bytes until the last 0 in the string.
//===========================================================================================================================

size_t	memrlen( const void *inSrc, size_t inLen )
{
	while( ( inLen > 0 ) && ( ( (const uint8_t *) inSrc )[ inLen - 1 ] == 0 ) ) --inLen;
	return( inLen );
}

//===========================================================================================================================
//	tolowerstr
//===========================================================================================================================

char *	tolowerstr( const void *inSrc, void *inDst, size_t inMaxLen )
{
	const unsigned char *		src;
	unsigned char *				dst;
	unsigned char *				lim;
	unsigned char				c;
	
	if( inMaxLen > 0 )
	{
		dst = (unsigned char *) inDst;
		lim = dst + ( inMaxLen - 1 );
		for( src = (const unsigned char *) inSrc; ( c = *src ) != '\0'; ++src )
		{
			if( dst >= lim ) break;
			*dst++ = (unsigned char) tolower( c );
		}
		*dst = '\0';
	}
	return( (char *) inDst );
}

//===========================================================================================================================
//	toupperstr
//===========================================================================================================================

char *	toupperstr( const void *inSrc, void *inDst, size_t inMaxLen )
{
	const unsigned char *		src;
	unsigned char *				dst;
	unsigned char *				lim;
	unsigned char				c;
	
	if( inMaxLen > 0 )
	{
		dst = (unsigned char *) inDst;
		lim = dst + ( inMaxLen - 1 );
		for( src = (const unsigned char *) inSrc; ( c = *src ) != '\0'; ++src )
		{
			if( dst >= lim ) break;
			*dst++ = (unsigned char) toupper( c );
		}
		*dst = '\0';
	}
	return( (char *) inDst );
}

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	BoyerMooreSearch
//===========================================================================================================================

const uint8_t *	BoyerMooreSearch( const void *inBuffer, size_t inBufferSize, const void *inPattern, size_t inPatternSize )
{
	const uint8_t *		found;
	const uint8_t *		buf;
	const uint8_t *		pat;
	size_t				skip[ 256 ];
	intptr_t			n;
	intptr_t			k;
	intptr_t			i;
	intptr_t			j;
	
	found = NULL;
	if( ( inPatternSize <= 0 ) || ( inPatternSize > inBufferSize ) ) goto exit;
	buf = (const uint8_t *) inBuffer;
	pat = (const uint8_t *) inPattern;
	
	// Build the skip table.
	
	n = inPatternSize - 1;
	for( k = 0; k < 256; ++k )	skip[ k ]		 = inPatternSize;
	for( k = 0; k < n; ++k )	skip[ pat[ k ] ] = n - k;
	
	// Look for pattern in buffer.
	
	for( k = n; k < ( (intptr_t) inBufferSize ); k += skip[ buf[ k ] ] )
	{ 
		for( i = k, j = n; ( j >= 0 ) && ( buf[ i ] == pat[ j ] ); --i, --j ) {}
		if( j == -1 )
		{
			found = &buf[ i + 1 ];
			break;
		}
	}
	
exit:
	return( found );
}

//===========================================================================================================================
//	CleanseDiskString
//===========================================================================================================================

char *	CleanseDiskString( const void *inSrc, size_t inSrcSize, char *inDst, size_t inDstSize )
{
	const unsigned char *		src;
	const unsigned char *		end;
	unsigned char *				dst;
	unsigned char *				lim;
	unsigned char				c;
	
	require( inDstSize > 0, exit );
	
	src = (const unsigned char *) inSrc;
	end = src + inSrcSize;
	dst = (unsigned char *) inDst;
	lim = dst + ( inDstSize - 1 );
	while( ( src < end ) && ( ( ( c = *src )      == ' ' ) || !isprint( c ) ) ) ++src; // Skip leading spaces/non-printables.
	while( ( src < end ) && ( ( ( c = end[ -1 ] ) == ' ' ) || !isprint( c ) ) ) --end; // Skip trailing spaces/non-printables.
	
	// Copy string and replace non-printabe characters with spaces.
	
	while( ( src < end ) && ( dst < lim ) )
	{
		c = *src++;
		if( !isprint( c ) ) c = '?';
		*dst++ = c;
	}
	*dst = '\0';
	
exit:
	return( inDst );
}

//===========================================================================================================================
//	CleanseHFSVolumeName
//===========================================================================================================================

Boolean	CleanseHFSVolumeName( const void *inSrc, size_t inSrcLen, void *inDst, size_t inDstMaxLen )
{
	Boolean						changed;
	const unsigned char *		src;
	const unsigned char *		end;
	unsigned char				buf[ 32 ];
	unsigned char *				dst;
	unsigned char *				lim;
	unsigned char				c;
	size_t						len;
	
	if( inSrcLen == kSizeCString ) inSrcLen = strlen( (const char *) inSrc );
	if( inSrc != inDst ) check_ptr_overlap( inSrc, inSrcLen, inDst, inDstMaxLen );
	
	src = (const unsigned char *) inSrc;
	end = src + inSrcLen;
	dst = buf;
	lim = dst + ( sizeof( buf ) - 1 );
	
	while( ( src < end ) && ( dst < lim ) )
	{
		c = *src++;
		if( isspace( c ) )							c = ' ';	// Convert whitespace to a simple space.
		else if( ( c == ':' ) || ( c == '/' ) )		c = '_';	// Replace colon and slash to avoid confusing HFS/POSIX.
		else if( ( c == '.' ) && ( dst == buf ) )	continue;	// Strip leading periods.
		
		*dst++ = c;
	}
	
	len = TruncatedUTF8Length( buf, (size_t)( dst - buf ), 27 );		// Restrict to no more than 27 bytes.
	if( len == 0 )	strlcpy( (char *) buf, "Untitled", sizeof( buf ) );	// If cleansed name is empty, use a default name.
	else			buf[ len ] = '\0';
	
	changed = (Boolean)( strncmpx( (const char *) inSrc, inSrcLen, (char *) buf ) != 0 );
	if( inDst ) strlcpy( (char *) inDst, (const char *) buf, inDstMaxLen );
	
	return( changed );
}

//===========================================================================================================================
//	ConvertUTF8StringToRFC1034LabelString
//===========================================================================================================================

#define	kRFC1034MaxLabelSize		63
#define	kRFC1034MaxDomainNameSize	255
#define RFC1034IsDigit( X )			( ( ( X ) >= '0' ) && ( ( X ) <= '9' ) )
#define RFC1034IsUpperCase( X )		( ( ( X ) >= 'A' ) && ( ( X ) <= 'Z' ) )
#define RFC1034IsLowerCase( X )		( ( ( X ) >= 'a' ) && ( ( X ) <= 'z' ) )
#define RFC1034IsLetter( X )		( RFC1034IsUpperCase( X ) || RFC1034IsLowerCase( X ) )
#define RFC1034ValidChar( X, notfirst, notlast )		\
	( RFC1034IsLetter( X ) || RFC1034IsDigit( X ) || ( ( notfirst ) && ( notlast ) && ( ( X ) == '-' ) ) )

char *	ConvertUTF8StringToRFC1034LabelString( const char *inSrcString, char *inDstBuffer )
{
	const uint8_t *		src;
	const uint8_t *		end;
	      uint8_t *		dst;
	      uint8_t *		ptr;
	const uint8_t * 	lim;
		
	src = (const uint8_t *) inSrcString;
	end = src + strlen( inSrcString );
	dst = (uint8_t *) inDstBuffer;
	ptr = dst;
	lim = ptr + kRFC1034MaxLabelSize;
	while( ( src < end ) && ( ptr < lim ) )
	{
		// Delete straight single quote.
		
		if( src[ 0 ] == '\'' )
		{
			src++;
			continue;
		}
		
		// Delete Unicode curly apostrophe (UTF-8 0xE28099).
		
		if( ( ( src + 2 ) < end ) && ( src[ 0 ] == 0xE2 ) && ( src[ 1 ] == 0x80 ) && ( src[ 2 ] == 0x99 ) )
		{
			src += 3;
			continue;
		}
		
		// Copy valid characters and replace invalid characters with "-".
		
		if( RFC1034ValidChar( *src, ( ptr > dst ), ( src < ( end - 1 ) ) ) ) *ptr++ = *src;
		else if( ( ptr > dst ) && ( ptr[ -1 ] != '-' ) )					 *ptr++ = '-';
		++src;
	}
	
	// Truncate trailing '-' and return the null-terminated string.
	
	while( ( ptr > dst ) && ( ptr[ -1 ] == '-' ) ) --ptr;
	
	check( (int)( ptr - dst ) <= kRFC1034MaxLabelSize );
	*ptr = '\0';
	return( inDstBuffer );
}

//===========================================================================================================================
//	GetParentPath
//===========================================================================================================================

OSStatus	GetParentPath( const char *inPathPtr, size_t inPathLen, char *inBuffer, size_t inMaxLen, size_t *outLen )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	
	if( inPathLen == kSizeCString ) inPathLen = strlen( inPathPtr );
	ptr = memrchr( inPathPtr, '/', inPathLen );
	#if( TARGET_OS_WINDOWS )
	if( !ptr ) ptr = memrchr( inPathPtr, '\\', inPathLen );
	#endif
	len = ptr ? ( (size_t)( ptr - inPathPtr ) ) : 0;
	if( inBuffer )
	{
		require_action_quiet( len < inMaxLen, exit, err = kSizeErr );
		if( len > 0 ) memmove( inBuffer, inPathPtr, len );
		inBuffer[ len ] = '\0';
	}
	if( outLen ) *outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	GetLastFilePathSegment
//===========================================================================================================================

const char *	GetLastFilePathSegment( const char *inPtr, size_t inLen, size_t *outLen )
{
	const char *		ptr;
	const char *		end;
	
	if( inLen == kSizeCString )
	{
		for( ptr = inPtr; *ptr != '\0'; ++ptr )
		{
			#if( TARGET_OS_WINDOWS )
			if( ( *ptr == '/' ) || ( *ptr == '\\' ) )
			#else
			if( *ptr == '/' )
			#endif
			{
				inPtr = ptr + 1;
			}
		}
		if( outLen ) *outLen = strlen( inPtr );
	}
	else
	{
		end = inPtr + inLen;
		for( ptr = inPtr; ptr < end; ++ptr )
		{
			#if( TARGET_OS_WINDOWS )
			if( ( *ptr == '/' ) || ( *ptr == '\\' ) )
			#else
			if( *ptr == '/' )
			#endif
			{
				inPtr = ptr + 1;
			}
		}
		if( outLen ) *outLen = (size_t)( end - inPtr );
	}
	return( inPtr );
}

//===========================================================================================================================
//	INIFindSection
//===========================================================================================================================

Boolean
	INIFindSection( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outSrc )
{
	Boolean				found = false;
	uint32_t			flags;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	
	while( INIGetNext( inSrc, inEnd, &flags, &namePtr, &nameLen, &valuePtr, &valueLen, &inSrc ) )
	{
		if( ( flags & kINIFlag_Section ) && ( strnicmpx( namePtr, nameLen, inName ) == 0 ) )
		{
			if( outValuePtr ) *outValuePtr = valuePtr;
			if( outValueLen ) *outValueLen = valueLen;
			found = true;
			break;
		}
	}
	if( outSrc ) *outSrc = inSrc;
	return( found );
}

//===========================================================================================================================
//	INIGetNext
//===========================================================================================================================

Boolean
	INIGetNext( 
		const char *	inSrc, 
		const char *	inEnd, 
		uint32_t *		outFlags, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outSrc )
{
	Boolean				more = false;
	uint32_t			flags = 0;
	const char *		ptr;
	const char *		end;
	const char *		namePtr;
	const char *		nameEnd;
	const char *		valuePtr;
	const char *		valueEnd;
	char				c;
	
	for( ;; )
	{
		// Parse a line.
		
		while( ( inSrc < inEnd ) && isspace_safe( *inSrc ) ) ++inSrc;
		if( inSrc == inEnd ) goto exit;
		ptr = inSrc;
		while( ( inSrc < inEnd ) && ( ( c = *inSrc ) != '\r' ) && ( c != '\n' ) ) ++inSrc;
		end = inSrc;
		if( ( inSrc < inEnd ) && ( *inSrc == '\r' ) ) ++inSrc;
		if( ( inSrc < inEnd ) && ( *inSrc == '\n' ) ) ++inSrc;
		if( ( *ptr == '#' ) || ( *ptr == ';' ) ) continue; // Skip comment lines.
		
		// Parse section header in the following formats:
		//
		// [name]
		// [name "value"]
		// [ name "value" ]
		
		if( *ptr == '[' )
		{
			++ptr;
			while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
			namePtr = ptr;
			while( ( ptr < end ) && ( ( c = *ptr ) != ']' ) && !isspace_safe( c ) ) ++ptr;
			if( ptr == end ) continue; // Premature end of line.
			nameEnd = ptr;
			while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
			if( ptr == end ) continue; // Premature end of line.
			if( *ptr == ']' )
			{
				valuePtr = NULL;
				valueEnd = NULL;
				flags = kINIFlag_Section;
				break;
			}
			if( *ptr != '"' ) continue; // Malformed: no start quote for section value.
			
			valuePtr = ++ptr; 
			while( ( ptr < end ) && ( *ptr != '"' ) ) ++ptr;
			if( ptr == end ) continue;  // Malformed: no end quote for section value.
			valueEnd = ptr++;
			while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
			if( ( ptr == end ) || ( *ptr != ']' ) ) continue; // Malformed: no closing bracket for section header.
			flags = kINIFlag_Section;
			break;
		}
		
		// Parse property in the following formats:
		//
		// name
		// name=value
		// name="value"
		// name = value
		// name = "value"
		//     name = "value"
		
		namePtr = ptr;
		while( ( ptr < end ) && ( ( c = *ptr ) != '=' ) && !isspace_safe( c ) ) ++ptr;
		nameEnd = ptr;
		while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
		if( ptr == end )
		{
			valuePtr = NULL;
			valueEnd = NULL;
			flags =  kINIFlag_Property;
			break;
		}
		if( *ptr != '=' ) continue; // Malformed: non-equal sign after property name.
		++ptr;
		while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
		if( ( ptr < end ) && ( *ptr == '"' ) )
		{
			valuePtr = ++ptr;
			while( ( ptr < end ) && ( *ptr != '"' ) ) ++ptr;
			valueEnd = ptr;
		}
		else
		{
			valuePtr = ptr;
			valueEnd = end;
			while( ( valuePtr < valueEnd ) && isspace_safe( valueEnd[ -1 ] ) ) --valueEnd;
		}
		flags = kINIFlag_Property;
		break;
	}
	
	if( outFlags )		*outFlags		= flags;
	if( outNamePtr )	*outNamePtr		= namePtr;
	if( outNameLen )	*outNameLen		= (size_t)( nameEnd - namePtr );
	if( outValuePtr )	*outValuePtr	= valuePtr;
	if( outValueLen )	*outValueLen	= (size_t)( valueEnd - valuePtr );
	more = true;
	
exit:
	if( outSrc ) *outSrc = inSrc;
	return( more );
}

//===========================================================================================================================
//	IsASCII7PrintableString
//===========================================================================================================================

Boolean	IsASCII7PrintableString( const char *inStr, const size_t inSize )
{
	const unsigned char * const		str = (const unsigned char *) inStr;
	unsigned char					c;
	size_t							i;
	
	for( i = 0; i < inSize; ++i )
	{
		c = str[ i ];
		if( ( c < 0x20 ) || ( c > 0x7E ) )	// <space> to ~
		{
			return( false );
		}
	}
	return( true );
}

//===========================================================================================================================
//	MapStringToValue
//===========================================================================================================================

int	MapStringToValue( const char *inString, int inDefaultValue, ... )
{
	va_list				args;
	const char *		str;
	int					val;
	int					x;
	
	check( inString );
	
	val = inDefaultValue;
	va_start( args, inDefaultValue );
	for( ;; )
	{
		str = va_arg( args, const char * );
		if( !str ) break;
		
		x = va_arg( args, int );
		if( strcmp( inString, str ) == 0 )
		{
			val = x;
			break;
		}
	}
	va_end( args );
	return( val );
}

//===========================================================================================================================
//	MapValueToString
//===========================================================================================================================

const char *	MapValueToString( int inValue, const char *inDefaultFormat, char *inBuffer, size_t inBufferSize, ... )
{
	va_list				args;
	const char *		s;
	int					i;
	int					n;
	
	DEBUG_USE_ONLY( n );
	
	va_start( args, inBufferSize );
	for( ;; )
	{
		s = va_arg( args, const char * );
		if( !s ) break;
		
		i = va_arg( args, int );
		if( inValue == i ) break;
	}
	va_end( args );
	
	if( !s )
	{
		if( inDefaultFormat && inBuffer )
		{
			inBuffer[ 0 ] = '\0';
			n = snprintf( inBuffer, inBufferSize, inDefaultFormat, inValue );
			check( n >= 0 );
			s = inBuffer;
		}
		else
		{
			s = inDefaultFormat;
		}
	}
	return( s );
}

//===========================================================================================================================
//	MIMETypeToExtension
//===========================================================================================================================

typedef struct
{
	const char *		mimeType;
	const char *		extension;
	
}	MIMETypeExtension;

static const MIMETypeExtension		kMIMEMap[] =
{
	// Keep this list sorted by MIME type since it's used with bsearch.
	
	{ "audio/mpeg",			"mp3"	}, 
	{ "audio/x-aiff",		"aiff"	}, 
	{ "audio/x-m4a",		"m4a"	}, 
	{ "audio/x-m4p",		"m4p"	}, 
	{ "audio/x-wav",		"wav"	}, 
	{ "image/bmp",			"bmp"	}, 
	{ "image/gif",			"gif"	}, 
	{ "image/jp2",			"jp2"	}, 
	{ "image/jpeg",			"jpg"	}, 
	{ "image/none",			""		}, 
	{ "image/pict",			"pict"	}, 
	{ "image/png",			"png"	}, 
	{ "image/tiff",			"tiff"	}, 
	{ "video/mp4",			"mp4"	}, 
	{ "video/quicktime",	"mov"	}, 
	{ "video/x-m4v",		"m4v"	} 
};

static int		_MIMETypeMatch( const void *inA, const void *inB );

const char *	MIMETypeToExtension( const char *inMIMEType )
{
	MIMETypeExtension				key;
	const MIMETypeExtension *		p;
	
	key.mimeType = inMIMEType;
	p = (const MIMETypeExtension *) bsearch( &key, kMIMEMap, countof( kMIMEMap ), sizeof( kMIMEMap[ 0 ] ), _MIMETypeMatch );
	return( p ? p->extension : "" );
}

static int	_MIMETypeMatch( const void *inA, const void *inB )
{
	const MIMETypeExtension * const		a = (const MIMETypeExtension *) inA;
	const MIMETypeExtension * const		b = (const MIMETypeExtension *) inB;
	
	return( stricmp( a->mimeType, b->mimeType ) );
}

//===========================================================================================================================
//	NameValueListFree
//===========================================================================================================================

void	NameValueListFree( NameValueListItem *inList )
{
	NameValueListItem *		item;
	
	while( ( item = inList ) != NULL )
	{
		inList = item->next;
		if( item->name )  free( item->name );
		if( item->value ) free( item->value );
		free( item );
	}
}

#if 0
#pragma mark -
#pragma mark == NMEA ==
#endif

static OSStatus	_NMEAParseGPGGA( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPGLL( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPGSA( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPGSV( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPHDT( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPRMC( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPVTG( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseGPZDA( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParseOHPR( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParsePAACD( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParsePAGCD( NMEAData *outData, const char *inSrc, const char *inEnd );
static OSStatus	_NMEAParsePASCD( NMEAData *outData, const char *inSrc, const char *inEnd );

//===========================================================================================================================
//	NMEAGenerate
//===========================================================================================================================

OSStatus	NMEAGenerate( const NMEAData *inData, char *inBuf, size_t inMaxLen )
{
	OSStatus			err;
	char *				dst = inBuf;
	char *				lim = inBuf + inMaxLen;
	const char *		checksumStart;
	int					x, hh, mm, yy, dd;
	double				whole, frac, ss;
	uint8_t				checksum;
	
	if( stricmp( inData->type, kNMEAType_GPRMC ) == 0 )
	{
		err = SNPrintF_Add( &dst, lim, "$" );
		require_noerr_quiet( err, exit );
		checksumStart = dst;
		
		// TalkerID and SentenceID.
		
		err = SNPrintF_Add( &dst, lim, "GPRMC" );
		require_noerr_quiet( err, exit );
		
		// UTC time in hhmmss[.frac] format, relative to the "date" field.
		
		frac = modf( inData->u.GPRMC.time, &whole );
		require_action( ( whole >= 0 ) && ( whole <= 82800 ), exit, err = kRangeErr );
		x  = (int) whole;
		hh = x / 3600;
		x  = x % 3600;
		mm = x / 60;
		ss = ( x % 60 ) + frac;
		err = SNPrintF_Add( &dst, lim, ",%02u%02u%02.3f", hh, mm, ss );
		require_noerr_quiet( err, exit );
		
		// Status: 'A' = active/valid, 'V' = void/invalid.
		
		require_action( ( inData->u.GPRMC.status == kNMEA_Active ) || ( inData->u.GPRMC.status == kNMEA_Void ), exit, 
			err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%c", inData->u.GPRMC.status );
		require_noerr_quiet( err, exit );
		
		// Latitude in ddmm.mmmmm format.
		
		frac = modf( inData->u.GPRMC.latitude, &whole );
		require_action( ( whole >= 0 ) && ( whole <= 90 ), exit, err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%02u%06.3f", (unsigned int) whole, frac * 60 );
		require_noerr_quiet( err, exit );
		
		// Latitude North/South indicator.
		
		require_action( ( inData->u.GPRMC.nsLatitude == kNMEA_North ) || ( inData->u.GPRMC.nsLatitude == kNMEA_South ), exit, 
			err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%c", inData->u.GPRMC.nsLatitude );
		require_noerr_quiet( err, exit );
		
		// Longitude in dddmm.mmmm format.
		
		frac = modf( inData->u.GPRMC.longitude, &whole );
		require_action( ( whole >= 0 ) && ( whole <= 180 ), exit, err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%03u%06.3f", (unsigned int) whole, frac * 60 );
		require_noerr_quiet( err, exit );
		
		// Longitude East/West indicator.
		
		require_action( ( inData->u.GPRMC.ewLongitude == kNMEA_East ) || ( inData->u.GPRMC.ewLongitude == kNMEA_West ), exit, 
			err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%c", inData->u.GPRMC.ewLongitude );
		require_noerr_quiet( err, exit );
		
		// Speed over ground in knots.
		
		err = SNPrintF_Add( &dst, lim, ",%.2f", inData->u.GPRMC.speed );
		require_noerr_quiet( err, exit );
		
		// Track angle in degrees (true).
		
		err = SNPrintF_Add( &dst, lim, ",%.2f", inData->u.GPRMC.track );
		require_noerr_quiet( err, exit );
		
		// Date in ddmmyy format.
		
		SecondsToYMD_HMS( ( (int64_t) inData->u.GPRMC.date ) * kSecondsPerDay, &yy, &mm, &dd, NULL, NULL, NULL );
		err = SNPrintF_Add( &dst, lim, ",%02u%02u%02u", dd, mm, yy % 100 );
		require_noerr_quiet( err, exit );
		
		// Magnetic variation in degrees.
		
		err = SNPrintF_Add( &dst, lim, ",%.2f", inData->u.GPRMC.variation );
		require_noerr_quiet( err, exit );
		
		// Magnetic variation East/West indicator.
		
		require_action( ( inData->u.GPRMC.ewVariation == kNMEA_East ) || 
						( inData->u.GPRMC.ewVariation == kNMEA_West ) ||
						( inData->u.GPRMC.ewVariation == '?' ), exit, 
			err = kRangeErr );
		err = SNPrintF_Add( &dst, lim, ",%?c", ( inData->u.GPRMC.ewVariation != '?' ), inData->u.GPRMC.ewVariation );
		require_noerr_quiet( err, exit );
		
		// Mode (NMEA version 2.3 and later).
		
		if( inData->u.GPRMC.mode != 0 )
		{
			err = SNPrintF_Add( &dst, lim, ",%?c", ( inData->u.GPRMC.mode != '?' ), inData->u.GPRMC.mode );
			require_noerr_quiet( err, exit );
		}
		
		// Checksum.
		
		checksum = 0;
		while( checksumStart < dst ) checksum ^= *checksumStart++;
		err = SNPrintF_Add( &dst, lim, "*%02X", checksum );
		require_noerr_quiet( err, exit );
	}
	else
	{
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	NMEAParse
//===========================================================================================================================

OSStatus	NMEAParse( NMEAData *outData, NMEAFlags inFlags, const char *inPtr, size_t inLen, const char **outNext )
{
	const char *		src = inPtr;
	const char *		end = src + ( ( inLen == kSizeCString ) ? strlen( src ) : inLen );
	const char *		end2;
	const char *		ptr;
	size_t				len;
	char				c;
	uint8_t				u8;
	OSStatus			err;
	
	memset( outData, 0, sizeof( *outData ) );
	outData->flags = inFlags & kNMEAFlag_AllowMalloc;
	
	if( inFlags & kNMEAFlag_SkipUntilStart )	while( ( src < end ) && ( *src != '$' ) ) ++src;
	else										while( ( src < end ) && isspace_safe( *src ) ) ++src;
	if( src >= end )
	{
		if( outNext ) *outNext = src;
		err = kNotFoundErr;
		goto exit;
	}
	if( ( src < end ) && ( *src == '$' ) )
	{
		++src;
		outData->flags |= kNMEAFlag_HasStart;
	}
	for( ptr = src; ( ptr < end ) && ( ( c = *ptr ) != '\r' ) && ( c != '\n' ); ++ptr ) {}
	if( outNext )
	{
		for( end2 = ptr; ( end2 < end ) && ( ( ( c = *end2 ) == '\r' ) || ( c == '\n' ) ); ++end2 ) {}
		*outNext = end2;
	}
	end = ptr;
	
	// Calculate and verify checksum (XOR of everything after $ and before *).
	
	for( u8 = 0, ptr = src; ( ptr < end ) && ( *ptr != '*' ); ++ptr ) u8 ^= *ptr;
	if( ( ptr < end ) && ( *ptr == '*' ) )
	{
		if( ( ( end - ptr ) >= 3 ) && IsHexPair( &ptr[ 1 ] ) && ( HexPairToByte( &ptr[ 1 ] ) == u8 ) )
		{
			outData->flags |= kNMEAFlag_ValidChecksum;
		}
		end = ptr;
	}
	
	// Parse the type (e.g. "GPRMC", "OHPR", etc.).
	
	for( ptr = src; ( src < end ) && ( *src != ',' ); ++src ) {}
	len = (size_t)( src - ptr );
	require_action( len < sizeof( outData->type ), exit, err = kTypeErr );
	memcpy( outData->type, ptr, len );
	outData->type[ len ] = '\0';
	if( ( src < end ) && ( *src == ',' ) ) ++src;
	
	// The rest of the sentence is type specific so parse based on those.
	
	if( 0 ) {}
	else if( stricmp( outData->type, kNMEAType_GPGGA )	== 0 )	err = _NMEAParseGPGGA( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPGLL )	== 0 )	err = _NMEAParseGPGLL( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPGSA )	== 0 )	err = _NMEAParseGPGSA( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPGSV )	== 0 )	err = _NMEAParseGPGSV( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPHDT )	== 0 )	err = _NMEAParseGPHDT( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPRMC )	== 0 )	err = _NMEAParseGPRMC( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPVTG )	== 0 )	err = _NMEAParseGPVTG( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPZDA )	== 0 )	err = _NMEAParseGPZDA( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_GPZDA )	== 0 )	err = _NMEAParseGPZDA( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_OHPR )	== 0 )	err = _NMEAParseOHPR( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_PAACD )	== 0 )	err = _NMEAParsePAACD( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_PAGCD )	== 0 )	err = _NMEAParsePAGCD( outData, src, end );
	else if( stricmp( outData->type, kNMEAType_PASCD )	== 0 )	err = _NMEAParsePASCD( outData, src, end );
	else														err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NMEAFree
//===========================================================================================================================

void	NMEAFree( NMEAData *inData )
{
	if( strcmp( inData->type, kNMEAType_PAACD ) == 0 )
	{
		inData->u.PAACD.sampleCountActual = 0;
		inData->u.PAACD.samples = NULL;
		ForgetMem( &inData->u.PAACD.sampleDynamicStorage );
	}
	else if( strcmp( inData->type, kNMEAType_PAGCD ) == 0 )
	{
		inData->u.PAGCD.sampleCountActual = 0;
		inData->u.PAGCD.samples = NULL;
		ForgetMem( &inData->u.PAGCD.sampleDynamicStorage );
	}
	else if( strcmp( inData->type, kNMEAType_PASCD ) == 0 )
	{
		inData->u.PASCD.sampleCountActual = 0;
		inData->u.PASCD.samples = NULL;
		ForgetMem( &inData->u.PASCD.sampleDynamicStorage );
	}
}

//===========================================================================================================================
//	_NMEAParseGPGGA
//===========================================================================================================================

static OSStatus	_NMEAParseGPGGA( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	int					n, field;
	unsigned int		hh, mm, dd;
	double				d;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// UTC time in hhmmss.ss format.
			
			case 1:
				d = 0;
				n = SNScanF( ptr, len, "%2u%2u%lf", &hh, &mm, &d );
				require_action_quiet( n >= 3, exit, err = kMalformedErr );
				outData->u.GPGGA.time = ( hh * 3600 ) + ( mm * 60 ) + d;
				break;
			
			// Latitude in ddmm.mmmmm format.
			
			case 2:
				n = SNScanF( ptr, len, "%2u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPGGA.latitude = dd + ( d / 60 );
				break;
			
			// Latitude North/South indicator.
			
			case 3:
				outData->u.GPGGA.nsLatitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Longitude in dddmm.mmmm format.
			
			case 4:
				n = SNScanF( ptr, len, "%3u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPGGA.longitude = dd + ( d / 60 );
				break;
			
			// Longitude East/West indicator.
			
			case 5:
				outData->u.GPGGA.ewLongitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Fix Quality.
			
			case 6:
				outData->u.GPGGA.fixQuality = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Number of satellites in view.
			
			case 7:
				SNScanF( ptr, len, "%u", &outData->u.GPGGA.satellites );
				break;
			
			// Horizontal Dilution of Precision (HDOP) 1.0 to 9.9.
			
			case 8:
				SNScanF( ptr, len, "%lf", &outData->u.GPGGA.hdop );
				break;
			
			// Antenna altitude above mean seal level.
			
			case 9:
				SNScanF( ptr, len, "%lf", &outData->u.GPGGA.altitude );
				break;
			
			// Unit of measure for altitude. 'M' = meters.
			
			case 10:
				outData->u.GPGGA.altitudeUnits = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Geoidal separation.
			
			case 11:
				SNScanF( ptr, len, "%lf", &outData->u.GPGGA.geoidalSeparation );
				break;
			
			// Unit of measure for geoidal separation. 'M' = meters.
			
			case 12:
				outData->u.GPGGA.geoidalUnits = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Seconds since last DGPS update.
			
			case 13:
				SNScanF( ptr, len, "%lf", &outData->u.GPGGA.dgpsAge );
				break;
			
			// Differential reference station ID.
			
			case 14:
				SNScanF( ptr, len, "%.s", sizeof( outData->u.GPGGA.stationID ), outData->u.GPGGA.stationID );
				break;
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParseGPGLL
//===========================================================================================================================

static OSStatus	_NMEAParseGPGLL( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	int					n, field;
	unsigned int		hh, mm, ss, dd;
	double				d;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Latitude in ddmm.mmmmm format.
			
			case 1:
				n = SNScanF( ptr, len, "%2u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPGLL.latitude = dd + ( d / 60 );
				break;
			
			// Latitude North/South indicator.
			
			case 2:
				outData->u.GPGLL.nsLatitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Longitude in dddmm.mmmm format.
			
			case 3:
				n = SNScanF( ptr, len, "%3u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPGLL.longitude = dd + ( d / 60 );
				break;
			
			// Longitude East/West indicator.
			
			case 4:
				outData->u.GPGLL.ewLongitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// UTC time in hhmmss[.frac] format.
			
			case 5:
				d = 0;
				n = SNScanF( ptr, len, "%2u%2u%2u%lf", &hh, &mm, &ss, &d );
				require_action_quiet( n >= 3, exit, err = kMalformedErr );
				outData->u.GPGLL.time = ( hh * 3600 ) + ( mm * 60 ) + ss + d;
				break;
			
			// Status: 'A' = active/valid, 'V' = void/invalid.
			
			case 6:
				outData->u.GPGLL.status = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Mode (NMEA version 2.3 and later).
			
			case 7:
				outData->u.GPGLL.mode = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParseGPGSA
//===========================================================================================================================

static OSStatus	_NMEAParseGPGSA( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	const char *		ptr;
	size_t				len;
	int					field;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Satellite acquisition mode.
			
			case 1:
				outData->u.GPGSA.acquisitionMode = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Position mode.
			
			case 2:
				outData->u.GPGSA.positionMode = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Satellites used in the position solution.
			
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
				SNScanF( ptr, len, "%u", &outData->u.GPGSA.satellite[ field - 3 ] );
				break;
			
			// Position Dilution of Precision (PDOP) = 1.0 to 9.9.
			
			case 15:
				SNScanF( ptr, len, "%lf", &outData->u.GPGSA.pdop );
				break;
			
			// Horizontal Dilution of Precision (HDOP) 1.0 to 9.9.
			
			case 16:
				SNScanF( ptr, len, "%lf", &outData->u.GPGSA.hdop );
				break;
			
			// Vertical Dilution of Precision (VDOP) = 1.0 to 9.9
			
			case 17:
				SNScanF( ptr, len, "%lf", &outData->u.GPGSA.vdop );
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	return( kNoErr );
}

//===========================================================================================================================
//	_NMEAParseGPGSV
//===========================================================================================================================

static OSStatus	_NMEAParseGPGSV( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	int					n, field;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Total number of sentences in this cycle.
			
			case 1:
				n = SNScanF( ptr, len, "%d", &outData->u.GPGSV.totalSentences );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			// Sentence number.
			
			case 2:
				n = SNScanF( ptr, len, "%d", &outData->u.GPGSV.sentenceNumber );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			// Number of satellites in view.
			
			case 3:
				n = SNScanF( ptr, len, "%d", &outData->u.GPGSV.satellitesInView );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			// Satellite number. PRN number.
			
			case  4:
			case  8:
			case 12:
			case 16:
				n = SNScanF( ptr, len, "%d", &outData->u.GPGSV.satellites[ ( field - 4 ) / 4 ].satelliteNumber );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				++outData->u.GPGSV.satellitesCount;
				break;
			
			// Elevation in degrees.
			
			case  5:
			case  9:
			case 13:
			case 17:
				n = SNScanF( ptr, len, "%lf", &outData->u.GPGSV.satellites[ ( field - 4 ) / 4 ].elevation );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			// Azimuth in degrees.
			
			case  6:
			case 10:
			case 14:
			case 18:
				n = SNScanF( ptr, len, "%lf", &outData->u.GPGSV.satellites[ ( field - 4 ) / 4 ].azimuth );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			// SNR.
			
			case  7:
			case 11:
			case 15:
			case 19:
				n = SNScanF( ptr, len, "%lf", &outData->u.GPGSV.satellites[ ( field - 4 ) / 4 ].snr );
				require_action_quiet( n == 1, exit, err = kMalformedErr );
				break;
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParseGPHDT
//===========================================================================================================================

static OSStatus	_NMEAParseGPHDT( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	const char *		ptr;
	size_t				len;
	int					field;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Heading angle in degrees.
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.GPHDT.heading );
				break;
			
			// Degrees true. 'T' means true.
			
			case 2:
				outData->u.GPHDT.degreesTrue = ( len > 0 ) ? *ptr : '?';
				break;
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	return( kNoErr );
}

//===========================================================================================================================
//	_NMEAParseGPRMC
//===========================================================================================================================

static OSStatus	_NMEAParseGPRMC( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	int					n, field;
	unsigned int		hh, mm, ss, dd, yy;
	int64_t				s64;
	double				d;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// UTC time in hhmmss[.frac] format, relative to the "date" field.
			
			case 1:
				d = 0;
				n = SNScanF( ptr, len, "%2u%2u%2u%lf", &hh, &mm, &ss, &d );
				require_action_quiet( n >= 3, exit, err = kMalformedErr );
				outData->u.GPRMC.time = ( hh * 3600 ) + ( mm * 60 ) + ss + d;
				outData->u.GPRMC.dateTime += outData->u.GPRMC.time;
				break;
			
			// Status: 'A' = active/valid, 'V' = void/invalid.
			
			case 2:
				outData->u.GPRMC.status = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Latitude in ddmm.mmmmm format.
			
			case 3:
				n = SNScanF( ptr, len, "%2u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPRMC.latitude = dd + ( d / 60 );
				break;
			
			// Latitude North/South indicator.
			
			case 4:
				outData->u.GPRMC.nsLatitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Longitude in dddmm.mmmm format.
			
			case 5:
				n = SNScanF( ptr, len, "%3u%lf", &dd, &d );
				require_action_quiet( n == 2, exit, err = kMalformedErr );
				outData->u.GPRMC.longitude = dd + ( d / 60 );
				break;
			
			// Longitude East/West indicator.
			
			case 6:
				outData->u.GPRMC.ewLongitude = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Speed over ground in knots.
			
			case 7:
				SNScanF( ptr, len, "%lf", &outData->u.GPRMC.speed );
				break;
			
			// Track angle in degrees (true).
			
			case 8:
				SNScanF( ptr, len, "%lf", &outData->u.GPRMC.track );
				break;
			
			// Date in ddmmyy format.
			
			case 9:
				n = SNScanF( ptr, len, "%2u%2u%2u", &dd, &mm, &yy, &ptr );
				require_action_quiet( n == 3, exit, err = kMalformedErr );
				
				s64 = YMD_HMStoSeconds( (int)( ( yy < 60 ) ? ( 2000 + yy ) : ( 1900 + yy ) ), (int) mm, (int) dd, 0, 0, 0, 0 );
				outData->u.GPRMC.date = (uint32_t)( s64 / kSecondsPerDay );
				outData->u.GPRMC.dateTime += ( s64 - ( ( (int64_t) kDaysTo2001_01_01 ) * kSecondsPerDay ) );
				break;
			
			// Magnetic variation in degrees.
			
			case 10:
				SNScanF( ptr, len, "%lf", &outData->u.GPRMC.variation );
				break;
			
			// Magnetic variation East/West indicator.
			
			case 11:
				outData->u.GPRMC.ewVariation = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Mode (NMEA version 2.3 and later).
			
			case 12:
				outData->u.GPRMC.mode = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParseGPVTG
//===========================================================================================================================

static OSStatus	_NMEAParseGPVTG( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	const char *		ptr;
	size_t				len;
	int					field;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Course over ground in degrees. True.
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.GPVTG.courseTrue );
				break;
			
			// Type of course over ground.
			
			case 2:
				outData->u.GPVTG.courseTrueType = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Course over ground in degrees. Magnetic.
			
			case 3:
				SNScanF( ptr, len, "%lf", &outData->u.GPVTG.courseMagnetic );
				break;
			
			// Type of course over ground in degrees. Magnetic.
			
			case 4:
				outData->u.GPVTG.courseMagneticType = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Speed in knots.
			
			case 5:
				SNScanF( ptr, len, "%lf", &outData->u.GPVTG.speedKnots );
				break;
			
			// Unit for speed in knots.
			
			case 6:
				outData->u.GPVTG.speedKnotsUnit = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Speed in kilometers/hour.
			
			case 7:
				SNScanF( ptr, len, "%lf", &outData->u.GPVTG.speedKPH );
				break;
			
			// Unit for speed in kilometers/hour.
			
			case 8:
				outData->u.GPVTG.speedKPHUnit = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Mode.
			
			case 9:
				outData->u.GPVTG.mode = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	return( kNoErr );
}

//===========================================================================================================================
//	_NMEAParseGPZDA
//===========================================================================================================================

static OSStatus	_NMEAParseGPZDA( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	size_t				len;
	int					n, field;
	double				d;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// UTC time in hhmmss.ss format.
			
			case 1:
				n = SNScanF( ptr, len, "%2u%2u%lf", &outData->u.GPZDA.hour, &outData->u.GPZDA.minute, &outData->u.GPZDA.second );
				require_action_quiet( n == 3, exit, err = kMalformedErr );
				break;
			
			// Day of the month.
			
			case 2:
				SNScanF( ptr, len, "%u", &outData->u.GPZDA.day );
				require_action_quiet( ( outData->u.GPZDA.day >= 0 ) && ( outData->u.GPZDA.day <= 31 ), 
					exit, err = kMalformedErr );
				break;
			
			// Month.
			
			case 3:
				SNScanF( ptr, len, "%u", &outData->u.GPZDA.month );
				require_action_quiet( ( outData->u.GPZDA.month >= 0 ) && ( outData->u.GPZDA.month <= 12 ), 
					exit, err = kMalformedErr );
				break;
			
			// Year.
			
			case 4:
				SNScanF( ptr, len, "%u", &outData->u.GPZDA.year );
				break;
			
			// Local zone hour offset from GMT.
			
			case 5:
				SNScanF( ptr, len, "%d", &outData->u.GPZDA.zoneHours );
				break;
			
			// Local zone minute offset from GMT.
			
			case 6:
				SNScanF( ptr, len, "%d", &outData->u.GPZDA.zoneMinutes );
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	
	d = (double) YMD_HMStoSeconds( outData->u.GPZDA.year, outData->u.GPZDA.month, outData->u.GPZDA.day, 
		outData->u.GPZDA.hour, outData->u.GPZDA.minute, 0, kDaysTo2001_01_01 );
	d += outData->u.GPZDA.second;
	d += ( outData->u.GPZDA.zoneHours * kSecondsPerHour );
	d += ( outData->u.GPZDA.zoneMinutes * kSecondsPerMinute );
	outData->u.GPZDA.dateTime = d;
	
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParseOHPR
//===========================================================================================================================

static OSStatus	_NMEAParseOHPR( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	const char *		ptr;
	size_t				len;
	int					field;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Heading in degrees (corrected for declination if possible).
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.OHPR.heading );
				break;
			
			// Pitch angle in degrees.
			
			case 2:
				SNScanF( ptr, len, "%lf", &outData->u.OHPR.pitch );
				break;
			
			// Roll angle in degrees.
			
			case 3:
				SNScanF( ptr, len, "%lf", &outData->u.OHPR.roll );
				break;
			
			// Temperature of the compass board in degrees C.
			
			case 4:
				SNScanF( ptr, len, "%lf", &outData->u.OHPR.temperature );
				break;
			
			// Depth in feet.
			
			case 5:
				SNScanF( ptr, len, "%lf", &outData->u.OHPR.depth );
				break;
			
			// Ignore unknown fields.
			
			default:
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	return( kNoErr );
}

//===========================================================================================================================
//	_NMEAParsePAACD
//===========================================================================================================================

static OSStatus	_NMEAParsePAACD( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	int					field, i2;
	size_t				i, len;
	PAACDSample *		samplePtr;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Reference time in seconds
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.PAACD.timestamp );
				break;
			
			// Reference gravity magnitude.
			
			case 2:
				SNScanF( ptr, len, "%lf", &outData->u.PAACD.gValue );
				break;
			
			// Sample count.
			
			case 3:
				SNScanF( ptr, len, "%u", &outData->u.PAACD.sampleCountExpected );
				break;
			
			// Sample fields.
			
			default:
				i = (size_t)( ( field - 4 ) / 4 );
				if( i >= countof( outData->u.PAACD.sampleFixedStorage ) )
				{
					if( !( outData->flags & kNMEAFlag_AllowMalloc ) ) continue;
					if( i >= ( (size_t) outData->u.PAACD.sampleCountActual ) )
					{
						len = ( i + 1 ) * sizeof( *samplePtr );
						samplePtr = (PAACDSample *) realloc( outData->u.PAACD.sampleDynamicStorage, len );
						require_action( samplePtr, exit, err = kNoMemoryErr );
						if( !outData->u.PAACD.sampleDynamicStorage )
						{
							memcpy( samplePtr, outData->u.PAACD.sampleFixedStorage, i * sizeof( *samplePtr ) );
						}
						samplePtr[ i ].timeOffset				= 0;
						samplePtr[ i ].xAxis					= 0;
						samplePtr[ i ].yAxis					= 0;
						samplePtr[ i ].zAxis					= 0;
						outData->u.PAACD.sampleDynamicStorage	= samplePtr;
						outData->u.PAACD.samples				= samplePtr;
					}
				}
				else
				{
					outData->u.PAACD.samples = outData->u.PAACD.sampleFixedStorage;
				}
				samplePtr = &outData->u.PAACD.samples[ i ];
				i2 = ( field - 4 ) % 4;
				if( i2 == 0 ) { SNScanF( ptr, len, "%lf", &samplePtr->timeOffset ); ++outData->u.PAACD.sampleCountActual; }
				if( i2 == 1 )	SNScanF( ptr, len, "%lf", &samplePtr->xAxis );
				if( i2 == 2 )	SNScanF( ptr, len, "%lf", &samplePtr->yAxis );
				else			SNScanF( ptr, len, "%lf", &samplePtr->zAxis );
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParsePAGCD
//===========================================================================================================================

static OSStatus	_NMEAParsePAGCD( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	int					field, i2;
	size_t				i, len;
	PAGCDSample *		samplePtr;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Reference time in seconds
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.PAGCD.timestamp );
				break;
			
			// Sample count.
			
			case 2:
				SNScanF( ptr, len, "%u", &outData->u.PAGCD.sampleCountExpected );
				break;
			
			// Sample fields.
			
			default:
				i = (size_t)( ( field - 3 ) / 4 );
				if( i >= countof( outData->u.PAGCD.sampleFixedStorage ) )
				{
					if( !( outData->flags & kNMEAFlag_AllowMalloc ) ) continue;
					if( i >= ( (size_t) outData->u.PAGCD.sampleCountActual ) )
					{
						len = ( i + 1 ) * sizeof( *samplePtr );
						samplePtr = (PAGCDSample *) realloc( outData->u.PAGCD.sampleDynamicStorage, len );
						require_action( samplePtr, exit, err = kNoMemoryErr );
						if( !outData->u.PAGCD.sampleDynamicStorage )
						{
							memcpy( samplePtr, outData->u.PAGCD.sampleFixedStorage, i * sizeof( *samplePtr ) );
						}
						samplePtr[ i ].timeOffset				= 0;
						samplePtr[ i ].xAxis					= 0;
						samplePtr[ i ].yAxis					= 0;
						samplePtr[ i ].zAxis					= 0;
						outData->u.PAGCD.sampleDynamicStorage	= samplePtr;
						outData->u.PAGCD.samples				= samplePtr;
					}
				}
				else
				{
					outData->u.PAGCD.samples = outData->u.PAGCD.sampleFixedStorage;
				}
				samplePtr = &outData->u.PAGCD.samples[ i ];
				i2 = ( field - 3 ) % 4;
				if( i2 == 0 ) { SNScanF( ptr, len, "%lf", &samplePtr->timeOffset ); ++outData->u.PAGCD.sampleCountActual; }
				if( i2 == 1 )	SNScanF( ptr, len, "%lf", &samplePtr->xAxis );
				if( i2 == 2 )	SNScanF( ptr, len, "%lf", &samplePtr->yAxis );
				else			SNScanF( ptr, len, "%lf", &samplePtr->zAxis );
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NMEAParsePASCD
//===========================================================================================================================

static OSStatus	_NMEAParsePASCD( NMEAData *outData, const char *inSrc, const char *inEnd )
{
	OSStatus			err;
	const char *		ptr;
	int					field;
	size_t				i, len;
	PASCDSample *		samplePtr;
	
	for( field = 1; TextSep( inSrc, inEnd, ",", &ptr, &len, &inSrc ); ++field )
	{
		switch( field )
		{
			// Reference time in seconds
			
			case 1:
				SNScanF( ptr, len, "%lf", &outData->u.PASCD.timestamp );
				break;
			
			// Type of sensor.
			
			case 2:
				outData->u.PASCD.sensorType = ( len > 0 ) ? *ptr : '?';
				break;
			
			// State of transmission.
			
			case 3:
				outData->u.PASCD.transmissionState = ( len > 0 ) ? *ptr : '?';
				break;
			
			// Slip detect.
			
			case 4:
				SNScanF( ptr, len, "%u", &outData->u.PASCD.slipDetect );
				break;
			
			// Sample count.
			
			case 5:
				SNScanF( ptr, len, "%u", &outData->u.PASCD.sampleCountExpected );
				break;
			
			// Sample fields.
			
			default:
				i = (size_t)( ( field - 6 ) / 2 );
				if( i >= countof( outData->u.PASCD.sampleFixedStorage ) )
				{
					if( !( outData->flags & kNMEAFlag_AllowMalloc ) ) continue;
					if( i >= ( (size_t) outData->u.PASCD.sampleCountActual ) )
					{
						len = ( i + 1 ) * sizeof( *samplePtr );
						samplePtr = (PASCDSample *) realloc( outData->u.PASCD.sampleDynamicStorage, len );
						require_action( samplePtr, exit, err = kNoMemoryErr );
						if( !outData->u.PASCD.sampleDynamicStorage )
						{
							memcpy( samplePtr, outData->u.PASCD.sampleFixedStorage, i * sizeof( *samplePtr ) );
						}
						samplePtr[ i ].timeOffset				= 0;
						samplePtr[ i ].speed					= 0;
						outData->u.PASCD.sampleDynamicStorage	= samplePtr;
						outData->u.PASCD.samples				= samplePtr;
					}
				}
				else
				{
					outData->u.PASCD.samples = outData->u.PASCD.sampleFixedStorage;
				}
				samplePtr = &outData->u.PASCD.samples[ i ];
				if( ( ( field - 6 ) % 2 ) == 0 )
				{
					SNScanF( ptr, len, "%lf", &samplePtr->timeOffset );
					++outData->u.PASCD.sampleCountActual;
				}
				else
				{
					SNScanF( ptr, len, "%lf", &samplePtr->speed );
				}
				break;
		}
	}
	outData->flags |= kNMEAFlag_Parsed;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	NumberToOrdinalSuffixCString
//===========================================================================================================================

const char *	NumberToOrdinalSuffixCString( int inNumber )
{
	static const char *		kOrdinalStrings[] = 
	{
		"th", 	// e.g. 5th
		"st", 	// e.g. 1st
		"nd", 	// e.g. 2nd
		"rd"	// e.g. 3rd
	};
	
	if( ( ( ( inNumber %= 100 ) > 9 ) && ( inNumber < 20 ) ) || ( ( inNumber %= 10 ) > 3 ) )
	{
		inNumber = 0;
	}
	return( kOrdinalStrings[ inNumber ] );
}

//===========================================================================================================================
//	ParseCommandLineIntoArgV
//===========================================================================================================================

OSStatus	ParseCommandLineIntoArgV( const char *inCmdLine, int *outArgC, char ***outArgV )
{
	OSStatus					err;
	size_t						len;
	const unsigned char *		src;
	const unsigned char *		end;
	unsigned char				c;
	unsigned char				c2;
	char *						buf;
	char *						dst;
	char *						arg;
	int							singleQuote;
	int							doubleQuote;
	char **						argv;
	int							usedArgs;
	int							freeArgs;
	
	argv = NULL;
	
	// Make a buffer to hold the entire line. We'll carve this up into arg strings.
	
	src = (const unsigned char *) inCmdLine;
	len = strlen( inCmdLine );
	end = src + len;
	
	buf = (char *) malloc( len + 1 );
	require_action( buf, exit, err = kNoMemoryErr );
	dst = buf;
	
	// Start the array with room for a reasonable number of args.
	
	usedArgs = 0;
	freeArgs = 16;
	argv = (char **) malloc( ( (size_t)( freeArgs + 1 ) ) * sizeof( *argv ) );
	require_action( argv, exit, err = kNoMemoryErr );
	
	// Parse each argument from the string.
	//
	// See <http://www.mpi-inf.mpg.de/~uwe/lehre/unixffb/quoting-guide.html> for details.
	
	for( ;; )
	{
		while( ( src < end ) && isspace( *src ) ) ++src; // Skip leading spaces.
		if( src >= end ) break;
		
		arg = dst;
		singleQuote = 0;
		doubleQuote = 0;
		while( src < end )
		{
			c = *src++;
			if( singleQuote )
			{
				// Single quotes protect everything (even backslashes, newlines, etc.) except single quotes.
				
				if( c == '\'' )
				{
					singleQuote = 0;
					continue;
				}
			}
			else if( doubleQuote )
			{
				// Double quotes protect everything except double quotes and backslashes. A backslash can be 
				// used to protect " or \ within double quotes. A backslash-newline pair disappears completely.
				// A backslash that does not precede ", \, or newline is taken literally.
				
				if( c == '"' )
				{
					doubleQuote = 0;
					continue;
				}
				else if( c == '\\' )
				{
					if( src < end )
					{
						c2 = *src;
						if( ( c2 == '"' ) || ( c2 == '\\' ) )
						{
							++src;
							c = c2;
						}
						else if( c2 == '\n' )
						{
							++src;
							continue;
						}
					}
				}
			}
			else if( c == '\\' )
			{
				// A backslash protects the next character, except if it is a newline. If a backslash precedes 
				// a newline, it prevents the newline from being interpreted as a command separator, but the 
				// backslash-newline pair disappears completely.
				
				if( src < end )
				{
					c = *src++;
					if( c == '\n' ) continue;
				}
			}
			else if( c == '\'' )
			{
				singleQuote = 1;
				continue;
			}
			else if( c == '"' )
			{
				doubleQuote = 1;
				continue;
			}
			else if( isspace( c ) )
			{
				break;
			}
			
			*dst++ = (char) c;
		}
		*dst++ = '\0';
		
		// Grow the argv array if there's not enough room.
		
		if( usedArgs >= freeArgs )
		{
			char **		tmpv;
			int			i;
			
			freeArgs *= 2;
			tmpv = (char **) malloc( ( (size_t)( freeArgs + 1 ) ) * sizeof( *tmpv ) );
			require_action( tmpv, exit, err = kNoMemoryErr );
			
			for( i = 0; i < usedArgs; ++i )
			{
				tmpv[ i ] = argv[ i ];
			}
			free( argv );
			argv = tmpv;
		}
		argv[ usedArgs++ ] = arg;
	}
	argv[ usedArgs ] = NULL; // argv arrays have a NULL entry at the end.
	
	*outArgC = usedArgs;
	*outArgV = argv;
	argv = NULL;
	if( usedArgs > 0 ) buf = NULL; // Only consume the string buffer if some string data is used.
	err = kNoErr;
	
exit:
	if( argv ) free( argv );
	if( buf )  free( buf );
	return( err );
}

//===========================================================================================================================
//	FreeCommandLineArgs
//===========================================================================================================================

void	FreeCommandLineArgV( int inArgC, char **inArgV )
{
	(void) inArgC; // Unused
	
	if( inArgV )
	{
		// Note: the first arg is the beginning of a single buffer for all the args.
		
		if( inArgV[ 0 ] ) free( inArgV[ 0 ] );
		free( inArgV );
	}
}

//===========================================================================================================================
//	ParseCommaSeparatedNameValuePair
//===========================================================================================================================

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
		const char **	outSrc )
{
	OSStatus		err;
	
	require_action_quiet( inSrc < inEnd, exit, err = kNotFoundErr );
	
	err = ParseEscapedString( inSrc, inEnd, '=', inNameBuf, inNameMaxLen, outNameCopiedLen, outNameTotalLen, &inSrc );
	require_noerr_quiet( err, exit );
	
	err = ParseEscapedString( inSrc, inEnd, ',', inValueBuf, inValueMaxLen, outValueCopiedLen, outValueTotalLen, &inSrc );
	require_noerr_quiet( err, exit );
	
exit:
	if( outSrc ) *outSrc = inSrc;
	return( err );
}

//===========================================================================================================================
//	ParseEscapedString
//===========================================================================================================================

OSStatus
	ParseEscapedString( 
		const char *	inSrc, 
		const char *	inEnd, 
		char			inDelimiter, 
		char *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outCopiedLen, 
		size_t *		outTotalLen, 
		const char **	outSrc )
{
	OSStatus		err;
	char			c;
	char *			dst;
	char *			lim;
	size_t			len;
	
	dst = inBuf;
	lim = dst + ( ( inMaxLen > 0 ) ? ( inMaxLen - 1 ) : 0 ); // Leave room for null terminator.
	len = 0;
	while( ( inSrc < inEnd ) && ( ( c = *inSrc++ ) != inDelimiter ) )
	{
		if( c == '\\' )
		{
			require_action_quiet( inSrc < inEnd, exit, err = kUnderrunErr );
			c = *inSrc++;
		}
		if( dst < lim )
		{
			if( inBuf ) *dst = c;
			++dst;
		}
		++len;
	}
	if( inBuf && ( inMaxLen > 0 ) ) *dst = '\0';
	err = kNoErr;
	
exit:
	if( outCopiedLen )	*outCopiedLen	= (size_t)( dst - inBuf );
	if( outTotalLen )	*outTotalLen	= len;
	if( outSrc )		*outSrc			= inSrc;
	return( err );
}

//===========================================================================================================================
//	ParseLine
//===========================================================================================================================

Boolean	ParseLine( const char *inSrc, const char *inEnd, const char **outLinePtr, size_t *outLineLen, const char **outNext )
{
	const char *		linePtr;
	char				c;
	
	if( inSrc == inEnd )
		return( false );
	
	linePtr = inSrc;
	for( c = 0; ( inSrc < inEnd ) && ( ( c = *inSrc ) != '\n' ) && ( c != '\r' ); ++inSrc ) {}
	*outLinePtr = linePtr;
	*outLineLen = (size_t)( inSrc - linePtr );
	if( inSrc < inEnd )
	{
		++inSrc;
		if( ( c == '\r' ) && ( inSrc < inEnd ) && ( *inSrc == '\n' ) )
			++inSrc;
	}
	*outNext = inSrc;
	return( true );
}

//===========================================================================================================================
//	ParseQuotedEscapedString
//===========================================================================================================================

Boolean
	ParseQuotedEscapedString( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inDelimiters, 
		char *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outCopiedLen, 
		size_t *		outTotalLen, 
		const char **	outSrc )
{
	const unsigned char *		src;
	const unsigned char *		end;
	unsigned char *				dst;
	unsigned char *				lim;
	unsigned char				c;
	unsigned char				c2;
	size_t						totalLen;
	Boolean						singleQuote;
	Boolean						doubleQuote;
	
	if( inEnd == NULL ) inEnd = inSrc + strlen( inSrc );
	src = (const unsigned char *) inSrc;
	end = (const unsigned char *) inEnd;
	dst = (unsigned char *) inBuf;
	lim = dst + inMaxLen;
	while( ( src < end ) && isspace_safe( *src ) ) ++src; // Skip leading spaces.
	if( src >= end ) return( false );
	
	// Parse each argument from the string.
	//
	// See <http://www.mpi-inf.mpg.de/~uwe/lehre/unixffb/quoting-guide.html> for details.
	
	totalLen = 0;
	singleQuote = false;
	doubleQuote = false;
	while( src < end )
	{
		c = *src++;
		if( singleQuote )
		{
			// Single quotes protect everything (even backslashes, newlines, etc.) except single quotes.
			
			if( c == '\'' )
			{
				singleQuote = false;
				continue;
			}
		}
		else if( doubleQuote )
		{
			// Double quotes protect everything except double quotes and backslashes. A backslash can be 
			// used to protect " or \ within double quotes. A backslash-newline pair disappears completely.
			// A backslash followed by x or X and 2 hex digits (e.g. "\x1f") is stored as that hex byte.
			// A backslash followed by 3 octal digits (e.g. "\377") is stored as that octal byte.
			// A backslash that does not precede ", \, x, X, or a newline is taken literally.
			
			if( c == '"' )
			{
				doubleQuote = false;
				continue;
			}
			else if( c == '\\' )
			{
				if( src < end )
				{
					c2 = *src;
					if( ( c2 == '"' ) || ( c2 == '\\' ) )
					{
						++src;
						c = c2;
					}
					else if( c2 == '\n' )
					{
						++src;
						continue;
					}
					else if( ( c2 == 'x' ) || ( c2 == 'X' ) )
					{
						++src;
						c = c2;
						if( ( ( end - src ) >= 2 ) && IsHexPair( src ) )
						{
							c = HexPairToByte( src );
							src += 2;
						}
					}
					else if( isoctal_safe( c2 ) )
					{
						if( ( ( end - src ) >= 3 ) && IsOctalTriple( src ) )
						{
							c = OctalTripleToByte( src );
							src += 3;
						}
					}
				}
			}
		}
		else if( strchr( inDelimiters, c ) )
		{
			break;
		}
		else if( c == '\\' )
		{
			// A backslash protects the next character, except a newline, x, X and 2 hex bytes or 3 octal bytes. 
			// A backslash followed by a newline disappears completely.
			// A backslash followed by x or X and 2 hex digits (e.g. "\x1f") is stored as that hex byte.
			// A backslash followed by 3 octal digits (e.g. "\377") is stored as that octal byte.
			
			if( src < end )
			{
				c = *src;
				if( c == '\n' )
				{
					++src;
					continue;
				}
				else if( ( c == 'x' ) || ( c == 'X' ) )
				{
					++src;
					if( ( ( end - src ) >= 2 ) && IsHexPair( src ) )
					{
						c = HexPairToByte( src );
						src += 2;
					}
				}
				else if( isoctal_safe( c ) )
				{
					if( ( ( end - src ) >= 3 ) && IsOctalTriple( src ) )
					{
						c = OctalTripleToByte( src );
						src += 3;
					}
					else
					{
						++src;
					}
				}
				else
				{
					++src;
				}
			}
		}
		else if( c == '\'' )
		{
			singleQuote = true;
			continue;
		}
		else if( c == '"' )
		{
			doubleQuote = true;
			continue;
		}
		
		if( dst < lim )
		{
			if( inBuf ) *dst = c;
			++dst;
		}
		++totalLen;
	}
	
	if( outCopiedLen )	*outCopiedLen	= (size_t)( dst - ( (unsigned char *) inBuf ) );
	if( outTotalLen )	*outTotalLen	= totalLen;
	if( outSrc )		*outSrc			= (const char *) src;
	return( true );
}

//===========================================================================================================================
//	Regex code from Rob Pike and Brian Kernighan (I just renamed it slightly and made some things const)
//
//	C	Matches any literal character C.
//	.	Matches any single character.
//	^	Matches the beginning of the input string.
//	$	Matches the end of the input string
//	*	Matches zero or more occurrences of the previous character.
//===========================================================================================================================

static int RegexMatchHere(const char *regexp, const char *text);
static int RegexMatchStar(int c, const char *regexp, const char *text);

STATIC_INLINE int RegexMatchChar(int a, int b)
{
	return tolower_safe(a) == tolower_safe(b);
}

// match: search for regexp anywhere in text
int RegexMatch(const char *regexp, const char *text)
{
	if (regexp[0] == '^')
		return RegexMatchHere(regexp+1, text);
	do {    // must look even if string is empty
		if (RegexMatchHere(regexp, text))
			return 1;
	} while (*text++ != '\0');
	return 0;
}

// matchhere: search for regexp at beginning of text
static int RegexMatchHere(const char *regexp, const char *text)
{
   if (regexp[0] == '\0')
	   return 1;
   if (regexp[1] == '*')
	   return RegexMatchStar(regexp[0], regexp+2, text);
   if (regexp[0] == '$' && regexp[1] == '\0')
	   return *text == '\0';
   if (*text!='\0' && (regexp[0]=='.' || RegexMatchChar(regexp[0], *text)))
	   return RegexMatchHere(regexp+1, text+1);
   return 0;
}

// matchstar: search for c*regexp at beginning of text
static int RegexMatchStar(int c, const char *regexp, const char *text)
{
   do {   // a * matches zero or more instances
	   if (RegexMatchHere(regexp, text))
		   return 1;
   } while (*text != '\0' && (RegexMatchChar(*text++, c) || c == '.'));
   return 0;
}

//===========================================================================================================================
//	ReplaceDifferentString -- Replaces a string if it's different with NULL being equivalent to an empty string.
//===========================================================================================================================

OSStatus	ReplaceDifferentString( char **ioString, const char *inNewString )
{
	char * const	oldString = *ioString;
	OSStatus		err;
	char *			tempStr;
	
	if( strcmp( inNewString ? inNewString : "", oldString ? oldString : "" ) != 0 )
	{
		if( inNewString && ( *inNewString != '\0' ) )
		{
			tempStr = strdup( inNewString );
			require_action( tempStr, exit, err = kNoMemoryErr );
		}
		else
		{
			tempStr = NULL;
		}
		if( oldString ) free( oldString );
		*ioString = tempStr;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ReplaceString
//===========================================================================================================================

OSStatus	ReplaceString( char **ioStr, size_t *ioLen, const void *inStr, size_t inLen )
{
	OSStatus		err;
	char *			str;
	
	if( inStr )
	{
		if( inLen == kSizeCString ) inLen = strlen( (const char *) inStr );
		str = (char *) malloc( inLen + 1 );
		require_action( str, exit, err = kNoMemoryErr );
		memcpy( str, inStr, inLen );
		str[ inLen ] = '\0';
	}
	else
	{
		str = NULL;
	}
	FreeNullSafe( *ioStr );
	*ioStr = str;
	if( ioLen ) *ioLen = inLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SplitText -- splits text into an array of segments using a set of delimiters.
//===========================================================================================================================

void
	SplitText( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inDelims, 
		size_t			inMaxPairs, 
		size_t *		outPairs, 
		const char **	inPtrs, 
		size_t *		inLens )
{
	size_t				nPairs;
	const char *		segmentPtr;
	char				srcChar;
	const char *		delims;
	char				delimChar;
	
	if( inEnd == NULL ) inEnd = inSrc + strlen( inSrc );
	
	nPairs = 0;
	if( ( inMaxPairs > 0 ) && ( inSrc < inEnd ) )
	{
		segmentPtr = inSrc;
		for( ; inSrc < inEnd; ++inSrc )
		{
			srcChar = *inSrc;
			for( delims = inDelims; ( delimChar = *delims ) != '\0'; ++delims )
			{
				if( srcChar == delimChar )
				{
					inPtrs[ nPairs ] = segmentPtr;
					inLens[ nPairs ] = (size_t)( inSrc - segmentPtr );
					if( ++nPairs >= inMaxPairs ) goto exit;
					segmentPtr = inSrc + 1;
					break;
				}
			}
		}
		inPtrs[ nPairs ] = segmentPtr;
		inLens[ nPairs ] = (size_t)( inSrc - segmentPtr );
		++nPairs;
	}
	
exit:
	if( outPairs ) *outPairs = nPairs;
}

//===========================================================================================================================
//	GetFileExtensionFromString
//===========================================================================================================================

const char *	GetFileExtensionFromString( const char *inString )
{
	const char *		p;
	
	// Search backward for a '.' (period). Check if the first character of the extension is a digit or a space. If it is, 
	// it probably isn't a real extension, but something like "My Document 2000.12.10" or "09. My Song Title".
	
	p = strrchr( inString, '.' );
	if( p )
	{
		++p;
		if( ( ( *p >= '0' ) && ( *p <= '9' ) ) || ( *p == ' ' ) || ( *p == '%' ) )
		{
			p = "";
		}
	}
	else
	{
		p = "";
	}
	return( p );
}

//===========================================================================================================================
//	StringArray_Append
//===========================================================================================================================

OSStatus	StringArray_Append( char ***ioArray, size_t *ioCount, const char *inStr )
{
	OSStatus		err;
	char *			newStr;
	size_t			oldCount;
	size_t			newCount;
	char **			oldArray;
	char **			newArray;
	
	newStr = strdup( inStr );
	require_action( newStr, exit, err = kNoMemoryErr );
	
	oldCount = *ioCount;
	newCount = oldCount + 1;
	newArray = (char **) malloc( newCount * sizeof( *newArray ) );
	require_action( newArray, exit, err = kNoMemoryErr );
	
	if( oldCount > 0 )
	{
		oldArray = *ioArray;
		memcpy( newArray, oldArray, oldCount * sizeof( *oldArray ) );
		free( oldArray );
	}
	newArray[ oldCount ] = newStr;
	newStr = NULL;
	
	*ioArray = newArray;
	*ioCount = newCount;
	err = kNoErr;
	
exit:
	if( newStr ) free( newStr );
	return( err );
}

//===========================================================================================================================
//	StringArray_Free
//===========================================================================================================================

void	StringArray_Free( char **inArray, size_t inCount )
{
	size_t		i;
	
	for( i = 0; i < inCount; ++i )
	{
		free( inArray[ i ] );
	}
	if( inCount > 0 ) free( inArray );
}

//===========================================================================================================================
//	StringListFree
//===========================================================================================================================

void	StringListFree( StringListItem *inList )
{
	StringListItem *		item;
	
	while( ( item = inList ) != NULL )
	{
		inList = item->next;
		if( item->str ) free( item->str );
		free( item );
	}
}

//===========================================================================================================================
//	TextCompareNatural -- See <http://www.naturalordersort.org> for details.
//
//	Based on Stuart Cheshire's Natural Order code.
//===========================================================================================================================

int
	TextCompareNatural( 
		const void *	inLeftPtr, 
		size_t			inLeftLen, 
		const char *	inRightPtr, 
		size_t			inRightLen, 
		Boolean			inCaseSensitive )
{
	const uint8_t *		leftPtr;
	const uint8_t *		rightPtr;
	const uint8_t *		leftEnd;
	const uint8_t *		rightEnd;
	uint8_t				leftChar;
	uint8_t				rightChar;
	int					skippedZeros;
	const uint8_t *		tempLeftPtr;
	const uint8_t *		tempRightPtr;
	
	leftPtr  = (const uint8_t *) inLeftPtr;
	rightPtr = (const uint8_t *) inRightPtr;
	leftEnd  = leftPtr + inLeftLen;
	rightEnd = rightPtr + inRightLen;
	
	skippedZeros = 0;
	while( ( leftPtr < leftEnd ) || ( rightPtr < rightEnd ) )
	{
		// Compare non-digit characters.
		
		while( ( leftPtr < leftEnd ) && ( rightPtr < rightEnd ) )
		{
			leftChar  = *leftPtr;
			rightChar = *rightPtr;
			
			// If both characters are digits, exit loop.
			
			if( isdigit( leftChar ) && isdigit( rightChar ) )
			{
				break;
			}
			
			// Both are not digits so compare normally.
			
			if( !inCaseSensitive )
			{
				leftChar  = (uint8_t) tolower( leftChar );
				rightChar = (uint8_t) tolower( rightChar );
			}
			if(      leftChar < rightChar ) return( -1 );
			else if( leftChar > rightChar ) return( 1 );
			++leftPtr;
			++rightPtr;
		}
		
		// If we're reached the end of either string we're done. If not equal, the shorter one is "less".
		
		if( ( leftPtr == leftEnd ) && ( rightPtr == rightEnd ) )	return( 0 );
		if(   leftPtr == leftEnd )									return( -1 );
		if(  rightPtr == rightEnd )									return( 1 );
		
		// Equal so far and both have characters remaining. Skip over leading zeros on numbers
		// (but not a single lone zero). The final zero is not skipped so numbers sort above
		// letters, but below things like spaces. The number of skipped zeros is counted so 
		// a longer string is greater in a tie.
		
		while( ( leftPtr[ 0 ] == '0' ) && ( leftPtr < ( leftEnd - 1 ) ) && isdigit( leftPtr[ 1 ] ) )
		{
			++leftPtr;
			++skippedZeros;
		}
		while( ( rightPtr[ 0 ] == '0' ) && ( rightPtr < ( rightEnd - 1 ) ) && isdigit( rightPtr[ 1 ] ) )
		{
			++rightPtr;
			--skippedZeros;
		}
		
		// Save off our current location (everything up to here is equal).
		
		tempLeftPtr = leftPtr;
		tempRightPtr = rightPtr;
		
		// Read all the digits and see if one runs out first.
		
		while( ( tempLeftPtr  < leftEnd )  && isdigit( *tempLeftPtr ) && 
			   ( tempRightPtr < rightEnd ) && isdigit( *tempRightPtr ) )
		{
			++tempLeftPtr;
			++tempRightPtr;
		}
		
		// If one string has a digit remaining (and the other doesn't), it's greater, otherwise less.
		
		if( ( tempLeftPtr  < leftEnd )  && isdigit( *tempLeftPtr ) )  return( 1 );
		if( ( tempRightPtr < rightEnd ) && isdigit( *tempRightPtr ) ) return( -1 );
		
		// Neither strings have any contiguous digits remaining and both have equal-length numbers 
		// so lexicographically compare to see which is greater. Lexicographic comparison works for 
		// numbers that are equal-length digit sequences.
		
		while( leftPtr < tempLeftPtr )
		{
			leftChar  = *leftPtr;
			rightChar = *rightPtr;
			if(      leftChar < rightChar ) return( -1 );
			else if( leftChar > rightChar ) return( 1 );
			++leftPtr;
			++rightPtr;
		}
		
		// Both strings are equal to this point so use the number of leading zeros to break the tie.
		
		if(      skippedZeros < 0 ) return( -1 );
		else if( skippedZeros > 0 ) return( 1 );
	}
	return( 0 );
}

//===========================================================================================================================
//	TextSep
//===========================================================================================================================

Boolean
	TextSep( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inDelims, 
		const char **	outStr, 
		size_t *		outLen, 
		const char **	outNext )
{
	const char *		token;
	char				srcChar;
	const char *		delims;
	char				delimChar;
	
	for( token = inSrc; inSrc < inEnd; ++inSrc )
	{
		srcChar = *inSrc;
		for( delims = inDelims; ( delimChar = *delims ) != '\0'; ++delims )
		{
			if( srcChar == delimChar )
			{
				*outStr = token;
				*outLen = (size_t)( inSrc - token );
				if( outNext ) *outNext = inSrc + 1;
				return( true );
			}
		}
	}
	if( token != inEnd )
	{
		*outStr = token;
		*outLen = (size_t)( inEnd - token );
		if( outNext ) *outNext = inEnd;
		return( true );
	}
	
	if( outNext ) *outNext = inEnd;
	return( false );
}

//===========================================================================================================================
//	TruncateUTF8
//
//	Notes on UTF-8:
//	0xxxxxxx represents a 7-bit ASCII value from 0x00 to 0x7F
//	10xxxxxx is a continuation byte of a multi-byte character
//	110xxxxx is the first byte of a 2-byte character (11 effective bits; values 0x     80 - 0x     800-1)
//	1110xxxx is the first byte of a 3-byte character (16 effective bits; values 0x    800 - 0x   10000-1)
//	11110xxx is the first byte of a 4-byte character (21 effective bits; values 0x  10000 - 0x  200000-1)
//	111110xx is the first byte of a 5-byte character (26 effective bits; values 0x 200000 - 0x 4000000-1)
//	1111110x is the first byte of a 6-byte character (31 effective bits; values 0x4000000 - 0x80000000-1)
//
//	UTF-16 surrogate pairs are used in UTF-16 to encode values larger than 0xFFFF.
//	Although UTF-16 surrogate pairs are not supposed to appear in legal UTF-8, we want to be defensive
//	about that too. (See <http://www.unicode.org/faq/utf_bom.html#34>, "What are surrogates?")
//	The first of pair is a UTF-16 value in the range 0xD800-0xDBFF (11101101 1010xxxx 10xxxxxx in UTF-8).
//	and the second    is a UTF-16 value in the range 0xDC00-0xDFFF (11101101 1011xxxx 10xxxxxx in UTF-8).
//===========================================================================================================================

size_t	TruncateUTF8( const void *inSrcPtr, size_t inSrcLen, void *inDstPtr, size_t inMaxLen, Boolean inNullTerminate )
{
	size_t		len;
	
	if( inMaxLen == 0 ) return( 0 );
	len = TruncatedUTF8Length( inSrcPtr, inSrcLen, inNullTerminate ? ( inMaxLen - 1 ) : inMaxLen );
	memmove( inDstPtr, inSrcPtr, len );
	if( inNullTerminate ) ( (char *) inDstPtr )[ len ] = '\0';
	return( len );
}

size_t	TruncatedUTF8Length( const void *inStr, size_t inLen, size_t inMax )
{
	const uint8_t *		src;
	uint8_t				b0;
	uint8_t				b1;
	uint8_t				b2;
	int					n;
	int					test;
	int					mask;
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inStr );
	if( inLen > inMax ) inLen = inMax;
	src = (const uint8_t *) inStr;
	
	for( ;; )
	{
		// Back up to the first byte of a UTF-8 character.
		
		n  = 0;
		b1 = 0;
		b2 = 0;
		while( ( inLen > 0 ) && ( ( b0 = src[ inLen - 1 ] ) & 0x80 ) )
		{
			++n;
			--inLen;
			b2 = b1;
			b1 = b0;
			if( ( b1 & 0xC0 ) == 0xC0 ) break; // Stop at the first byte of a UTF-8 character.
		}
		if( n == 0 ) break;		// == 0 is a single-byte UTF-8 character (or string is empty).
		if( n  > 6 ) continue;	//  > 6 is an invalid UTF-8 byte sequence.
		
		// See if the encoded count of bytes in the character matches the actual count.
		
		test = ( 0xFF << ( 8 - n ) ) & 0xFF;
		mask = test | ( 1 << ( ( 8 - n ) - 1 ) );
		if( ( b1 & mask ) != test ) continue; // Mismatch means truncated or invalid character.
		
		// Keep truncating if this is the first half of a UTF-16 surrogate pair (i.e. don't split pairs).
		
		if( ( b1 == 0xED ) && ( ( b2 & 0xF0 ) == 0xA0 ) ) continue;
		
		// Looks good. Add back the count and we're done.
		
		inLen += ( (size_t) n );
		break;
	}
	return( inLen );
}

//===========================================================================================================================
//	UTF16strlen
//===========================================================================================================================

size_t	UTF16strlen( const void *inString )
{
	const uint16_t *		src;
	
	for( src = (const uint16_t *) inString; *src; ++src ) {}
	return( (size_t)( src - ( (const uint16_t *) inString ) ) );
}

//===========================================================================================================================
//	ValidDNSName
//
//	Based on mDNSResponder's AppendDNSNameString.
//===========================================================================================================================

OSStatus	ValidDNSName( const char *inStr )
{
	const char *		src;
	size_t				ptr, lim, len;
	int					v0;
	int					v1;
	int					v2;
	int					val;
	
	src = inStr;
	ptr = 0;
	lim = kRFC1034MaxDomainNameSize;
	if( !( *src ) ) return( kUnderrunErr );									// Illegal empty string.
	while( *src && ( ptr < lim ) )											// While more characters, and space to put them...
	{
		len = ptr++;														// Record where the length would go
		if( *src == '.' ) return( kMalformedErr ); 							// Illegal empty label
		while( *src && ( *src != '.' ) && ( ptr < lim ) )					// While we have characters in the label...
		{
			if( *src++ == '\\' )											// If escape character, check next character
			{
				++src;														// Assume we'll just take the next character
				if( RFC1034IsDigit( src[ -1 ] ) && RFC1034IsDigit( src[ 0 ] ) && RFC1034IsDigit( src[ 1 ] ) )
				{
					v0 = src[ -1 ] - '0';									// If three decimal digits,
					v1 = src[  0 ] - '0';									// then interpret as three-digit decimal
					v2 = src[  1 ] - '0';
					val = ( v0 * 100 ) + ( v1 * 10 ) + v2;
					if( val <= 255 ) src += 2;								// If valid three-digit decimal value, use it
				}
			}
			++ptr;
		}
		if( *src ) ++src;													// Skip over the trailing dot (if present)
		if( ( ptr - len ) - 1 > kRFC1034MaxLabelSize ) return( kSizeErr );	// If illegal label, abort
	}
	if( *src ) return( kOverrunErr );										// Failure: We didn't successfully consume all input
	return( kNoErr );														// Success: return new value of ptr
}

//===========================================================================================================================
//	XMLEscape
//===========================================================================================================================

void	XMLEscape( const void *inSrc, size_t inLen, void *inBuf, size_t *outLen )
{
	const char *		src;
	const char *		end;
	char *				dst;
	char				c;
	const char *		replace;
	
	src = (const char *) inSrc;
	end = src + ( ( inLen == kSizeCString ) ? strlen( src ) : inLen );
	dst = (char *) inBuf;
	for( ; src < end; ++src )
	{
		c = *src;
		switch( c )
		{
			case '&':  replace = "&amp;";  break;
			case '"':  replace = "&quot;"; break;
			case '\'': replace = "&#39;";  break; // No &apos; in HTML 4, but &#39; works in HTML, XHTML, and XML.
			case '<':  replace = "&lt;";   break;
			case '>':  replace = "&gt;";   break;
			default:
				if( inBuf ) *dst = c;
				++dst;
				continue;
		}
		while( ( c = *replace++ ) != '\0' )
		{
			if( inBuf ) *dst = c;
			++dst;
		}
	}
	*outLen = (size_t)( dst - ( (char *) inBuf ) );
}

//===========================================================================================================================
//	XMLEscapeCopy
//===========================================================================================================================

OSStatus	XMLEscapeCopy( const void *inSrc, size_t inLen, char **outStr, size_t *outLen )
{
	OSStatus		err;
	size_t			len;
	char *			buf;
	
	XMLEscape( inSrc, inLen, NULL, &len );
	
	buf = (char *) malloc( len + 1 );
	require_action( buf, exit, err = kNoMemoryErr );
	
	XMLEscape( inSrc, inLen, buf, &len );
	buf[ len ] = '\0';
	
	*outStr = buf;
	if( outLen ) *outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == DNS ==
#endif

//===========================================================================================================================
//	Code from the Darwin mDNSResponder project
//===========================================================================================================================

// RFC 1034/1035 specify that a domain label consists of a length byte plus up to 63 characters
#define MAX_DOMAIN_LABEL 63

// RFC 1034/1035 specify that a domain name, including length bytes, data bytes, and terminating zero, may be up to 255 bytes long
#define MAX_DOMAIN_NAME 255

static uint8_t *	AppendDNSNameString(uint8_t *const name, const char *cstring);
static int			DomainEndsInDot(const char *dom);
static uint16_t 	DomainNameLengthLimit(const uint8_t *const name, const uint8_t *limit);
#define				DomainNameLength(name) DomainNameLengthLimit((name), (name) + MAX_DOMAIN_NAME)

// AppendDNSNameString appends zero or more labels to an existing (possibly empty) domainname.
// The C string is in conventional DNS syntax:
// Textual labels, escaped as necessary using the usual DNS '\' notation, separated by dots.
// If successful, AppendDNSNameString returns a pointer to the next unused byte
// in the domainname buffer (i.e. the next byte after the terminating zero).
// If unable to construct a legal domain name (i.e. label more than 63 bytes, or total more than 256 bytes)
// AppendDNSNameString returns NULL.
static uint8_t *AppendDNSNameString(uint8_t *const name, const char *cstring)
{
	const char    *cstr      = cstring;
	uint8_t       *      ptr = name + DomainNameLength(name) - 1;	// Find end of current name
	const uint8_t *const lim = name + MAX_DOMAIN_NAME - 1;			// Limit of how much we can add (not counting final zero)
	while (*cstr && ptr < lim)										// While more characters, and space to put them...
	{
		uint8_t *lengthbyte = ptr++;								// Record where the length is going to go
		if (*cstr == '.') { dlogassert("AppendDNSNameString: Illegal empty label in name \"%s\"", cstring); return(NULL); }
		while (*cstr && *cstr != '.' && ptr < lim)					// While we have characters in the label...
		{
			uint8_t c = (uint8_t)*cstr++;							// Read the character
			if (c == '\\')											// If escape character, check next character
			{
				c = (uint8_t)*cstr++;								// Assume we'll just take the next character
				if (isdigit_safe(cstr[-1]) && isdigit_safe(cstr[0]) && isdigit_safe(cstr[1]))
				{													// If three decimal digits,
					int v0 = cstr[-1] - '0';						// then interpret as three-digit decimal
					int v1 = cstr[ 0] - '0';
					int v2 = cstr[ 1] - '0';
					int val = v0 * 100 + v1 * 10 + v2;
					if (val <= 255) { c = (uint8_t)val; cstr += 2; }// If valid three-digit decimal value, use it
				}
			}
			*ptr++ = c;												// Write the character
		}
		if (*cstr) cstr++;											// Skip over the trailing dot (if present)
		if (ptr - lengthbyte - 1 > MAX_DOMAIN_LABEL)				// If illegal label, abort
			return(NULL);
		*lengthbyte = (uint8_t)(ptr - lengthbyte - 1);				// Fill in the length byte
	}

	*ptr++ = 0;														// Put the null root label on the end
	if (*cstr) return(NULL);										// Failure: We didn't successfully consume all input
	else return(ptr);												// Success: return new value of ptr
}

// Extension of  DNSServiceConstructFullName to also escape '%' and ':' for scope IDs and port numbers.
//
// Note: Need to make sure we don't write more than kDNSServiceMaxDomainName (1009) bytes to fullName
// In earlier builds this constant was defined to be 1005, so to avoid buffer overruns on clients
// compiled with that constant we'll actually limit the output to 1005 bytes.

OSStatus DNSServiceConstructFullNameEx
(
    char       *const fullName,
    const char *const service, // May be NULL
    const char *const regtype,
    const char *const domain
)
{
    const size_t len = !regtype ? 0 : (strlen(regtype) - (DomainEndsInDot(regtype) ? 1 : 0));
    char       *fn   = fullName;
    char *const lim  = fullName + 1005;
    const char *s    = service;
    const char *r    = regtype;
    const char *d    = domain;

    // regtype must be at least "x._udp" or "x._tcp"
    if (len < 6 || !domain || !domain[0]) return kParamErr;
    if (strnicmp((regtype + len - 4), "_tcp", 4) && strnicmp((regtype + len - 4), "_udp", 4)) return kParamErr;

    if (service && *service)
    {
        while (*s)
        {
            unsigned char c = (unsigned char)(*s++);    // Needs to be unsigned, or values like 0xFF will be interpreted as < 32
            if ((c <= ' ') || (c == '%') || (c == ':')) // Escape non-printable characters
            {
                if (fn+4 >= lim) goto fail;
                *fn++ = '\\';
                *fn++ = '0' + (c / 100);
                *fn++ = '0' + (c /  10) % 10;
                c     = '0' + (c      ) % 10;
            }
            else if (c == '.' || (c == '\\'))   // Escape dot and backslash literals
            {
                if (fn+2 >= lim) goto fail;
                *fn++ = '\\';
            }
            else
            if (fn+1 >= lim) goto fail;
            *fn++ = (char)c;
        }
        *fn++ = '.';
    }

    while (*r) if (fn+1 >= lim) goto fail;else *fn++ = *r++;
    if (!DomainEndsInDot(regtype)) { if (fn+1 >= lim) goto fail;else *fn++ = '.';}

    while (*d) if (fn+1 >= lim) goto fail;else *fn++ = *d++;
    if (!DomainEndsInDot(domain)) { if (fn+1 >= lim) goto fail;else *fn++ = '.';}

    *fn = '\0';
    return kNoErr;

fail:
    *fn = '\0';
    return kParamErr;
}

// Returns length of a domain name INCLUDING the byte for the final null label
// e.g. for the root label "." it returns one
// For the FQDN "com." it returns 5 (length byte, three data bytes, final zero)
// Legal results are 1 (just root label) to 256 (MAX_DOMAIN_NAME)
// If the given domainname is invalid, result is 257 (MAX_DOMAIN_NAME+1)
static uint16_t DomainNameLengthLimit(const uint8_t *const name, const uint8_t *limit)
{
	const uint8_t *src = name;
	while (src < limit && *src <= MAX_DOMAIN_LABEL)
	{
		if (*src == 0) return((uint16_t)(src - name + 1));
		src += 1 + *src;
	}
	return(MAX_DOMAIN_NAME+1);
}

// DomainEndsInDot returns 1 if name ends with a dot, 0 otherwise
// (DNSServiceConstructFullName depends this returning 1 for true, rather than any non-zero value meaning true)

static int DomainEndsInDot(const char *dom)
{
    while (dom[0] && dom[1])
    {
        if (dom[0] == '\\') // advance past escaped byte sequence
        {
            if (isdigit_safe(dom[1]) && isdigit_safe(dom[2]) && isdigit_safe(dom[3]))
                dom += 4;           // If "\ddd"    then skip four
            else dom += 2;          // else if "\x" then skip two
        }
        else dom++;                 // else goto next character
    }
    return (dom[0] == '.');
}

// MakeDomainNameFromDNSNameString makes a native DNS-format domainname from a C string.
// The C string is in conventional DNS syntax:
// Textual labels, escaped as necessary using the usual DNS '\' notation, separated by dots.
// If successful, MakeDomainNameFromDNSNameString returns a pointer to the next unused byte
// in the domainname buffer (i.e. the next byte after the terminating zero).
// If unable to construct a legal domain name (i.e. label more than 63 bytes, or total more than 256 bytes)
// MakeDomainNameFromDNSNameString returns NULL.
uint8_t *	MakeDomainNameFromDNSNameString(uint8_t *const name, const char *cstr)
{
	name[0] = 0;									// Make an empty domain name
	return(AppendDNSNameString(name, cstr));		// And then add this string to it
}

#if 0
#pragma mark -
#pragma mark == Numeric Suffixes ==
#endif

//===========================================================================================================================
//	Numeric Suffixes
//===========================================================================================================================

// Based on code from mDNSResponder.

// appends a numerical suffix to a label, with the number following a whitespace and enclosed
// in parentheses (rich text) or following two consecutive hyphens (RFC 1034 domain label).
void AppendNumericSuffix(void *str, size_t len, size_t maxLen, size_t val, Boolean RichText, size_t *newLen)
{
	uint8_t *name = (uint8_t *) str;
	size_t divisor = 1, chars = 2;	// Shortest possible RFC1034 name suffix is 2 characters ("-2")
	if (RichText) chars = 4;		// Shortest possible RichText suffix is 4 characters (" (2)")

	// Truncate trailing spaces from RichText names
	if (RichText) while ((len > 0) && (name[len-1] == ' ')) len--;

	while (val >= divisor * 10) { divisor *= 10; chars++; }

	len = TruncatedUTF8Length(name, len, maxLen - chars);

	if (RichText) { name[len++] = ' '; name[len++] = '('; }
	else          { name[len++] = '-'; }

	while (divisor)
	{
		name[len++] = (uint8_t)('0' + val / divisor);
		val     %= divisor;
		divisor /= 10;
	}

	if (RichText) name[len++] = ')';
	
	*newLen = len;
}

// Returns true if a rich text label ends in " (nnn)", or if an RFC 1034
// name ends in "-nnn", where n is some decimal number.
Boolean ContainsNumericSuffix(const void *str, size_t len, Boolean RichText)
{
	const uint8_t *const name = (const uint8_t *) str;
	size_t l = len;

	if (RichText)
	{
		if (l < 4) return false;						// Need at least " (2)"
		if (name[--l] != ')') return false;				// Last char must be ')'
		if (!isdigit(name[l-1])) return false;			// Preceeded by a digit
		l--;
		while (l > 2 && isdigit(name[l-1])) l--;		// Strip off digits
		return (name[l-1] == '(' && name[l - 2] == ' ');
	}
	else
	{
		if (l < 2) return false;						// Need at least "-2"
		if (!isdigit(name[l-1])) return false;			// Last char must be a digit
		l--;
		while (l > 1 && isdigit(name[l-1])) l--;		// Strip off digits
		return (name[l-1] == '-');
	}
}

void IncrementNumericSuffix(void *str, size_t len, size_t maxLen, Boolean RichText, size_t *newLen)
{
	IncrementNumericSuffixEx(str, len, maxLen, RichText, newLen, 10);
}

void IncrementNumericSuffixEx(void *str, size_t len, size_t maxLen, Boolean RichText, size_t *newLen, uint32_t maxVal)
{
	uint8_t *name = (uint8_t *) str;
	uint32_t val = 0;

	if (ContainsNumericSuffix(name, len, RichText))
		val = RemoveNumericSuffix(name, len, RichText, &len);

	// If no existing suffix, start by renaming "Foo" as "Foo (2)" or "Foo-2" as appropriate.
	// If existing suffix in the range 2-9, increment it.
	// If we've had ten conflicts already, there are probably too many hosts trying to use the same name,
	// so add a random increment to improve the chances of finding an available name next time.
	if      (val ==  0)		val = 2;
	else if (val < maxVal)	val++;
	else					val += 1 + (Random32() % 128);

	AppendNumericSuffix(name, len, maxLen, val, RichText, newLen);
}

// removes an auto-generated suffix (appended on a name collision) from a label.  caller is
// responsible for ensuring that the label does indeed contain a suffix.  returns the number
// from the suffix that was removed.
uint32_t RemoveNumericSuffix(void *str, size_t len, Boolean RichText, size_t *newLen)
{
	uint8_t *name = (uint8_t *) str;
	uint32_t val = 0, multiplier = 1;

	// Chop closing parentheses from RichText suffix
	if (RichText && len >= 1 && name[len-1] == ')') len--;

	// Get any existing numerical suffix off the name
	while ((len > 0) && isdigit(name[len-1])) { val += (name[len-1] - '0') * multiplier; multiplier *= 10; len--; }

	// Chop opening parentheses or dash from suffix
	if (RichText)
	{
		if (len >= 2 && name[len-1] == '(' && name[len-2] == ' ') len -= 2;
	}
	else
	{
		if (len >= 1 && name[len-1] == '-') len -= 1;
	}

	*newLen = len;
	
	return(val);
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	StringUtilsSuffixesTest
//===========================================================================================================================

OSStatus	StringUtilsSuffixesTest( void );
OSStatus	StringUtilsSuffixesTest( void )
{
	OSStatus		err;
	char			str[ 128 ];
	size_t			oldLen;
	size_t			newLen;
	
	// Rich Text
	
	strlcpy( str, "My Name", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "My Name (2)" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "My Name (2)", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "My Name (3)" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "My Name (10)", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strncmp( str, "My Name (", 9 ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "My Name (100)", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strncmp( str, "My Name (", 9 ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "My Name  ", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "My Name (2)" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "(2)", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "(2) (2)" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, " (2)", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, true, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, " (3)" ) == 0, exit, err = kResponseErr );
	
	// DNS-compatible 
	
	strlcpy( str, "my-name", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "my-name-2" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "my-name-2", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "my-name-3" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "my-name-10", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strncmp( str, "my-name-", 8 ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "my-name-100", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strncmp( str, "my-name-", 8 ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "2", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "2-2" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "-2", sizeof( str ) );
	oldLen = strlen( str );
	IncrementNumericSuffix( str, oldLen, sizeof( str ) - 1, false, &newLen );
	require_action( newLen < sizeof( str ), exit, err = kResponseErr );
	str[ newLen ] = '\0';
	require_action( strcmp( str, "-3" ) == 0, exit, err = kResponseErr );
	
	err = kNoErr;
	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == SNScanF ==
#endif

//===========================================================================================================================
//	SNScanF
//===========================================================================================================================

int	SNScanF( const void *inString, size_t inSize, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VSNScanF( inString, inSize, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	VSNScanF - va_list version of SNScanF.
//===========================================================================================================================

int	VSNScanF( const void *inStr, size_t inLen, const char *inFormat, va_list inArgs )
{
	int								matched = 0;
	const unsigned char *			src = (const unsigned char *) inStr;
	const unsigned char * const		end = src + ( ( inLen == kSizeCString ) ? strlen( (const char *) inStr ) : inLen );
	const unsigned char *			fmt = (const unsigned char *) inFormat;
	const unsigned char *			old;
	const unsigned char *			setStart;
	const unsigned char *			setEnd;
	const unsigned char *			set;
	const unsigned char *			end2;
	int								notSet, suppress, alt, storePtr, negative, fieldWidth, sizeModifier, base, v, n;
	unsigned char *					s;
	int *							i;
	unsigned char					c;
	int64_t							x;
	void *							p;
	const unsigned char **			ptrArg;
	size_t *						sizeArg;
	size_t							len;
	char							buf[ 64 ];
	double							d;
	
	for( ;; )
	{
		// Skip whitespace. 1 or more whitespace in the format matches 0 or more whitepsace in the string.
		
		if( isspace( *fmt ) )
		{
			++fmt;
			while( isspace( *fmt ) )				  ++fmt;
			while( ( src < end ) && isspace( *src ) ) ++src;
		}
		if( *fmt == '\0' ) break;
		
		// If it's not a conversion, it must match exactly. Otherwise, move onto conversion handling.
		
		if( *fmt != '%' )
		{
			if( src >= end )		break;
			if( *fmt++ != *src++ )	break;
			continue;
		}
		++fmt;
		
		// Flags
		
		suppress = 0;
		alt		 = 0;
		storePtr = 0;
		for( ;; )
		{
			c = *fmt;
			if(      c == '*' ) suppress = 1;
			else if( c == '#' ) alt		+= 1;
			else if( c == '&' ) storePtr = 1;
			else break;
			++fmt;
		}
		
		// Field width. If none, use INT_MAX to simplify no-width vs width cases.
		
		if( isdigit( *fmt ) )
		{
			fieldWidth = 0;
			do
			{
				fieldWidth = ( fieldWidth * 10 ) + ( *fmt++ - '0' );
			
			}	while( isdigit( *fmt ) );
			if( fieldWidth < 0 ) goto exit; // Handle oversized integer appearing negative.
		}
		else if( *fmt == '.' )
		{
			++fmt;
			fieldWidth = va_arg( inArgs, int );
			if( fieldWidth < 0 ) goto exit;
		}
		else
		{
			fieldWidth = INT_MAX;
		}
		
		// Size modifier. Note: converts double-char (e.g. hh) into unique char (e.g. H) for easier processing later.
		
		c = *fmt;
		switch( c )
		{
			case 'h':
				if( *( ++fmt ) == 'h' ) { sizeModifier = 'H'; ++fmt; }	// hh for char *  / unsigned char *
				else					  sizeModifier = 'h';			// h  for short * / unsigned short *
				break;
			
			case 'l':
				if( *( ++fmt ) == 'l' ) { sizeModifier = 'L'; ++fmt; }	// ll for long long * / unsigned long long *
				else					  sizeModifier = 'l';			// l  for long *      / unsigned long *
				break;
			
			case 'j':	// j for intmax_t * / uintmax_t *
			case 'z':	// z for size_t *
			case 't':	// t for ptrdiff_t *
				sizeModifier = c;
				++fmt;
				break;
			
			default:
				sizeModifier = 0;
				break;
		}
		if( *fmt == '\0' ) break;
		
		// Conversions
		
		switch( *fmt++ )
		{
			case 'd':	// %d: Signed decimal integer.
				base = 10;
				break;
			
			case 'u':	// %u: Unsigned decimal integer.
				base = 10;
				break;
			
			case 'p':	// %x/%X/%p: Hexidecimal integer.
				if( sizeModifier == 0 ) sizeModifier = 'p';
			case 'x':
			case 'X':
				base = 16;
				break;
			
			case 'o':	// %o: Octal integer.
				base = 8;
				break;
			
			case 'i':	// %i: Integer using an optional prefix to determine base (e.g. 10, 0xA, 012, 0b1010 for decimal 10).
				base = 0;
				break;
			
			case 'b':	// %b: Binary integer.
				base = 2;
				break;
			
			case 'f':	// %f: floating point number.
				
				while( ( src < end ) && isspace( *src ) ) ++src;
				old = src;
				end2 = ( ( end - src ) <= ( (ptrdiff_t) fieldWidth ) ) ? end : ( src + fieldWidth );
				if(    ( src < end2 ) && ( *src == '-' ) )							++src;
				while( ( src < end2 ) && isdigit_safe( *src ) )						++src;
				if(    ( src < end2 ) && ( *src == '.' ) )							++src;
				while( ( src < end2 ) && isdigit_safe( *src ) )						++src;
				if(    ( src < end2 ) && ( tolower_safe( *src ) == 'e' ) )			++src;
				if(    ( src < end2 ) && ( ( *src == '+' ) || ( *src == '-' ) ) )	++src;
				while( ( src < end2 ) && isdigit_safe( *src ) )						++src;
				
				len = (size_t)( src - old );
				require_quiet( len < sizeof( buf ), exit );
				memcpy( buf, old, len );
				buf[ len ] = '\0';
				n = sscanf( buf, "%lf", &d );
				if( n != 1 ) goto exit;
				
				if( suppress ) continue;
				p = va_arg( inArgs, void * );
				if( !p ) goto exit;
				if(      sizeModifier == 'l' )	*( (double *) p ) = d;
				else if( sizeModifier == 0 )	*( (float *)  p ) = (float) d;
				else goto exit;
				++matched;
				continue;
			
			case 'c':	// %c: 1 or more characters.
			
				if( sizeModifier != 0 ) goto exit;
				if( storePtr )
				{
					len = (size_t)( end - src );
					if( len > (size_t) fieldWidth )
					{
						len = (size_t) fieldWidth;
					}
					if( suppress ) { src += len; continue; }
					
					ptrArg = va_arg( inArgs, const unsigned char ** );
					if( ptrArg ) *ptrArg = src;
					
					sizeArg = va_arg( inArgs, size_t * );
					if( sizeArg ) *sizeArg = len;
					
					src += len;
				}
				else
				{
					if( fieldWidth == INT_MAX )			fieldWidth = 1;
					if( ( end - src ) < fieldWidth )	goto exit;
					if( suppress )						{ src += fieldWidth; continue; }
					
					s = va_arg( inArgs, unsigned char * );
					if( !s ) goto exit;
					
					while( fieldWidth-- > 0 ) *s++ = *src++;
				}
				++matched;
				continue;
			
			case 's':	// %s: string of non-whitespace characters with a null terminator.
			
				if( sizeModifier != 0 ) goto exit;
				
				// Skip leading white space first since fieldWidth does not include leading whitespace.
				
				while( ( src < end ) && isspace( *src ) ) ++src;
				if( !alt && ( ( src >= end ) || ( *src == '\0' ) ) ) goto exit;
				
				// Copy the string until a null terminator, whitespace, or the max fieldWidth is hit.
				
				if( suppress )
				{
					while( ( src < end ) && ( *src != '\0' ) && !isspace( *src ) && ( fieldWidth-- > 0 ) ) ++src;
				}
				else if( storePtr )
				{
					old = src;
					while( ( src < end ) && ( *src != '\0' ) && !isspace( *src ) && ( fieldWidth-- > 0 ) ) ++src;
					
					ptrArg = va_arg( inArgs, const unsigned char ** );
					if( ptrArg ) *ptrArg = old;
					
					sizeArg = va_arg( inArgs, size_t * );
					if( sizeArg ) *sizeArg = (size_t)( src - old );
					
					++matched;
				}
				else
				{
					s = va_arg( inArgs, unsigned char * );
					if( !s ) goto exit;
					
					while( ( src < end ) && ( *src != '\0' ) && !isspace( *src ) && ( fieldWidth-- > 0 ) ) *s++ = *src++;
					*s = '\0';
					
					++matched;
				}
				continue;
			
			case '[':	// %[: Match a scanset (set between brackets or the compliment set if it starts with ^).
			
				if( sizeModifier != 0 ) goto exit;
				
				notSet = ( *fmt == '^' );	// A scanlist starting with ^ matches all characters not in the scanlist.
				if( notSet ) ++fmt;
				setStart = fmt;
				if( *fmt == ']' ) ++fmt;	// A scanlist (after a potential ^) starting with ] includes ] in the set.
				
				// Find the end of the scanlist.
				
				while( ( *fmt != '\0' ) && ( *fmt != ']' ) ) ++fmt;
				if( *fmt == '\0' ) goto exit;
				setEnd = fmt++;
				
				// Parse until a mismatch, null terminator, or the max fieldWidth is hit.
				
				old = src;
				if( notSet )
				{
					while( ( src < end ) && ( *src != '\0' ) && ( fieldWidth-- > 0 ) )
					{
						c = *src;
						for( set = setStart; ( set < setEnd ) && ( *set != c ); ++set ) {}
						if( set < setEnd ) break;
						++src;
					}
				}
				else
				{
					while( ( src < end ) && ( *src != '\0' ) && ( fieldWidth-- > 0 ) )
					{
						c = *src;
						for( set = setStart; ( set < setEnd ) && ( *set != c ); ++set ) {}
						if( set >= setEnd ) break;
						++src;
					}
				}
				if( ( old == src ) && !alt ) goto exit;
				if( !suppress )
				{
					if( storePtr )
					{
						ptrArg = va_arg( inArgs, const unsigned char ** );
						if( ptrArg ) *ptrArg = old;
						
						sizeArg = va_arg( inArgs, size_t * );
						if( sizeArg ) *sizeArg = (size_t)( src - old );
					}
					else
					{
						s = va_arg( inArgs, unsigned char * );
						if( !s ) goto exit;
						
						while( old < src ) *s++ = *old++;
						*s = '\0';
					}
					++matched;
				}
				continue;
			
			case '%':	// %%: Match a literal % character.
				
				if( sizeModifier != 0 )		goto exit;
				if( fieldWidth != INT_MAX ) goto exit;
				if( suppress )				goto exit;
				if( src >= end )			goto exit;
				if( *src++ != '%' )			goto exit;
				continue;
			
			case 'n':	// %n: Return the number of characters read so far.
			
				if( sizeModifier != 0 )		goto exit;
				if( fieldWidth != INT_MAX )	goto exit;
				if( suppress )				goto exit;
				
				if( alt )
				{
					ptrArg = va_arg( inArgs, const unsigned char ** );
					if( !ptrArg ) goto exit;
					*ptrArg = src;
				}
				else
				{
					i = va_arg( inArgs, int * );
					if( !i ) goto exit;
					*i = (int)( src - ( (const unsigned char *) inStr ) );
				}
				continue;
			
			default:	// Unknown conversion.
				goto exit;
		}
		
		// Number conversion. Skip leading white space since number conversions ignore leading white space.
		
		while( ( src < end ) && isspace( *src ) ) ++src;
		
		// Handle +/- prefix for negative/positive (even for unsigned numbers).
		
		negative = 0;
		if( ( ( end - src ) > 1 ) && ( fieldWidth > 0 ) )
		{
			if( src[ 0 ] == '-' )
			{
				negative = 1;
				++src;
				--fieldWidth;
			}
			else if( src[ 0 ] == '+' )
			{
				++src;
				--fieldWidth;
			}
		}
		
		// Detect the base for base 0 and skip valid prefixes.
		
		old = src;
		if( base == 0 )
		{
			if( ( ( end - src ) > 2 ) && ( fieldWidth >= 2 ) && 
				( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'x' ) && isxdigit( src[ 2 ] ) )
			{
				base		 = 16;
				src			+=  2;
				fieldWidth	-=  2;
			}
			else if( ( ( end - src ) > 2 ) && ( fieldWidth >= 2 ) && 
					 ( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'b' ) && 
					 ( ( src[ 2 ] == '0' ) || ( src[ 2 ] == '1' ) ) )
			{
				base		 = 2;
				src			+= 2;
				fieldWidth	-= 2;
			}
			else if( ( ( end - src ) > 1 ) && ( fieldWidth >= 1 ) && 
					 ( src[ 0 ] == '0' ) && ( src[ 1 ] >= '0' ) && ( src[ 1 ] <= '7' ) )
			{
				base		 = 8;
				src			+= 1;
				fieldWidth	-= 1;
			}
			else
			{
				base = 10;
			}
		}
		else if( ( base == 16 ) && ( ( end - src ) >= 2 ) && ( fieldWidth >= 2 ) && 
				 ( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'x' ) )
		{
			src			+= 2;
			fieldWidth	-= 2;
		}
		else if( ( base == 2 ) && ( ( end - src ) >= 2 ) && ( fieldWidth >= 2 ) && 
				 ( src[ 0 ] == '0' ) && ( tolower( src[ 1 ] ) == 'b' ) )
		{
			src			+= 2;
			fieldWidth	-= 2;
		}
		
		// Convert the string to a number.
		
		x = 0;
		while( ( src < end ) && ( fieldWidth-- > 0 ) )
		{
			c = *src;
			if(      isdigit(  c ) ) v = c - '0';
			else if( isxdigit( c ) ) v = 10 + ( tolower( c ) - 'a' );
			else break;
			if( v >= base ) break;
			
			x = ( x * base ) + v;
			++src;
		}
		if( src == old ) goto exit;
		if( suppress )	 continue;
		if( negative )	 x = -x;
		
		// Store the result.
		
		p = va_arg( inArgs, void * );
		if( !p ) goto exit;
		
		switch( sizeModifier )
		{
			case   0: *( (int       *) p ) = (int)					x; break;
			case 'l': *( (long      *) p ) = (long)					x; break;
			case 'H': *( (char      *) p ) = (char)					x; break;
			case 'h': *( (short     *) p ) = (short)				x; break;
			case 'L': *( (int64_t   *) p ) =						x; break;
			case 'j': *( (intmax_t  *) p ) = (intmax_t)				x; break;
			case 'z': *( (size_t    *) p ) = (size_t)				x; break;
			case 't': *( (ptrdiff_t *) p ) = (ptrdiff_t)			x; break;
			case 'p': *( (void     **) p ) = (void *)( (uintptr_t)	x ); break;
			default: goto exit;
		}
		++matched;
	}
	
exit:
	return( matched );
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	SNScanFTest
//===========================================================================================================================

OSStatus	SNScanFTest( void )
{
	OSStatus			err;
	int					n;
	int					d;
	int					d2;
	int					d3;
	int					d4;
	int64_t				ll;
	uint64_t			ull;
	unsigned int		u;
	unsigned short		us[ 6 ];
	char				s[ 256 ];
	char				s2[ 256 ];
	const char *		str;
	const char *		str2;
	size_t				size;
	size_t				size2;
	void *				p;
	size_t				z;
	float				f;
	double				df;
	
	// ISO C99 fscanf Example 1 (7.19.6.2 #19).
	
	str  = "25 54.32E-1 thompson";
	size = strlen( str );
	n = SNScanF( str, size, "%d%f%s", &d, &f, s );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( d == 25, exit, err = kResponseErr );
	require_action( RoundTo( f, 1000 )  == RoundTo( 5.432, 1000 ), exit, err = kResponseErr );
	require_action( strcmp( s, "thompson" ) == 0, exit, err = kResponseErr );
	
	// ISO C99 fscanf Example 2 (7.19.6.2 #20).
	
	str  = "56789 0123 56a72";
	size = strlen( str );
	n = SNScanF( str, size, "%2d%f%*d %[0123456789]", &d, &f, s );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( d == 56, exit, err = kResponseErr );
	require_action( f == 789, exit, err = kResponseErr );
	require_action( strcmp( s, "56" ) == 0, exit, err = kResponseErr );
	
	// ISO C99 fscanf Example 4 (7.19.6.2 #23).
	
	d4 = -999;
	n = SNScanF( "123", sizeof_string( "123" ), "%d%n%n%d", &d, &d2, &d3, &d4 );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	require_action( d2 == 3, exit, err = kResponseErr );
	require_action( d3 == 3, exit, err = kResponseErr );
	require_action( d4 == -999, exit, err = kResponseErr );
	
	// Misc Tests
	
	n = SNScanF( "123", 3, "%d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "-123", 4, "%d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == -123, exit, err = kResponseErr );
	
	n = SNScanF( "+123", 4, "%d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "123", 3, "%u", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "123", 3, "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "-123", 4, "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == -123, exit, err = kResponseErr );
	
	n = SNScanF( "0x123", 5, "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0x123, exit, err = kResponseErr );
	
	n = SNScanF( "0123", 4, "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0123, exit, err = kResponseErr );
	
	n = SNScanF( "123", 3, "%x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0x123, exit, err = kResponseErr );
	
	n = SNScanF( "0x123", 5, "%x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0x123, exit, err = kResponseErr );
	
	n = SNScanF( "123", 3, "%o", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0123, exit, err = kResponseErr );
	
	n = SNScanF( "123 456", 7, "%*d %d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 456, exit, err = kResponseErr );
	
	n = SNScanF( "123456", 6, "%3d%3d", &d, &d2 );
	require_action( n == 2, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	require_action( d2 == 456, exit, err = kResponseErr );
	
	n = SNScanF( "123", sizeof_string( "123" ), "%lld", &ll );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ll == INT64_C( 123 ), exit, err = kResponseErr );
	
	n = SNScanF( "-9223372036854775807", sizeof_string( "-9223372036854775807" ), "%lld", &ll );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ll == INT64_C( -9223372036854775807 ), exit, err = kResponseErr );
	
	n = SNScanF( "+9223372036854775807", sizeof_string( "+9223372036854775807" ), "%lld", &ll );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ll == INT64_C( 9223372036854775807 ), exit, err = kResponseErr );
	
	n = SNScanF( "18446744073709551615", sizeof_string( "18446744073709551615" ), "%llu", &ull );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ull == UINT64_C( 18446744073709551615 ), exit, err = kResponseErr );
	
	n = SNScanF( "7FFFFFFFFFFFFFFF", sizeof_string( "7FFFFFFFFFFFFFFF" ), "%llx", &ull );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ull == UINT64_C( 0x7FFFFFFFFFFFFFFF ), exit, err = kResponseErr );
	
	n = SNScanF( "FFFFFFFFFFFFFFFF", sizeof_string( "FFFFFFFFFFFFFFFF" ), "%llx", &ull );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ull == UINT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = kResponseErr );
	
	n = SNScanF( "FFFF", sizeof_string( "FFFF" ), "%llx", &ull );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ull == UINT64_C( 0xFFFF ), exit, err = kResponseErr );
	
	n = SNScanF( "FFFFFFFF", sizeof_string( "FFFFFFFF" ), "%llx", &ull );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ull == UINT64_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	
	n = SNScanF( "0b1111011", sizeof_string( "0b1111011" ), "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "0b1111011", sizeof_string( "0b1111011" ), "%b", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "1111011", sizeof_string( "1111011" ), "%b", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	n = SNScanF( "0", sizeof_string( "0" ), "%f", &f );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( f == 0, exit, err = kResponseErr );
	
	n = SNScanF( "0.0", kSizeCString, "%f", &f );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( f == 0, exit, err = kResponseErr );
	
	n = SNScanF( " 123.45  ", kSizeCString, "%f", &f );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( RoundTo( f, 100 ) == RoundTo( 123.45, 100 ), exit, err = kResponseErr );
	
	df = 3.1415;
	n = SNScanF( " 123.45  ", kSizeCString, "%lf", &df );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( RoundTo( df, 1000 ) == RoundTo( 123.45, 1000 ), exit, err = kResponseErr );
	
	n = SNScanF( " -123.45  ", kSizeCString, "%f", &f );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( RoundTo( f, 1000 ) == RoundTo( -123.45, 1000 ), exit, err = kResponseErr );
	
	n = SNScanF( "1234.56 99 333.44 77", kSizeCString, "%2f%lf%d%*f%d", &f, &df, &d, &d2 );
	require_action( n == 4, exit, err = kResponseErr );
	require_action( f == 12, exit, err = kResponseErr );
	require_action( RoundTo( df, 100 ) == RoundTo( 34.56, 100 ), exit, err = kResponseErr );
	require_action( d == 99, exit, err = kResponseErr );
	require_action( d2 == 77, exit, err = kResponseErr );
	
	memset( s, 'X', sizeof( s ) );
	n = SNScanF( "aaa", 3, "%c", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( s[ 0 ] == 'a', exit, err = kResponseErr );
	require_action( s[ 1 ] == 'X', exit, err = kResponseErr );
	
	memset( s, 'X', sizeof( s ) );
	n = SNScanF( "aaa", 3, "%2c", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( s[ 0 ] == 'a', exit, err = kResponseErr );
	require_action( s[ 1 ] == 'a', exit, err = kResponseErr );
	require_action( s[ 2 ] == 'X', exit, err = kResponseErr );
	
	n = SNScanF( "this is a test", kSizeCString, "%&c", &str, &size );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( size == 14, exit, err = kResponseErr );
	require_action( strncmpx( str, size, "this is a test" ) == 0, exit, err = kResponseErr );
	
	str = NULL;
	str2 = NULL;
	n = SNScanF( "pretty kewl123", kSizeCString, "%&11c%d%&c", &str, &size, &d, &str2, &size2 );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( size == 11, exit, err = kResponseErr );
	require_action( strncmpx( str, size, "pretty kewl" ) == 0, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	require_action( size2 == 0, exit, err = kResponseErr );
	require_action( *str2 == '\0', exit, err = kResponseErr );
	
	memset( s, 'A', sizeof( s ) );
	n = SNScanF( "test", 4, "%s", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "test" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "test% 0", 7, "test%% %d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0, exit, err = kResponseErr );
	
	str = "test% 0";
	n = SNScanF( str, 7, "test%% %d%n%#n", &d, &d2, &str2 );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0, exit, err = kResponseErr );
	require_action( d2 == 7, exit, err = kResponseErr );
	require_action( str2 == ( str + 7 ), exit, err = kResponseErr );
	
	n = SNScanF( "-2147483648", sizeof_string( "-2147483648" ), "%d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == ( -2147483647 - 1 ), exit, err = kResponseErr );
	
	n = SNScanF( "4294967295", sizeof_string( "4294967295" ), "%u", &u );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( u == 4294967295U, exit, err = kResponseErr );
	
	d = 123;
	n = SNScanF( "test", 4, "%d", &d );
	require_action( n == 0, exit, err = kResponseErr );
	require_action( d == 123, exit, err = kResponseErr );
	
	memset( s, 'x', sizeof( s ) );
	memset( s2, 'x', sizeof( s2 ) );
	n = SNScanF( "testbob", 7, "%4s%s", s, s2 );
	require_action( n == 2, exit, err = kResponseErr );
	require_action( strcmp( s, "test" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( s2, "bob" ) == 0, exit, err = kResponseErr );
	
	str  = "200 Entering Passive Mode (60000,12,2,23,3,34)\r\n";
	size = strlen( str );
	n = SNScanF( str, size, "%d Entering Passive Mode (%hu,%hu,%hu,%hu,%hu,%hu)\r\n", &d, 
		&us[ 0 ], &us[ 1 ], &us[ 2 ], &us[ 3 ], &us[ 4 ], &us[ 5 ] );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( d == 200, exit, err = kResponseErr );
	require_action( us[ 0 ] == 60000, exit, err = kResponseErr );
	require_action( us[ 1 ] ==    12, exit, err = kResponseErr );
	require_action( us[ 2 ] ==     2, exit, err = kResponseErr );
	require_action( us[ 3 ] ==    23, exit, err = kResponseErr );
	require_action( us[ 4 ] ==     3, exit, err = kResponseErr );
	require_action( us[ 5 ] ==    34, exit, err = kResponseErr );
	
	n = SNScanF( "ab23cd45", sizeof_string( "ab23cd45" ), "%[^0123456789]%[^\n]", s, s2 );
	require_action( n == 2, exit, err = kResponseErr );
	require_action( strcmp( s, "ab" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( s2, "23cd45" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "ab23cd45", sizeof_string( "ab23cd45" ), "%[^0123456789]%4[^\n]%d", s, s2, &d );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( strcmp( s, "ab" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( s2, "23cd" ) == 0, exit, err = kResponseErr );
	require_action( d == 45, exit, err = kResponseErr );
	
	n = SNScanF( "<[cba]>", sizeof_string( "<[cba]>" ), "<%[][abc]>", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "[cba]" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "<aaa>", sizeof_string( "<[aaa]>" ), "<%[^][0123456789<>]>", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "aaa" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "\"quote test\"", sizeof_string( "\"quote test\"" ), "\"%[^\"]", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "quote test" ) == 0, exit, err = kResponseErr );
		
	strlcpy( s, "empty", sizeof( s ) );
	n = SNScanF( "\"\"", sizeof_string( "\"\"" ), "\"%[^\"]\"", s );
	require_action( n == 0, exit, err = kResponseErr );
	require_action( strcmp( s, "empty" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "\"\"", sizeof_string( "\"\"" ), "\"%#[^\"]\"", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "\"quote test\"", sizeof_string( "\"quote test\"" ), "\"%#[^\"]", s );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( strcmp( s, "quote test" ) == 0, exit, err = kResponseErr );
	
	strlcpy( s, "bad", sizeof( s ) );
	n = SNScanF( "test", sizeof_string( "test" ), "%[0123456789]", s );
	require_action( n == 0, exit, err = kResponseErr );
	require_action( strcmp( s, "bad" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "abcdef", sizeof_string( "abcdef" ), "%.s%s", 3, s, s2 );
	require_action( n == 2, exit, err = kResponseErr );
	require_action( strcmp( s, "abc" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( s2, "def" ) == 0, exit, err = kResponseErr );
	
	n = SNScanF( "12345", sizeof_string( "12345" ), "%.d", 4, &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 1234, exit, err = kResponseErr );
	
	n = SNScanF( "0x12345678", sizeof_string( "0x12345678" ), "%p", &p );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( p == (void *)(uintptr_t) 0x12345678, exit, err = kResponseErr );
	
	ll = -1;
	n = SNScanF( "0x12345678", sizeof_string( "0x12345678" ), "%llp", &ll );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( ll == INT64_C( 0x12345678 ), exit, err = kResponseErr );
	
	// Field Width Tests
	
	n = SNScanF( "-0x10", kSizeCString, "%i", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == -16, exit, err = kResponseErr );
	
	d = 1;
	n = SNScanF( "-10", kSizeCString, "%1d", &d );
	require_action( n == 0, exit, err = kResponseErr );
	require_action( d == 1, exit, err = kResponseErr );
	
	d = 1;
	n = SNScanF( "-10", kSizeCString, "%2d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == -1, exit, err = kResponseErr );
	
	d = 1;
	n = SNScanF( "-10", kSizeCString, "%3d", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == -10, exit, err = kResponseErr );
	
	d = -100;
	n = SNScanF( "0x12", kSizeCString, "%1x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0, exit, err = kResponseErr );
	
	d = -100;
	n = SNScanF( "0x12", kSizeCString, "%2x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0, exit, err = kResponseErr );
	
	d = -100;
	n = SNScanF( "0x12", kSizeCString, "%3x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 1, exit, err = kResponseErr );
	
	d = -100;
	n = SNScanF( "0x12", kSizeCString, "%4x", &d );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( d == 0x12, exit, err = kResponseErr );
	
	// Size modifiers.
	
	z = 234;
	n = SNScanF( "123", kSizeCString, "%zu", &z );
	require_action( n == 1, exit, err = kResponseErr );
	require_action( z == 123, exit, err = kResponseErr );
	
	// Parsing

{
	int					addr1;
	char				type1;
	const char *		name1;
	size_t				size1;
	int					addr2;
	char				type2;
	const char *		name2;
	
	addr1 = 0;
	type1 = 0;
	name1 = "";
	size1 = 0;
	addr2 = 0;
	type2 = 0;
	name2 = "";
	size2 = 0;
	n = SNScanF( 
		"000080ac T _init\n"
		"0009ceb0 b uDNS_AddRecordToService$$from$$mDNSResponder"
		, kSizeCString, 
		"%x%*[ ]%c%*[ ]%&[^\n]%#*1[\n]"
		"%x%*[ ]%c%*[ ]%&[^\n]%#*1[\n]", 
		&addr1, &type1, &name1, &size1, 
		&addr2, &type2, &name2, &size2 );
	require_action( n == 6, exit, err = kResponseErr );
	require_action( addr1 == 0x000080ac, exit, err = kResponseErr );
	require_action( type1 == 'T', exit, err = kResponseErr );
	require_action( strncmpx( name1, size1, "_init" ) == 0, exit, err = kResponseErr );
	require_action( addr2 == 0x0009ceb0, exit, err = kResponseErr );
	require_action( type2 == 'b', exit, err = kResponseErr );
	require_action( strncmpx( name2, size2, "uDNS_AddRecordToService$$from$$mDNSResponder" ) == 0, exit, err = kResponseErr );
}
	// Success!
	
	err = kNoErr;
	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "NetUtils.h"

#if( TARGET_OS_POSIX )
	static OSStatus	_GetFirstIPInterface( char *inNameBuf, size_t inMaxLen, uint32_t *outIndex );
#endif

//===========================================================================================================================
//	StringUtilsStringToIPv6AddressTestOne
//===========================================================================================================================

OSStatus
	StringUtilsStringToIPv6AddressTestOne( 
		const char *			inStr, 
		StringToIPAddressFlags 	inFlags, 
		int						inIsError, 
		const char *			inHex, 
		uint32_t				inScope, 
		int						inPort, 
		int						inPrefix );

OSStatus
	StringUtilsStringToIPv6AddressTestOne( 
		const char *			inStr, 
		StringToIPAddressFlags 	inFlags, 
		int						inIsError, 
		const char *			inHex, 
		uint32_t				inScope, 
		int						inPort, 
		int						inPrefix )
{
	OSStatus			err;
	uint8_t				ipv6[ 32 ];
	uint32_t			scope;
	int					port;
	int					prefix;
	const char *		s;
	
	memset( ipv6, 'A', sizeof( ipv6 ) );
	scope	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	prefix	= -1;
	s		= NULL;
	err = StringToIPv6Address( inStr, inFlags, ipv6, &scope, &port, &prefix, &s );
	require_action( inIsError ? ( err != kNoErr ) : ( err == kNoErr ), exit, err = kResponseErr );
	require_action( memcmp( ipv6, inHex, 16 ) == 0, exit, err = kResponseErr );
	require_action( memcmp( &ipv6[ 16 ], "AAAAAAAAAAAAAAAAAAAA", 16 ) == 0, exit, err = kResponseErr );
	require_action( scope == inScope, exit, err = kResponseErr );
	require_action( port == inPort, exit, err = kResponseErr );
	require_action( prefix == inPrefix, exit, err = kResponseErr );
	if( inIsError )
	{
		require_action( !s, exit, err = kResponseErr );
	}
	else
	{
		require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsStringToIPv6AddressTest
//===========================================================================================================================

OSStatus	StringUtilsStringToIPv6AddressTest( void );
OSStatus	StringUtilsStringToIPv6AddressTest( void )
{
	OSStatus		err;
#if( TARGET_OS_POSIX )
	char			ifname[ IF_NAMESIZE ];
	uint32_t		ifindex;
	char			str[ 256 ];
	
	*ifname = '\0';
	ifindex = 0;
	_GetFirstIPInterface( ifname, sizeof( ifname ), &ifindex );
#endif
	
	// Valid Cases
	
	err = StringUtilsStringToIPv6AddressTestOne( "::\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::/64\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", UINT32_C( 0xFFFFFFFF ), -1, 64 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::%5/0\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 5, -1, 0 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::%5:80/64\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 5, 80, 64 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::1\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "1::\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "1::1\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "1::1%5:80/128\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 5, 80, 128 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:2:3:4:5:6:7\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x06\x00\x07", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0000:0001:0002:0003:0004:0005:0006:0007\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x06\x00\x07", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f%5\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, -1, -1 );
	require_noerr( err, exit );
	
#if( TARGET_OS_POSIX )
	SNPrintF( str, sizeof( str ), "fe80::5445:5245:444f%%%s%c%c", ifname, '\0', 'Z' );
	err = StringUtilsStringToIPv6AddressTestOne( str, kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", ifindex, -1, -1 );
	require_noerr( err, exit );
#endif
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f]\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f]/64\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", UINT32_C( 0xFFFFFFFF ), -1, 64 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f%5]\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f]%5\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f]:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", UINT32_C( 0xFFFFFFFF ), 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[fe80::5445:5245:444f%5]:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f]%5:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f]%5:80/1\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, 80, 1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f%5:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "fe80::5445:5245:444f%5:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "FF01::101\0Z", kStringToIPAddressFlagsNone, 
		0, "\xFF\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:0:0:0:0:0:13.1.68.3\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:0:0:0:0:0:13.1.68.3/32\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", UINT32_C( 0xFFFFFFFF ), -1, 32 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::13.1.68.3\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:0:0:0:0:ffff:129.144.52.38\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::FFFF:129.144.52.38\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::FFFF:129.144.52.38%5\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 5, -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::ffff:129.144.52.38%5:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "::FFFF:129.144.52.38%5:80/128\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 5, 80, 128 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[::FFFF:129.144.52.38]:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", UINT32_C( 0xFFFFFFFF ), 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[::FFFF:129.144.52.38%5]:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 5, 80, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[::FFFF:129.144.52.38]%5:80\0Z", kStringToIPAddressFlagsNone, 
		0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 5, 80, -1 );
	require_noerr( err, exit );
	
	// Error Cases
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:12345:3:4:5:6:7\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:2:3:4:5:6:7/129\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[0:1:2:3:4:5:6:7]/:80000\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:2:3:4:5:6:7%42949672960\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( ":\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "test\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0::1:2:3:4:5:6:7\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "\0Z", kStringToIPAddressFlagsNone, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "[0:1:2:3:4:5:6:7]:80\0Z", kStringToIPAddressFlagsNoPort, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:2:3:4:5:6:7/32\0Z", kStringToIPAddressFlagsNoPrefix, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv6AddressTestOne( "0:1:2:3:4:5:6:7%5\0Z", kStringToIPAddressFlagsNoScope, 
		1, "AAAAAAAAAAAAAAAA", UINT32_C( 0xFFFFFFFF ), -1, -1 );
	require_noerr( err, exit );
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsStringToIPv4AddressTest
//===========================================================================================================================

OSStatus	StringUtilsStringToIPv4AddressTest( void );
OSStatus	StringUtilsStringToIPv4AddressTest( void )
{
	OSStatus					err;
	uint32_t					ipv4;
	int							port;
	uint32_t					subnet;
	uint32_t					router;
	const char *				s;
	
	// Valid Cases
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "0.0.0.0\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == 0, exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "255.255.255.255\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164:80\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == 80, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164/24\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFF00 ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0x11CD1601 ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164:5009/24\0Z", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == 5009, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFF00 ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0x11CD1601 ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164\0Z", kStringToIPAddressFlagsNoPort, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164\0Z", kStringToIPAddressFlagsNoPrefix, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	ipv4	= 0;
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "17.205.22.164\0Z", kStringToIPAddressFlagsNoPrefix, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x11CD16A4 ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( s && ( s[ 0 ] == '\0' ) && ( s[ 1 ] == 'Z' ), exit, err = kResponseErr );
	
	// Error Cases
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "0", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "999.2.3.4", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1:2.3.4", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1.2.3.4:80", kStringToIPAddressFlagsNoPort, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1.2.3.4/24", kStringToIPAddressFlagsNoPrefix, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1.2.3.4:80/24", kStringToIPAddressFlagsNoPrefix, &ipv4, &port, &subnet, &router, &s );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( ipv4 == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( port == -1, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( !s, exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1.2.3.4:80/32", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x01020304 ), exit, err = kResponseErr );
	require_action( port == 80, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0x01020305 ), exit, err = kResponseErr );
	require_action( s && ( *s == '\0' ), exit, err = kResponseErr );
	
	ipv4	= UINT32_C( 0xFFFFFFFF );
	port	= -1;
	subnet	= UINT32_C( 0xFFFFFFFF );
	router	= UINT32_C( 0xFFFFFFFF );
	s		= NULL;
	err = StringToIPv4Address( "1.2.3.4:80/0", kStringToIPAddressFlagsNone, &ipv4, &port, &subnet, &router, &s );
	require_noerr( err, exit );
	require_action( ipv4 == UINT32_C( 0x01020304 ), exit, err = kResponseErr );
	require_action( port == 80, exit, err = kResponseErr );
	require_action( subnet == UINT32_C( 0x00000000 ), exit, err = kResponseErr );
	require_action( router == UINT32_C( 0x00000001 ), exit, err = kResponseErr );
	require_action( s && ( *s == '\0' ), exit, err = kResponseErr );
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsIPv6AddressToCStringTestOne
//===========================================================================================================================

OSStatus	StringUtilsIPv6AddressToCStringTestOne( const char *inHex, uint32_t inScope, int inPort, int inPrefix, const char *inMatch );
OSStatus	StringUtilsIPv6AddressToCStringTestOne( const char *inHex, uint32_t inScope, int inPort, int inPrefix, const char *inMatch )
{
	OSStatus		err;
	char			str[ 256 ];	
	char *			p;
	char *			q;
	
	q = str + countof( str );
	
	memset( str, 'A', countof( str ) );
	p = IPv6AddressToCString( (const uint8_t *) inHex, inScope, inPort, inPrefix, str, 0 );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, inMatch ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( inMatch ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsIPv6AddressToCStringTest
//===========================================================================================================================

OSStatus	StringUtilsIPv6AddressToCStringTest( void );
OSStatus	StringUtilsIPv6AddressToCStringTest( void )
{
	OSStatus		err;
#if( TARGET_OS_POSIX )
	char			ifname[ IF_NAMESIZE ];
	uint32_t		ifindex;
	char			str[ 256 ];
	
	*ifname = '\0';
	ifindex = 0;
	_GetFirstIPInterface( ifname, sizeof( ifname ), &ifindex );
#endif
	
	// Normal
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"::" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 0, -1, -1, 
		"::1" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"1::" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 0, -1, -1, 
		"1::1" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 0, -1, -1, 
		"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x06\x00\x07", 0, -1, -1, 
		"::1:2:3:4:5:6:7" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x06\x00\x07\x00\x08", 0, -1, -1, 
		"1:2:3:4:5:6:7:8" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x06\x00\x07\x00\x08", 0, -1, -1, 
		"1:2:3:4:5:6:7:8" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 0, -1, -1, 
		"fe80::5445:5245:444f" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFF\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01", 0, -1, -1, 
		"ff01::101" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFF\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01", 0, -1, -1, 
		"ff01::101" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x10\x80\x00\x00\x00\x00\x00\x00\x00\x08\x08\x00\x20\x0c\x41\x7a", 0, -1, -1, 
		"1080::8:800:200c:417a" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x02\x00\x03\x00\x04\x00\x05", 0, -1, -1, 
		"1::2:3:4:5" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x04\x00\x05", 0, -1, -1, 
		"1::3:4:5" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x05", 0, -1, -1, 
		"1::4:5" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05", 0, -1, -1, 
		"1::5" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"1:2:3:4:5::" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x03\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"1:2:3:4::" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"1:2:3::" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x01\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, -1, -1, 
		"1:2::" );
	require_noerr( err, exit );

#if( TARGET_OS_POSIX )
	SNPrintF( str, sizeof( str ), "fe80::5445:5245:444f%%%s", ifname );
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", ifindex, -1, -1, str );
	require_noerr( err, exit );
#endif
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 0, 8080, -1, 
		"[fe80::5445:5245:444f]:8080" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 0, -1, 64, 
		"fe80::5445:5245:444f/64" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 1000, 1234, -1, 
		"[fe80::5445:5245:444f%1000]:1234" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 0, 1234, 5, 
		"[fe80::5445:5245:444f]:1234/5" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 1000, 0, 100, 
		"fe80::5445:5245:444f%1000/100" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\xFE\x80\x00\x00\x00\x00\x00\x00\x00\x00\x54\x45\x52\x45\x44\x4F", 1000, 65535, 60, 
		"[fe80::5445:5245:444f%1000]:65535/60" );
	require_noerr( err, exit );
	
	// IPv4-mapped/compatible
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 0, -1, -1, 
		"::13.1.68.3" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 1000, -1, -1, 
		"::13.1.68.3%1000" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 1000, 1, -1, 
		"[::13.1.68.3%1000]:1" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 1000, 1, 128, 
		"[::13.1.68.3%1000]:1/128" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 1000, 0, 32, 
		"::13.1.68.3%1000/32" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 0, 0, 96, 
		"::13.1.68.3/96" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 0, -1, -1, 
		"::ffff:129.144.52.38" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 500, -1, -1, 
		"::ffff:129.144.52.38%500" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x81\x90\x34\x26", 0, 99, -1, 
		"[::ffff:129.144.52.38]:99" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, 80, -1, 
		"[::]:80" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 1000, 80, -1, 
		"[::%1000]:80" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 1000, -1, -1, 
		"::%1000" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 1000, -1, 12, 
		"::%1000/12" );
	require_noerr( err, exit );
	
	// Forced brackets.
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0, kIPv6AddressToCStringForceIPv6Brackets, -1, 
		"[::]" );
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTestOne( 
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x01\x44\x03", 1000, kIPv6AddressToCStringForceIPv6Brackets, 12, 
		"[::13.1.68.3%1000]/12" );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsConversionsTest
//===========================================================================================================================

OSStatus	StringUtilsConversionsTest( void );
OSStatus	StringUtilsConversionsTest( void )
{
	OSStatus			err;
	char				str[ 256 ];
	char				str2[ 256 ];
	const char *		p;
	const char *		q;
	const char *		match;
	uint8_t				buf[ 256 ];
	uint32_t			u32;
	uint32_t			code;
	uint64_t			u64;
	size_t				writtenBytes;
	size_t				totalBytes;
	const uint8_t *		a;
	
	// IPv4AddressToString
	
	q = str + countof( str );
	
	match = "1.2.3.4";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0x01020304, 0, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	match = "1.12.123.255";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0x010C7BFF, 0, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	match = "255.255.255.255";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0xFFFFFFFF, 0, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	match = "255.255.255.255:123";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0xFFFFFFFF, 123, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	match = "0.0.0.0";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0x00000000, 0, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	match = "0.0.0.0:123";
	memset( str, 'A', sizeof( str ) );
	p = IPv4AddressToCString( 0x00000000, 123, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( p += ( strlen( match ) + 1 ); p < q; ++p ) { require_action( *p == 'A', exit, err = kResponseErr ); }
	
	// BCD
	
	snprintf( str, sizeof( str ), "%u", 0x20081125 );
	u64 = BCDTextToInt( str, kSizeCString, NULL );
	require_action( u64 == 20081125, exit, err = -1 );
	require_action( strcmp( BCDTextFromInt( u64, str2, sizeof( str2 ) ), str ) == 0, exit, err = -1 );
	
	snprintf( str, sizeof( str ), "%u", 0x2008 );
	u64 = BCDTextToInt( str, kSizeCString, NULL );
	require_action( u64 == 2008, exit, err = -1 );
	require_action( strcmp( BCDTextFromInt( u64, str2, sizeof( str2 ) ), str ) == 0, exit, err = -1 );
	
	snprintf( str, sizeof( str ), "%u", 0x61 );
	u64 = BCDTextToInt( str, kSizeCString, NULL );
	require_action( u64 == 61, exit, err = -1 );
	require_action( strcmp( BCDTextFromInt( u64, str2, sizeof( str2 ) ), str ) == 0, exit, err = -1 );
	
	// FourCharCodeToCString / TextToFourCharCode
	
	u32 = 0x7764466C /* 'wdFl' */;
	p = FourCharCodeToCString( u32, str );
	require_action( ( p == str ) && ( strcmp( str, "wdFl" ) == 0 ), exit, err = -1 );
	
	u32 = 0x77644600 /* 'wdF' */;
	p = FourCharCodeToCString( u32, str );
	require_action( ( p == str ) && ( strcmp( str, "wdF " ) == 0 ), exit, err = -1 );
	
	code = TextToFourCharCode( "wdFl", 4 );
	require_action( code == 0x7764466C, exit, err = -1 );
	
	code = TextToFourCharCode( "wdF", 3 );
	require_action( code == 0x77644620, exit, err = -1 );
	
	code = TextToFourCharCode( "wdF", 4 );
	require_action( code == 0x77644620, exit, err = -1 );
	
	// TextToHardwareAddress
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToHardwareAddress( "AA:BB:CC:00:11:22:33:FF", kSizeCString, 8, buf );
	require_noerr( err, exit );
	require_action( buf[ 8 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0xAA ) && ( buf[ 1 ] == 0xBB ) && ( buf[ 2 ] == 0xCC ) && 
					( buf[ 3 ] == 0x00 ) && ( buf[ 4 ] == 0x11 ) && ( buf[ 5 ] == 0x22 ) &&
					( buf[ 6 ] == 0x33 ) && ( buf[ 7 ] == 0xFF ), 
					exit, err = kResponseErr );
	
	// TextToMACAddress tests
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "00242B8C2B03", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x24\x2B\x8C\x2B\x03\xBB", 7 ) == 0, exit, err = -1 );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA:BB:CC:00:11:22", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0xAA ) && ( buf[ 1 ] == 0xBB ) && ( buf[ 2 ] == 0xCC ) && 
					( buf[ 3 ] == 0x00 ) && ( buf[ 4 ] == 0x11 ) && ( buf[ 5 ] == 0x22 ), 
					exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA-BB-CC-00-11-22", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0xAA ) && ( buf[ 1 ] == 0xBB ) && ( buf[ 2 ] == 0xCC ) && 
					( buf[ 3 ] == 0x00 ) && ( buf[ 4 ] == 0x11 ) && ( buf[ 5 ] == 0x22 ), 
					exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA BB CC 00 11 22", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0xAA ) && ( buf[ 1 ] == 0xBB ) && ( buf[ 2 ] == 0xCC ) && 
					( buf[ 3 ] == 0x00 ) && ( buf[ 4 ] == 0x11 ) && ( buf[ 5 ] == 0x22 ), 
					exit, err = kResponseErr );

	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "0:1:2:3:4:5", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0x00 ) && ( buf[ 1 ] == 0x01 ) && ( buf[ 2 ] == 0x02 ) && 
					( buf[ 3 ] == 0x03 ) && ( buf[ 4 ] == 0x04 ) && ( buf[ 5 ] == 0x05 ), 
					exit, err = kResponseErr );

	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "0-1-2-3-4-5", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0x00 ) && ( buf[ 1 ] == 0x01 ) && ( buf[ 2 ] == 0x02 ) && 
					( buf[ 3 ] == 0x03 ) && ( buf[ 4 ] == 0x04 ) && ( buf[ 5 ] == 0x05 ), 
					exit, err = kResponseErr );

	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "0 1 2 3 4 5", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0x00 ) && ( buf[ 1 ] == 0x01 ) && ( buf[ 2 ] == 0x02 ) && 
					( buf[ 3 ] == 0x03 ) && ( buf[ 4 ] == 0x04 ) && ( buf[ 5 ] == 0x05 ), 
					exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA:1:CC:2:11:3", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0xAA ) && ( buf[ 1 ] == 0x01 ) && ( buf[ 2 ] == 0xCC ) && 
					( buf[ 3 ] == 0x02 ) && ( buf[ 4 ] == 0x11 ) && ( buf[ 5 ] == 0x03 ), 
					exit, err = kResponseErr );

	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "1:BB:2:00:3:22", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	require_action( ( buf[ 0 ] == 0x01 ) && ( buf[ 1 ] == 0xBB ) && ( buf[ 2 ] == 0x02 ) && 
					( buf[ 3 ] == 0x00 ) && ( buf[ 4 ] == 0x03 ) && ( buf[ 5 ] == 0x22 ), 
					exit, err = kResponseErr );
	
	memset( buf, 'z', sizeof( buf ) );
	err = TextToMACAddress( "001122334455@", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x11\x22\x33\x44\x55z", 7 ) == 0, exit, err = kOverrunErr );
	
	memset( buf, 'z', sizeof( buf ) );
	err = TextToMACAddress( "00  11-22:33 - 4 : 5", kSizeCString, buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x11\x22\x33\x04\x05z", 7 ) == 0, exit, err = kOverrunErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA:BB:CC@00:11:22", kSizeCString, buf );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA:BB:CC:00:11:", kSizeCString, buf );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );

	memset( buf, 0xBB, sizeof( buf ) );
	err = TextToMACAddress( "AA:BB:CC:00:11", kSizeCString, buf );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( buf[ 6 ] == 0xBB, exit, err = kOverrunErr );
	
	// TextToHardwareAddressScalar
	
	require_action( TextToMACAddressScalar( "AA:BB:CC:00:11:22", kSizeCString ) == UINT64_C( 0x00AABBCC001122 ), exit, err = -1 );
	require_action( TextToMACAddressScalar( "", kSizeCString ) == 0, exit, err = -1 );
	
	// HardwareAddressToCString
	
	memset( str, 'Z', sizeof( str ) );
	buf[ 0 ] = 1; buf[ 1 ] = 2; buf[ 2 ] = 3; buf[ 3 ] = 4; buf[ 4 ] = 5; buf[ 5 ] = 6;  buf[ 6 ] = 7;  buf[ 7 ] = 8;
	p = HardwareAddressToCString( buf, 8, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "01:02:03:04:05:06:07:08" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	buf[ 0 ] = 0x01; buf[ 1 ] = 0x12; buf[ 2 ] = 0x23; buf[ 3 ] = 0xAB; buf[ 4 ] = 0xBC; buf[ 5 ] = 0xEF; buf[ 6 ] = 0x11; buf[ 7 ] = 0x22;
	p = HardwareAddressToCString( buf, 8, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "01:12:23:AB:BC:EF:11:22" ) == 0, exit, err = kResponseErr );
	
	// MACAddressToCString
	
	memset( str, 'Z', sizeof( str ) );
	buf[ 0 ] = 1; buf[ 1 ] = 2; buf[ 2 ] = 3; buf[ 3 ] = 4; buf[ 4 ] = 5; buf[ 5 ] = 6;
	p = MACAddressToCString( buf, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "01:02:03:04:05:06" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	buf[ 0 ] = 0x01; buf[ 1 ] = 0x12; buf[ 2 ] = 0x23; buf[ 3 ] = 0xAB; buf[ 4 ] = 0xBC; buf[ 5 ] = 0xEF;
	p = MACAddressToCString( buf, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "01:12:23:AB:BC:EF" ) == 0, exit, err = kResponseErr );
	
	// TextToNumVersion
	
	err = TextToNumVersion( "1.2.3", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x01238000, exit, err = kResponseErr );
	
	err = TextToNumVersion( "1.2.3b4", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x01236004, exit, err = kResponseErr );
	
	err = TextToNumVersion( "1.0", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x01008000, exit, err = kResponseErr );
	
	err = TextToNumVersion( "1.0b4", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x01006004, exit, err = kResponseErr );
	
	err = TextToNumVersion( "6.2.basebinary", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x06208000, exit, err = kResponseErr );
	
	err = TextToNumVersion( "7.1auto20070325T0400-M28", kSizeCString, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0x07108000, exit, err = kResponseErr );
	
	// NumVersionToCString
	
	memset( str, 'Z', sizeof( str ) );
	p = NumVersionToCString( 0x01236004, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "1.2.3b4" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = NumVersionToCString( 0x01238000, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "1.2.3" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = NumVersionToCString( 0x01008000, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "1.0" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = NumVersionToCString( 0x01006004, str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "1.0b4" ) == 0, exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = NumVersionToCString( UINT32_C( 0xFFFF20FF ), str );
	require_action( str == p, exit, err = kResponseErr );
	require_action( strcmp( str, "255.15.15d255" ) == 0, exit, err = kResponseErr );
	require_action( str[ 14 ] == 'Z', exit, err = kResponseErr );
	
	// TextToSourceVersion/SourceVersionToCString
	
	require_action( TextToSourceVersion( "1",				kSizeCString ) ==      10000, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "1.2",				kSizeCString ) ==      10200, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "1.2.3",			kSizeCString ) ==      10203, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "731.0.99",		kSizeCString ) ==    7310099, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "214747.99.99",	kSizeCString ) == 2147479999, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "214748.99.99",	kSizeCString ) ==          0, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "214747.100.99",	kSizeCString ) ==          0, exit, err = kResponseErr );
	require_action( TextToSourceVersion( "214747.99.100",	kSizeCString ) ==          0, exit, err = kResponseErr );
	
	require_action( strcmp( SourceVersionToCString(      10000, str ), "1" )			== 0, exit, err = kResponseErr );
	require_action( strcmp( SourceVersionToCString(      10200, str ), "1.2" )			== 0, exit, err = kResponseErr );
	require_action( strcmp( SourceVersionToCString(      10203, str ), "1.2.3" )		== 0, exit, err = kResponseErr );
	require_action( strcmp( SourceVersionToCString(    7310099, str ), "731.0.99" )		== 0, exit, err = kResponseErr );
	require_action( strcmp( SourceVersionToCString( 2147479999, str ), "214747.99.99" )	== 0, exit, err = kResponseErr );
	
	// HexToData
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "00BF22";
	err = HexToData( p, kSizeCString, kHexToData_NoFlags, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( buf[ 3 ] == 0xBB, exit, err = kResponseErr );
	require_action( writtenBytes == 3, exit, err = kResponseErr );
	require_action( totalBytes == 3, exit, err = kResponseErr );
	require_action( memcmp( buf, "\x00\xBF\x22", 3 ) == 0, exit, err = kResponseErr );
	require_action( q == ( p + 6 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "   00bF22  ";
	err = HexToData( p, kSizeCString, kHexToData_NoFlags, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( buf[ 3 ] == 0xBB, exit, err = kResponseErr );
	require_action( writtenBytes == 3, exit, err = kResponseErr );
	require_action( totalBytes == 3, exit, err = kResponseErr );
	require_action( memcmp( buf, "\x00\xBF\x22", 3 ) == 0, exit, err = kResponseErr );
	require_action( q == ( p + 9 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "  00 bf 22  ";
	err = HexToData( p, kSizeCString, kHexToData_NoFlags, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require( err == kFormatErr, exit );
	require_action( memcmp( buf, "\x00\xbb\xbb\xbb\xbb", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 1, exit, err = kResponseErr );
	require_action( totalBytes == 1, exit, err = kResponseErr );
	require_action( q == ( p + 4 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "  00 bf 22  ";
	err = HexToData( p, kSizeCString, kHexToData_IgnoreWhitespace, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\xbf\x22\xbb\xbb", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 3, exit, err = kResponseErr );
	require_action( totalBytes == 3, exit, err = kResponseErr );
	require_action( q == ( p + 12 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "00bf2";
	err = HexToData( p, kSizeCString, kHexToData_WholeBytes, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\xbf\xbb\xbb\xbb", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 2, exit, err = kResponseErr );
	require_action( totalBytes == 2, exit, err = kResponseErr );
	require_action( q == ( p + 5 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "00bf2";
	err = HexToData( p, kSizeCString, kHexToData_NoFlags, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\xbf\x20\xbb\xbb", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 3, exit, err = kResponseErr );
	require_action( totalBytes == 3, exit, err = kResponseErr );
	require_action( q == ( p + 5 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "00bf23";
	err = HexToData( p, kSizeCString, kHexToData_NoFlags, buf, 2, &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\xbf\xbb\xbb\xbb", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 2, exit, err = kResponseErr );
	require_action( totalBytes == 3, exit, err = kResponseErr );
	require_action( q == ( p + 6 ), exit, err = kResponseErr );
	
	memset( buf, 0xBB, sizeof( buf ) );
	p = "0x12, 0x05 0x00-0xab\tA5";
	u32 = kHexToData_IgnoreWhitespace | kHexToData_IgnoreDelimiters | kHexToData_IgnorePrefixes;
	err = HexToData( p, kSizeCString, u32, buf, sizeof( buf ), &writtenBytes, &totalBytes, &q );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x12\x05\x00\xab\xA5", 5 ) == 0, exit, err = kResponseErr );
	require_action( writtenBytes == 5, exit, err = kResponseErr );
	require_action( totalBytes == 5, exit, err = kResponseErr );
	require_action( q == ( p + strlen( p ) ), exit, err = kResponseErr );
	
	// DataToHexCString
	
	memset( str, 'Z', sizeof( str ) );
	p = DataToHexCString( "\x00\xBF\x22", 3, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "00bf22" ) == 0, exit, err = kResponseErr );
	require_action( str[ 7 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = DataToHexCString( "", 0, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "" ) == 0, exit, err = kResponseErr );
	require_action( str[ 1 ] == 'Z', exit, err = kResponseErr );
	
	// TextToInt32 - Decimal
	
	require_action( TextToInt32( "1234", kSizeCString, 10 ) == 1234, exit, err = kResponseErr );
	require_action( TextToInt32( "1,234", kSizeCString, 10 ) == 1234, exit, err = kResponseErr );
	require_action( TextToInt32( "-123", kSizeCString, 10 ) == -123, exit, err = kResponseErr );
	
	// TextToInt32 - Hex
	
	require_action( TextToInt32( "123A", kSizeCString, 16 ) == 0x123A, exit, err = kResponseErr );
	require_action( TextToInt32( "0x123A", kSizeCString, 16 ) == 0x123A, exit, err = kResponseErr );
	require_action( TextToInt32( "0x123A", kSizeCString, 10 ) == 0x123A, exit, err = kResponseErr );
	require_action( TextToInt32( "0x123A", kSizeCString, 0 ) == 0x123A, exit, err = kResponseErr );
		
	// TextToInt32 - Octal
	
	require_action( TextToInt32( "123", kSizeCString, 8 ) == 0123, exit, err = kResponseErr );
	require_action( TextToInt32( "0123", kSizeCString, 8 ) == 0123, exit, err = kResponseErr );
	require_action( TextToInt32( "0123", kSizeCString, 0 ) == 0123, exit, err = kResponseErr );
	
	// TextToInt32 - Binary
	
	require_action( TextToInt32( "1001001", kSizeCString, 2 ) == 73, exit, err = kResponseErr );
	require_action( TextToInt32( "0b1001001", kSizeCString, 2 ) == 73, exit, err = kResponseErr );
	require_action( TextToInt32( "0b1001001", kSizeCString, 10 ) == 73, exit, err = kResponseErr );
	require_action( TextToInt32( "0b1001001", kSizeCString, 0 ) == 73, exit, err = kResponseErr );
	
	// TextToInt32 - Edge cases
	
	require_action( TextToInt32( "+123", kSizeCString, 10 ) == +123, exit, err = kResponseErr );
	require_action( TextToInt32( " +123 ", kSizeCString, 10 ) == +123, exit, err = kResponseErr );
	require_action( TextToInt32( "0", kSizeCString, 10 ) == 0, exit, err = kResponseErr );
	require_action( TextToInt32( "-0", kSizeCString, 10 ) == 0, exit, err = kResponseErr );
	require_action( TextToInt32( "-1", kSizeCString, 10 ) == -1, exit, err = kResponseErr );
	require_action( TextToInt32( "", kSizeCString, 10 ) == 0, exit, err = kResponseErr );
	require_action( TextToInt32( "abcde", kSizeCString, 10 ) == 0, exit, err = kResponseErr );
	require_action( TextToInt32( "abcde", kSizeCString, 16 ) == 0xABCDE, exit, err = kResponseErr );
	require_action( TextToInt32( "-2147483648", kSizeCString, 10 ) == ( -2147483647 - 1 ), exit, err = kResponseErr );
	require_action( ( (uint32_t) TextToInt32( "4294967295", kSizeCString, 10 ) ) == 4294967295U, exit, err = kResponseErr );
	
	// NormalizeUUIDString
	
	err = NormalizeUUIDString( "a5", kSizeCString, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "a5" ) == 0, exit, err = -1 );
	
	err = NormalizeUUIDString( "a5", kSizeCString, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlags_None, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "000000a5-0000-1000-8000-00805f9b34fb" ) == 0, exit, err = -1 );
	
	err = NormalizeUUIDString( "000000a5-0000-1000-8000-00805f9b34fb", kSizeCString, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "a5" ) == 0, exit, err = -1 );
	
	err = NormalizeUUIDString( "000000a5-0000-1000-8000-00805f9b34fb", kSizeCString, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlags_None, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "000000a5-0000-1000-8000-00805f9b34fb" ) == 0, exit, err = -1 );
	
	err = NormalizeUUIDString( "6ba7b810-9dad-11d1-80b4-00c04fd430c8", kSizeCString, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "6ba7b810-9dad-11d1-80b4-00c04fd430c8" ) == 0, exit, err = -1 );
	
	err = NormalizeUUIDString( "6ba7b810-9dad-11d1-80b4-00c04fd430c8", kSizeCString, NULL, kUUIDFlag_ShortForm, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "6ba7b810-9dad-11d1-80b4-00c04fd430c8" ) == 0, exit, err = -1 );
	
	// UUIDtoCString
	
	memset( str, 'Z', sizeof( str ) );
	p = UUIDtoCString( "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 0, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "6ba7b810-9dad-11d1-80b4-00c04fd430c8" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	p = UUIDtoCString( "\x10\xb8\xa7\x6b\xad\x9d\xd1\x11\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 1, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "6ba7b810-9dad-11d1-80b4-00c04fd430c8" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringEx( "\xA5", 1, false, a, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "000000a5-0000-1000-8000-00805f9b34fb" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringEx( "\x28\xFF", 2, false, a, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "000028ff-0000-1000-8000-00805f9b34fb" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringEx( "\x12\x34\x56\x78", 4, false, a, str );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "12345678-0000-1000-8000-00805f9b34fb" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	err = -1;
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringFlags( "\xAB", 1, a, kUUIDFlag_ShortForm, str, &err );
	require_noerr( err, exit );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "ab" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	err = -1;
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringFlags( "\x00\x00\x12\x34\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16, a, 
		kUUIDFlag_ShortForm, str, &err );
	require_noerr( err, exit );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "1234" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	err = -1;
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringFlags( "\xFF\xFF\xFF\xFF\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16, a, 
		kUUIDFlag_ShortForm, str, &err );
	require_noerr( err, exit );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "ffffffff" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	err = -1;
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringFlags( "\xFF\xFF\xFF\xFF\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFC", 16, a, 
		kUUIDFlag_ShortForm, str, &err );
	require_noerr( err, exit );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "ffffffff-0000-1000-8000-00805f9b34fc" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	err = -1;
	memset( str, 'Z', sizeof( str ) );
	a = ( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" );
	p = UUIDtoCStringFlags( "\x00\x00\x12\x34\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB\x00", 17, a, 
		kUUIDFlag_ShortForm, str, &err );
	require_action( err != kNoErr, exit, err = kResponseErr );
	require_action( p == str, exit, err = kResponseErr );
	require_action( strcmp( str, "00000000-0000-0000-0000-000000000000" ) == 0, exit, err = kResponseErr );
	require_action( *( str + ( strlen( str ) + 1 ) ) == 'Z', exit, err = kResponseErr );
	
	// StringToUUID
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUID( "6ba7b810-9dad-11d1-80b4-00c04fd430c8", kSizeCString, 0, buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUID( "6ba7b810-9dad-11d1-80b4-00c04fd430c8", kSizeCString, 1, buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x10\xb8\xa7\x6b\xad\x9d\xd1\x11\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUID( "6ba7b81-9dad-11d1-80b4-00c04fd430c8", kSizeCString, 0, buf );
	require_action( err, exit, err = kResponseErr );
	require_action( buf[ 0 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUID( "6ba7b810-9dad-11d1-80b4-00c04fd430cy", kSizeCString, 0, buf );
	require_action( err, exit, err = kResponseErr );
	require_action( buf[ 0 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUID( "6ba7b81y-9dad-11d1-80b4-00c04fd430c8", kSizeCString, 0, buf );
	require_action( err, exit, err = kResponseErr );
	require_action( buf[ 0 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUIDEx( "5", kSizeCString, 0, 
		( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\x05\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUIDEx( "12", kSizeCString, 0, 
		( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\x12\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUIDEx( "A5", kSizeCString, 0, 
		( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\xA5\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUIDEx( "28FF", kSizeCString, 0, 
		( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x28\xFF\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	err = StringToUUIDEx( "12345678", kSizeCString, 0, 
		( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x12\x34\x56\x78\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = kResponseErr );
	require_action( buf[ 16 ] == 'Z', exit, err = kResponseErr );
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsANSICExtensionsTest
//===========================================================================================================================

OSStatus	StringUtilsANSICExtensionsTest( void );
OSStatus	StringUtilsANSICExtensionsTest( void )
{
	OSStatus			err;
	char *				s;
	char *				e;
	char				str[ 256 ];
	size_t				n;
	const char *		p;
	
	// snprintf_add
	
	s = str;
	e = str + 10;
	err = snprintf_add( &s, e, "12345" );
	require_noerr( err, exit );
	require_action( strcmp( str, "12345" ) == 0, exit, err = -1 );
	
	err = snprintf_add( &s, e, "67890" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( str, "123456789" ) == 0, exit, err = -1 );
	
	err = snprintf_add( &s, e, "ABCDE" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( str, "123456789" ) == 0, exit, err = -1 );
	
	
	s = str;
	e = str + 10;
	err = snprintf_add( &s, e, "12345" );
	require_noerr( err, exit );
	require_action( strcmp( str, "12345" ) == 0, exit, err = -1 );
	
	err = snprintf_add( &s, e, "6789" );
	require_noerr( err, exit );
	require_action( strcmp( str, "123456789" ) == 0, exit, err = -1 );
	
	err = snprintf_add( &s, e, "ABCDE" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( str, "123456789" ) == 0, exit, err = -1 );
	
	
	strlcpy( str, "abc", sizeof( str ) );
	s = str;
	err = snprintf_add( &s, str, "12345" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( str, "abc" ) == 0, exit, err = -1 );
	
	// strdup
	
	s = strdup( "test" );
	require_action( s, exit, err = kResponseErr );
	require_action( strcmp( s, "test" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = strndup( "test", 3 );
	require_action( s, exit, err = kResponseErr );
	require_action( strcmp( s, "tes" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = strndup( "test\0zzz", 8 );
	require_action( s, exit, err = kResponseErr );
	require_action( strcmp( s, "test" ) == 0, exit, err = kResponseErr );
	free( s );
	
	// stricmp
	
	require_action( stricmp( "test", "test" ) == 0, exit, err = kResponseErr );
	require_action( stricmp( "test", "tes" ) > 0, exit, err = kResponseErr );
	require_action( stricmp( "test", "tests" ) < 0, exit, err = kResponseErr );
	require_action( stricmp( "test", "abcd" ) > 0, exit, err = kResponseErr );
	require_action( stricmp( "abcd", "test" ) < 0, exit, err = kResponseErr );
	require_action( stricmp( "AbCd", "abcd" ) == 0, exit, err = kResponseErr );
	require_action( stricmp( "", "abcd" ) < 0, exit, err = kResponseErr );
	require_action( stricmp( "abcd", "" ) > 0, exit, err = kResponseErr );
	require_action( stricmp( "AbCd", "abcde" ) < 0, exit, err = kResponseErr );
	require_action( stricmp( "abcde", "AbCd" ) > 0, exit, err = kResponseErr );
	
	// strnicmp
	
	require_action( strnicmp( "test", "test", 5 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "test", "test", 4 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "test", "test", 3 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "test", "test", 0 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "", "", 0 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "test", "tests", 4 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "tests", "test", 4 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "tests", "test", 5 ) > 0, exit, err = kResponseErr );
	require_action( strnicmp( "test", "tests", 5 ) < 0, exit, err = kResponseErr );
	require_action( strnicmp( "abcd", "ABCD", 4 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "ABCD", "abcd", 4 ) == 0, exit, err = kResponseErr );
	require_action( strnicmp( "ABcd", "ab--", 2 ) == 0, exit, err = kResponseErr );
	
	// strcmp_prefix/stricmp_prefix/strncmp_prefix/strnicmp_prefix/strnicmp_suffix
	
	require_action( strcmp_prefix( "testing", "test" ) == 0, exit, err = kResponseErr );
	require_action( strcmp_prefix( "test", "testing" ) != 0, exit, err = kResponseErr );
	require_action( strcmp_prefix( "TESTING", "test" ) != 0, exit, err = kResponseErr );
	
	require_action( stricmp_prefix( "testing", "test" ) == 0, exit, err = kResponseErr );
	require_action( stricmp_prefix( "test", "testing" ) != 0, exit, err = kResponseErr );
	require_action( stricmp_prefix( "TESTING", "test" ) == 0, exit, err = kResponseErr );
	
	require_action( strnicmp_prefix( "test", 4, "test" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "test", 5, "test" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "tester", 6, "test" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "test", 3, "test" ) < 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "/System/Library/", 16, "/System" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "/usr", 10, "/System" ) != 0, exit, err = kResponseErr );
	
	require_action( strnicmp_prefix( "test", 4, "TEST" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "TEST", 5, "test" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "tester", 6, "TeSt" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "test", 3, "test" ) < 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "/system/library/", 16, "/System" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_prefix( "/usr", 10, "/system" ) != 0, exit, err = kResponseErr );
	
	require_action( strcmp_suffix( "http://www.apple.com/index.html", "index.html" ) == 0, exit, err = kResponseErr );
	require_action( strcmp_suffix( "http://www.apple.com/index.html", "INDEX.html" ) != 0, exit, err = kResponseErr );
	require_action( strcmp_suffix( "index.html", "http://www.apple.com/index.html" ) != 0, exit, err = kResponseErr );
	require_action( strcmp_suffix( "index.html", "index.html" ) == 0, exit, err = kResponseErr );
	
	require_action( strncmp_suffix( "http://www.apple.com/index.html", 31, "index.html" ) == 0, exit, err = kResponseErr );
	require_action( strncmp_suffix( "http://www.apple.com/index.html", 31, "index.HTML" ) != 0, exit, err = kResponseErr );
	require_action( strncmp_suffix( "index.html", 10, "index.html" ) == 0, exit, err = kResponseErr );
	
	require_action( stricmp_suffix( "http://www.apple.com/index.html", "index.html" ) == 0, exit, err = kResponseErr );
	require_action( stricmp_suffix( "*", "INDEX.html" ) != 0, exit, err = kResponseErr );
	require_action( stricmp_suffix( "index.html", "http://www.apple.com/index.html" ) != 0, exit, err = kResponseErr );
	require_action( stricmp_suffix( "index.html", "index.html" ) == 0, exit, err = kResponseErr );
	
	require_action( strnicmp_suffix( "http://www.apple.com/index.html", 31, "index.html" ) == 0, exit, err = kResponseErr );
	require_action( strnicmp_suffix( "*", 1, "INDEX.html" ) != 0, exit, err = kResponseErr );
	require_action( strnicmp_suffix( "index.html", 10, "index.html" ) == 0, exit, err = kResponseErr );
	
	// strncmpx
	
	require_action( strnicmpx( "test", 3, "tEs" ) == 0, exit, err = kResponseErr );
	require_action( strnicmpx( "tes", 3, "TEST" ) < 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "TEsT" ) == 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "Testing" ) < 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "teS" ) > 0, exit, err = kResponseErr );
	require_action( strnicmpx( "", 0, "TES" ) < 0, exit, err = kResponseErr );
	
	// strnicmpx
	
	require_action( strnicmpx( "test", 3, "tes" ) == 0, exit, err = kResponseErr );
	require_action( strnicmpx( "tes", 3, "test" ) < 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "test" ) == 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "testing" ) < 0, exit, err = kResponseErr );
	require_action( strnicmpx( "test", 4, "tes" ) > 0, exit, err = kResponseErr );
	require_action( strnicmpx( "", 0, "tes" ) < 0, exit, err = kResponseErr );
	
	// strnlen
	
	require_action( strnlen( "test", 5 ) == 4, exit, err = kResponseErr );
	require_action( strnlen( "test", 4 ) == 4, exit, err = kResponseErr );
	require_action( strnlen( "test", 3 ) == 3, exit, err = kResponseErr );
	require_action( strnlen( "test", 0 ) == 0, exit, err = kResponseErr );
	require_action( strnlen( "", 1 ) == 0, exit, err = kResponseErr );
	require_action( strnlen( "", 0 ) == 0, exit, err = kResponseErr );
		
	// stristr
	
	require_action( stristr( "abc123xyz", "AbC" ), exit, err = kResponseErr );
	require_action( stristr( "abc123xyz", "123" ), exit, err = kResponseErr );
	require_action( stristr( "abc123xyz", "XYZ" ), exit, err = kResponseErr );
	require_action( !stristr( "abc123xyz", "abcd" ), exit, err = kResponseErr );
	require_action( stristr( "abc123xyz", "" ), exit, err = kResponseErr );
	require_action( stristr( "", "" ), exit, err = kResponseErr );
	require_action( !stristr( "", "a" ), exit, err = kResponseErr );
	
	// strnstr
	
	p = "abc123xyz";
	require_action( strnstr( p, "abc", 9 ) == &p[ 0 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strnstr( p, "bc", 9 ) == &p[ 1 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strnstr( p, "123", 6 ) == &p[ 3 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strnstr( p, "abc", 3 ) == &p[ 0 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strnstr( p, "abc", 2 ) == NULL, exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strnstr( p, "23x", 7 ) == &p[ 4 ], exit, err = kResponseErr );
	
	// strncasestr
	
	p = "abc123xyz";
	require_action( strncasestr( p, "ABC", 9 ) == &p[ 0 ], exit, err = kResponseErr );
	
	p = "aBc123xyz";
	require_action( strncasestr( p, "bc", 9 ) == &p[ 1 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strncasestr( p, "123", 6 ) == &p[ 3 ], exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strncasestr( p, "abc", 3 ) == &p[ 0 ], exit, err = kResponseErr );
	
	p = "ABc123xyz";
	require_action( strncasestr( p, "ABC", 2 ) == NULL, exit, err = kResponseErr );
	
	p = "abc123xyz";
	require_action( strncasestr( p, "23X", 7 ) == &p[ 4 ], exit, err = kResponseErr );
	
	// strlcat
	
	memset( str, 'Z', sizeof( str ) );
	strlcpy( str, "test", sizeof( str ) );
	n = strlcat( str, "abc", 10 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "testabc" ) == 0, exit, err = kResponseErr );
	require_action( str[ 8 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	strlcpy( str, "test", sizeof( str ) );
	n = strlcat( str, "abc", 6 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "testa" ) == 0, exit, err = kResponseErr );
	require_action( str[ 6 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	strlcpy( str, "test", sizeof( str ) );
	n = strlcat( str, "abc", 5 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "test" ) == 0, exit, err = kResponseErr );
	require_action( str[ 5 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	strlcpy( str, "test", sizeof( str ) );
	n = strlcat( str, "abc", 0 );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( strcmp( str, "test" ) == 0, exit, err = kResponseErr );
	require_action( str[ 5 ] == 'Z', exit, err = kResponseErr );
	
	// strlcpy
	
	memset( str, 'Z', sizeof( str ) );
	n = strlcpy( str, "testing", 8 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "testing" ) == 0, exit, err = kResponseErr );
	require_action( str[ 8 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	n = strlcpy( str, "testing", 3 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "te" ) == 0, exit, err = kResponseErr );
	require_action( str[ 3 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	n = strlcpy( str, "testing", 1 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( strcmp( str, "" ) == 0, exit, err = kResponseErr );
	require_action( str[ 2 ] == 'Z', exit, err = kResponseErr );
	
	memset( str, 'Z', sizeof( str ) );
	n = strlcpy( str, "testing", 0 );
	require_action( n == 7, exit, err = kResponseErr );
	require_action( str[ 0 ] == 'Z', exit, err = kResponseErr );
	
	// strnspn
	
	require_action( strnspn( "abcd1a2", kSizeCString, "abcdefghi" ) == 4, exit, err = kResponseErr );
	require_action( strnspn( "abcd1a2", 3, "abcdefghi" ) == 3, exit, err = kResponseErr );
	require_action( strnspn( "1234abcd", kSizeCString, "1234abcd" ) == 8, exit, err = kResponseErr );
	
	require_action( strnspnx( "aaa1122", kSizeCString, "abcde1234" ), exit, err = kResponseErr );
	require_action( strnspnx( "abcd1a2", 3, "abcdefghi" ), exit, err = kResponseErr );
	require_action( strnspnx( "1234abcd", kSizeCString, "1234abcd" ), exit, err = kResponseErr );
	
	// strsep_compat
	
	*str = '\0';
	s = str;
	p = strsep_compat( &s, "," );
	require_action( p && ( *p == '\0' ), exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	
	strlcpy( str, ",", sizeof( str ) );
	s = str;
	p = strsep_compat( &s, "," );
	require_action( p && ( *p == '\0' ), exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( p && ( *p == '\0' ), exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	
	strlcpy( str, "token1", sizeof( str ) );
	s = str;
	p = strsep_compat( &s, "," );
	require_action( p, exit, err = -1 );
	require_action( strnlen( p, 7 ) == 6, exit, err = -1 );
	require_action( strcmp( p, "token1" ) == 0, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	
	strlcpy( str, "token1,tok2", sizeof( str ) );
	s = str;
	p = strsep_compat( &s, "," );
	require_action( p, exit, err = -1 );
	require_action( strnlen( p, 7 ) == 6, exit, err = -1 );
	require_action( strcmp( p, "token1" ) == 0, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( p, exit, err = -1 );
	require_action( strnlen( p, 5 ) == 4, exit, err = -1 );
	require_action( strcmp( p, "tok2" ) == 0, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	
	strlcpy( str, "aaa,bbbb,", sizeof( str ) );
	s = str;
	p = strsep_compat( &s, "," );
	require_action( p, exit, err = -1 );
	require_action( strnlen( p, 4 ) == 3, exit, err = -1 );
	require_action( strcmp( p, "aaa" ) == 0, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( p, exit, err = -1 );
	require_action( strnlen( p, 5 ) == 4, exit, err = -1 );
	require_action( strcmp( p, "bbbb" ) == 0, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( p && ( *p == '\0' ), exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	p = strsep_compat( &s, "," );
	require_action( !p, exit, err = -1 );
	
	// memrchr
	
	p = "file.txt";
	n = strlen( p );
	s = (char *) memrchr( p, '.', n );
	require_action( s, exit, err = kResponseErr );
	require_action( strcmp( s, ".txt" ) == 0, exit, err = kResponseErr );
	
	p = "file.txt";
	n = strlen( p );
	s = (char *) memrchr( p, ',', n );
	require_action( !s, exit, err = kResponseErr );
	
	p = "file.txt.txt";
	n = strlen( p );
	s = (char *) memrchr( p, '.', n );
	require_action( s, exit, err = kResponseErr );
	require_action( strcmp( s, ".txt" ) == 0, exit, err = kResponseErr );
	
	p = "";
	n = strlen( p );
	s = (char *) memrchr( p, ',', n );
	require_action( !s, exit, err = kResponseErr );
	
	// memrlen
	
	require_action( memrlen( NULL, 0 ) == 0, exit, err = -1 );
	require_action( memrlen( "\x00\x00\x00", 3 ) == 0, exit, err = -1 );
	require_action( memrlen( "\x01\x00\x00", 3 ) == 1, exit, err = -1 );
	require_action( memrlen( "\x00\x01\x00", 3 ) == 2, exit, err = -1 );
	require_action( memrlen( "\x00\x00\x03", 3 ) == 3, exit, err = -1 );
	require_action( memrlen( "\x11\x00\x10", 3 ) == 3, exit, err = -1 );
	require_action( memrlen( "\x01\x00\x00\x01", 4 ) == 4, exit, err = -1 );
	
	// tolowerstr
	
	require_action( strcmp( tolowerstr( "AbCdEfGhI", str, sizeof( str ) ), "abcdefghi" ) == 0, exit, err = -1 );
	require_action( strcmp( tolowerstr( "abcdefghi", str, sizeof( str ) ), "abcdefghi" ) == 0, exit, err = -1 );
	require_action( strcmp( tolowerstr( "This Is A Test", str, sizeof( str ) ), "this is a test" ) == 0, exit, err = -1 );
	require_action( strcmp( tolowerstr( "!TEST!", str, sizeof( str ) ), "!test!" ) == 0, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsINITest
//===========================================================================================================================

// INI file example from Wikipedia.
static const char * const		kStringUtilsINITest1 = 
			"; last modified 1 April 2001 by John Doe\n"
/* 1 */		"[owner]\n"
/* 2 */		"name=John Doe\n"
/* 3 */		"organization=Acme Widgets Inc.\n"
			" \n"
/* 4 */		"[database]\n"
			"; use IP address in case network name resolution is not working\n"
/* 5 */		"server=192.0.2.62     \n"
/* 6 */		"port=143\n"
/* 7 */		"file=\"payroll.dat\"\n";

// Test file containing each variation of formatting.
static const char * const		kStringUtilsINITest2 = 
			"# Comment\n"
/* 1 */		"[ section1 ]\r\n"
/* 2 */		"name2=value2\n"
/* 3 */		"name3 =value3\r\n"
/* 4 */		"name4= value4\n"
/* 5 */		"name5 = value5\n"
/* 6 */		"name6 = value6\r"
/* 7 */		" \tname7 = value7\r"
/* 8 */		"name8 = value8 and more\r"
/* 9 */		"name-9 = \"value9 and more\"\r"
			" \n"
/* 10 */	"[ section10 \"section value 10\" ]\n"
			"# Comment\n"
/* 11 */	"name11 = value11\n";

OSStatus	StringUtilsINITest( void );
OSStatus	StringUtilsINITest( void )
{
	OSStatus			err;
	int					i;
	uint32_t			flags;
	const char *		src;
	const char *		end;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	
	// Test 1
	
	src = kStringUtilsINITest1;
	end = src + strlen( src );
	for( i = 1; INIGetNext( src, end, &flags, &namePtr, &nameLen, &valuePtr, &valueLen, &src ); ++i )
	{
		switch( i )
		{
			case 1:
				require_action( flags == kINIFlag_Section, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "owner" ) == 0, exit, err = -1 );
				require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
				break;
			
			case 2:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "John Doe" ) == 0, exit, err = -1 );
				break;
			
			case 3:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "organization" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "Acme Widgets Inc." ) == 0, exit, err = -1 );
				break;
			
			case 4:
				require_action( flags == kINIFlag_Section, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "database" ) == 0, exit, err = -1 );
				require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
				break;
			
			case 5:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "server" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "192.0.2.62" ) == 0, exit, err = -1 );
				break;
			
			case 6:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "port" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "143" ) == 0, exit, err = -1 );
				break;
			
			case 7:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "file" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "payroll.dat" ) == 0, exit, err = -1 );
				break;
			
			default:
				break;
		}
	}
	require_action( src == end, exit, err = -1 );
	require_action( i == 8, exit, err = -1 );
	
	src = kStringUtilsINITest1;
	require_action( INIFindSection( src, end, "owner", &valuePtr, &valueLen, &src ), exit, err = -1 );
	require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
	require_action( INIFindSection( src, end, "database", &valuePtr, &valueLen, &src ), exit, err = -1 );
	require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
	require_action( !INIFindSection( src, end, "database", &valuePtr, &valueLen, &src ), exit, err = -1 );
	
	// Test 2
	
	src = kStringUtilsINITest2;
	end = src + strlen( src );
	for( i = 1; INIGetNext( src, end, &flags, &namePtr, &nameLen, &valuePtr, &valueLen, &src ); ++i )
	{
		switch( i )
		{
			case 1:
				require_action( flags == kINIFlag_Section, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "section1" ) == 0, exit, err = -1 );
				require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
				break;
			
			case 2:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name2" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value2" ) == 0, exit, err = -1 );
				break;
			
			case 3:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name3" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value3" ) == 0, exit, err = -1 );
				break;
			
			case 4:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name4" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value4" ) == 0, exit, err = -1 );
				break;
			
			case 5:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name5" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value5" ) == 0, exit, err = -1 );
				break;
			
			case 6:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name6" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value6" ) == 0, exit, err = -1 );
				break;
			
			case 7:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name7" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value7" ) == 0, exit, err = -1 );
				break;
			
			case 8:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name8" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value8 and more" ) == 0, exit, err = -1 );
				break;
			
			case 9:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name-9" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value9 and more" ) == 0, exit, err = -1 );
				break;
			
			case 10:
				require_action( flags == kINIFlag_Section, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "section10" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "section value 10" ) == 0, exit, err = -1 );
				break;
			
			case 11:
				require_action( flags == kINIFlag_Property, exit, err = -1 );
				require_action( strncmpx( namePtr, nameLen, "name11" ) == 0, exit, err = -1 );
				require_action( strncmpx( valuePtr, valueLen, "value11" ) == 0, exit, err = -1 );
				break;
			
			default:
				break;
		}
	}
	require_action( src == end, exit, err = -1 );
	require_action( i == 12, exit, err = -1 );
	
	src = kStringUtilsINITest2;
	require_action( INIFindSection( src, end, "section1", &valuePtr, &valueLen, &src ), exit, err = -1 );
	require_action( ( valuePtr == NULL ) && ( valueLen == 0 ), exit, err = -1 );
	require_action( INIFindSection( src, end, "section10", &valuePtr, &valueLen, &src ), exit, err = -1 );
	require_action( strncmpx( valuePtr, valueLen, "section value 10" ) == 0, exit, err = -1 );
	require_action( !INIFindSection( src, end, "section1", &valuePtr, &valueLen, &src ), exit, err = -1 );
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringUtilsMiscTest
//===========================================================================================================================

OSStatus	StringUtilsMiscTest( void );
OSStatus	StringUtilsMiscTest( void )
{
	OSStatus			err;
	char				str[ 128 ];
	char				old[ 128 ];
	char *				ptr;
	char *				end;
	const uint8_t *		uptr;
	size_t				len;
	uint32_t			bits;
	const char *		match;
	const char *		s;
	const char *		sEnd;
	const char *		token;
	int					argc;
	char **				argv;
	Boolean				good;
	size_t				pairs;
	const char *		ptrs[ 10 ];
	size_t				lens[ 10 ];
	char				name[ 64 ];
	size_t				nameCopiedLen;
	size_t				nameTotalLen;
	char				value[ 256 ];
	size_t				valueCopiedLen;
	size_t				valueTotalLen;
	Boolean				changed;
	int					tempInt, lineNum, i;
	NMEAData			nmeaData;
	double				d, d2;
	
	NMEAInit( &nmeaData );
	
	// BitListString
	
	memset( str, 'Z', sizeof( str ) );
	len = 1234;
	
	ptr = BitListString_Make( 0, str, &len );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, "" ) == 0, exit, err = kResponseErr );
	require_action( len == 0, exit, err = kResponseErr );
	
	ptr = BitListString_Make( 0x800, str, NULL );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, "11" ) == 0, exit, err = kResponseErr );
	
	ptr = BitListString_Make( 0x5, str, &len );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, "0,2" ) == 0, exit, err = kResponseErr );
	require_action( len == 3, exit, err = kResponseErr );
	
	ptr = BitListString_Make( UINT32_C( 0xFFFFFFFF ), str, NULL );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31" ) == 0, 
		exit, err = kResponseErr );
	
	bits = 0x1234;
	err = BitListString_Parse( "", kSizeCString, &bits );
	require_noerr( err, exit );
	require_action( bits == 0, exit, err = kResponseErr );
	
	err = BitListString_Parse( "0,2\0Z", kSizeCString, &bits );
	require_noerr( err, exit );
	require_action( bits == 0x5, exit, err = kResponseErr );
	
	err = BitListString_Parse( "11\0Z", kSizeCString, &bits );
	require_noerr( err, exit );
	require_action( bits == 0x800, exit, err = kResponseErr );
	
	err = BitListString_Parse( "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31", 
		kSizeCString, &bits );
	require_noerr( err, exit );
	require_action( bits == UINT32_C( 0xFFFFFFFF ), exit, err = kResponseErr );
	
	strlcpy( str, "this is a test", sizeof( str ) );
	uptr = BoyerMooreSearch( str, strlen( str ), "abc", 3 );
	require_action( !uptr, exit, err = kResponseErr );
	
	strlcpy( str, "this is a test", sizeof( str ) );
	uptr = BoyerMooreSearch( str, strlen( str ), "test", 4 );
	require_action( uptr, exit, err = kResponseErr );
	require_action( uptr == ( (const uint8_t *) &str[ 10 ] ), exit, err = kResponseErr );
	
	strlcpy( str, "this is a test", sizeof( str ) );
	uptr = BoyerMooreSearch( str, strlen( str ), "this", 4 );
	require_action( uptr, exit, err = kResponseErr );
	require_action( uptr == ( (const uint8_t *) &str[ 0 ] ), exit, err = kResponseErr );
	
	strlcpy( str, "test", sizeof( str ) );
	uptr = BoyerMooreSearch( str, strlen( str ), "test", 4 );
	require_action( uptr, exit, err = kResponseErr );
	require_action( uptr == ( (const uint8_t *) &str[ 0 ] ), exit, err = kResponseErr );
	
	strlcpy( str, "test", sizeof( str ) );
	uptr = BoyerMooreSearch( "abc", 3, "Test", 4 );
	require_action( !uptr, exit, err = kResponseErr );
	
	// CleanseHFSVolumeName
	
	s = "Time Capsule";
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( !changed, exit, err = kResponseErr );
	require_action( strcmp( str, s ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "Time Capsule with a bunch of extra stuff";
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "Time Capsule with a bunch o" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = ".SomeEnglish\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab/\xe3\x81\xa1\xe3\x81\xaf"; // Strip, Convert, and Exactly at the Max Len.
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "SomeEnglish\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab_\xe3\x81\xa1\xe3\x81\xaf" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = ".SomeEnglishAB\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab/\xe3\x81\xa1\xe3\x81\xaf"; // Strip, Convert, and Truncate in middle of UTF-8 character.
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "SomeEnglishAB\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab_\xe3\x81\xa1" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF"; // Japanese UTF-8
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( !changed, exit, err = kResponseErr );
	require_action( strcmp( str, s ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF \xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF"; // Japanese UTF-8 VERY LONG
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF \xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "My \xE3\x81\x93 Disk"; // Japanese UTF-8 with ASCII
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( !changed, exit, err = kResponseErr );
	require_action( strcmp( str, s ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "My\rOther\tBackup\nDisk";
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "My Other Backup Disk" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "My/Other:Disk"; // Colons and slashes
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "My_Other_Disk" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = ".MyPeriodifiedDisk"; // Beginning period
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "MyPeriodifiedDisk" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = "..MyPeriodifiedDisk"; // Beginning periods
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "MyPeriodifiedDisk" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = ".My.Periodified.Disk."; // Beginning period and three more
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "My.Periodified.Disk." ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	s = ".\xE3\x81\x93MyPeriodifiedDisk"; // Japanese UTF-8 and beginning period
	memset( str, 'z', sizeof( str ) );
	changed = CleanseHFSVolumeName( s, kSizeCString, str, sizeof( str ) );
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "\xE3\x81\x93MyPeriodifiedDisk" ) == 0, exit, err = kResponseErr );
	require_action( str[ strlen( str ) + 1 ] == 'z', exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	require_action( StringIsAFPVolumeNameSafe( "Time Capsule" ), exit, err = kResponseErr );
	require_action( !StringIsAFPVolumeNameSafe( "Time Capsule with a bunch of extra stuff" ), exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( "My \xE3\x81\x93 Disk" ), exit, err = kResponseErr );
	require_action( !StringIsAFPVolumeNameSafe( "My\rOther\tBackup\nDisk" ), exit, err = kResponseErr );
	require_action( !StringIsAFPVolumeNameSafe( "My/Other:Disk" ), exit, err = kResponseErr );
	
	strlcpy( str, "My\rOther\tBackup\nDisk", sizeof( str ) );
	changed = CleanseHFSVolumeName( str, kSizeCString, str, sizeof( str ) ); // src == dst
	require_action( changed, exit, err = kResponseErr );
	require_action( strcmp( str, "My Other Backup Disk" ) == 0, exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	strlcpy( str, "Time Capsule", sizeof( str ) );
	changed = CleanseHFSVolumeName( str, kSizeCString, str, sizeof( str ) ); // src == dst
	require_action( !changed, exit, err = kResponseErr );
	require_action( strcmp( str, "Time Capsule" ) == 0, exit, err = kResponseErr );
	require_action( StringIsAFPVolumeNameSafe( str ), exit, err = kResponseErr );
	
	// GetLastFilePathSegment
	
	len = 0;
	s = GetLastFilePathSegment( "a.txt", kSizeCString, &len );
	require_action( strcmp( s, "a.txt" ) == 0, exit, err = -1 );
	require_action( len == 5, exit, err = -1 );
	s = GetLastFilePathSegment( "a.txt", kSizeCString, NULL );
	require_action( strcmp( s, "a.txt" ) == 0, exit, err = -1 );
	
	len = 0;
	s = GetLastFilePathSegment( "a.txt", 5, &len );
	require_action( strcmp( s, "a.txt" ) == 0, exit, err = -1 );
	require_action( len == 5, exit, err = -1 );
	
	s = GetLastFilePathSegment( "a/b/c/d.txt", kSizeCString, NULL );
	require_action( strcmp( s, "d.txt" ) == 0, exit, err = -1 );
	
	len = 0;
	s = GetLastFilePathSegment( "a/b/c/d.txt", 11, &len );
	require_action( strcmp( s, "d.txt" ) == 0, exit, err = -1 );
	require_action( len == 5, exit, err = -1 );
	
	// GetParentPath
	
	err = GetParentPath( "/a/b/c/d.txt", kSizeCString, str, sizeof( str ), &len );
	require_noerr( err, exit );
	require_action( strcmp( str, "/a/b/c" ) == 0, exit, err = -1 );
	require_action( len == strlen( str ), exit, err = -1 );
	
	err = GetParentPath( "a/b/c/", kSizeCString, str, sizeof( str ), &len );
	require_noerr( err, exit );
	require_action( strcmp( str, "a/b/c" ) == 0, exit, err = -1 );
	require_action( len == strlen( str ), exit, err = -1 );
	
	err = GetParentPath( "d.txt", kSizeCString, str, sizeof( str ), &len );
	require_noerr( err, exit );
	require_action( strcmp( str, "" ) == 0, exit, err = -1 );
	require_action( len == strlen( str ), exit, err = -1 );
	
	err = GetParentPath( "/a/b/c/d.txt", kSizeCString, NULL, 0, &len );
	require_noerr( err, exit );
	require_action( len <= strlen( "/a/b/c/d.txt" ), exit, err = -1 );
	require_action( strncmpx( "/a/b/c/d.txt", len, "/a/b/c" ) == 0, exit, err = -1 );
	
	err = GetParentPath( "d.txt", kSizeCString, NULL, 0, &len );
	require_noerr( err, exit );
	require_action( len == 0, exit, err = kResponseErr );
	
	//
	// ParseCommaSeparatedNameValuePair
	//
	
	s = "waMA=00-1C-B3-FE-A9-69,raMA=00-1C-B3-FE-A7-F0,raNm=bb\\\\52\\,dev,syDs=Apple Time Capsule V7.4d6 dev ,"
		"syFl=0x00000A0C,syAP=106,syVs=7.4d6 dev ,srcv=74000.6";
	sEnd = s + strlen( s );
	
	// 1 -- waMA=00-1C-B3-FE-A9-69
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "waMA" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "00-1C-B3-FE-A9-69" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 2 -- raMA=00-1C-B3-FE-A7-F0
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "raMA" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "00-1C-B3-FE-A7-F0" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 3 -- raNm=bb\\52\,dev
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "raNm" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "bb\\52,dev" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 4 -- syDs=Apple Time Capsule V7.4d6 dev 
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "syDs" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "Apple Time Capsule V7.4d6 dev " ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 5 -- syFl=0x00000A0C
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "syFl" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "0x00000A0C" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 5 -- syAP=106
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "syAP" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "106" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 6 -- syVs=7.4d6 dev 
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "syVs" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "7.4d6 dev " ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 7 -- srcv=74000.6
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_noerr( err, exit );
	require_action( strcmp( name, "srcv" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "74000.6" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 8 -- end
	
	name[ 0 ]		= '\0';
	nameCopiedLen	= 0;
	nameTotalLen	= 0;
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	err = ParseCommaSeparatedNameValuePair( s, sEnd, name, sizeof( name ), &nameCopiedLen, &nameTotalLen, 
		value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require( err != kNoErr, exit );
	require_action( strcmp( name, "" ) == 0, exit, err = -1 );
	require_action( nameCopiedLen == strlen( name ), exit, err = -1 );
	require_action( nameTotalLen == nameCopiedLen, exit, err = -1 );
	require_action( strcmp( value, "" ) == 0, exit, err = -1 );
	require_action( valueCopiedLen == strlen( value ), exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	//
	// ParseLine
	//
	
	match = "This is a line";
	s = match;
	sEnd = s + strlen( s );
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( ( len == strlen( match ) ) && ( memcmp( token, match, len ) == 0 ), exit, err = -1 );
	require_action( !ParseLine( s, sEnd, &token, &len, &s ), exit, err = -1 );
	
	s = 
		"This is a line 1\r"
		"This is a line 2\r\n"
		"This is a line 3\n"
		"This is a line 4\r"
		"\r"
		"\n" // Note: will coalapse with previously line because it looks like CRLF.
		"\r\n"
		"\r"
		"This is a line 5\r";
	sEnd = s + strlen( s );
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "This is a line 1" ) == 0, exit, err = -1 );
	
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "This is a line 2" ) == 0, exit, err = -1 );
	
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "This is a line 3" ) == 0, exit, err = -1 );
	
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "This is a line 4" ) == 0, exit, err = -1 );
	
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "" ) == 0, exit, err = -1 );
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "" ) == 0, exit, err = -1 );
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "" ) == 0, exit, err = -1 );
	
	require_action( ParseLine( s, sEnd, &token, &len, &s ),  exit, err = -1 );
	require_action( strnicmpx( token, len, "This is a line 5" ) == 0, exit, err = -1 );
	
	require_action( !ParseLine( s, sEnd, &token, &len, &s ), exit, err = -1 );
	
	//
	// ParseQuotedEscapedString
	//
	
	s = "path=/foo/bar/index.html name1=ab\\x63 'name2=my \\x12 name 2' \"name3=my \\x6eam\\145 3\" name4=ab\\143";
	sEnd = s + strlen( s );
	
	// 0 -- path=/foo/bar/index.html
	
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( good, exit, err = -1 );
	require_action( strncmpx( value, valueCopiedLen, "path=/foo/bar/index.html" ) == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 1 -- name1=ab\\x63
	
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( good, exit, err = -1 );
	require_action( strncmpx( value, valueCopiedLen, "name1=abc" ) == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 2 -- 'name2=my \x12 name 2'
	
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( good, exit, err = -1 );
	require_action( strncmpx( value, valueCopiedLen, "name2=my \\x12 name 2" ) == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 3 -- "name3=my \\x6eam\\145 3"
	
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( good, exit, err = -1 );
	require_action( strncmpx( value, valueCopiedLen, "name3=my name 3" ) == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 4 -- name4=ab\\143
	
	value[ 0 ]		= '\0';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( good, exit, err = -1 );
	require_action( strncmpx( value, valueCopiedLen, "name4=abc" ) == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	
	// 5 -- end
	
	value[ 0 ]		= 'z';
	valueCopiedLen	= 0;
	valueTotalLen	= 0;
	good = ParseQuotedEscapedString( s, sEnd, " ", value, sizeof( value ), &valueCopiedLen, &valueTotalLen, &s );
	require_action( !good, exit, err = -1 );
	require_action( valueTotalLen == 0, exit, err = -1 );
	require_action( valueTotalLen == valueCopiedLen, exit, err = -1 );
	require_action( value[ 0 ] == 'z', exit, err = -1 );
	
	// SplitText
	
	SplitText( "databases/1/browse/artist", NULL, "/", countof( ptrs ), &pairs, ptrs, lens );
	require_action( pairs == 4, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 0 ], lens[ 0 ], "databases" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 1 ], lens[ 1 ], "1" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 2 ], lens[ 2 ], "browse" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 3 ], lens[ 3 ], "artist" ) == 0, exit, err = kResponseErr );
	
	SplitText( "/x/y/z", NULL, "/", countof( ptrs ), &pairs, ptrs, lens );
	require_action( pairs == 4, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 0 ], lens[ 0 ], "" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 1 ], lens[ 1 ], "x" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 2 ], lens[ 2 ], "y" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 3 ], lens[ 3 ], "z" ) == 0, exit, err = kResponseErr );
	
	SplitText( "x", NULL, "/", countof( ptrs ), &pairs, ptrs, lens );
	require_action( pairs == 1, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 0 ], lens[ 0 ], "x" ) == 0, exit, err = kResponseErr );
	
	SplitText( "", NULL, "/", countof( ptrs ), &pairs, ptrs, lens );
	require_action( pairs == 0, exit, err = kResponseErr );
	
	SplitText( "x/y/", NULL, "/", countof( ptrs ), &pairs, ptrs, lens );
	require_action( pairs == 3, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 0 ], lens[ 0 ], "x" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 1 ], lens[ 1 ], "y" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( ptrs[ 2 ], lens[ 2 ], "" ) == 0, exit, err = kResponseErr );
	
	// ReplaceDifferentString
	
	ptr = NULL;
	err = ReplaceDifferentString( &ptr, NULL );
	require_noerr( err, exit );
	require_action( ptr == NULL, exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, "" );
	require_noerr( err, exit );
	require_action( ptr == NULL, exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, "test" );
	require_noerr( err, exit );
	require_action( ptr && ( strcmp( ptr, "test" ) == 0 ), exit, err = kResponseErr );
	
	end = ptr;
	err = ReplaceDifferentString( &ptr, "test" );
	require_noerr( err, exit );
	require_action( ptr && ( ptr == end ) && ( strcmp( ptr, "test" ) == 0 ), exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, "test2" );
	require_noerr( err, exit );
	require_action( ptr && ( strcmp( ptr, "test2" ) == 0 ), exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, "" );
	require_noerr( err, exit );
	require_action( ptr == NULL, exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, "test" );
	require_noerr( err, exit );
	require_action( ptr && ( strcmp( ptr, "test" ) == 0 ), exit, err = kResponseErr );
	
	err = ReplaceDifferentString( &ptr, NULL );
	require_noerr( err, exit );
	require_action( ptr == NULL, exit, err = kResponseErr );
	
	// TextCompareNatural
	
	require_action( TextCompareNatural( "test1.c", 7, "test1.c", 7, false ) == 0, exit, err = kResponseErr );
	require_action( TextCompareNatural( "test1.c", 7, "test2.c", 7, false ) < 0, exit, err = kResponseErr );
	require_action( TextCompareNatural( "test10.c", 7, "test2.c", 7, false ) > 0, exit, err = kResponseErr );
	
	// TextSep 1
	
	s = "TCP,UDP|SCTP/RTP";
	sEnd = s + strlen( s );
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "TCP" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "UDP" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "SCTP" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "RTP" ) == 0, exit, err = kResponseErr );
	
	token = NULL;
	len = 0;
	require_action( s == sEnd, exit, err = kResponseErr );
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( !good, exit, err = kResponseErr );
	require_action( s == sEnd, exit, err = kResponseErr );
	require_action( token == NULL, exit, err = kResponseErr );
	require_action( len == 0, exit, err = kResponseErr );
	
	// TextSep 2
	
	s = "";
	sEnd = s;
	token = NULL;
	len = 0;
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( !good, exit, err = kResponseErr );
	require_action( token == NULL, exit, err = kResponseErr );
	require_action( len == 0, exit, err = kResponseErr );
	require_action( s == sEnd, exit, err = kResponseErr );
	
	// TextSep 3
	
	s = "TCP,,UDP";
	sEnd = s + strlen( s );
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "TCP" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "UDP" ) == 0, exit, err = kResponseErr );
	
	token = NULL;
	len = 0;
	require_action( s == sEnd, exit, err = kResponseErr );
	good = TextSep( s, sEnd, ",|/", &token, &len, &s );
	require_action( !good, exit, err = kResponseErr );
	require_action( s == sEnd, exit, err = kResponseErr );
	require_action( token == NULL, exit, err = kResponseErr );
	require_action( len == 0, exit, err = kResponseErr );
	
	// TextSep 4
	
	s = ",,UDP";
	sEnd = s + strlen( s );
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "" ) == 0, exit, err = kResponseErr );
	
	good = TextSep( s, sEnd, ",", &token, &len, &s );
	require_action( good, exit, err = kResponseErr );
	require_action( strncmpx( token, len, "UDP" ) == 0, exit, err = kResponseErr );
	
	// ConvertUTF8StringToRFC1034LabelString
	
	end = str + sizeof( str );
	
	match = "";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Testing";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "Testing", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Testing";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "-Testing---", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Bobs-Test-String-of-Normal-Length";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "Bob's Test String of Normal Length", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Bobs-Test-String-of-A-Really-Long-Length-That-Should-be-Truncat";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "Bob's Test String of A Really Long Length That Should be Truncated", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Bobs-Test-String-of-A-Really-Long-Length-That-Should-be-Truncat";
	memset( str, 'A', sizeof( str ) );
	strlcpy( str, "Bob's Test String of A Really Long Length That Should be Truncated", sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( str, str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	
	match = "Bobs-Simple-Unicode-Test";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "Bob\xE2\x80\x99s Simple Unicode Test", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	match = "Bobs-Test-String-of-A-Really-Long-Unicode-String-That-Should-be";
	memset( str, 'A', sizeof( str ) );
	ptr = ConvertUTF8StringToRFC1034LabelString( "Bob\xE2\x80\x99s Test String of A Really Long Unicode String That Should be Truncated", str );
	require_action( ptr == str, exit, err = kResponseErr );
	require_action( strcmp( str, match ) == 0, exit, err = kResponseErr );
	for( ptr = str + 64; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	// MapStringToValue
	
	tempInt = MapStringToValue( "blue", -1, 
		"red",   1, 
		"green", 2, 
		"blue",  3, 
		NULL );
	require_action( tempInt == 3, exit, err = -1 );
	
	tempInt = MapStringToValue( "purple", -1, 
		"red",   1, 
		"green", 2, 
		"blue",  3, 
		NULL );
	require_action( tempInt == -1, exit, err = -1 );
	
	// MapValueToString
	
	s = MapValueToString( 3, "<%d unknown>", str, sizeof( str ), 
		"red", 		1, 
		"green", 	2, 
		"blue", 	3, 
		NULL );
	require_action( s && ( strcmp( s, "blue" ) == 0 ), exit, err = kResponseErr );
	
	s = MapValueToString( 8, "<%d unknown>", str, sizeof( str ), 
		"red", 		1, 
		"green", 	2, 
		"blue", 	3, 
		NULL );
	require_action( s && ( strcmp( s, "<8 unknown>" ) == 0 ), exit, err = kResponseErr );
	
	s = MapValueToString( 8, "test %d", NULL, 0, 
		"red", 		1, 
		"green", 	2, 
		"blue", 	3, 
		NULL );
	require_action( s && ( strcmp( s, "test %d" ) == 0 ), exit, err = kResponseErr );
	
	s = MapValueToString( 8, NULL, NULL, 0, 
		"red", 		1, 
		"green", 	2, 
		"blue", 	3, 
		NULL );
	require_action( !s, exit, err = kResponseErr );
	
	// MIMETypeToExtension
	
	require_action( strcmp( MIMETypeToExtension( "image/jpeg" ),	"jpg" ) == 0, exit, err = -1 );
	require_action( strcmp( MIMETypeToExtension( "image/png" ),		"png" ) == 0, exit, err = -1 );
	require_action( strcmp( MIMETypeToExtension( "audio/mpeg" ),	"mp3" ) == 0, exit, err = -1 );
	require_action( strcmp( MIMETypeToExtension( "video/x-m4v" ),	"m4v" ) == 0, exit, err = -1 );
	require_action( strcmp( MIMETypeToExtension( "" ),				"" )	== 0, exit, err = -1 );
	require_action( strcmp( MIMETypeToExtension( "???" ),			"" )	== 0, exit, err = -1 );
	
	// NMEAGenerate
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62", kSizeCString, NULL );
	require_noerr( err, exit );
	err = NMEAGenerate( &nmeaData, str, sizeof( str ) );
	require_noerr( err, exit );
	require_action( strcmp( str, "$GPRMC,081836.000,A,3751.650,S,14507.360,E,0.00,360.00,130998,11.30,E*7C" ) == 0, exit, err = -1 );
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, str, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPRMC,092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A*43", kSizeCString, NULL );
	require_noerr( err, exit );
	err = NMEAGenerate( &nmeaData, str, sizeof( str ) );
	require_noerr( err, exit );
	require_action( strcmp( str, "$GPRMC,092750.000,A,5321.680,N,00630.337,W,0.02,31.66,280511,0.00,,A*5D" ) == 0, exit, err = -1 );
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, str, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	
	// NMEAParse
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPGGA,225833.0,3719.951324,N,12201.808796,W,1,09,0.6,69.8,M,-27.0,M,,*5F", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPGGA ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.time, 10 ) == RoundTo( 82713.0, 10 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.latitude, 1000000 ) == RoundTo( 3719.951324, 1000000 ), exit, err = -1 );
	require_action( nmeaData.u.GPGGA.nsLatitude == 'N', exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.longitude, 1000000 ) == RoundTo( 12201.808796, 1000000 ), exit, err = -1 );
	require_action( nmeaData.u.GPGGA.ewLongitude == 'W', exit, err = -1 );
	require_action( nmeaData.u.GPGGA.fixQuality == kNMEAFixQuality_GPS, exit, err = -1 );
	require_action( nmeaData.u.GPGGA.satellites == 9, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.hdop, 10 ) == RoundTo( 0.6, 10 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.altitude, 10 ) == RoundTo( 69.8, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPGGA.altitudeUnits == 'M', exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGGA.geoidalSeparation, 10 ) == RoundTo( -27.0, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPGGA.geoidalUnits == 'M', exit, err = -1 );
	require_action( nmeaData.u.GPGGA.dgpsAge == 0, exit, err = -1 );
	require_action( *nmeaData.u.GPGGA.stationID == '\0', exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, kNMEAFlag_SkipUntilStart, 
		"1.1,1.8*39\r\n"
		"$GPGLL,3818.7633,N,12056.6890,W,041546.000,A,A*4D", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPGLL ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGLL.latitude, 10000 ) == RoundTo( 37.3127, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPGLL.nsLatitude == kNMEA_North, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGLL.longitude, 10000 ) == RoundTo( 121.9448, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPGLL.ewLongitude == kNMEA_West, exit, err = -1 );
	require_action( nmeaData.u.GPGLL.time == 15346, exit, err = -1 );
	require_action( nmeaData.u.GPGLL.status == kNMEA_Active, exit, err = -1 );
	require_action( nmeaData.u.GPGLL.mode == kNMEAMode_Autonomous, exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPGSA,A,3,27,19,22,21,11,18,24,,,,,,2.1,1.1,1.8*39", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPGSA ) == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.acquisitionMode == 'A', exit, err = -1 );
	require_action( nmeaData.u.GPGSA.positionMode == '3', exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 0 ] == 27, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 1 ] == 19, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 2 ] == 22, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 3 ] == 21, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 4 ] == 11, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 5 ] == 18, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 6 ] == 24, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 7 ] == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 8 ] == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 9 ] == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 10 ] == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSA.satellite[ 11 ] == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSA.pdop, 10000 ) == RoundTo( 2.1000, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSA.hdop, 10000 ) == RoundTo( 1.1000, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSA.vdop, 10000 ) == RoundTo( 1.8000, 10000 ), exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPGSV,3,1,11,32,80,095,21,20,61,310,36,31,42,046,15,01,39,226,35*76", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_GPGSV ) == 0, exit, err = -1 );
	require_action( nmeaData.u.GPGSV.totalSentences == 3, exit, err = -1 );
	require_action( nmeaData.u.GPGSV.sentenceNumber == 1, exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellitesInView == 11, exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellitesCount == 4, exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellites[ 0 ].satelliteNumber == 32, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 0 ].elevation, 1 ) == RoundTo( 80, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 0 ].azimuth, 1 ) == RoundTo( 95, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 0 ].snr, 1 ) == RoundTo( 21, 1 ), exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellites[ 1 ].satelliteNumber == 20, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 1 ].elevation, 1 ) == RoundTo( 61, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 1 ].azimuth, 1 ) == RoundTo( 310, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 1 ].snr, 1 ) == RoundTo( 36, 1 ), exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellites[ 2 ].satelliteNumber == 31, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 2 ].elevation, 1 ) == RoundTo( 42, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 2 ].azimuth, 1 ) == RoundTo( 46, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 2 ].snr, 1 ) == RoundTo( 15, 1 ), exit, err = -1 );
	require_action( nmeaData.u.GPGSV.satellites[ 3 ].satelliteNumber == 1, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 3 ].elevation, 1 ) == RoundTo( 39, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 3 ].azimuth, 1 ) == RoundTo( 226, 1 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPGSV.satellites[ 3 ].snr, 1 ) == RoundTo( 35, 1 ), exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPHDT,0.0000,T*05", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_GPHDT ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPHDT.heading, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPHDT.degreesTrue == 'T', exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPHDT,359.9999,T*0A", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_GPHDT ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPHDT.heading, 10000 ) == RoundTo( 359.9999, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPHDT.degreesTrue == 'T', exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPRMC ) == 0, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.time == 29916, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.status == kNMEA_Active, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.latitude, 10000 ) == RoundTo( 37.8608, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.nsLatitude == kNMEA_South, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.longitude, 10000 ) == RoundTo( 145.1226, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.ewLongitude == kNMEA_East, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.speed, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.track, 10000 ) == RoundTo( 360.0000, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.date == 729645, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.variation, 10000 ) == RoundTo( 11.3000, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.ewVariation == kNMEA_East, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.mode == kNMEAMode_NotPresent, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.dateTime == -72632484, exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPRMC,092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A*43", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPRMC ) == 0, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.time == 34070, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.status == kNMEA_Active, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.latitude, 10000 ) == RoundTo( 53.3613, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.nsLatitude == kNMEA_North, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.longitude, 10000 ) == RoundTo( 6.5056, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.ewLongitude == kNMEA_West, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.speed, 10000 ) == RoundTo( 0.0200, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.track, 10000 ) == RoundTo( 31.6600, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.date == 734285, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPRMC.variation, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
	require_action( nmeaData.u.GPRMC.ewVariation == '?', exit, err = -1 );
	require_action( nmeaData.u.GPRMC.mode == kNMEAMode_Autonomous, exit, err = -1 );
	require_action( nmeaData.u.GPRMC.dateTime == 328267670, exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPVTG ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPVTG.courseTrue, 10 ) == RoundTo( 54.7, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPVTG.courseTrueType == 'T', exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPVTG.courseMagnetic, 10 ) == RoundTo( 34.4, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPVTG.courseMagneticType == 'M', exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPVTG.speedKnots, 10 ) == RoundTo( 5.5, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPVTG.speedKnotsUnit == 'N', exit, err = -1 );
	require_action( RoundTo( nmeaData.u.GPVTG.speedKPH, 10 ) == RoundTo( 10.2, 10 ), exit, err = -1 );
	require_action( nmeaData.u.GPVTG.speedKPHUnit == 'K', exit, err = -1 );
	require_action( nmeaData.u.GPVTG.mode == 0, exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, "$GPZDA,234500,09,06,1995,-12,45*6C", kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( stricmp( nmeaData.type, kNMEAType_GPZDA ) == 0, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.hour == 23, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.minute == 45, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.second == 0, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.day == 9, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.month == 6, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.year == 1995, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.zoneHours == -12, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.zoneMinutes == 45, exit, err = -1 );
	require_action( nmeaData.u.GPZDA.dateTime == -175606200, exit, err = -1 );
	
	s = 
	"$OHPR,172.5,25.4,-147.2,26.9,0.0000*15\r\n"
	"$OHPR,172.5,25.2,-147.0,26.9,0.0000*11\r\n"
	",,,A*43"
	"$OHPR,172.2,25.1,-146.7,26.9,0.0000*13\r\n"
	"abcdef";
	NMEAInit( &nmeaData );
	for( lineNum = 1; ( err = NMEAParse( &nmeaData, kNMEAFlag_SkipUntilStart, s, kSizeCString, &s ) ) == kNoErr; ++lineNum )
	{
		switch( lineNum )
		{
			case 1:
				require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
				require_action( stricmp( nmeaData.type, kNMEAType_OHPR ) == 0, exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.heading, 10000 ) == RoundTo( 172.5, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.pitch, 10000 ) == RoundTo( 25.4, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.roll, 10000 ) == RoundTo( -147.2, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.temperature, 10000 ) == RoundTo( 26.9, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.depth, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
				break;
			
			case 2:
				require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
				require_action( stricmp( nmeaData.type, kNMEAType_OHPR ) == 0, exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.heading, 10000 ) == RoundTo( 172.5, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.pitch, 10000 ) == RoundTo( 25.2, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.roll, 10000 ) == RoundTo( -147.0, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.temperature, 10000 ) == RoundTo( 26.9, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.depth, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
				break;
			
			case 3:
				require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
				require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
				require_action( stricmp( nmeaData.type, kNMEAType_OHPR ) == 0, exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.heading, 10000 ) == RoundTo( 172.5, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.pitch, 10000 ) == RoundTo( 25.2, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.roll, 10000 ) == RoundTo( -146.7, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.temperature, 10000 ) == RoundTo( 26.9, 10000 ), exit, err = -1 );
				require_action( RoundTo( nmeaData.u.OHPR.depth, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
				break;
				
			default:
				require_action( 0, exit, err = -1 );
				break;
		}
	}
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( s && ( *s == '\0' ), exit, err = -1 );
	
	// PAACD
	
	s = 
		"$PAACD,40000.001,9.80665,10"
		",0.00,0.0340,,1.0000"
		",0.10,-0.0106,,1.0000"
		",0.20,0.0283,,1.0000"
		",0.30,0.0298,,1.0000"
		",0.40,0.0412,,1.0000"
		",0.50,-0.0302,,1.0000"
		",0.60,-0.0165,,1.0000"
		",0.70,0.0268,,1.0000"
		",0.80,-0.0222,,1.0000"
		",0.90,0.0054,,1.0000"
		"*7B";
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PAACD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.timestamp, 1000 ) == RoundTo( 40000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PAACD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PAACD.sampleCountActual == 8, exit, err = -1 );
	d = 0.00;
	for( i = 0; i < nmeaData.u.PAACD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PAACD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( nmeaData.u.PAACD.samples[ i ].yAxis == 0, exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAACD.samples[ i ].zAxis, 10000 ) == RoundTo( 1.0000, 10000 ), exit, err = -1 );
		d += 0.10;
	}
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 0 ].xAxis, 10000 ) == RoundTo( 0.0340, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 1 ].xAxis, 10000 ) == RoundTo( -0.0106, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 2 ].xAxis, 10000 ) == RoundTo( 0.0283, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 3 ].xAxis, 10000 ) == RoundTo( 0.0298, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 4 ].xAxis, 10000 ) == RoundTo( 0.0412, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 5 ].xAxis, 10000 ) == RoundTo( -0.0302, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 6 ].xAxis, 10000 ) == RoundTo( -0.0165, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 7 ].xAxis, 10000 ) == RoundTo( 0.0268, 10000 ), exit, err = -1 );
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, kNMEAFlag_AllowMalloc, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PAACD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.timestamp, 1000 ) == RoundTo( 40000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PAACD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PAACD.sampleCountActual == 10, exit, err = -1 );
	d = 0.00;
	for( i = 0; i < nmeaData.u.PAACD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PAACD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( nmeaData.u.PAACD.samples[ i ].yAxis == 0, exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAACD.samples[ i ].zAxis, 10000 ) == RoundTo( 1.0000, 10000 ), exit, err = -1 );
		d += 0.10;
	}
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 0 ].xAxis, 10000 ) == RoundTo( 0.0340, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 1 ].xAxis, 10000 ) == RoundTo( -0.0106, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 2 ].xAxis, 10000 ) == RoundTo( 0.0283, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 3 ].xAxis, 10000 ) == RoundTo( 0.0298, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 4 ].xAxis, 10000 ) == RoundTo( 0.0412, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 5 ].xAxis, 10000 ) == RoundTo( -0.0302, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 6 ].xAxis, 10000 ) == RoundTo( -0.0165, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 7 ].xAxis, 10000 ) == RoundTo( 0.0268, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 8 ].xAxis, 10000 ) == RoundTo( -0.0222, 10000 ), exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAACD.samples[ 9 ].xAxis, 10000 ) == RoundTo( 0.0054, 10000 ), exit, err = -1 );
	NMEAFree( &nmeaData );
	
	// PAGCD
	
	s = 
		"$PAGCD,20000.001,10"
		",0.00,,,12.1234"
		",0.10,,,12.1234"
		",0.20,,,12.1234"
		",0.30,,,12.1234"
		",0.40,,,12.1234"
		",0.50,,,12.1234"
		",0.60,,,12.1234"
		",0.70,,,12.1234"
		",0.80,,,12.1234"
		",0.90,,,12.1234"
		"*7C";
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PAGCD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAGCD.timestamp, 1000 ) == RoundTo( 20000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PAGCD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PAGCD.sampleCountActual == 8, exit, err = -1 );
	d = 0.00;
	for( i = 0; i < nmeaData.u.PAGCD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].xAxis, 10000 ) == RoundTo( 12.1234, 10000 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].yAxis, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].zAxis, 10000 ) == RoundTo( 0, 10000), exit, err = -1 );
		d += 0.10;
	}
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, kNMEAFlag_AllowMalloc, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PAGCD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PAGCD.timestamp, 1000 ) == RoundTo( 20000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PAGCD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PAGCD.sampleCountActual == 10, exit, err = -1 );
	d = 0.00;
	for( i = 0; i < nmeaData.u.PAGCD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].xAxis, 10000 ) == RoundTo( 12.1234, 10000 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].yAxis, 10000 ) == RoundTo( 0, 10000 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PAGCD.samples[ i ].zAxis, 10000 ) == RoundTo( 0, 10000), exit, err = -1 );
		d += 0.10;
	}
	NMEAFree( &nmeaData );
	
	// PASCD
	
	s = 
		"$PASCD,1000.001,C,D,0,10"
		",0.00,0.123"
		",0.10,1.123"
		",0.20,2.123"
		",0.30,3.123"
		",0.40,4.123"
		",0.50,5.123"
		",0.60,6.123"
		",0.70,7.123"
		",0.80,8.123"
		",0.90,9.123"
		"*41";
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, 0, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PASCD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PASCD.timestamp, 1000 ) == RoundTo( 1000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PASCD.sensorType == kNMEASensorType_Combined, exit, err = -1 );
	require_action( nmeaData.u.PASCD.transmissionState == kNMEATransmissionState_Drive, exit, err = -1 );
	require_action( nmeaData.u.PASCD.slipDetect == 0, exit, err = -1 );
	require_action( nmeaData.u.PASCD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PASCD.sampleCountActual == 8, exit, err = -1 );
	d  = 0.00;
	d2 = 0.123;
	for( i = 0; i < nmeaData.u.PASCD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PASCD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PASCD.samples[ i ].speed, 1000 ) == RoundTo( d2, 1000 ), exit, err = -1 );
		d  += 0.10;
		d2 += 1;
	}
	
	NMEAInit( &nmeaData );
	err = NMEAParse( &nmeaData, kNMEAFlag_AllowMalloc, s, kSizeCString, NULL );
	require_noerr( err, exit );
	require_action( nmeaData.flags & kNMEAFlag_HasStart, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_ValidChecksum, exit, err = -1 );
	require_action( nmeaData.flags & kNMEAFlag_Parsed, exit, err = -1 );
	require_action( strcmp( nmeaData.type, kNMEAType_PASCD ) == 0, exit, err = -1 );
	require_action( RoundTo( nmeaData.u.PASCD.timestamp, 1000 ) == RoundTo( 1000.001, 1000 ), exit, err = -1 );
	require_action( nmeaData.u.PASCD.sensorType == kNMEASensorType_Combined, exit, err = -1 );
	require_action( nmeaData.u.PASCD.transmissionState == kNMEATransmissionState_Drive, exit, err = -1 );
	require_action( nmeaData.u.PASCD.slipDetect == 0, exit, err = -1 );
	require_action( nmeaData.u.PASCD.sampleCountExpected == 10, exit, err = -1 );
	require_action( nmeaData.u.PASCD.sampleCountActual == 10, exit, err = -1 );
	d  = 0.00;
	d2 = 0.123;
	for( i = 0; i < nmeaData.u.PASCD.sampleCountActual; ++i )
	{
		require_action( RoundTo( nmeaData.u.PASCD.samples[ i ].timeOffset, 100 ) == RoundTo( d, 100 ), exit, err = -1 );
		require_action( RoundTo( nmeaData.u.PASCD.samples[ i ].speed, 1000 ) == RoundTo( d2, 1000 ), exit, err = -1 );
		d  += 0.10;
		d2 += 1;
	}
	NMEAFree( &nmeaData );
	
	// NumberToOrdinalSuffixCString
	
	require_action( strcmp( NumberToOrdinalSuffixCString(    1 ), "st" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(    2 ), "nd" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(    3 ), "rd" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(    4 ), "th" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(  123 ), "rd" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(  222 ), "nd" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString(  201 ), "st" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( NumberToOrdinalSuffixCString( 1000 ), "th" ) == 0, exit, err = kResponseErr );
	
	// ParseCommandLineIntoArgV
	
	s = "";
	err = ParseCommandLineIntoArgV( s, &argc, &argv );
	require_noerr( err, exit );
	require_action( argc == 0, exit, err = kResponseErr );
	require_action( argv, exit, err = kResponseErr );
	require_action( argv[ 0 ] == NULL, exit, err = kResponseErr );
	FreeCommandLineArgV( argc, argv );
	
	s = "MyTool";
	err = ParseCommandLineIntoArgV( s, &argc, &argv );
	require_noerr( err, exit );
	require_action( argc == 1, exit, err = kResponseErr );
	require_action( argv, exit, err = kResponseErr );
	require_action( strcmp( argv[ 0 ], "MyTool" ) == 0, exit, err = kResponseErr );
	require_action( argv[ 1 ] == NULL, exit, err = kResponseErr );
	FreeCommandLineArgV( argc, argv );
	
	s = "   MyTool 12\\3'4'5\"6\" \\\n '' \"\" --xy\\\\z \"test'test\"     \t\t'abc\"xyz'";
	err = ParseCommandLineIntoArgV( s, &argc, &argv );
	require_noerr( err, exit );
	require_action( argc == 8, exit, err = kResponseErr );
	require_action( argv, exit, err = kResponseErr );
	require_action( strcmp( argv[ 0 ], "MyTool" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 1 ], "123456" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 2 ], "" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 3 ], "" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 4 ], "" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 5 ], "--xy\\z" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 6 ], "test'test" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 7 ], "abc\"xyz" ) == 0, exit, err = kResponseErr );
	require_action( argv[ 8 ] == NULL, exit, err = kResponseErr );
	FreeCommandLineArgV( argc, argv );
	
	s = "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20";
	err = ParseCommandLineIntoArgV( s, &argc, &argv );
	require_noerr( err, exit );
	require_action( argc == 21, exit, err = kResponseErr );
	require_action( argv, exit, err = kResponseErr );
	require_action( strcmp( argv[  0 ],  "0" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  1 ],  "1" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  2 ],  "2" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  3 ],  "3" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  4 ],  "4" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  5 ],  "5" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  6 ],  "6" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  7 ],  "7" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  8 ],  "8" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[  9 ],  "9" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 10 ], "10" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 11 ], "11" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 12 ], "12" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 13 ], "13" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 14 ], "14" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 15 ], "15" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 16 ], "16" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 17 ], "17" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 18 ], "18" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 19 ], "19" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( argv[ 20 ], "20" ) == 0, exit, err = kResponseErr );
	require_action( argv[ 21 ] == NULL, exit, err = kResponseErr );
	FreeCommandLineArgV( argc, argv );
	
	// TruncateUTF8
	
	memset( str, 'z', sizeof( str ) );
	strlcpy( old, "testing", sizeof( old ) );
	len = TruncateUTF8( old, kSizeCString, str, 4, true );
	require_action( len == 3, exit, err = -1 );
	require_action( strcmp( str, "tes" ) == 0, exit, err = -1 );
	require_action( str[ 4 ] == 'z', exit, err = -1 );
	require_action( str[ 5 ] == 'z', exit, err = -1 );
	require_action( str[ 6 ] == 'z', exit, err = -1 );
	
	*str = '\0';
	strlcpy( old, "te\xC3\xA9", sizeof( old ) );
	len = TruncateUTF8( old, kSizeCString, str, 4, true );
	require_action( len == 2, exit, err = -1 );
	require_action( strcmp( str, "te" ) == 0, exit, err = -1 );
	
	// TruncatedUTF8Length
	
	strlcpy( str, "testing", sizeof( str ) );
	len = TruncatedUTF8Length( str, strlen( str ), 10 );
	require_action( len == 7, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "testing" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "testing", sizeof( str ) );
	len = TruncatedUTF8Length( str, strlen( str ), 4 );
	require_action( len == 4, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "test" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "te\xC3\xA9", sizeof( str ) );
	len = TruncatedUTF8Length( str, strlen( str ), 4 );
	require_action( len == 4, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "te\xC3\xA9" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "te\xC3\xA9ing", sizeof( str ) );
	len = TruncatedUTF8Length( str, strlen( str ), 3 );
	require_action( len == 2, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "te" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "\xC3\xA9", sizeof( str ) );
	len = TruncatedUTF8Length( str, strlen( str ), 1 );
	require_action( len == 0, exit, err = kResponseErr );
	
	strlcpy( str, "\xf0\x9d\x84\x9e", sizeof( str ) ); // Unicode codepoint 0x1D11E (musical G clef)
	len = TruncatedUTF8Length( str, strlen( str ), 4 );
	require_action( len == 4, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "\xf0\x9d\x84\x9e" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "\xf0\x9d\x84\x9e", sizeof( str ) ); // Unicode codepoint 0x1D11E (musical G clef)
	len = TruncatedUTF8Length( str, strlen( str ), 3 );
	require_action( len == 0, exit, err = kResponseErr );
	
	strlcpy( str, "z\xf0\x9d\x84\x9e", sizeof( str ) ); // Unicode codepoint 0x1D11E (musical G clef)
	len = TruncatedUTF8Length( str, strlen( str ), 4 );
	require_action( len == 1, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "z" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "\xED\xA5\x82\xED\xB3\x88", sizeof( str ) ); // Two UTF-16 surrogates encoded as UTF-8 (invalid UTF-8)
	len = TruncatedUTF8Length( str, strlen( str ), 6 );
	require_action( len == 6, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "\xED\xA5\x82\xED\xB3\x88" ) == 0, exit, err = kResponseErr );
	
	strlcpy( str, "\xED\xA5\x82\xED\xB3\x88", sizeof( str ) ); // Two UTF-16 surrogates encoded as UTF-8 (invalid UTF-8)
	len = TruncatedUTF8Length( str, strlen( str ), 5 );
	require_action( len == 0, exit, err = kResponseErr );
	
	strlcpy( str, "z\xED\xA5\x82\xED\xB3\x88", sizeof( str ) ); // Two UTF-16 surrogates encoded as UTF-8 (invalid UTF-8)
	len = TruncatedUTF8Length( str, strlen( str ), 6 );
	require_action( len == 1, exit, err = kResponseErr );
	str[ len ] = '\0';
	require_action( strcmp( str, "z" ) == 0, exit, err = kResponseErr );
	
	// ValidDNSName
	
	err = ValidDNSName( "x" );
	require_noerr( err, exit );
	
	err = ValidDNSName( "www" );
	require_noerr( err, exit );
	
	err = ValidDNSName( "www.apple.com" );
	require_noerr( err, exit );
	
	err = ValidDNSName( "\\119\\119\\119.apple.com" );
	require_noerr( err, exit );
	
	err = ValidDNSName( "" );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	err = ValidDNSName( "." );
	require_action( err != kNoErr, exit, err = kResponseErr );

	err = ValidDNSName( "a_dns_label_that_is_bigger_than_63_characters_and_should_fail_this.apple.com" );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	err = ValidDNSName( "a_large_but_still_valid_dns_label_should_fail_for_total_size."
						"a_large_but_still_valid_dns_label_should_fail_for_total_size."
						"a_large_but_still_valid_dns_label_should_fail_for_total_size."
						"a_large_but_still_valid_dns_label_should_fail_for_total_size."
						"a_large_but_still_valid_dns_label_should_fail_for_total_size." );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	// XMLEscape
	
	strlcpy( old, "This & that <angle> \xc3\xa9 \"quote' it", sizeof( old ) );
	match = "This &amp; that &lt;angle&gt; \xc3\xa9 &quot;quote&#39; it";
	err = XMLEscapeCopy( old, kSizeCString, &ptr, &len );
	require_noerr( err, exit );
	require_action( strlen( match ) == len, exit, err = -1 );
	require_action( strcmp( ptr, match ) == 0, exit, err = -1 );
	free( ptr );
	
	err = kNoErr;

exit:
	NMEAFree( &nmeaData );
	return( err );
}

//===========================================================================================================================
//	StringUtilsTest
//===========================================================================================================================

OSStatus	StringUtilsTest( void )
{
	OSStatus		err;
	
	err = StringUtilsStringToIPv6AddressTest();
	require_noerr( err, exit );
	
	err = StringUtilsStringToIPv4AddressTest();
	require_noerr( err, exit );
	
	err = StringUtilsIPv6AddressToCStringTest();
	require_noerr( err, exit );
	
	err = StringUtilsConversionsTest();
	require_noerr( err, exit );
	
	err = StringUtilsANSICExtensionsTest();
	require_noerr( err, exit );
	
	err = StringUtilsINITest();
	require_noerr( err, exit );
	
	err = StringUtilsMiscTest();
	require_noerr( err, exit );
	
	err = StringUtilsSuffixesTest();
	require_noerr( err, exit );
	
	err = SNScanFTest();
	require_noerr( err, exit );
	
exit:
	printf( "StringUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	_GetFirstIPInterface
//===========================================================================================================================

static OSStatus	_GetFirstIPInterface( char *inNameBuf, size_t inMaxLen, uint32_t *outIndex )
{
	OSStatus				err;
	struct ifaddrs *		ifaList;
	struct ifaddrs *		ifa;
	
	err = getifaddrs( &ifaList );
	require_noerr( err, exit );
	
	for( ifa = ifaList; ifa; ifa = ifa->ifa_next )
	{
		if( ifa->ifa_name && ( ifa->ifa_addr->sa_family == AF_INET ) )
		{
			if( inNameBuf ) strlcpy( inNameBuf, ifa->ifa_name, inMaxLen );
			if( outIndex )  *outIndex = if_nametoindex( ifa->ifa_name );
			break;
		}
	}
	freeifaddrs( ifaList );
	require_action( ifa, exit, err = kNotFoundErr );
	
exit:
	return( err );
}
#endif

#endif // !EXCLUDE_UNIT_TESTS
