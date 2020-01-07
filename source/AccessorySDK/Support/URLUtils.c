/*
	File:    	URLUtils.c
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
	
	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#include "URLUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "StringUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <stddef.h>
	#include <string.h>
#endif

#if 0
#pragma mark == Encoding/Decoding ==
#endif

//===========================================================================================================================
//	URL Encoding
//===========================================================================================================================

#define URL_UNRESERVED				( 1 << 0 ) // ALPHA / DIGIT / "-" / "." / "_" / "~"
#define URL_GEN_DELIMS				( 1 << 1 ) // ":" / "/" / "?" / "#" / "[" / "]" / "@"
#define URL_SUB_DELIMS				( 1 << 2 ) // "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
#define URL_PCHAR					( 1 << 3 ) // unreserved / pct-encoded / sub-delims / ":" / "@"
#define URL_QUERY_OR_FRAGMENT		( 1 << 4 ) // *( pchar / "/" / "?" )
#define URL_PATH					( 1 << 5 ) // "/" / *pchar / sub-delims

#define URL_QUERY_MASK				( URL_UNRESERVED | URL_SUB_DELIMS | URL_QUERY_OR_FRAGMENT )
#define URL_PATH_MASK				( URL_PCHAR | URL_SUB_DELIMS | URL_PATH )

static const uint8_t		kURLEscapeTable[ 256 ] =
{
	// 0-31 (0x00-0x1F) are invalid and must always be escaped.
	
	0,  0,  0,  0,  0,  0,  0,  0, // 0-7		NUL SOH STX ETX  EOT ENQ ACK BEL
	0,  0,  0,  0,  0,  0,  0,  0, // 8-15		BS  HT  LF  VT   FF  CR  SO  SI 
	0,  0,  0,  0,  0,  0,  0,  0, // 16-23		DLE DC1 DC2 DC3  DC4 NAK SYN ETB
	0,  0,  0,  0,  0,  0,  0,  0, // 24-31		CAN EM  SUB ESC  FS  GS  RS  US 
	
	//				URL_UNRESERVED	URL_GEN_DELIMS	URL_SUB_DELIMS	URL_PCHAR	URL_QUERY_OR_FRAGMENT		URL_PATH
	/* ' '  32 */	0, 
	/* '!'  33 */									URL_SUB_DELIMS,	
	/* '"'  34 */	0,
	/* '#'  35 */					URL_GEN_DELIMS,
	/* '$'  36 */									URL_SUB_DELIMS,
	/* '%'  37 */	0,
	/* '&'  38 */									URL_SUB_DELIMS,
	/* '''  39 */									URL_SUB_DELIMS,
	/* '('  40 */									URL_SUB_DELIMS,
	/* ')'  41 */									URL_SUB_DELIMS,
	/* '*'  42 */									URL_SUB_DELIMS,
	/* '+'  43 */									URL_SUB_DELIMS,
	/* ','  44 */									URL_SUB_DELIMS,
	/* '-'  45 */	URL_UNRESERVED,
	/* '.'  46 */	URL_UNRESERVED,
	/* '/'  47 */					URL_GEN_DELIMS								| URL_QUERY_OR_FRAGMENT		| URL_PATH,
	/* '0'  48 */	URL_UNRESERVED,
	/* '1'  49 */	URL_UNRESERVED,
	/* '2'  50 */	URL_UNRESERVED,
	/* '3'  51 */	URL_UNRESERVED,
	/* '4'  52 */	URL_UNRESERVED,
	/* '5'  53 */	URL_UNRESERVED,
	/* '6'  54 */	URL_UNRESERVED,
	/* '7'  55 */	URL_UNRESERVED,
	/* '8'  56 */	URL_UNRESERVED,
	/* '9'  57 */	URL_UNRESERVED,
	/* ':'  58 */					URL_GEN_DELIMS					| URL_PCHAR,
	/* ';'  59 */									URL_SUB_DELIMS,
	/* '<'  60 */	0,
	/* '='  61 */									URL_SUB_DELIMS,
	/* '>'  62 */	0,
	/* '?'  63 */					URL_GEN_DELIMS								| URL_QUERY_OR_FRAGMENT,
	/* '@'  64 */					URL_GEN_DELIMS					| URL_PCHAR,
	/* 'A'  65 */	URL_UNRESERVED,
	/* 'B'  66 */	URL_UNRESERVED,
	/* 'C'  67 */	URL_UNRESERVED,
	/* 'D'  68 */	URL_UNRESERVED,
	/* 'E'  69 */	URL_UNRESERVED,
	/* 'F'  70 */	URL_UNRESERVED,
	/* 'G'  71 */	URL_UNRESERVED,
	/* 'H'  72 */	URL_UNRESERVED,
	/* 'I'  73 */	URL_UNRESERVED,
	/* 'J'  74 */	URL_UNRESERVED,
	/* 'K'  75 */	URL_UNRESERVED,
	/* 'L'  76 */	URL_UNRESERVED,
	/* 'M'  77 */	URL_UNRESERVED,
	/* 'N'  78 */	URL_UNRESERVED,
	/* 'O'  79 */	URL_UNRESERVED,
	/* 'P'  80 */	URL_UNRESERVED,
	/* 'Q'  81 */	URL_UNRESERVED,
	/* 'R'  82 */	URL_UNRESERVED,
	/* 'S'  83 */	URL_UNRESERVED,
	/* 'T'  84 */	URL_UNRESERVED,
	/* 'U'  85 */	URL_UNRESERVED,
	/* 'V'  86 */	URL_UNRESERVED,
	/* 'W'  87 */	URL_UNRESERVED,
	/* 'X'  88 */	URL_UNRESERVED,
	/* 'Y'  89 */	URL_UNRESERVED,
	/* 'Z'  90 */	URL_UNRESERVED,
	/* '['  91 */					URL_GEN_DELIMS,
	/* '\'  92 */	0,
	/* ']'  93 */					URL_GEN_DELIMS,
	/* '^'  94 */	0,
	/* '_'  95 */	URL_UNRESERVED,
	/* '`'  96 */	0,
	/* 'a'  97 */	URL_UNRESERVED,
	/* 'b'  98 */	URL_UNRESERVED,
	/* 'c'  99 */	URL_UNRESERVED,
	/* 'd' 100 */	URL_UNRESERVED,
	/* 'e' 101 */	URL_UNRESERVED,
	/* 'f' 102 */	URL_UNRESERVED,
	/* 'g' 103 */	URL_UNRESERVED,
	/* 'h' 104 */	URL_UNRESERVED,
	/* 'i' 105 */	URL_UNRESERVED,
	/* 'j' 106 */	URL_UNRESERVED,
	/* 'k' 107 */	URL_UNRESERVED,
	/* 'l' 108 */	URL_UNRESERVED,
	/* 'm' 109 */	URL_UNRESERVED,
	/* 'n' 110 */	URL_UNRESERVED,
	/* 'o' 111 */	URL_UNRESERVED,
	/* 'p' 112 */	URL_UNRESERVED,
	/* 'q' 113 */	URL_UNRESERVED,
	/* 'r' 114 */	URL_UNRESERVED,
	/* 's' 115 */	URL_UNRESERVED,
	/* 't' 116 */	URL_UNRESERVED,
	/* 'u' 117 */	URL_UNRESERVED,
	/* 'v' 118 */	URL_UNRESERVED,
	/* 'w' 119 */	URL_UNRESERVED,
	/* 'x' 120 */	URL_UNRESERVED,
	/* 'y' 121 */	URL_UNRESERVED,
	/* 'z' 122 */	URL_UNRESERVED,
	/* '{' 123 */	0,
	/* '|' 124 */	0,
	/* '}' 125 */	0,
	/* '~' 126 */	URL_UNRESERVED,
	/* DEL 127 */	0, 

	// 128-255 (0x80-0xFF) are invalid and must always be escaped.
	
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

