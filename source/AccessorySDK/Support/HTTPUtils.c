/*
	File:    	HTTPUtils.c
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
	
	Copyright (C) 2007-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "HTTPUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "StringUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <ctype.h>
	#include <stddef.h>
	#include <stdlib.h>
	#include <string.h>
	#include <time.h>
#endif

#if( TARGET_OS_POSIX )
	#include <fcntl.h>
	#include <sys/uio.h>
#endif

#if( TARGET_HAS_SOCKETS )
	#include "NetUtils.h"
#endif

//===========================================================================================================================
//	Private
//===========================================================================================================================

// A token in the HTTP spec is defined using the following rules:
//
// token          = 1*<any CHAR except CTLs or separators>
// CTL            = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
// separators     = "(" | ")" | "<" | ">" | "@"
//                | "," | ";" | ":" | "\" | <">
//                | "/" | "[" | "]" | "?" | "="
//                | "{" | "}" | SP | HT
//
// We intentionally do not treat '/' as a separator because some HTTP-based protocols, such as RTSP, 
// use '/' in tokens (e.g. "RTP/AVP/UDP;unicast") so we have to exclude that character.

#define IsHTTPTokenDelimiterChar( C )	( ( ( C ) < 32 ) || ( ( C ) >= 127 ) || IsHTTPSeparatorChar( C ) )
#define IsHTTPSeparatorChar( C )		( strchr( "()<>@,;:\\\"[]?={} \t", ( C ) ) != NULL )

#if 0
#pragma mark == HTTP Headers ==
#endif

//===========================================================================================================================
//	HTTPHeader_InitRequest
//===========================================================================================================================

OSStatus	HTTPHeader_InitRequest( HTTPHeader *inHeader, const char *inMethod, const char *inURL, const char *inProtocol )
{
	OSStatus		err;
	int				methodLen, urlOffset, urlEnd, n;
	
	n = snprintf( inHeader->buf, sizeof( inHeader->buf ), "%s%n %n%s%n %s\r\n",
		inMethod, &methodLen, &urlOffset, inURL, &urlEnd, inProtocol ? inProtocol : "HTTP/1.1" );
	require_action( ( n > 0 ) && ( n < ( (int) sizeof( inHeader->buf ) ) ), exit, err = kOverrunErr );
	
	inHeader->methodPtr = inHeader->buf;
	inHeader->methodLen = (size_t) methodLen;
	
	inHeader->urlPtr	= inHeader->buf + urlOffset;
	inHeader->urlLen 	= (size_t)( urlEnd - urlOffset );
	
	inHeader->len = (size_t) n;
	err = kNoErr;
	
exit:
	inHeader->firstErr = err;
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_InitResponse
//===========================================================================================================================

OSStatus
	HTTPHeader_InitResponse(
		HTTPHeader *	inHeader,
		const char *	inProtocol,
		int				inStatusCode,
		const char *	inReasonPhrase )
{
	OSStatus		err;
	int				n;
	
	if( !inProtocol )		inProtocol		= "HTTP/1.1";
	if( !inReasonPhrase )	inReasonPhrase	= HTTPGetReasonPhrase( inStatusCode );
	
	n = snprintf( inHeader->buf, sizeof( inHeader->buf ), "%s %u %s\r\n", inProtocol, inStatusCode, inReasonPhrase );
	require_action( ( n > 0 ) && ( n < ( (int) sizeof( inHeader->buf ) ) ), exit, err = kOverrunErr );
	
	inHeader->len = (size_t) n;
	err = kNoErr;
	
exit:
	inHeader->firstErr = err;
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_Commit
//===========================================================================================================================

OSStatus	HTTPHeader_Commit( HTTPHeader *inHeader )
{
	OSStatus		err;
	char *			buf;
	size_t			len;
	
	err = inHeader->firstErr;
	require_noerr_string( err, exit, "earlier error occurred" );
	
	buf = inHeader->buf;
	len = inHeader->len;
	require_action_string( len > 0, exit, err = kNotPreparedErr, "header not initialized" );
	
	// Append the final empty line to indicate the end of the header.
	
	require_action( ( len + 2 ) < sizeof( inHeader->buf ), exit, err = kOverrunErr );
	buf[ len++ ] = '\r';
	buf[ len++ ] = '\n';
	inHeader->len = len;
	
	inHeader->firstErr = kAlreadyInUseErr; // Mark in-use to prevent further changes to it.
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_Uncommit
//===========================================================================================================================

OSStatus	HTTPHeader_Uncommit( HTTPHeader *inHeader )
{
	OSStatus		err;
	char *			buf;
	size_t			len;
	
	require_action( inHeader->firstErr == kAlreadyInUseErr, exit, err = kStateErr );
	
	buf = inHeader->buf;
	len = inHeader->len;
	require_action( len > 4, exit, err = kSizeErr );
	require_action( len < sizeof( inHeader->buf ), exit, err = kSizeErr );
	require_action( buf[ len - 4 ] == '\r', exit, err = kMalformedErr );
	require_action( buf[ len - 3 ] == '\n', exit, err = kMalformedErr );
	require_action( buf[ len - 2 ] == '\r', exit, err = kMalformedErr );
	require_action( buf[ len - 1 ] == '\n', exit, err = kMalformedErr );
	
	inHeader->len = len - 2;
	inHeader->firstErr = kNoErr;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_AddField
//===========================================================================================================================

OSStatus	HTTPHeader_AddField( HTTPHeader *inHeader, const char *inName, const char *inValue )
{
	OSStatus			err;
	char *				buf;
	size_t				len;
	size_t				maxLen;
	int					n;
	
	err = inHeader->firstErr;
	require_noerr( err, exit );
	
	buf = inHeader->buf;
	len = inHeader->len;
	maxLen = sizeof( inHeader->buf );
	require_action_string( len > 2, exit, err = kNotPreparedErr, "header not initialized" );
	require_action_string( len < maxLen, exit, err = kNotPreparedErr, "bad header length" );
	
	maxLen = maxLen - len;
	n = snprintf( &buf[ len ], maxLen, "%s: %s\r\n", inName, inValue );
	require_action( ( n > 0 ) && ( n < ( (int) maxLen ) ), exit, err = kOverrunErr );
	
	inHeader->len += ( (size_t) n );
	err = kNoErr;
	
exit:
	if( err && !inHeader->firstErr ) inHeader->firstErr = err;
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_AddFieldF
//===========================================================================================================================

OSStatus	HTTPHeader_AddFieldF( HTTPHeader *inHeader, const char *inName, const char *inFormat, ... )
{
	OSStatus			err;
	char *				buf;
	size_t				len;
	size_t				maxLen;
	va_list				args;
	int					n;
	
	err = inHeader->firstErr;
	require_noerr( err, exit );
	
	buf = inHeader->buf;
	len = inHeader->len;
	maxLen = sizeof( inHeader->buf );
	require_action_string( len > 2, exit, err = kNotPreparedErr, "header not initialized" );
	require_action_string( len < maxLen, exit, err = kNotPreparedErr, "bad header length" );

	maxLen = maxLen - len;
	n = snprintf( &buf[ len ], maxLen, "%s: ", inName );
	va_start( args, inFormat );
	n += vsnprintf( &buf[ len + n ], maxLen - n, inFormat, args );
	va_end( args );
	n += snprintf( &buf[ len + n ], maxLen - n, "\r\n" );
	require_action( ( n > 0 ) && ( n < ( (int) maxLen ) ), exit, err = kOverrunErr );
	
	inHeader->len += ( (size_t) n );
	err = kNoErr;
	
exit:
	if( err && !inHeader->firstErr ) inHeader->firstErr = err;
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HTTPHeader_Parse
//
//	Parses an HTTP header. This assumes the "buf" and "len" fields are set. The other fields are set by this function.
//===========================================================================================================================

OSStatus	HTTPHeader_Parse( HTTPHeader *ioHeader )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	const char *		ptr;
	size_t				len;
	char				c;
	const char *		value;
	int					x;
	
	require_action( ioHeader->len < sizeof( ioHeader->buf ), exit, err = kParamErr );
	
	// Reset fields up-front to good defaults to simplify handling of unused fields later.
	
	ioHeader->methodPtr			= "";
	ioHeader->methodLen			= 0;
	ioHeader->method			= kHTTPMethod_Unset;
	ioHeader->urlPtr			= "";
	ioHeader->urlLen			= 0;
	memset( &ioHeader->url, 0, sizeof( ioHeader->url ) );
	ioHeader->protocolPtr		= "";
	ioHeader->protocolLen		= 0;
	ioHeader->statusCode		= -1;
	ioHeader->reasonPhrasePtr	= "";
	ioHeader->reasonPhraseLen	= 0;
	ioHeader->channelID			= 0;
	ioHeader->contentLength		= 0;
	ioHeader->persistent		= false;
	
	src = ioHeader->buf;
	
	// Parse the start line. This will also determine if it's a request or response.
	// Requests are in the format <method> <url> <protocol>/<majorVersion>.<minorVersion>, for example:
	//
	//		GET /abc/xyz.html HTTP/1.1
	//		GET http://www.host.com/abc/xyz.html HTTP/1.1
	//		GET http://user:password@www.host.com/abc/xyz.html HTTP/1.1
	//
	// Responses are in the format <protocol>/<majorVersion>.<minorVersion> <statusCode> <reasonPhrase>, for example:
	//
	//		HTTP/1.1 404 Not Found
	
	ptr = src;
	end = src + ioHeader->len;
	for( c = 0; ( ptr < end ) && ( ( c = *ptr ) != ' ' ) && ( c != '/' ); ++ptr ) {}
	require_action( ptr < end, exit, err = kMalformedErr );
	
	if( c == ' ' ) // Requests have a space after the method. Responses have '/' after the protocol.
	{
		ioHeader->methodPtr = src;
		ioHeader->methodLen = len = (size_t)( ptr - src );
		++ptr;
		if(      strnicmpx( src, len, "GET" )		== 0 ) ioHeader->method = kHTTPMethod_GET;
		else if( strnicmpx( src, len, "POST" )		== 0 ) ioHeader->method = kHTTPMethod_POST;
		else if( strnicmpx( src, len, "PUT" )		== 0 ) ioHeader->method = kHTTPMethod_PUT;
		else if( strnicmpx( src, len, "DELETE" )	== 0 ) ioHeader->method = kHTTPMethod_DELETE;
		
		// Parse the URL.
		
		ioHeader->urlPtr = ptr;
		while( ( ptr < end ) && ( *ptr != ' ' ) ) ++ptr;
		ioHeader->urlLen = (size_t)( ptr - ioHeader->urlPtr );
		require_action( ptr < end, exit, err = kMalformedErr );
		++ptr;
		
		err = URLParseComponents( ioHeader->urlPtr, ioHeader->urlPtr + ioHeader->urlLen, &ioHeader->url, NULL );
		require_noerr( err, exit );
		
		// Parse the protocol and version.
		
		ioHeader->protocolPtr = ptr;
		while( ( ptr < end ) && ( ( c = *ptr ) != '\r' ) && ( c != '\n' ) ) ++ptr;
		ioHeader->protocolLen = (size_t)( ptr - ioHeader->protocolPtr );
		require_action( ptr < end, exit, err = kMalformedErr );
		++ptr;
	}
	else // Response
	{
		// Parse the protocol version.
		
		ioHeader->protocolPtr = src;
		for( ++ptr; ( ptr < end ) && ( *ptr != ' ' ); ++ptr ) {}
		ioHeader->protocolLen = (size_t)( ptr - ioHeader->protocolPtr );
		require_action( ptr < end, exit, err = kMalformedErr );
		++ptr;
		
		// Parse the status code.
		
		x = 0;
		for( c = 0; ( ptr < end ) && ( ( c = *ptr ) >= '0' ) && ( c <= '9' ); ++ptr ) x = ( x * 10 ) + ( c - '0' ); 
		ioHeader->statusCode = x;
		if( c == ' ' ) ++ptr;
		
		// Parse the reason phrase.
		
		ioHeader->reasonPhrasePtr = ptr;
		while( ( ptr < end ) && ( ( c = *ptr ) != '\r' ) && ( c != '\n' ) ) ++ptr;
		ioHeader->reasonPhraseLen = (size_t)( ptr - ioHeader->reasonPhrasePtr );
		require_action( ptr < end, exit, err = kMalformedErr );
		++ptr;
	}
	
	// There should at least be a blank line after the start line so make sure there's more data.
	
	require_action( ptr < end, exit, err = kMalformedErr );
	
	// Determine persistence. Note: HTTP 1.0 defaults to non-persistent if a Connection header field is not present.
	
	err = HTTPGetHeaderField( ioHeader->buf, ioHeader->len, kHTTPHeader_Connection, NULL, NULL, &value, &len, NULL );
	if( err )	ioHeader->persistent = (Boolean)( strnicmpx( ioHeader->protocolPtr, ioHeader->protocolLen, "HTTP/1.0" ) != 0 );
	else		ioHeader->persistent = (Boolean)( strnicmpx( value, len, "close" ) != 0 );
	
	// Content-Length is such a common field that we get it here during general parsing.
	
	HTTPScanFHeaderValue( ioHeader->buf, ioHeader->len, "Content-Length", "%llu", &ioHeader->contentLength );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPHeader_Validate
//
//	Parses for the end of an HTTP header and updates the HTTPHeader structure so it's ready to parse. Returns true if valid.
//	This assumes the "buf" and "len" fields are set. The other fields are set by this function.
//===========================================================================================================================

Boolean	HTTPHeader_Validate( HTTPHeader *inHeader )
{
	const char *		src;
	const char *		end;
	
	require( inHeader->len < sizeof( inHeader->buf ), exit );
	src = inHeader->buf;
	end = src + inHeader->len;

	// Search for an empty line (HTTP-style header/body separator). CRLFCRLF, LFCRLF, or LFLF accepted.
	// $$$ TO DO: Start from the last search location to avoid re-searching the same data over and over.
	
	for( ;; )
	{
		while( ( src < end ) && ( src[ 0 ] != '\n' ) ) ++src;
		if( src >= end ) goto exit;
		++src;
		if( ( ( end - src ) >= 2 ) && ( src[ 0 ] == '\r' ) && ( src[ 1 ] == '\n' ) ) // CFLFCRLF or LFCRLF
		{
			src += 2;
			break;
		}
		else if( ( ( end - src ) >= 1 ) && ( src[ 0 ] == '\n' ) ) // LFLF
		{
			src += 1;
			break;
		}
	}
	inHeader->extraDataPtr	= src;
	inHeader->extraDataLen	= (size_t)( end - src );
	inHeader->len			= (size_t)( src - inHeader->buf );
	return( true );
	
exit:
	return( false );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HTTPGetHeaderField
//===========================================================================================================================

OSStatus
	HTTPGetHeaderField( 
		const char *	inHeaderPtr, 
		size_t			inHeaderLen, 
		const char *	inName, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNext )
{
	const char *		src;
	const char *		end;
	size_t				matchLen;
	char				c;
	
	if( inHeaderLen == kSizeCString ) inHeaderLen = strlen( inHeaderPtr );
	src = inHeaderPtr;
	end = src + inHeaderLen;
	matchLen = inName ? strlen( inName ) : 0;
	for( ;; )
	{
		const char *		linePtr;
		const char *		lineEnd;
		size_t				lineLen;
		const char *		valuePtr;
		const char *		valueEnd;
		
		// Parse a line and check if it begins with the header field we're looking for.
		
		linePtr = src;
		while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
		if( src >= end ) break;
		lineEnd = src;
		lineLen = (size_t)( src - linePtr );
		if( ( src < end ) && ( *src == '\r' ) ) ++src;
		if( ( src < end ) && ( *src == '\n' ) ) ++src;
		
		if( !inName ) // Null name means to find the next header for iteration.
		{
			const char *		nameEnd;
			
			nameEnd = linePtr;
			while( ( nameEnd < lineEnd ) && ( *nameEnd != ':' ) ) ++nameEnd;
			if( nameEnd >= lineEnd ) continue;
			matchLen = (size_t)( nameEnd - linePtr );
		}
		else if( ( lineLen <= matchLen ) || ( linePtr[ matchLen ] != ':' ) || 
				 ( strnicmp( linePtr, inName, matchLen ) != 0 ) )
		{
			continue;
		}
		
		// Found the header field. Separate name and value and skip leading whitespace in the value.
		
		valuePtr = linePtr + matchLen + 1;
		valueEnd = lineEnd;
		while( ( valuePtr < valueEnd ) && ( ( ( c = *valuePtr ) == ' ' ) || ( c == '\t' ) ) ) ++valuePtr;
		
		// If the next line is a continuation line then keep parsing until we get to the true end.
		
		while( ( src < end ) && ( ( ( c = *src ) == ' ' ) || ( c == '\t' ) ) )
		{
			++src;
			while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
			valueEnd = src;
			if( ( src < end ) && ( *src == '\r' ) ) ++src;
			if( ( src < end ) && ( *src == '\n' ) ) ++src;
		}
		
		if( outNamePtr )	*outNamePtr		= linePtr;
		if( outNameLen )	*outNameLen		= matchLen;
		if( outValuePtr )	*outValuePtr	= valuePtr;
		if( outValueLen )	*outValueLen	= (size_t)( valueEnd - valuePtr );
		if( outNext )		*outNext		= src;
		return( kNoErr );
	}
	return( kNotFoundErr );
}

//===========================================================================================================================
//	HTTPMakeDateString
//===========================================================================================================================

char *	HTTPMakeDateString( time_t inTime, char *inBuffer, size_t inMaxLen )
{
	struct tm		tmTmp;
	struct tm *		tmPtr;
	
	tmPtr = gmtime_r( &inTime, &tmTmp );
	require_action_quiet( tmPtr, exit, inBuffer = "" );
	
	inBuffer[ 0 ] = '\0';
	strftime( inBuffer, inMaxLen, "%a, %d %b %Y %H:%M:%S GMT", tmPtr );
	
exit:
	return( inBuffer );
}

//===========================================================================================================================
//	HTTPParseByteRangeRequest
//===========================================================================================================================

OSStatus	HTTPParseByteRangeRequest( const char *inStr, size_t inLen, int64_t *outStart, int64_t *outEnd )
{
	OSStatus			err;
	int64_t				rangeStart;
	int64_t				rangeEnd;
	const char *		ptr;
	const char *		end;
	const char *		src;
	char				c;
	int64_t				x;
	
	if( inLen == kSizeCString ) inLen = strlen( inStr );
	
	// Make sure the units are "bytes".
	
	require_action_quiet( inLen >= sizeof_string( "bytes=" ), exit, err = kUnderrunErr );
	require_action_quiet( memcmp( inStr, "bytes=", sizeof_string( "bytes=" ) ) == 0, exit, err = kTypeErr );
	src = inStr + sizeof_string( "bytes=" );
	end = inStr + inLen;
	require_action_quiet( src < end, exit, err = kUnderrunErr );
	
	// See RFC 2616 section 14.35 for the exact details, but this parses an HTTP byte range request in one of 3 forms:
	//
	//		"bytes=1000-1999"	- Bytes 1000-1999 inclusive (byte at offset 1999 *is* included).
	//		"bytes=1000-"		- Bytes 1000 to the end of the file.
	//		"bytes=-1000"		- Last 1000 bytes of the file.
	
	if( *src == '-' )	// Last N bytes (e.g. "-1000")
	{
		x = 0;
		for( ptr = ++src; ( ptr < end ) && ( ( c = *ptr ) >= '0' ) && ( c <= '9' ); ++ptr ) x = ( x * 10 ) + ( c - '0' ); 
		require_action_quiet( src < ptr, exit, err = kValueErr );
		
		rangeStart = -x;
		rangeEnd   = -1;
	}
	else
	{
		c = 0;
		x = 0;
		for( ptr = src; ( ptr < end ) && ( ( c = *ptr ) >= '0' ) && ( c <= '9' ); ++ptr ) x = ( x * 10 ) + ( c - '0' ); 
		require_action_quiet( src < ptr, exit, err = kValueErr );
		require_action_quiet( c == '-', exit, err = kMalformedErr );
		rangeStart = x;
		++ptr;
		
		if( ptr < end )	// Bytes from an explicit range (e.g. "1000-1999")
		{
			x = 0;
			for( ; ( ptr < end ) && ( ( c = *ptr ) >= '0' ) && ( c <= '9' ); ++ptr ) x = ( x * 10 ) + ( c - '0' ); 
			require_action_quiet( src < ptr, exit, err = kValueErr );
			
			rangeEnd = x + 1; // HTTP uses inclusive ends so make exclusive to simplify work for the caller.
			require_action_quiet( rangeStart < rangeEnd, exit, err = kRangeErr );
		}
		else			// Bytes from an offset to the end of the file (e.g. "1000-").
		{
			rangeEnd = -1;
		}
	}
	
	*outStart = rangeStart;
	*outEnd   = rangeEnd;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPParseParameter
//===========================================================================================================================

OSStatus
	HTTPParseParameter( 
		const void *	inSrc, 
		const void *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char *			outDelimiter, 
		const char **	outSrc )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	const char *		namePtr;
	const char *		nameEnd;
	const char *		valuePtr;
	const char *		valueEnd;
	char				delimiter;
	
	src = (const char *) inSrc;
	end = (const char *) inEnd;
	require_action_quiet( src < end, exit, err = kEndingErr );
	
	// Parse the name.
	
	while( ( src < end ) && isspace_safe( *src ) ) ++src; // Skip whitespace before name.
	err = HTTPParseToken( src, end, &namePtr, &nameEnd, &src );
	require_noerr_quiet( err, exit );
	while( ( src < end ) && isspace_safe( *src ) ) ++src; // Skip whitespace after name.
	
	// Parse the optional value.
	
	valuePtr = NULL;
	valueEnd = NULL;
	if( ( src < end ) && ( *src == '=' ) )
	{
		++src;
		while( ( src < end ) && isspace_safe( *src ) ) ++src; // Skip whitespace before value.
		if( ( src < end ) && ( *src == '\"' ) ) // Quoted String
		{
			valuePtr = ++src;
			while( ( src < end ) && ( ( *src != '"' ) || ( src[ -1 ] == '\\' ) ) ) ++src;
			require_action_quiet( src < end, exit, err = kMalformedErr );
			valueEnd = src++;
		}
		else
		{
			err = HTTPParseToken( src, end, &valuePtr, &valueEnd, &src );
			require_noerr_quiet( err, exit );
		}
		while( ( src < end ) && isspace_safe( *src ) ) ++src; // Skip whitespace after value.
	}
	
	// Skip the delimiter and any trailing whitespace.
	
	delimiter = '\0';
	if( ( src < end ) && IsHTTPSeparatorChar( *src ) )
	{
		delimiter = *src++;
		while( ( src < end ) && isspace_safe( *src ) ) ++src;
	}
	
	if( outNamePtr )	*outNamePtr		= namePtr;
	if( outNameLen )	*outNameLen		= (size_t)( nameEnd - namePtr );
	if( outValuePtr )	*outValuePtr	= valuePtr;
	if( outValueLen )	*outValueLen	= (size_t)( valueEnd - valuePtr );
	if( outDelimiter )	*outDelimiter	= delimiter;
	if( outSrc )		*outSrc			= src;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPParseRTPInfo
//===========================================================================================================================

OSStatus	HTTPParseRTPInfo( const char *inHeaderPtr, size_t inHeaderLen, uint16_t *outSeq, uint32_t *outTS )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	size_t				len;
	bool				gotSeq;
	bool				gotTS;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	uint16_t			seq;
	uint32_t			ts;
	unsigned int		x;
	int					n;
	
	// Parse the RTP-Info header field such as "RTP-Info: seq=1387;rtptime=488224".
	
	err = HTTPGetHeaderField( inHeaderPtr, inHeaderLen, "RTP-Info", NULL, NULL, &src, &len, NULL );
	require_noerr_quiet( err, exit );
	end = src + len;
	
	gotSeq	= false;
	gotTS	= false;
	seq		= 0;
	ts		= 0;
	while( HTTPParseParameter( src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src ) == kNoErr )
	{
		if( strnicmpx( namePtr, nameLen, "seq" ) == 0 )
		{
			n = SNScanF( valuePtr, valueLen, "%u", &x );
			require_action( n == 1, exit, err = kMalformedErr );
			seq = (uint16_t) x;
			gotSeq = true;
		}
		else if( strnicmpx( namePtr, nameLen, "rtptime" ) == 0 )
		{
			n = SNScanF( valuePtr, valueLen, "%u", &x );
			require_action( n == 1, exit, err = kMalformedErr );
			ts = x;
			gotTS = true;
		}
	}
	if( outSeq )
	{
		require_action( gotSeq, exit, err = kNotFoundErr );
		*outSeq = seq;
	}
	if( outTS )
	{
		require_action( gotTS, exit, err = kNotFoundErr );
		*outTS = ts;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPParseToken
//===========================================================================================================================

OSStatus
	HTTPParseToken( 
		const void *	inSrc, 
		const void *	inEnd, 
		const char **	outTokenPtr, 
		const char **	outTokenEnd, 
		const char **	outSrc )
{
	const unsigned char *		src;
	const unsigned char *		end;
	unsigned char				c;
	
	src = (const unsigned char *) inSrc;
	end = (const unsigned char *) inEnd;
	for( ; src < end; ++src )
	{
		c = *src;
		if( IsHTTPTokenDelimiterChar( c ) )
		{
			break;
		}
	}
	
	if( outTokenPtr ) *outTokenPtr = (const char *) inSrc;
	if( outTokenEnd ) *outTokenEnd = (const char *) src;
	if( outSrc )	  *outSrc	   = (const char *) src;
	return( kNoErr );
}

//===========================================================================================================================
//	HTTPScanFHeaderValue
//===========================================================================================================================

int	HTTPScanFHeaderValue( const char *inHeaderPtr, size_t inHeaderLen, const char *inName, const char *inFormat, ... )
{
	int					n;
	const char *		valuePtr;
	size_t				valueLen;
	va_list				args;
	
	n = (int) HTTPGetHeaderField( inHeaderPtr, inHeaderLen, inName, NULL, NULL, &valuePtr, &valueLen, NULL );
	require_noerr_quiet( n, exit );
	
	va_start( args, inFormat );
	n = VSNScanF( valuePtr, valueLen, inFormat, args );
	va_end( args );
	
exit:
	return( n );	
}

#if( TARGET_HAS_SOCKETS )

#if 0
#pragma mark -
#pragma mark == Networking ==
#endif

//===========================================================================================================================
//	HTTPReadHeader
//===========================================================================================================================

OSStatus	HTTPReadHeader( HTTPHeader *inHeader, NetTransportRead_f inRead_f, void *inRead_ctx )
{
	OSStatus		err;
	char *			buf;
	char *			src;
	char *			dst;
	char *			lim;
	char *			end;
	size_t			len;
	
	buf = inHeader->buf;
	src = buf;
	dst = buf + inHeader->len;
	lim = buf + sizeof( inHeader->buf );
	for( ;; )
	{
		// If there's data from a previous read, move it to the front to search it first.
		
		len = inHeader->extraDataLen;
		if( len > 0 )
		{
			require_action( len <= (size_t)( lim - dst ), exit, err = kParamErr );
			memmove( dst, inHeader->extraDataPtr, len );
			inHeader->extraDataLen = 0;
		}
		else
		{
			len = (size_t)( lim - dst );
			require_action( len > 0, exit, err = kNoSpaceErr );
			err = inRead_f( dst, len, &len, inRead_ctx );
			require_noerr_quiet( err, exit );
		}
		dst += len;
		inHeader->len += len;
		
		// Find an empty line (separates the header and body). The HTTP spec defines it as CRLFCRLF, but some
		// use LFLF or weird combos like CRLFLF so this handles CRLFCRLF, LFLF, and CRLFLF (but not CRCR).
		
		end = dst;
		for( ;; )
		{
			while( ( src < end ) && ( *src != '\n' ) ) ++src;
			if( src >= end ) break;
			
			len = (size_t)( end - src );
			if( ( len >= 3 ) && ( src[ 1 ] == '\r' ) && ( src[ 2 ] == '\n' ) ) // CRLFCRLF or LFCRLF.
			{
				end = src + 3;
				goto foundHeader;
			}
			else if( ( len >= 2 ) && ( src[ 1 ] == '\n' ) ) // LFLF or CRLFLF.
			{
				end = src + 2;
				goto foundHeader;
			}
			else if( len < 3 )
			{
				break;
			}
			++src;
		}
	}
	
foundHeader:
	inHeader->len = (size_t)( end - buf );
	err = HTTPHeader_Parse( inHeader );
	require_noerr( err, exit );
	inHeader->extraDataPtr = end;
	inHeader->extraDataLen = (size_t)( dst - end );
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPReadLine
//===========================================================================================================================

OSStatus	HTTPReadLine( HTTPHeader *inHeader, NetTransportRead_f inRead_f, void *inRead_ctx, const char **outPtr, size_t *outLen )
{
	OSStatus			err;
	const char *		buf;
	const char *		src;
	const char *		end;
	const char *		end2;
	char *				dst;
	char *				lim;
	size_t				len;
	
	for( ;; )
	{
		// Search for a line ending. May be either CRLF or LF.
		
		buf = inHeader->extraDataPtr;
		src = buf;
		end = buf + inHeader->extraDataLen;
		while( ( src < end ) && ( *src != '\n' ) ) ++src;
		if( src < end )
		{
			end2 = ( ( src > buf ) && ( src[ -1 ] == '\r' ) ) ? ( src - 1 ) : src;
			*outPtr = buf;
			*outLen = (size_t)( end2 - buf );
			inHeader->extraDataPtr = ++src;
			inHeader->extraDataLen = (size_t)( end - src );
			err = kNoErr;
			break;
		}
		
		// Make sure the extra data is moved back to the header structure in case it was stored externally.
		
		dst = &inHeader->buf[ inHeader->len ];
		lim = inHeader->buf + sizeof( inHeader->buf );
		len = (size_t)( lim - dst );
		require_action( inHeader->extraDataLen <= len, exit, err = kSizeErr );
		if( dst != inHeader->extraDataPtr )
		{
			memmove( dst, inHeader->extraDataPtr, inHeader->extraDataLen );
			inHeader->extraDataPtr = dst;
		}
		dst += inHeader->extraDataLen;
		
		// Try to read more data.
		
		len = (size_t)( lim - dst );
		require_action( len > 0, exit, err = kNoSpaceErr );
		err = inRead_f( dst, len, &len, inRead_ctx );
		require_noerr_quiet( err, exit );
		inHeader->extraDataLen += len;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_HTTPReadHeader
//===========================================================================================================================

OSStatus	NetSocket_HTTPReadHeader( NetSocketRef inSock, HTTPHeader *inHeader, int32_t inTimeoutSecs )
{
	OSStatus		err;
	size_t			len;
	char *			buf;
	char *			dst;
	char *			lim;
	char *			src;
	char *			end;
	size_t			rem;
	
	buf = inHeader->buf;
	dst = buf;
	lim = buf + sizeof( inHeader->buf );
	src = buf;
	for( ;; )
	{
		len = (size_t)( lim - dst );
		require_action( len > 0, exit, err = kNoSpaceErr );
		
		err = NetSocket_Read( inSock, 1, len, dst, &len, inTimeoutSecs );
		if( err ) goto exit;
		end = dst + len;
		dst = end;
		
		// Find an empty line (separates the header and body). The HTTP spec defines it as CRLFCRLF, but some
		// use LFLF or weird combos like CRLFLF so this handles CRLFCRLF, LFLF, and CRLFLF (but not CRCR).
		
		for( ;; )
		{
			while( ( src < end ) && ( *src != '\n' ) ) ++src;
			if( src >= end ) break;
			
			rem = (size_t)( end - src );
			if( ( rem >= 3 ) && ( src[ 1 ] == '\r' ) && ( src[ 2 ] == '\n' ) ) // CRLFCRLF or LFCRLF.
			{
				end = src + 3;
				goto foundHeader;
			}
			else if( ( rem >= 2 ) && ( src[ 1 ] == '\n' ) ) // LFLF or CRLFLF.
			{
				end = src + 2;
				goto foundHeader;
			}
			else if( rem <= 1 )
			{
				break;
			}
			++src;
		}
	}
	
foundHeader:
	
	inHeader->len = (size_t)( end - buf );
	err = HTTPHeader_Parse( inHeader );
	require_noerr( err, exit );
	
	inSock->leftoverPtr = end;
	inSock->leftoverEnd = dst;
	
exit:
	return( err );
}

#endif // TARGET_HAS_SOCKETS

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( LOGUTILS_ENABLED )
//===========================================================================================================================
//	LogHTTP
//===========================================================================================================================

#if( DEBUG )
	#define kDefaultHTTPLogLevel		kLogLevelNotice
#else
	#define kDefaultHTTPLogLevel		kLogLevelOff
#endif

ulog_define( HTTPRequests,  kDefaultHTTPLogLevel, kLogFlags_None, "", NULL );
ulog_define( HTTPResponses, kDefaultHTTPLogLevel, kLogFlags_None, "", NULL );

void
	LogHTTP( 
		LogCategory *		inRequestCategory, 
		LogCategory *		inResponseCategory, 
		const void *		inHeaderPtr, size_t inHeaderLen, 
		const void *		inBodyPtr,   size_t inBodyLen )
{
	const char *		src;
	const char *		end;
	const char *		eol;
	const char *		ptr;
	unsigned char		c;
	int					isRequest;
	LogCategory *		category;
	
	if( !inRequestCategory )   inRequestCategory  = &log_category_from_name( HTTPRequests );
	if( !inResponseCategory )  inResponseCategory = &log_category_from_name( HTTPResponses );
	if( !log_category_enabled( inRequestCategory,  kLogLevelInfo ) && 
		!log_category_enabled( inResponseCategory, kLogLevelInfo ) )
	{
		return;
	}
	
	src = (const char *) inHeaderPtr;
	end = src + inHeaderLen;
	c = '\0';
	for( eol = src; ( eol < end ) && ( ( c = *eol ) != '\r' ) && ( c != '\n' ); ++eol ) {} // Find end of line.
	for( ptr = src; ( ptr < end ) && ( ( c = *ptr ) != ' ' )  && ( c != '/'  ); ++ptr ) {} // Detect request or response.
	if( c == ' ' )	{ isRequest = 1; category = inRequestCategory; }
	else			{ isRequest = 0; category = inResponseCategory; }
	
	if( log_category_enabled( category, kLogLevelVerbose ) ) // Multi-line
	{
		const char *		prefix = "";
		const char *		suffix = "";
		const uint8_t *		bodyPtr;
		const uint8_t *		bodyEnd;
		int					printable = true;
		
		if( inHeaderPtr )
		{
			if( isRequest )	prefix = "==================== HTTP REQUEST  ====================\n";
			else			prefix = "-------------------- HTTP RESPONSE --------------------\n";
		}
		if( inBodyLen > 0 )
		{
			bodyPtr = (const uint8_t *) inBodyPtr;
			bodyEnd = bodyPtr + inBodyLen;
			for( ; bodyPtr < bodyEnd; ++bodyPtr )
			{
				c = *bodyPtr;
				if( !( ( c >= 9 ) && ( c <= 13 ) ) && !( ( c >= 32 ) && ( c <= 126 ) ) )
				{
					printable = false;
					break;
				}
			}
			suffix = ( bodyEnd[ -1 ] == '\n' ) ? "\n" : "\n\n";
		}
		if( printable )
		{
			ulog( category, kLogLevelMax, "%s%{text}%{text}%s", prefix, inHeaderPtr, inHeaderLen, 
				inBodyPtr, inBodyLen, suffix );
		}
		else
		{
			ulog( category, kLogLevelMax, "%s%{text}<< BINARY DATA >>\n%.1H\n", prefix, inHeaderPtr, inHeaderLen, 
				inBodyPtr, (int) inBodyLen, 64 );
		}
	}
	else if( log_category_enabled( category, kLogLevelInfo ) ) // Single line
	{
		if( inHeaderPtr )
		{
			if( isRequest )	ulog( inRequestCategory,  kLogLevelMax, "HTTP Request:  %.*s\n",   (int)( eol - src ), src );
			else			ulog( inResponseCategory, kLogLevelMax, "HTTP Response: %.*s\n\n", (int)( eol - src ), src );
		}
	}
}
#endif // LOGUTILS_ENABLED

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HTTPUtils_Test
//===========================================================================================================================

OSStatus	HTTPUtils_Test( void )
{
	OSStatus				err;
	int64_t					rangeStart;
	int64_t					rangeEnd;
	const char *			str;
	size_t					len;
	const char *			namePtr;
	size_t					nameLen;
	const char *			valuePtr;
	size_t					valueLen;
	const char *			nextLine;
	const char *			ptr;
	int						i;
	HTTPHeader				header;
	
	// Byte Range Requests.
	
	err = HTTPParseByteRangeRequest( "bytes=0-499", kSizeCString, &rangeStart, &rangeEnd );
	require_noerr( err, exit );
	require_action( ( rangeStart == 0 ) && ( rangeEnd == 500 ), exit, err = kResponseErr );
	
	err = HTTPParseByteRangeRequest( "bytes=500-999", kSizeCString, &rangeStart, &rangeEnd );
	require_noerr( err, exit );
	require_action( ( rangeStart == 500 ) && ( rangeEnd == 1000 ), exit, err = kResponseErr );
	
	err = HTTPParseByteRangeRequest( "bytes=-500", kSizeCString, &rangeStart, &rangeEnd );
	require_noerr( err, exit );
	require_action( ( rangeStart == -500 ) && ( rangeEnd == -1 ), exit, err = kResponseErr );
	
	err = HTTPParseByteRangeRequest( "bytes=9500-", kSizeCString, &rangeStart, &rangeEnd );
	require_noerr( err, exit );
	require_action( ( rangeStart == 9500 ) && ( rangeEnd == -1 ), exit, err = kResponseErr );
	
	// Header Parsing.
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Accept-Language: en\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3\r\n"
	"Connection: keep-alive\r\n"
	" and more\r\n"
	" and more 2\r\n"
	"Host: localhost:3689\r\n"
	"\r\nzzz";
	
	header.len = strlen( str );
	require_action( header.len <= sizeof( header.buf ), exit, err = kSizeErr );
	memcpy( header.buf, str, header.len );
	err = HTTPHeader_Parse( &header );
	require_noerr( err, exit );
	require_action( strnicmpx( header.methodPtr, header.methodLen, "GET" ) == 0, exit, err = kResponseErr );
	require_action( header.method == kHTTPMethod_GET, exit, err = kResponseErr );
	
	err = HTTPGetHeaderField( str, kSizeCString, "connection", &namePtr, &nameLen, &valuePtr, &valueLen, &nextLine );
	require_noerr( err, exit );
	require_action( strncmpx( namePtr, nameLen, "Connection" ) == 0, exit, err = kResponseErr );
	require_action( strncmpx( valuePtr, valueLen, "keep-alive\r\n and more\r\n and more 2" ) == 0, exit, err = kResponseErr );
	require_action( strncmp( nextLine, "Host:", 5 ) == 0, exit, err = kResponseErr );
	
	err = HTTPGetHeaderField( str, kSizeCString, kHTTPHeader_ContentLength, &namePtr, &nameLen, &valuePtr, &valueLen, &nextLine );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	i = 0;
	for( ptr = str; HTTPGetHeaderField( ptr, kSizeCString, NULL, &namePtr, &nameLen, &valuePtr, &valueLen, &ptr ) == kNoErr; )
	{
		if( i == 0 )
		{
			require_action( strncmpx( namePtr, nameLen, kHTTPHeader_Accept ) == 0, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "*/*" ) == 0, exit, err = kResponseErr );
		}
		else if( i == 5 )
		{
			require_action( strncmpx( namePtr, nameLen, kHTTPHeader_Host ) == 0, exit, err = kResponseErr );
			require_action( strncmpx( valuePtr, valueLen, "localhost:3689" ) == 0, exit, err = kResponseErr );
		}
		++i;
	}
	require_action( i == 6, exit, err = kResponseErr );
	
	// Header Validation
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: ";
	header.len = strlen( str );
	memcpy( header.buf, str, header.len );
	require_action( !HTTPHeader_Validate( &header ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Accept-Language: en\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3\r\n"
	"Connection: keep-alive\r\n"
	" and more\r\n"
	" and more 2\r\n"
	"Host: localhost:3689\r\n"
	"\r\n";
	header.len = strlen( str );
	memcpy( header.buf, str, header.len );
	require_action( HTTPHeader_Validate( &header ), exit, err = -1 );
	require_action( header.extraDataLen == 0, exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Host: localhost:3689\r\n"
	"\r\ntest";
	header.len = strlen( str );
	memcpy( header.buf, str, header.len );
	require_action( HTTPHeader_Validate( &header ), exit, err = -1 );
	require_action( header.extraDataLen == 4, exit, err = -1 );
	require_action( memcmp( header.extraDataPtr, "test", 4 ) == 0, exit, err = -1 );
	
	// Header Building 1.
	
	err = HTTPHeader_InitRequest( &header, "GET", "/index.html", "HTTP/1.1" );
	require_noerr( err, exit );
	
	err = HTTPHeader_AddFieldF( &header, kHTTPHeader_ContentLength, "%d", 200 );
	require_noerr( err, exit );
	
	err = HTTPHeader_Commit( &header );
	require_noerr( err, exit );
	
	err = HTTPHeader_Uncommit( &header );
	require_noerr( err, exit );
	
	err = HTTPHeader_AddFieldF( &header, "X-Test", "%d", 123 );
	require_noerr( err, exit );
	
	err = HTTPHeader_Commit( &header );
	require_noerr( err, exit );
	
	str = 
		"GET /index.html HTTP/1.1\r\n"
		"Content-Length: 200\r\n"
		"X-Test: 123\r\n"
		"\r\n";
	len = strlen( str );
	require_action( header.len == len, exit, err = kResponseErr );
	require_action( header.buf[ len ] == '\0', exit, err = kResponseErr );
	require_action( strcmp( header.buf, str ) == 0, exit, err = kResponseErr );
	
	// Header Building 2.
	
	err = HTTPHeader_InitRequest( &header, "GET", "/index.html", "HTTP/1.1" );
	require_noerr( err, exit );
	
	err = HTTPHeader_AddFieldF( &header, kHTTPHeader_Server, "TestServer/1.0" );
	require_noerr( err, exit );
	
	err = HTTPHeader_AddFieldF( &header, "X-Test", "y" );
	require_noerr( err, exit );
	
	err = HTTPHeader_Commit( &header );
	require_noerr( err, exit );
	
	str = 
		"GET /index.html HTTP/1.1\r\n"
		"X-Test: y\r\n"
		"Server: TestServer/1.0\r\n"
		"\r\n";
	len = strlen( str );
	require_action( header.len == len, exit, err = kResponseErr );
	require_action( header.buf[ len ] == '\0', exit, err = kResponseErr );
	require_action( strcmp( header.buf, str ) == 0, exit, err = kResponseErr );
	
	err = kNoErr;
	
exit:
	printf( "HTTPUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