//===========================================================================================================================
//	URLEncode
//===========================================================================================================================

OSStatus
	URLEncode( 
		URLEncodeType	inType, 
		const void *	inSourceData, 
		size_t			inSourceSize, 
		void *			inEncodedDataBuffer, 
		size_t			inEncodedDataBufferSize, 
		size_t *		outEncodedSize )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	uint8_t *			dst;
	uint8_t *			lim;
	uint8_t				c;
	uint8_t				mask;
	uint8_t				badC;
	
	if( inSourceSize == kSizeCString ) inSourceSize = strlen( (const char *) inSourceData );
	
	src = (const uint8_t *) inSourceData;
	end = src + inSourceSize;
	dst = (uint8_t *) inEncodedDataBuffer;
	lim = dst + inEncodedDataBufferSize;
	
	if( inType == kURLEncodeType_Generic )
	{
		mask = URL_UNRESERVED;
		badC = '\0';
	}
	else if( inType == kURLEncodeType_Query )
	{
		mask = URL_QUERY_MASK;
		badC = '&';
	}
	else
	{
		dlogassert( "unknown type: %d", inType );
		err = kParamErr;
		goto exit;
	}
	
	while( src < end )
	{
		c = *src++;
		if( ( kURLEscapeTable[ c ] & mask ) && ( c != badC ) )
		{
			if( inEncodedDataBuffer )
			{
				require_action_quiet( dst < lim, exit, err = kOverrunErr );
				*dst = c;
			}
			++dst;
		}
		else
		{
			if( inEncodedDataBuffer )
			{
				require_action_quiet( ( lim - dst ) >= 3, exit, err = kOverrunErr );
				dst[ 0 ] = '%';
				dst[ 1 ] = (uint8_t)( kHexDigitsUppercase[ c >> 4 ] );
				dst[ 2 ] = (uint8_t)( kHexDigitsUppercase[ c & 0x0F ] );
			}
			dst += 3;
		}
	}
	err = kNoErr;
	
exit:
	*outEncodedSize = (size_t)( dst - ( (uint8_t *) inEncodedDataBuffer ) );
	return( err );
}

//===========================================================================================================================
//	URLEncodeCopy
//===========================================================================================================================

OSStatus
	URLEncodeCopy( 
		URLEncodeType	inType, 
		const void *	inSourceData, 
		size_t			inSourceSize, 
		void *			outEncodedStr, 
		size_t *		outEncodedLen )
{
	OSStatus		err;
	size_t			encodedLen;
	char *			encodedStr;
	
	encodedStr = NULL;
	
	if( inSourceSize == kSizeCString ) inSourceSize = strlen( (const char *) inSourceData );
	
	err = URLEncode( inType, inSourceData, inSourceSize, NULL, 0, &encodedLen );
	require_noerr( err, exit );
	
	encodedStr = (char *) malloc( encodedLen + 1 );
	require_action( encodedStr, exit, err = kNoMemoryErr );
	
	err = URLEncode( inType, inSourceData, inSourceSize, encodedStr, encodedLen, &encodedLen );
	require_noerr( err, exit );
	encodedStr[ encodedLen ] = '\0';
	
	*( (void **) outEncodedStr ) 		= encodedStr;
	if( outEncodedLen ) *outEncodedLen	= encodedLen;
	encodedStr = NULL;
	
exit:
	if( encodedStr ) free( encodedStr );
	return( err );
}

//===========================================================================================================================
//	URLDecodeEx
//===========================================================================================================================

OSStatus
	URLDecodeEx( 
		const void *	inEncodedData, 
		size_t 			inEncodedSize, 
		void *			inDecodedDataBuffer, 
		size_t			inDecodedDataBufferSize, 
		size_t *		outDecodedSize, 
		int *			outChanges )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	uint8_t *			dst;
	uint8_t *			lim;
	uint8_t				c1;
	uint8_t				c2;
	int					changes;
	
	if( inEncodedSize == kSizeCString ) inEncodedSize = strlen( (const char *) inEncodedData );
	src = (const uint8_t *) inEncodedData;
	end = src + inEncodedSize;
	dst = (uint8_t *) inDecodedDataBuffer;
	lim = dst + inDecodedDataBufferSize;
	
	changes = 0;
	while( src < end )
	{
		c1 = *src++;
		if( c1 == '%' )
		{
			require_action_quiet( ( end - src ) >= 2, exit, err = kUnderrunErr );
			
			c1 = *src++;
			if(      ( c1 >= '0' ) && ( c1 <= '9' ) ) c1 = c1 - '0';
			else if( ( c1 >= 'a' ) && ( c1 <= 'f' ) ) c1 = 10 + ( c1 - 'a' );
			else if( ( c1 >= 'A' ) && ( c1 <= 'F' ) ) c1 = 10 + ( c1 - 'A' );
			else { err = kMalformedErr; goto exit; }
			
			c2 = *src++;
			if(      ( c2 >= '0' ) && ( c2 <= '9' ) ) c2 = c2 - '0';
			else if( ( c2 >= 'a' ) && ( c2 <= 'f' ) ) c2 = 10 + ( c2 - 'a' );
			else if( ( c2 >= 'A' ) && ( c2 <= 'F' ) ) c2 = 10 + ( c2 - 'A' );
			else { err = kMalformedErr; goto exit; }
			
			c1 = (uint8_t)( ( c1 << 4 ) | c2 );
			++changes;
		}
		else if( c1 == '+' )
		{
			c1 = ' '; // Some application/x-www-form-urlencoded may use '+' instead of %20 for spaces.
			++changes;
		}
		
		if( inDecodedDataBuffer )
		{
			require_action_quiet( dst < lim, exit, err = kOverrunErr );
			*dst = c1;
		}
		++dst;
	}
	err = kNoErr;
	
exit:
	if( outDecodedSize ) *outDecodedSize = (size_t)( dst - ( (uint8_t *) inDecodedDataBuffer ) );
	if( outChanges )	 *outChanges	 = changes;
	return( err );
}

//===========================================================================================================================
//	URLDecodeCopy
//===========================================================================================================================

OSStatus	URLDecodeCopy( const void *inEncodedData, size_t inEncodedSize, void *outDecodedPtr, size_t *outDecodedSize )
{
	OSStatus		err;
	size_t			decodedSize;
	char *			decodedData;
	
	decodedData = NULL;
	
	if( inEncodedSize == kSizeCString ) inEncodedSize = strlen( (const char *) inEncodedData );
	
	err = URLDecode( inEncodedData, inEncodedSize, NULL, 0, &decodedSize );
	require_noerr_quiet( err, exit );
	
	decodedData = (char *) malloc( decodedSize + 1 );
	require_action( decodedData, exit, err = kNoMemoryErr );
	
	err = URLDecode( inEncodedData, inEncodedSize, decodedData, decodedSize, &decodedSize );
	require_noerr_quiet( err, exit );
	decodedData[ decodedSize ] = '\0';
	
	*( (void **) outDecodedPtr ) 			= decodedData;
	if( outDecodedSize ) *outDecodedSize	= decodedSize;
	decodedData = NULL;
	
exit:
	if( decodedData ) free( decodedData );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	URLParseComponents
//===========================================================================================================================

OSStatus	URLParseComponents( const char *inSrc, const char *inEnd, URLComponents *outComponents, const char **outSrc )
{
	const char *		ptr;
	const char *		schemePtr;
	const char *		schemeEnd;
	const char *		userPtr;
	const char *		userEnd;
	const char *		passwordPtr;
	const char *		passwordEnd;
	const char *		hostPtr;
	const char *		hostEnd;
	const char *		pathPtr;
	const char *		pathEnd;
	const char *		queryPtr;
	const char *		queryEnd;
	const char *		fragmentPtr;
	const char *		fragmentEnd;
	char				c;
	
	/*
		URL breakdown from RFC 3986.
	
		 foo://example.com:8042/over/there?name=ferret#nose
		 \_/   \______________/\_________/ \_________/ \__/
		  |           |            |            |        |
		scheme    authority       path        query   fragment
		  |   _____________________|__
		 / \ /                        \ 
		 urn:example:animal:ferret:nose
	*/
	
	if( inEnd == NULL ) inEnd = inSrc + strlen( inSrc );
	
	// Parse an optional scheme (the "ftp" in "ftp://tom:secret@abc.com/test?x#y").
	
	schemePtr = NULL;
	schemeEnd = NULL;
	
	c = '\0';
	ptr = inSrc;
	while( ( ptr < inEnd ) && ( ( c = *ptr ) != ':' ) && ( c != '/' ) && ( c != '?' ) && ( c != '#' ) ) ++ptr;
	if( c == ':' )
	{
		schemePtr = inSrc;
		schemeEnd = ptr;
		inSrc = ptr + 1;
	}
	
	// Parse an optional authority (the "tom:secret@abc.com" in "ftp://tom:secret@abc.com/test?x#y").
	
	userPtr		= NULL;
	userEnd		= NULL;
	passwordPtr	= NULL;
	passwordEnd	= NULL;
	hostPtr		= NULL;
	hostEnd		= NULL;
	
	if( ( ( inEnd - inSrc ) >= 2 ) && ( inSrc[ 0 ] == '/' ) && ( inSrc[ 1 ] == '/' ) )
	{
		const char *		authorityPtr;
		const char *		authorityEnd;
		const char *		userInfoPtr;
		const char *		userInfoEnd;
		
		inSrc += 2;
		authorityPtr = inSrc;
		while( ( inSrc < inEnd ) && ( ( c = *inSrc ) != '/' ) && ( c != '?' ) && ( c != '#' ) ) ++inSrc;
		authorityEnd = inSrc;
		
		// Parse an optional userinfo (the "tom:secret" in the above URL).
		
		userInfoPtr = authorityPtr;
		userInfoEnd = userInfoPtr;
		while( ( userInfoEnd < authorityEnd ) && ( *userInfoEnd != '@' ) ) ++userInfoEnd;
		if( userInfoEnd < authorityEnd )
		{
			// Parse the username (the "tom" in the above URL).
			
			userPtr = userInfoPtr;
			userEnd = userPtr;
			while( ( userEnd < userInfoEnd ) && ( *userEnd != ':' ) ) ++userEnd;
			if( userEnd < userInfoEnd )
			{
				// The rest is password/auth info. Note: passwords in URLs are deprecated (see RFC 3986 section 3.2.1).
				
				passwordPtr = userEnd + 1;
				passwordEnd = userInfoEnd;
			}
			
			// The host is the rest of the authority (the "abc.com" in "ftp://tom:secret@abc.com/test?x#y").
			
			hostPtr = userInfoEnd + 1;
			hostEnd = authorityEnd;
		}
		else
		{
			// The host is the entire authority (the "abc.com" in "ftp://tom:secret@abc.com/test?x#y").
			
			hostPtr = authorityPtr;
			hostEnd = authorityEnd;
		}
	}
	
	// Parse the path (the "/test" in "ftp://tom:secret@abc.com/test?x#y").
	
	c = '\0';
	pathPtr = inSrc;
	while( ( inSrc < inEnd ) && ( ( c = *inSrc ) != '?' ) && ( c != '#' ) ) ++inSrc;
	pathEnd = inSrc;
	
	// Parse an optional query (the "x" in "ftp://tom:secret@abc.com/test?x#y").
	
	queryPtr = NULL;
	queryEnd = NULL;	
	if( c == '?' )
	{
		queryPtr = ++inSrc;
		while( ( inSrc < inEnd ) && ( ( c = *inSrc ) != '#' ) ) ++inSrc;
		queryEnd = inSrc;
	}
	
	// Parse an optional fragment  (the "y" in "ftp://tom:secret@abc.com/test?x#y").
	
	fragmentPtr = NULL;
	fragmentEnd = NULL;
	if( c == '#' )
	{
		fragmentPtr = ++inSrc;
		fragmentEnd = inEnd;
		inSrc = inEnd;
	}
	
	outComponents->schemePtr	= schemePtr;
	outComponents->schemeLen	= (size_t)( schemeEnd - schemePtr );
	outComponents->userPtr		= userPtr;
	outComponents->userLen		= (size_t)( userEnd - userPtr );
	outComponents->passwordPtr	= passwordPtr;
	outComponents->passwordLen	= (size_t)( passwordEnd - passwordPtr );
	outComponents->hostPtr		= hostPtr;
	outComponents->hostLen		= (size_t)( hostEnd - hostPtr );
	outComponents->pathPtr		= pathPtr;
	outComponents->pathLen		= (size_t)( pathEnd - pathPtr );
	outComponents->queryPtr		= queryPtr;
	outComponents->queryLen		= (size_t)( queryEnd - queryPtr );
	outComponents->fragmentPtr	= fragmentPtr;
	outComponents->fragmentLen	= (size_t)( fragmentEnd - fragmentPtr );
	outComponents->segmentPtr	= ( ( pathPtr < pathEnd ) && ( *pathPtr == '/' ) ) ? ( pathPtr + 1 ) : pathPtr;
	outComponents->segmentEnd	= pathEnd;
	if( outSrc ) *outSrc = inSrc;
	return( kNoErr );
}

//===========================================================================================================================
//	URLGetNextPathSegment
//===========================================================================================================================

OSStatus	URLGetNextPathSegment( URLComponents *inComps, const char **outSegmentPtr, size_t *outSegmentLen )
{
	const char *		src;
	const char *		ptr;
	const char *		end;
	
	src = inComps->segmentPtr;
	end = inComps->segmentEnd;
	for( ptr = src; ( ptr < end ) && ( *ptr != '/' ); ++ptr ) {}
	if( ptr != src )
	{
		*outSegmentPtr = src;
		*outSegmentLen = (size_t)( ptr - src );
		inComps->segmentPtr = ( ptr < end ) ? ( ptr + 1 ) : ptr;
		return( kNoErr );
	}
	return( kNotFoundErr );
}

//===========================================================================================================================
//	URLGetOrCopyNextVariable
//===========================================================================================================================

OSStatus
	URLGetOrCopyNextVariable( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		char **			outNameStorage, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char **			outValueStorage, 
		const char **	outSrc )
{
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	char *				nameStorage;
	const char *		valuePtr;
	size_t				valueLen;
	char *				valueStorage;
	int					changes;
	
	nameStorage  = NULL;
	valueStorage = NULL;
	
	err = URLGetNextVariable( inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, outSrc );
	require_noerr_quiet( err, exit );
	
	err = URLDecodeEx( namePtr, nameLen, NULL, 0, NULL, &changes );
	require_noerr( err, exit );
	if( changes > 0 )
	{
		err = URLDecodeCopy( namePtr, nameLen, &nameStorage, &nameLen );
		require_noerr( err, exit );
		namePtr = nameStorage;
	}
	
	if( outValueStorage )
	{
		err = URLDecodeEx( valuePtr, valueLen, NULL, 0, NULL, &changes );
		require_noerr( err, exit );
		if( changes > 0 )
		{
			err = URLDecodeCopy( valuePtr, valueLen, &valueStorage, &valueLen );
			require_noerr( err, exit );
			valuePtr = valueStorage;
		}
	}
	
	*outNamePtr			= namePtr;
	*outNameLen			= nameLen;
	*outNameStorage		= nameStorage;
	nameStorage			= NULL;
	
	if( outValuePtr ) *outValuePtr = valuePtr;
	if( outValueLen ) *outValueLen = valueLen;
	if( outValueStorage )
	{
		*outValueStorage	= valueStorage;
		valueStorage		= NULL;
	}
	
exit:
	if( nameStorage )  free( nameStorage );
	if( valueStorage ) free( valueStorage );
	return( err );
}

//===========================================================================================================================
//	URLGetNextVariable
//===========================================================================================================================

OSStatus
	URLGetNextVariable( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outSrc )
{
	OSStatus		err;
	char			c;
	const char *	namePtr;
	const char *	nameEnd;
	const char *	valuePtr;
	const char *	valueEnd;
	
	require_action_quiet( inSrc < inEnd, exit, err = kNotFoundErr );
	
	// Variables are in the form: "name1=value1&name2=value2"
	
	c = '\0';
	namePtr = inSrc;
	while( ( inSrc < inEnd ) && ( ( c = *inSrc ) != '=' ) && ( c != '&' ) ) ++inSrc;
	nameEnd = inSrc;
	if( inSrc < inEnd ) ++inSrc;
	
	if( c == '=' )
	{
		valuePtr = inSrc;
		while( ( inSrc < inEnd ) && ( *inSrc != '&' ) ) ++inSrc;
		valueEnd = inSrc;
		if( inSrc < inEnd ) ++inSrc;
	}
	else
	{
		valuePtr = NULL;
		valueEnd = NULL;
	}
	
	*outNamePtr		= namePtr;
	*outNameLen		= (size_t)( nameEnd - namePtr );
	*outValuePtr	= valuePtr;
	*outValueLen	= (size_t)( valueEnd - valuePtr );
	*outSrc			= inSrc;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	URLGetOrCopyVariable
//===========================================================================================================================

OSStatus
	URLGetOrCopyVariable( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char **			outValueStorage, 
		const char **	outSrc )
{
	OSStatus		err;
	const char *	namePtr		= NULL;
	size_t			nameLen		= 0;
	char *			nameStorage	= NULL;
	const char *	valuePtr	= NULL;
	size_t			valueLen;
	char *			valueStorage;
	int				match;
	int				changes;
	
	while( ( err = URLGetOrCopyNextVariable( inSrc, inEnd, 
		&namePtr,  &nameLen,  &nameStorage, 
		&valuePtr, &valueLen, NULL, 
		&inSrc ) ) == kNoErr )
	{
		match = ( strncmpx( namePtr, nameLen, inName ) == 0 );
		if( nameStorage ) free( nameStorage );
		if( !match ) continue;
		
		valueStorage = NULL;
		if( outValueStorage )
		{
			err = URLDecodeEx( valuePtr, valueLen, NULL, 0, NULL, &changes );
			require_noerr( err, exit );
			if( changes > 0 )
			{
				err = URLDecodeCopy( valuePtr, valueLen, &valueStorage, &valueLen );
				require_noerr( err, exit );
				valuePtr = valueStorage;
			}
		}
		
		if( outValuePtr )		*outValuePtr		= valuePtr;
		if( outValueLen )		*outValueLen		= valueLen;
		if( outValueStorage )	*outValueStorage	= valueStorage;
		if( outSrc )			*outSrc				= inSrc;
		return( kNoErr );
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	URLGetVariable
//===========================================================================================================================

OSStatus
	URLGetVariable( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outSrc )
{
	OSStatus		err;
	const char *	namePtr;
	size_t			nameLen;
	const char *	valuePtr;
	size_t			valueLen;
	
	namePtr  = NULL;
	nameLen  = 0;
	valuePtr = NULL;
	valueLen = 0;
	while( ( err = URLGetNextVariable( inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &inSrc ) ) == kNoErr )
	{
		if( strncmpx( namePtr, nameLen, inName ) == 0 )
		{
			if( outValuePtr )	*outValuePtr	= valuePtr;
			if( outValueLen )	*outValueLen	= valueLen;
			if( outSrc )		*outSrc			= inSrc;
			return( kNoErr );
		}
	}
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	URLUtils_Test
//===========================================================================================================================

OSStatus	URLUtils_Test( void )
{
	OSStatus			err;
	char				buf[ 256 ];
	size_t				len;
	const char *		src;
	const char *		end;
	const char *		ptr;
	URLComponents		urlComps;
	const char *		namePtr;
	size_t				nameLen;
	char *				nameStorage;
	const char *		valuePtr;
	size_t				valueLen;
	char *				valueStorage;
	int					i;
	char *				str;
	
	// Encoding/Decoding
	
	src = "This That";
	len = strlen( src );
	err = URLEncode( kURLEncodeType_Query, src, len, buf, sizeof( buf ), &len );
	require_noerr( err, exit );
	require_action( strncmpx( buf, len, "This%20That" ) == 0, exit, err = kResponseErr );
	
	src = "This%20That";
	len = strlen( src );
	err = URLDecode( src, len, buf, sizeof( buf ), &len );
	require_noerr( err, exit );
	require_action( strncmpx( buf, len, "This That" ) == 0, exit, err = kResponseErr );
	
	src = "name=this&that";
	len = strlen( src );
	err = URLEncode( kURLEncodeType_Query, src, len, buf, sizeof( buf ), &len );
	require_noerr( err, exit );
	require_action( strncmpx( buf, len, "name=this%26that" ) == 0, exit, err = kResponseErr );
	
	src = "0017F2F790F0@abc's \xEF\xA3\xBFtv";
	len = strlen( src );
	err = URLEncode( kURLEncodeType_Query, src, len, buf, sizeof( buf ), &len );
	require_noerr( err, exit );
	require_action( strncmpx( buf, len, "0017F2F790F0%40abc's%20%EF%A3%BFtv" ) == 0, exit, err = kResponseErr );
	
	src = "0017F2F790F0%40abc's%20%EF%A3%BFtv";
	len = strlen( src );
	err = URLDecode( src, len, buf, sizeof( buf ), &len );
	require_noerr( err, exit );
	require_action( strncmpx( buf, len, "0017F2F790F0@abc's \xEF\xA3\xBFtv" ) == 0, exit, err = kResponseErr );
	
	str = NULL;
	err = URLEncodeCopy( kURLEncodeType_Query, "0017F2F790F0@abc's \xEF\xA3\xBFtv", kSizeCString, &str, &len );
	require_noerr( err, exit );
	require_action( strcmp( str, "0017F2F790F0%40abc's%20%EF%A3%BFtv" ) == 0, exit, err = kResponseErr );
	require_action( len == strlen( str ), exit, err = kResponseErr );
	free( str );
	
	str = NULL;
	err = URLDecodeCopy( "0017F2F790F0%40abc's%20%EF%A3%BFtv", kSizeCString, &str, &len );
	require_noerr( err, exit );
	require_action( strcmp( str, "0017F2F790F0@abc's \xEF\xA3\xBFtv" ) == 0, exit, err = kResponseErr );
	require_action( len == strlen( str ), exit, err = kResponseErr );
	free( str );
	
	// Parsing
	
	src = "ftp://tom:secret@abc.com/test?x#y";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "ftp" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.userPtr, urlComps.userLen, "tom" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.passwordPtr, urlComps.passwordLen, "secret" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.hostPtr, urlComps.hostLen, "abc.com" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "/test" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.queryPtr, urlComps.queryLen, "x" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.fragmentPtr, urlComps.fragmentLen, "y" ) == 0, exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "http://www.ics.uci.edu/pub/ietf/uri/#Related";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "http" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.userPtr == NULL ) && ( urlComps.userLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.passwordPtr == NULL ) && ( urlComps.passwordLen == 0 ), exit, err = kResponseErr );
	require_action( strncmpx( urlComps.hostPtr, urlComps.hostLen, "www.ics.uci.edu" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "/pub/ietf/uri/" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.queryPtr == NULL ) && ( urlComps.queryLen == 0 ), exit, err = kResponseErr );
	require_action( strncmpx( urlComps.fragmentPtr, urlComps.fragmentLen, "Related" ) == 0, exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "urn:example:animal:ferret:nose";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "urn" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.userPtr == NULL ) && ( urlComps.userLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.passwordPtr == NULL ) && ( urlComps.passwordLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.hostPtr == NULL ) && ( urlComps.hostLen == 0 ), exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "example:animal:ferret:nose" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.queryPtr == NULL ) && ( urlComps.queryLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.fragmentPtr == NULL ) && ( urlComps.fragmentLen == 0 ), exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "http://joe:secret@www.host.com:123/folder/page.html?cgi-bin123#cool";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "http" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.userPtr, urlComps.userLen, "joe" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.passwordPtr, urlComps.passwordLen, "secret" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.hostPtr, urlComps.hostLen, "www.host.com:123" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "/folder/page.html" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.queryPtr, urlComps.queryLen, "cgi-bin123" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.fragmentPtr, urlComps.fragmentLen, "cool" ) == 0, exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "*";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( ( urlComps.schemePtr == NULL ) && ( urlComps.schemeLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.userPtr == NULL ) && ( urlComps.userLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.passwordPtr == NULL ) && ( urlComps.passwordLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.hostPtr == NULL ) && ( urlComps.hostLen == 0 ), exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "*" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.queryPtr == NULL ) && ( urlComps.queryLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.fragmentPtr == NULL ) && ( urlComps.fragmentLen == 0 ), exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "mailto:frank@wwdcdemo.example.com";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "mailto" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.userPtr == NULL ) && ( urlComps.userLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.passwordPtr == NULL ) && ( urlComps.passwordLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.hostPtr == NULL ) && ( urlComps.hostLen == 0 ), exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "frank@wwdcdemo.example.com" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.queryPtr == NULL ) && ( urlComps.queryLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.fragmentPtr == NULL ) && ( urlComps.fragmentLen == 0 ), exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	src = "file:///foo;bar";
	end = src + strlen( src );
	err = URLParseComponents( src, end, &urlComps, &ptr );
	require_noerr( err, exit );
	require_action( strncmpx( urlComps.schemePtr, urlComps.schemeLen, "file" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.userPtr == NULL ) && ( urlComps.userLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.passwordPtr == NULL ) && ( urlComps.passwordLen == 0 ), exit, err = kResponseErr );
	require_action( urlComps.hostLen == 0, exit, err = kResponseErr );
	require_action( strncmpx( urlComps.pathPtr, urlComps.pathLen, "/foo;bar" ) == 0, exit, err = kResponseErr );
	require_action( ( urlComps.queryPtr == NULL ) && ( urlComps.queryLen == 0 ), exit, err = kResponseErr );
	require_action( ( urlComps.fragmentPtr == NULL ) && ( urlComps.fragmentLen == 0 ), exit, err = kResponseErr );
	require_action( ptr == end, exit, err = kResponseErr );
	
	// URL Segments.
	
	err = URLParseComponents( "http://www.example.com/path/to/my/resource/", NULL, &urlComps, NULL );
	require_noerr( err, exit );
	
	err = URLGetNextPathSegment( &urlComps, &ptr, &len );
	require_noerr( err, exit );
	require_action( ( len == 4 ) && ( memcmp( ptr, "path", len ) == 0 ), exit, err = -1 );
	
	err = URLGetNextPathSegment( &urlComps, &ptr, &len );
	require_noerr( err, exit );
	require_action( ( len == 2 ) && ( memcmp( ptr, "to", len ) == 0 ), exit, err = -1 );
	
	err = URLGetNextPathSegment( &urlComps, &ptr, &len );
	require_noerr( err, exit );
	require_action( ( len == 2 ) && ( memcmp( ptr, "my", len ) == 0 ), exit, err = -1 );
	
	err = URLGetNextPathSegment( &urlComps, &ptr, &len );
	require_noerr( err, exit );
	require_action( ( len == 8 ) && ( memcmp( ptr, "resource", len ) == 0 ), exit, err = -1 );
	
	err = URLGetNextPathSegment( &urlComps, &ptr, &len );
	require_action( err != kNoErr, exit, err = -1 );
	
	// Query variables.
	
	src = "field1=value1&field2=value2&field3=value3";
	end = src + strlen( src );
	i = 0;
	while( URLGetNextVariable( src, end, &namePtr, &nameLen, &valuePtr, &valueLen, &src ) == kNoErr )
	{
		++i;
		if( i == 1 )
		{
			require_action( strncmpx( namePtr, nameLen, "field1" ) == 0, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value1" ) == 0, exit, err = kResponseErr );
		}
		else if( i == 2 )
		{
			require_action( strncmpx( namePtr, nameLen, "field2" ) == 0, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value2" ) == 0, exit, err = kResponseErr );
		}
		else if( i == 3 )
		{
			require_action( strncmpx( namePtr, nameLen, "field3" ) == 0, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value3" ) == 0, exit, err = kResponseErr );
		}
	}
	require_action( i == 3, exit, err = kResponseErr );
	require_action( src == end, exit, err = kResponseErr );
	
	src = "field1=value1&field2=value2&field3=value3";
	end = src + strlen( src );
	err = URLGetVariable( src, end, "field2", &valuePtr, &valueLen, NULL );
	require_noerr( err, exit );
	require_action( strncmpx( valuePtr, valueLen, "value2" ) == 0, exit, err = kResponseErr );
	
	// Encoded Query Variable Iteration.
	
	src = "field1=value1&field+2=value%202&field%203=value3";
	end = src + strlen( src );
	i = 0;
	while( URLGetOrCopyNextVariable( src, end, &namePtr, &nameLen, &nameStorage, 
		&valuePtr, &valueLen, &valueStorage, &src ) == kNoErr )
	{
		++i;
		if( i == 1 )
		{
			require_action( strncmpx( namePtr, nameLen, "field1" ) == 0, exit, err = kResponseErr );
			require_action( nameStorage == NULL, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value1" ) == 0, exit, err = kResponseErr );
			require_action( valueStorage == NULL, exit, err = kResponseErr );
		}
		else if( i == 2 )
		{
			require_action( strncmpx( namePtr, nameLen, "field 2" ) == 0, exit, err = kResponseErr );
			require_action( nameStorage, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value 2" ) == 0, exit, err = kResponseErr );
			require_action( valueStorage, exit, err = kResponseErr );
			free( nameStorage );
			free( valueStorage );
		}
		else if( i == 3 )
		{
			require_action( strncmpx( namePtr, nameLen, "field 3" ) == 0, exit, err = kResponseErr );
			require_action( nameStorage, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "value3" ) == 0, exit, err = kResponseErr );
			require_action( valueStorage == NULL, exit, err = kResponseErr );
			free( nameStorage );
		}
	}
	require_action( i == 3, exit, err = kResponseErr );
	require_action( src == end, exit, err = kResponseErr );
	
	// Encoded Query Variable Extraction.
	
	src = "field1=value1&field+2=value%202&field%203=value3";
	end = src + strlen( src );
	
	err = URLGetOrCopyVariable( src, end, "field1", &valuePtr, &valueLen, &valueStorage, NULL );
	require_noerr( err, exit );
	require_action( strncmpx( valuePtr, valueLen, "value1" ) == 0, exit, err = kResponseErr );
	require_action( valueStorage == NULL, exit, err = kResponseErr );
	
	err = URLGetOrCopyVariable( src, end, "field 2", &valuePtr, &valueLen, &valueStorage, NULL );
	require_noerr( err, exit );
	require_action( strncmpx( valuePtr, valueLen, "value 2" ) == 0, exit, err = kResponseErr );
	require_action( valueStorage, exit, err = kResponseErr );
	free( valueStorage );
	
	err = URLGetOrCopyVariable( src, end, "field 3", &valuePtr, &valueLen, &valueStorage, NULL );
	require_noerr( err, exit );
	require_action( strncmpx( valuePtr, valueLen, "value3" ) == 0, exit, err = kResponseErr );
	require_action( valueStorage == NULL, exit, err = kResponseErr );
	
	err = URLGetOrCopyVariable( src, end, "field 4", &valuePtr, &valueLen, &valueStorage, NULL );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	printf( "URLUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
