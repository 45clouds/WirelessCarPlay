/*
	File:    	HTTPUtils.c
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

#include "HTTPUtils.h"

#include "Base64Utils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "URLUtils.h"

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

#include MD5_HEADER

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
	return( HTTPHeader_InitRequestF( inHeader, inProtocol, inMethod, "%s", inURL ) );
}

OSStatus	HTTPHeader_InitRequestF( HTTPHeader *inHeader, const char *inProtocol, const char *inMethod, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = HTTPHeader_InitRequestV( inHeader, inProtocol, inMethod, inFormat, args );
	va_end( args );
	return( err );
}

OSStatus
	HTTPHeader_InitRequestV( 
		HTTPHeader *	inHeader, 
		const char *	inProtocol, 
		const char *	inMethod, 
		const char *	inFormat, 
		va_list			inArgs )
{
	OSStatus		err;
	va_list			args;
	int				methodLen, urlOffset, urlEnd, n;
	
	va_copy( args, inArgs );
	n = SNPrintF( inHeader->buf, sizeof( inHeader->buf ), "%s%n %n%V%n %s\r\n", 
		inMethod, &methodLen, &urlOffset, inFormat, &args, &urlEnd, inProtocol ? inProtocol : "HTTP/1.1" );
	va_end( args );
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
	return( HTTPHeader_InitResponseEx( inHeader, inProtocol, inStatusCode, inReasonPhrase, kNoErr ) );
}

OSStatus
	HTTPHeader_InitResponseEx( 
		HTTPHeader *	inHeader, 
		const char *	inProtocol, 
		int				inStatusCode, 
		const char *	inReasonPhrase, 
		OSStatus		inError )
{
	OSStatus		err;
	int				n;
	
	if( !inProtocol )		inProtocol		= "HTTP/1.1";
	if( !inReasonPhrase )	inReasonPhrase	= HTTPGetReasonPhrase( inStatusCode );
	
	if( inError )
	{
		n = SNPrintF( inHeader->buf, sizeof( inHeader->buf ), "%s %u %s (%d)\r\n", 
			inProtocol, inStatusCode, inReasonPhrase, (int) inError );
	}
	else
	{
		n = SNPrintF( inHeader->buf, sizeof( inHeader->buf ), "%s %u %s\r\n", inProtocol, inStatusCode, inReasonPhrase );
	}
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
	
	// Append the final empty line to indicate the end of the header (except for '$' interleaved binary messages).
	
	if( ( len != 4 ) || ( buf[ 0 ] != '$' ) )
	{
		require_action( ( len + 2 ) < sizeof( inHeader->buf ), exit, err = kOverrunErr );
		buf[ len++ ] = '\r';
		buf[ len++ ] = '\n';
		buf[ len ]   = '\0';
		inHeader->len = len;
	}
	
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
//	HTTPHeader_SetField
//===========================================================================================================================

OSStatus	HTTPHeader_SetField( HTTPHeader *inHeader, const char *inName, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = HTTPHeader_SetFieldV( inHeader, inName, inFormat, args );
	va_end( args );
	return( err );
}

OSStatus	HTTPHeader_SetFieldV( HTTPHeader *inHeader, const char *inName, const char *inFormat, va_list inArgs )
{
	OSStatus			err;
	char *				buf;
	size_t				len;
	size_t				maxLen;
	const char *		headerFieldSrc;
	const char *		headerFieldEnd;
	va_list				args;
	int					n;
	
	err = inHeader->firstErr;
	require_noerr( err, exit );
	
	buf = inHeader->buf;
	len = inHeader->len;
	maxLen = sizeof( inHeader->buf );
	require_action_string( len > 2, exit, err = kNotPreparedErr, "header not initialized" );
	require_action_string( len < maxLen, exit, err = kNotPreparedErr, "bad header length" );
	
	err = HTTPGetHeaderField( buf, len, inName, &headerFieldSrc, NULL, NULL, NULL, &headerFieldEnd );
	if( err == kNoErr )
	{
		size_t		oldHeaderLen;
		size_t		newHeaderLen;
		size_t		oldEndOffset;
		size_t		newLen;
		size_t		offset;
	
		// Preflight the format string and args to see if there's enough space.
		
		va_copy( args, inArgs );
		n = MemPrintF( NULL, 0, "%s: %V\r\n", inName, inFormat, &args );
		va_end( args );
		require_action( n > 0, exit, err = kFormatErr );
		
		oldHeaderLen = (size_t)( headerFieldEnd - headerFieldSrc );
		newHeaderLen = (size_t) n;
		newLen = ( len - oldHeaderLen ) + newHeaderLen;
		require_action( newLen < maxLen, exit, err = kOverrunErr );
		
		// Shift existing data to accommodate the new data and the new null terminator.
		
		offset = (size_t)( headerFieldSrc - buf );
		if( oldHeaderLen != newHeaderLen )
		{
			oldEndOffset = offset + oldHeaderLen;
			memmove( &buf[ offset + newHeaderLen ], &buf[ oldEndOffset ], ( len - oldEndOffset ) + 1 );
		}
		
		// Insert the new data.
		
		va_copy( args, inArgs );
		MemPrintF( &buf[ offset ], newHeaderLen, "%s: %V\r\n", inName, inFormat, &args );	
		va_end( args );
		
		inHeader->len = newLen;
	}
	else
	{
		// Header field doesn't already exist so just append it.
		
		maxLen = maxLen - len;
		va_copy( args, inArgs );
		n = SNPrintF( &buf[ len ], maxLen, "%s: %V\r\n", inName, inFormat, &args );
		va_end( args );
		require_action( ( n > 0 ) && ( n < ( (int) maxLen ) ), exit, err = kOverrunErr );
		
		inHeader->len += ( (size_t) n );
	}
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
	
	// Check for a 4-byte interleaved binary data header (see RFC 2326 section 10.12). It has the following format:
	//
	//		'$' <1:channelID> <2:dataSize in network byte order> ... followed by dataSize bytes of binary data.
	
	src = ioHeader->buf;
	if( ( ioHeader->len == 4 ) && ( src[ 0 ] == '$' ) )
	{
		ioHeader->channelID		= Read8( &src[ 1 ] );
		ioHeader->contentLength	= ReadBig16( &src[ 2 ] );
		ioHeader->methodPtr		= src;
		ioHeader->methodLen		= 1;
		
		err = kNoErr;
		goto exit;
	}
	
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
	
	// Check for interleaved binary data (4 byte header that begins with $). See RFC 2326 section 10.12.
	
	require( inHeader->len < sizeof( inHeader->buf ), exit );
	src = inHeader->buf;
	end = src + inHeader->len;
	if( ( ( end - src ) >= 4 ) && ( src[ 0 ] == '$' ) )
	{
		src += 4;
	}
	else
	{
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
//	HTTPIsChunked
//===========================================================================================================================

Boolean	HTTPIsChunked( const char *inHeaderPtr, size_t inHeaderLen )
{
	OSStatus			err;
	const char *		valuePtr;
	const char *		valueEnd;
	size_t				valueLen;
	const char *		typePtr;
	size_t				typeLen;
	char				c;
	
	if( inHeaderLen == kSizeCString ) inHeaderLen = strlen( inHeaderPtr );
	
	// Parse a Transfer-Encoding header. Examples:
	//
	//		Transfer-Encoding: chunked
	//		Transfer-Encoding: gzip, chunked
	//		Transfer-Encoding: custom ; name=value, chunked
	//		Transfer-Encoding: custom ; name="a, b, c, d" ; x=y, chunked
	//		Transfer-Encoding: gzip, custom
	
	err = HTTPGetHeaderField( inHeaderPtr, inHeaderLen, kHTTPHeader_TransferEncoding, NULL, NULL, &valuePtr, &valueLen, NULL );
	require_noerr_quiet( err, exit );
	
	valueEnd = valuePtr + valueLen;
	while( valuePtr < valueEnd )
	{
		while( ( valuePtr < valueEnd ) && isspace_safe( *valuePtr ) ) ++valuePtr;
		typePtr = valuePtr;
		while( ( valuePtr < valueEnd ) && ( ( c = *valuePtr ) != ',' ) && ( c != ';' ) && !isspace_safe( c ) ) ++valuePtr;
		typeLen = (size_t)( valuePtr - typePtr );
		if( strnicmpx( typePtr, typeLen, "chunked" ) == 0 )
		{
			err = kNoErr;
			goto exit;
		}
		
		while( ( valuePtr < valueEnd ) && isspace_safe( *valuePtr ) ) ++valuePtr;
		if( ( valuePtr < valueEnd ) && ( *valuePtr == ';' ) )
		{
			++valuePtr;
			do
			{
				err = HTTPParseParameter( valuePtr, valueEnd, NULL, NULL, NULL, NULL, &c, &valuePtr );
				if( err ) break;
			
			}	while( c == ';' );
		}
		if( ( valuePtr < valueEnd ) && ( *valuePtr == ',' ) ) ++valuePtr;
	}
	err = kNotFoundErr;
	
exit:
	return( !err );
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

#if 0
#pragma mark -
#pragma mark == Authentication ==
#endif

//===========================================================================================================================
//	Authentication
//===========================================================================================================================

static OSStatus	_HTTPAddBasicAuth( HTTPClientAuthorizationInfoRef inAuthInfo );
static OSStatus	_HTTPAddDigestAuth( HTTPClientAuthorizationInfoRef inAuthInfo );
static void
	_HTTPMakeAuthDigest( 
		const char *inUsernamePtr,	size_t inUsernameLen, 
		const char *inPasswordPtr,	size_t inPasswordLen, 
		const char *inMethodPtr,	size_t inMethodLen, 
		const char *inURLPtr,		size_t inURLLen, 
		const char *inRealmPtr,		size_t inRealmLen, 
		const char *inNoncePtr,		size_t inNonceLen, 
		const char * const			inHexDigits, 
		char 						outDigestStr[ 32 + 1 ] );

//===========================================================================================================================
//	HTTPMakeTimedNonce
//===========================================================================================================================

OSStatus
	HTTPMakeTimedNonce( 
		const char *	inETagPtr, 
		size_t			inETagLen, 
		const void *	inKeyPtr, 
		size_t			inKeyLen, 
		char *			inNonceBuf, 
		size_t			inNonceLen, 
		size_t *		outNonceLen )
{
	OSStatus			err;
	unsigned int		timestamp;
	uint8_t				nonceBuf[ 64 ];
	int					n;
	MD5_CTX				md5Context;
	size_t				len;
	
	require_action( inNonceLen > 0, exit, err = kParamErr );
	
	// From RFC 2617, nonce is: Base64(time-stamp H(time-stamp ":" ETag ":" private-key)).
	// We define the timestamp as a decimal Unix seconds string (1970-01-01 00:00:00 epoch).
	// So the final format is:
	//
	// 		Base64(<timestamp string> " " <16-byte MD5 hash of (<timestamp string> ":" ETag ":" private-key)>)
	
	timestamp = (unsigned int) time( NULL );
	n = SNPrintF( nonceBuf, sizeof( nonceBuf ), "%u", timestamp );
	
	MD5_Init( &md5Context );
	MD5_Update( &md5Context, nonceBuf, (size_t) n );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inETagPtr, inETagLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inKeyPtr, inKeyLen );
	nonceBuf[ n++ ] = ' ';
	MD5_Final( &nonceBuf[ n ], &md5Context );
	n += 16;
	
	err = Base64Encode( nonceBuf, (size_t) n, inNonceBuf, inNonceLen, &len );
	require_noerr( err, exit );
	inNonceBuf[ len ] = '\0';
	if( outNonceLen ) *outNonceLen = len;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPVerifyTimedNonce
//===========================================================================================================================

OSStatus
	HTTPVerifyTimedNonce( 
		const char *	inNoncePtr, 
		size_t			inNonceLen, 
		unsigned int	inGoodSecs, 
		const char *	inETagPtr, 
		size_t			inETagLen, 
		const void *	inKeyPtr, 
		size_t			inKeyLen )
{
	OSStatus			err;
	unsigned int		nowSecs;
	unsigned int		nonceSecs;
	char				nonceStr[ 64 ];
	size_t				len;
	const char *		src;
	const char *		end;
	const char *		timestampPtr;
	size_t				timestampLen;
	MD5_CTX				md5Context;
	uint8_t				ourHash[ 16 ];
	
	// From RFC 2617, nonce is: Base64(time-stamp H(time-stamp ":" ETag ":" private-key)).
	// We define the timestamp as a decimal Unix seconds string (1970-01-01 00:00:00 epoch).
	// So the final format is:
	//
	// 		Base64(<timestamp string> " " <16-byte MD5 hash of (<timestamp string> ":" ETag ":" private-key)>)
	
	err = Base64Decode( inNoncePtr, inNonceLen, nonceStr, sizeof( nonceStr ), &len );
	require_noerr_quiet( err, exit );
	
	// Parse and verify the timestamp.
	
	nowSecs = (unsigned int) time( NULL );
	src = nonceStr;
	end = src + len;
	timestampPtr = src;
	for( nonceSecs = 0; ( src < end ) && isdigit_safe( *src ); ++src ) nonceSecs = ( nonceSecs * 10 ) + ( *src - '0' );
	timestampLen = (size_t)( src - timestampPtr );
	require_action_quiet( ( src < end ) && ( *src == ' ' ), exit, err = kMalformedErr );
	require_action_quiet( ( nowSecs - nonceSecs ) <= inGoodSecs, exit, err = kTimeoutErr );
	++src;
	
	// Verify that the hash embedded in the nonce matches the hash we generate.
	
	len = (size_t)( end - src );
	require_action_quiet( len == sizeof( ourHash ), exit, err = kSizeErr );
	
	MD5_Init( &md5Context );
	MD5_Update( &md5Context, timestampPtr, timestampLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inETagPtr, inETagLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inKeyPtr, inKeyLen );
	MD5_Final( ourHash, &md5Context );
	require_action_quiet( memcmp( ourHash, src, sizeof( ourHash ) ) == 0, exit, err = kIntegrityErr );
	
exit:
	return( err );		
}

//===========================================================================================================================
//	HTTPClientAuthorization_Init
//===========================================================================================================================

void	HTTPClientAuthorization_Init( HTTPClientAuthorizationInfoRef inAuthInfo )
{
	// User fields.
	
	inAuthInfo->allowedAuthSchemes	= kHTTPAuthorizationScheme_None;
	inAuthInfo->uppercaseHex		= false;
	inAuthInfo->username			= NULL;
	inAuthInfo->password			= NULL;
	
	inAuthInfo->requestHeader		= NULL;
	inAuthInfo->responseHeader		= NULL;
		
	// Private fields.
	
	inAuthInfo->algorithmStr		= NULL;
	inAuthInfo->algorithmLen		= 0;
	
	inAuthInfo->authSchemeStr		= NULL;
	inAuthInfo->authSchemeLen		= 0;
	
	inAuthInfo->domainStr			= NULL;
	inAuthInfo->domainLen			= 0;
	
	inAuthInfo->nonceStr			= NULL;
	inAuthInfo->nonceLen			= 0;
	
	inAuthInfo->opaqueStr			= NULL;
	inAuthInfo->opaqueLen			= 0;
	
	inAuthInfo->realmStr			= NULL;
	inAuthInfo->realmLen			= 0;
	
	inAuthInfo->qopStr				= NULL;
	inAuthInfo->qopLen				= 0;
	
	inAuthInfo->staleStr			= NULL;
	inAuthInfo->staleLen			= 0;
	
	inAuthInfo->lastAuthScheme		= kHTTPAuthorizationScheme_None;
}

//===========================================================================================================================
//	HTTPClientAuthorization_Free
//===========================================================================================================================

void	HTTPClientAuthorization_Free( HTTPClientAuthorizationInfoRef inAuthInfo )
{
	ForgetMem( &inAuthInfo->algorithmStr );
	inAuthInfo->algorithmLen = 0;
	
	ForgetMem( &inAuthInfo->authSchemeStr );
	inAuthInfo->authSchemeLen = 0;
	
	ForgetMem( &inAuthInfo->domainStr );
	inAuthInfo->domainLen = 0;
	
	ForgetMem( &inAuthInfo->nonceStr );
	inAuthInfo->nonceLen = 0;
	
	ForgetMem( &inAuthInfo->opaqueStr );
	inAuthInfo->opaqueLen = 0;
	
	ForgetMem( &inAuthInfo->realmStr );
	inAuthInfo->realmLen = 0;
	
	ForgetMem( &inAuthInfo->qopStr );
	inAuthInfo->qopLen = 0;
	
	ForgetMem( &inAuthInfo->staleStr );
	inAuthInfo->staleLen = 0;
}

//===========================================================================================================================
//	HTTPClientAuthorization_Apply
//===========================================================================================================================

OSStatus	HTTPClientAuthorization_Apply( HTTPClientAuthorizationInfoRef inAuthInfo )
{
	OSStatus			err;
	const char *		valuePtr;
	const char *		valueEnd;
	size_t				valueLen;
	const char *		schemePtr;
	const char *		schemeEnd;
	const char *		paramsPtr;
	const char *		paramsEnd;
	const char *		namePtr;
	size_t				nameLen;
	char *				tempStr;
	size_t				len;
	
	require_action( inAuthInfo->requestHeader, exit, err = kParamErr );
	require_action( inAuthInfo->responseHeader, exit, err = kParamErr );
	require_action( inAuthInfo->requestHeader->methodLen > 0, exit, err = kParamErr );
	require_action( inAuthInfo->requestHeader->urlLen > 0, exit, err = kParamErr );
	
	if( inAuthInfo->responseHeader->statusCode != kHTTPStatus_Unauthorized )
	{
		if( inAuthInfo->lastAuthScheme == kHTTPAuthorizationScheme_Digest )
		{
			err = _HTTPAddDigestAuth( inAuthInfo );
		}
		else if( inAuthInfo->lastAuthScheme == kHTTPAuthorizationScheme_Basic )
		{
			err = _HTTPAddBasicAuth( inAuthInfo );
		}
		else if( inAuthInfo->lastAuthScheme == kHTTPAuthorizationScheme_None )
		{
			// We haven't authorized yet so we have to wait for a 401 Unauthorized challenge to authenticate.
			
			err = kNoErr;
		}
		else
		{
			dlogassert( "bad lastAuthScheme %d", inAuthInfo->lastAuthScheme );
			err = kParamErr;
		}
		goto exit;
	}
	
	// We're handling a new response so clean up any cached values.
	
	HTTPClientAuthorization_Free( inAuthInfo );
	
	// Get the "WWW-Authenticate" header and parse the auth scheme and parameter section.
	
	err = HTTPGetHeaderField( inAuthInfo->responseHeader->buf, inAuthInfo->responseHeader->len, 
		"WWW-Authenticate", NULL, NULL, &valuePtr, &valueLen, NULL );
	require_noerr_action( err, exit, err = kMalformedErr );
	
	valueEnd = valuePtr + valueLen;
	err = HTTPParseToken( valuePtr, valueEnd, &schemePtr, &schemeEnd, &paramsPtr );
	require_noerr_action( err, exit, err = kMalformedErr );
	paramsEnd = valueEnd;
	
	len = (size_t)( schemeEnd - schemePtr );
	tempStr = strndup( schemePtr, len );
	require_action( tempStr, exit, err = kNoMemoryErr );
	inAuthInfo->authSchemeStr = tempStr;
	inAuthInfo->authSchemeLen = len;
	
	// Parse all the parameters.
	
	while( HTTPParseParameter( paramsPtr, paramsEnd, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &paramsPtr ) == kNoErr )
	{
		tempStr = strndup( valuePtr, valueLen );
		require_action( tempStr, exit, err = kNoMemoryErr );
		
		if( strnicmpx( namePtr, nameLen, "realm" ) == 0 )
		{
			inAuthInfo->realmStr = tempStr;
			inAuthInfo->realmLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "nonce" ) == 0 )
		{
			inAuthInfo->nonceStr = tempStr;
			inAuthInfo->nonceLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "qop" ) == 0 )
		{
			inAuthInfo->qopStr = tempStr;
			inAuthInfo->qopLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "algorithm" ) == 0 )
		{
			inAuthInfo->algorithmStr = tempStr;
			inAuthInfo->algorithmLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "domain" ) == 0 )
		{
			inAuthInfo->domainStr = tempStr;
			inAuthInfo->domainLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "opaque" ) == 0 )
		{
			inAuthInfo->opaqueStr = tempStr;
			inAuthInfo->opaqueLen = valueLen;
		}
		else if( strnicmpx( namePtr, nameLen, "stale" ) == 0 )
		{
			inAuthInfo->staleStr = tempStr;
			inAuthInfo->staleLen = valueLen;
		}
		else
		{
			free( tempStr );
			dlog( kLogLevelWarning, "%s: ### ignoring unknown auth param: %.*s=%.*s\n", 
				__ROUTINE__, (int) nameLen, namePtr, (int) valueLen, valuePtr );
		}
	}
	
	// Add auth info based on the scheme.
	
	if( strnicmpx( inAuthInfo->authSchemeStr, inAuthInfo->authSchemeLen, "Basic" ) == 0 )
	{
		err = _HTTPAddBasicAuth( inAuthInfo );
	}
	else if( strnicmpx( inAuthInfo->authSchemeStr, inAuthInfo->authSchemeLen, "Digest" ) == 0 )
	{
		err = _HTTPAddDigestAuth( inAuthInfo );
	}
	else
	{
		dlogassert( "unknown auth scheme: \"%.*s\"", (int) inAuthInfo->authSchemeLen, inAuthInfo->authSchemeStr );
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPAddBasicAuth
//===========================================================================================================================

static OSStatus	_HTTPAddBasicAuth( HTTPClientAuthorizationInfoRef inAuthInfo )
{
	OSStatus		err;
	char *			userPass;
	char *			base64UserPass;
	char *			fullValue;
	
	require_action( inAuthInfo->allowedAuthSchemes & kHTTPAuthorizationScheme_Basic, exit, err = kUnsupportedErr );
	require_action_quiet( inAuthInfo->username, exit, err = kAuthenticationErr );
	require_action_quiet( inAuthInfo->password, exit, err = kAuthenticationErr );
	
	// Generate Base64( <username>:<password> ).
	
	userPass = NULL;
	ASPrintF( &userPass, "%s:%s", inAuthInfo->username, inAuthInfo->password );
	require_action( userPass, exit, err = kNoMemoryErr );
	
	err = Base64EncodeCopy( userPass, kSizeCString, &base64UserPass, NULL );
	free( userPass );
	require_noerr( err, exit );
	
	fullValue = NULL;
	ASPrintF( &fullValue, "Basic %s", base64UserPass );
	free( base64UserPass );
	require_action( fullValue, exit, err = kNoMemoryErr );
	
	// If there's was existing Authorization header and the one we just generated is identical to it then
	// we must have already tried this string and it failed so just report an auth error to the caller.
	
	if( inAuthInfo->responseHeader->statusCode == kHTTPStatus_Unauthorized )
	{
		const char *		authorizationValuePtr;
		size_t				authorizationValueLen;
		
		authorizationValuePtr = NULL;
		authorizationValueLen = 0;
		HTTPGetHeaderField( inAuthInfo->requestHeader->buf, inAuthInfo->requestHeader->len, 
			"Authorization", NULL, NULL, &authorizationValuePtr, &authorizationValueLen, NULL );
		if( strncmpx( authorizationValuePtr, authorizationValueLen, fullValue ) == 0 )
		{
			free( fullValue );
			err = kAuthenticationErr;
			goto exit;
		}
		
		err = HTTPHeader_Uncommit( inAuthInfo->requestHeader );
		if( err ) free( fullValue );
		require_noerr( err, exit );
	}
	
	// Add/update the request header with the new Authorization header so it can re-sent.
	
	err = HTTPHeader_SetField( inAuthInfo->requestHeader, kHTTPHeader_Authorization, "%s", fullValue );
	free( fullValue );
	require_noerr( err, exit );
	
	inAuthInfo->lastAuthScheme = kHTTPAuthorizationScheme_Basic;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPAddDigestAuth
//===========================================================================================================================

static OSStatus	_HTTPAddDigestAuth( HTTPClientAuthorizationInfoRef inAuthInfo )
{
	OSStatus		err;
	char			digestStr[ 32 + 1 ];
	
	require_action( inAuthInfo->allowedAuthSchemes & kHTTPAuthorizationScheme_Digest, exit, err = kUnsupportedErr );
	require_action_quiet( inAuthInfo->username, exit, err = kAuthenticationErr );
	require_action_quiet( inAuthInfo->password, exit, err = kAuthenticationErr );
	
	// If there's already an authorization then normally that means the previous username/password we provided 
	// was wrong, but if there's a stale=true parameter, it just means we need to re-do the digest calculation 
	// with a new nonce value.
	
	if( inAuthInfo->responseHeader->statusCode == kHTTPStatus_Unauthorized )
	{
		const char *		authorizationValuePtr;
		size_t				authorizationValueLen;
	
		authorizationValuePtr = NULL;
		authorizationValueLen = 0;
		HTTPGetHeaderField( inAuthInfo->requestHeader->buf, inAuthInfo->requestHeader->len, 
			"Authorization", NULL, NULL, &authorizationValuePtr, &authorizationValueLen, NULL );
		if( ( authorizationValueLen > 0 ) && ( strnicmpx( inAuthInfo->staleStr, inAuthInfo->staleLen, "true" ) != 0 ) )
		{
			err = kAuthenticationErr;
			goto exit;
		}
		
		err = HTTPHeader_Uncommit( inAuthInfo->requestHeader );
		require_noerr( err, exit );
	}
	
	_HTTPMakeAuthDigest( 
		inAuthInfo->username,					strlen( inAuthInfo->username ), 
		inAuthInfo->password,					strlen( inAuthInfo->password ), 
		inAuthInfo->requestHeader->methodPtr,	inAuthInfo->requestHeader->methodLen, 
		inAuthInfo->requestHeader->urlPtr,		inAuthInfo->requestHeader->urlLen, 
		inAuthInfo->realmStr,					inAuthInfo->realmLen, 
		inAuthInfo->nonceStr,					inAuthInfo->nonceLen, 
		inAuthInfo->uppercaseHex ? kHexDigitsUppercase : kHexDigitsLowercase, 
		digestStr );
	
	// Add the "Authorization" header value with our response to the challenge.
	
	err = HTTPHeader_SetField( inAuthInfo->requestHeader, kHTTPHeader_Authorization, "Digest "
		"username=\"%s\", "
		"realm=\"%.*s\", " 
		"nonce=\"%.*s\", " 
		"uri=\"%.*s\", " 
		"response=\"%s\"", 
		inAuthInfo->username, 
		(int) inAuthInfo->realmLen,					inAuthInfo->realmStr, 
		(int) inAuthInfo->nonceLen,					inAuthInfo->nonceStr, 
		(int) inAuthInfo->requestHeader->urlLen,	inAuthInfo->requestHeader->urlPtr, 
		digestStr );
	require_noerr( err, exit );
	
	inAuthInfo->lastAuthScheme = kHTTPAuthorizationScheme_Digest;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPMakeAuthDigest
//===========================================================================================================================

static void
	_HTTPMakeAuthDigest( 
		const char *inUsernamePtr,	size_t inUsernameLen, 
		const char *inPasswordPtr,	size_t inPasswordLen, 
		const char *inMethodPtr,	size_t inMethodLen, 
		const char *inURLPtr,		size_t inURLLen, 
		const char *inRealmPtr,		size_t inRealmLen, 
		const char *inNoncePtr,		size_t inNonceLen, 
		const char * const			inHexDigits, 
		char 						outDigestStr[ 32 + 1 ] )
{
	MD5_CTX		md5Context;
	uint8_t		digest[ 16 ];
	char		a1Str[ 32 + 1 ];
	char		a2Str[ 32 + 1 ];
	
	// Calculate Hex( MD5( A1 ) ).
	
	MD5_Init( &md5Context );
	MD5_Update( &md5Context, inUsernamePtr, inUsernameLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inRealmPtr, inRealmLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inPasswordPtr, inPasswordLen );
	MD5_Final( digest, &md5Context );
	DataToHexCStringEx( digest, sizeof( digest ), a1Str, inHexDigits );
	
	// Calculate Hex( MD5( A2 ) ).
	
	MD5_Init( &md5Context );
	MD5_Update( &md5Context, inMethodPtr, inMethodLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inURLPtr, inURLLen );
	MD5_Final( digest, &md5Context );
	DataToHexCStringEx( digest, sizeof( digest ), a2Str, inHexDigits );
	
	// Calculate the final hash: MD5( Hex( MD5( A1 ) ):nonce:Hex( MD5( A2 ) ) ) and see if it matches.
	
	MD5_Init( &md5Context );
	MD5_Update( &md5Context, a1Str, strlen( a1Str ) );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, inNoncePtr, inNonceLen );
	MD5_Update( &md5Context, ":", 1 );
	MD5_Update( &md5Context, a2Str, strlen( a2Str ) );
	MD5_Final( digest, &md5Context );
	DataToHexCStringEx( digest, sizeof( digest ), outDigestStr, inHexDigits );
}

//===========================================================================================================================
//	HTTPVerifyAuthorization
//===========================================================================================================================

HTTPStatus	HTTPVerifyAuthorization( HTTPAuthorizationInfoRef ioAuthInfo )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		valuePtr;
	const char *		valueEnd;
	size_t				valueLen;
	const char *		schemePtr;
	const char *		schemeEnd;
	size_t				schemeLen;
	const char *		paramsPtr;
	const char *		paramsEnd;
	size_t				paramsLen;
	char *				decodedParams;
	char *				p;
	char *				passwordMem;
	const char *		passwordPtr;
	int					match;
	
	decodedParams = NULL;
	
	// If no authorization is required then there's nothing to do.
	
	require_action_quiet( ioAuthInfo->serverScheme != kHTTPAuthorizationScheme_None, exit, status = kHTTPStatus_OK );
	
	// If there's no authorization header (e.g. the initial GET) then reject immediately.
	
	err = HTTPGetHeaderField( ioAuthInfo->headerPtr, ioAuthInfo->headerLen, kHTTPHeader_Authorization, NULL, NULL, 
		&valuePtr, &valueLen, NULL );
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_Unauthorized );
	
	// Parse the scheme and use that to determine how we authorize.
	
	valueEnd = valuePtr + valueLen;
	err = HTTPParseToken( valuePtr, valueEnd, &schemePtr, &schemeEnd, &paramsPtr );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	schemeLen = (size_t)( schemeEnd - schemePtr );
	paramsEnd = valueEnd;
	paramsLen = (size_t)( paramsEnd - paramsPtr );
	
	// Verify Basic authentication, if allowed.
	// 		Authorization: Basic cGFzc3dvcmQ=
	
	if( strnicmpx( schemePtr, schemeLen, "Basic" ) == 0 )
	{
		require_action( ioAuthInfo->serverScheme & kHTTPAuthorizationScheme_Basic, exit, status = kHTTPStatus_Unauthorized );
		
		err = Base64DecodeCopy( paramsPtr, paramsLen, &decodedParams, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
		
		p = strchr( decodedParams, ':' );
		require_action( p, exit, status = kHTTPStatus_BadRequest );
		*p++ = '\0';
		
		ioAuthInfo->requestUsernamePtr = decodedParams;
		ioAuthInfo->requestUsernameLen = (size_t)( p - decodedParams );
		
		ioAuthInfo->requestPasswordPtr = p;
		ioAuthInfo->requestPasswordLen = strlen( p );
		
		passwordMem = NULL;
		passwordPtr = ioAuthInfo->serverPassword;
		if( !passwordPtr )
		{
			require_action( ioAuthInfo->copyPasswordFunction, exit, status = kHTTPStatus_InternalServerError );
			status = ioAuthInfo->copyPasswordFunction( ioAuthInfo, &passwordMem );
			require_quiet( status == kHTTPStatus_OK, exit );
			passwordPtr = passwordMem;
		}
		
		match = ( strncmpx( ioAuthInfo->requestPasswordPtr, ioAuthInfo->requestPasswordLen, passwordPtr ) == 0 );
		if( passwordMem ) free( passwordMem );
		if( !match )
		{
			dlog( kLogLevelNotice, "### Bad HTTP basic password: '%.*s'\n", 
				(int) ioAuthInfo->requestPasswordLen, ioAuthInfo->requestPasswordPtr );
			ioAuthInfo->badMatch = true;
			status = kHTTPStatus_Unauthorized;
			goto exit;
		}
	}
	
	// Verify Digest authentication, if allowed.
	// 		Authorization: Digest username="user", realm="realm", 
	// 			nonce="BF48F640E31BD14A4E22A2CA48E02EA95A8249E3", 
	// 			response="7809A64F92C06A0E91FCE8FBFAD78AE2"
	
	else if( strnicmpx( schemePtr, schemeLen, "Digest" ) == 0 )
	{
		const char *		namePtr;
		size_t				nameLen;
		char				ourDigest[ 32 + 1 ];
		
		require_action( ioAuthInfo->serverScheme & kHTTPAuthorizationScheme_Digest, exit, status = kHTTPStatus_Unauthorized );
		
		ioAuthInfo->requestUsernamePtr	= NULL;
		ioAuthInfo->requestUsernameLen	= 0;
		ioAuthInfo->requestRealmPtr 	= NULL;
		ioAuthInfo->requestRealmLen		= 0;
		ioAuthInfo->requestNoncePtr		= NULL;
		ioAuthInfo->requestNonceLen		= 0;
		ioAuthInfo->requestURIPtr		= NULL;
		ioAuthInfo->requestURILen		= 0;
		ioAuthInfo->requestResponsePtr	= NULL;
		ioAuthInfo->requestResponseLen	= 0;
		ioAuthInfo->requestAlgorithmPtr	= NULL;
		ioAuthInfo->requestAlgorithmLen	= 0;
		ioAuthInfo->requestCNoncePtr	= NULL;
		ioAuthInfo->requestCNonceLen	= 0;
		ioAuthInfo->requestOpaquePtr	= NULL;
		ioAuthInfo->requestOpaqueLen	= 0;
		ioAuthInfo->requestQOPPtr		= NULL;
		ioAuthInfo->requestQOPLen		= 0;
		ioAuthInfo->requestNCPtr		= NULL;
		ioAuthInfo->requestNCLen		= 0;
		
		while( HTTPParseParameter( paramsPtr, paramsEnd, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &paramsPtr ) == kNoErr )
		{	
			if( strnicmpx( namePtr, nameLen, "username" ) == 0 )
			{
				ioAuthInfo->requestUsernamePtr = valuePtr;
				ioAuthInfo->requestUsernameLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "realm" ) == 0 )
			{
				ioAuthInfo->requestRealmPtr = valuePtr;
				ioAuthInfo->requestRealmLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "nonce" ) == 0 )
			{
				ioAuthInfo->requestNoncePtr = valuePtr;
				ioAuthInfo->requestNonceLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "uri" ) == 0 )
			{
				ioAuthInfo->requestURIPtr = valuePtr;
				ioAuthInfo->requestURILen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "response" ) == 0 )
			{
				ioAuthInfo->requestResponsePtr = valuePtr;
				ioAuthInfo->requestResponseLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "algorithm" ) == 0 )
			{
				ioAuthInfo->requestAlgorithmPtr = valuePtr;
				ioAuthInfo->requestAlgorithmLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "cnonce" ) == 0 )
			{
				ioAuthInfo->requestCNoncePtr = valuePtr;
				ioAuthInfo->requestCNonceLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "opaque" ) == 0 )
			{
				ioAuthInfo->requestOpaquePtr = valuePtr;
				ioAuthInfo->requestOpaqueLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "qop" ) == 0 )
			{
				ioAuthInfo->requestQOPPtr = valuePtr;
				ioAuthInfo->requestQOPLen = valueLen;
			}
			else if( strnicmpx( namePtr, nameLen, "nc" ) == 0 )
			{
				ioAuthInfo->requestNCPtr = valuePtr;
				ioAuthInfo->requestNCLen = valueLen;
			}
			else
			{
				dlog( kLogLevelWarning, "### Ignoring unknown HTTP auth param: %.*s=%.*s\n", 
					(int) nameLen, namePtr, (int) valueLen, valuePtr );
			}
		}
		
		// Verify the nonce.
		
		require_action( ioAuthInfo->requestNonceLen > 0, exit, status = kHTTPStatus_BadRequest );
		if( ioAuthInfo->isValidNonceFunction )
		{
			if( !ioAuthInfo->isValidNonceFunction( ioAuthInfo ) )
			{
				dlog( kLogLevelNotice, "### Bad HTTP digest nonce: '%.*s'\n", 
					(int) ioAuthInfo->requestNonceLen, ioAuthInfo->requestNoncePtr );
				status = kHTTPStatus_Unauthorized;
				goto exit;
			}
		}
		else
		{
			require_action( ioAuthInfo->serverTimedNonceKeyPtr, exit, status = kHTTPStatus_InternalServerError );
			require_action( ioAuthInfo->serverTimedNonceKeyLen > 0, exit, status = kHTTPStatus_InternalServerError );
			
			err = HTTPVerifyTimedNonce( ioAuthInfo->requestNoncePtr, ioAuthInfo->requestNonceLen, 120, 
				kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
				ioAuthInfo->serverTimedNonceKeyPtr, ioAuthInfo->serverTimedNonceKeyLen );
			if( err )
			{
				dlog( kLogLevelNotice, "### Bad HTTP digest nonce: '%.*s' %d\n", 
					(int) ioAuthInfo->requestNonceLen, ioAuthInfo->requestNoncePtr, (int) err );
				if( err == kTimeoutErr ) ioAuthInfo->staleNonce = true;
				status = kHTTPStatus_Unauthorized;
				goto exit;
			}
		}
		
		passwordMem = NULL;
		passwordPtr = ioAuthInfo->serverPassword;
		if( !passwordPtr )
		{
			require_action( ioAuthInfo->copyPasswordFunction, exit, status = kHTTPStatus_InternalServerError );
			status = ioAuthInfo->copyPasswordFunction( ioAuthInfo, &passwordMem );
			require_quiet( status == kHTTPStatus_OK, exit );
			passwordPtr = passwordMem;
		}
		
		_HTTPMakeAuthDigest( 
			ioAuthInfo->requestUsernamePtr,			ioAuthInfo->requestUsernameLen, 
			passwordPtr,							strlen( passwordPtr ), 
			ioAuthInfo->requestMethodPtr,			ioAuthInfo->requestMethodLen, 
			ioAuthInfo->requestURLPtr,				ioAuthInfo->requestURLLen, 
			ioAuthInfo->requestRealmPtr,			ioAuthInfo->requestRealmLen, 
			ioAuthInfo->requestNoncePtr,			ioAuthInfo->requestNonceLen, 
			kHexDigitsLowercase, 
			ourDigest );
		if( passwordMem ) free( passwordMem );
		
		if( strnicmpx( ioAuthInfo->requestResponsePtr, ioAuthInfo->requestResponseLen, ourDigest ) != 0 )
		{
			dlog( kLogLevelNotice, "### HTTP digest mismatch (probably bad password):\n"
				"    ours:   %s\n"
				"    theirs: %.*s\n", 
				ourDigest, (int) ioAuthInfo->requestResponseLen, ioAuthInfo->requestResponsePtr );
			ioAuthInfo->badMatch = true;
			status = kHTTPStatus_Unauthorized;
			goto exit;
		}
	}
	
	// Unsupported authentication scheme.
	
	else
	{
		dlogassert( "unknown auth scheme: \"%.*s\"", (int) schemeLen, schemePtr );
		status = kHTTPStatus_BadRequest;
		goto exit;
	}
	
	status = kHTTPStatus_OK;
	
exit:
	ForgetMem( &decodedParams );
	return( status );
}

#if( TARGET_HAS_SOCKETS )

#if 0
#pragma mark -
#pragma mark == Networking ==
#endif

//===========================================================================================================================
//	HTTPDownloadFile
//===========================================================================================================================

OSStatus	HTTPDownloadFile( const char *inURL, size_t inMaxSize, char **outData, size_t *outSize )
{
	OSStatus			err;
	SocketRef			sock = kInvalidSocketRef;
	const char *		p;
	const char *		q;
	char *				host = NULL;
	size_t				len;
	HTTPHeader			header;
	int					n;
	fd_set				set;
	iovec_t				iov;
	iovec_t *			iop;
	int					ion;
	struct timeval		timeout;
	char *				bodyBuffer = NULL;
	size_t				bodySize;
	size_t				bodyOffset;
	
	// Parse the host out of the URL and make a null-terminated string out of it.
	
	require_action( strnicmp( inURL, "http://", sizeof_string( "http://" ) ) == 0, exit, err = kParamErr );
	p = inURL + sizeof_string( "http://" );
	q = strchr( p, '/' );
	if( q ) len = (size_t)( q - p );
	else	len = strlen( p );
	
	host = (char *) malloc( len + 1 );
	require_action( host, exit, err = kNoMemoryErr );
	memcpy( host, p, len );
	host[ len ] = '\0';
	
	// Connect to the server and send the request.
	
	err = TCPConnect( host, "80", 5, &sock );
	require_noerr_quiet( err, exit );
	
	HTTPHeader_InitRequest( &header, "GET", inURL, "HTTP/1.1" );
	HTTPHeader_SetField( &header, kHTTPHeader_Host, host );
	HTTPHeader_SetField( &header, kHTTPHeader_Connection, "close" );
	err = HTTPHeader_Commit( &header );
	require_noerr( err, exit );
	
	dlog( kLogLevelVerbose, "========== HTTP REQUEST header ==========\n" );
	dlog( kLogLevelVerbose, "%.*s", (int) header.len, header.buf );
	
	FD_ZERO( &set );
	
	iov.iov_base = header.buf;
	iov.iov_len  = (unsigned int) header.len;
	iop = &iov;
	ion = 1;
	for( ;; )
	{
		err = SocketWriteData( sock, &iop, &ion );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		if( err == EPIPE )			goto exit;
		require_noerr( err, exit );
		
		FD_SET( sock, &set );
		timeout.tv_sec  = 5;
		timeout.tv_usec = 0;
		n = select( (int)( sock + 1 ), NULL, &set, NULL, &timeout );
		require_action_quiet( n != 0, exit, err = kTimeoutErr );
		err = map_global_value_errno( n == 1, n );
		require_noerr( err, exit );
		require_action( FD_ISSET( sock, &set ), exit, err = kUnknownErr );
	}
	
	// Read the response header.
	
	header.len = 0;
	for( ;; )
	{
		err = SocketReadHTTPHeader( sock, &header );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		if( err == kConnectionErr )	goto exit;
		require_noerr( err, exit );
		
		FD_SET( sock, &set );
		timeout.tv_sec  = 5;
		timeout.tv_usec = 0;
		n = select( (int)( sock + 1 ), &set, NULL, NULL, &timeout );
		require_action_quiet( n != 0, exit, err = kTimeoutErr );
		err = map_global_value_errno( n == 1, n );
		require_noerr( err, exit );
		require_action( FD_ISSET( sock, &set ), exit, err = kUnknownErr );
	}
	
	dlog( kLogLevelVerbose, "========== HTTP RESPONSE header ==========\n" );
	dlog( kLogLevelVerbose, "%.*s", (int) header.len, header.buf );
	require_action_quiet( header.statusCode == 200, exit, err = header.statusCode );
	
	// Read the response body.
	
	require_action( header.contentLength <= SIZE_MAX, exit, err = kSizeErr );
	bodySize = (size_t) header.contentLength;
	require_action( ( inMaxSize == 0 ) || ( bodySize < inMaxSize ), exit, err = kSizeErr );
	bodyBuffer = (char *) malloc( bodySize + 1 );
	require_action( bodyBuffer, exit, err = kNoMemoryErr );
	
	bodyOffset = 0;
	len = header.extraDataLen;
	if( len > 0 )
	{
		len = Min( len, bodySize );
		memmove( bodyBuffer, header.extraDataPtr, len );
		header.extraDataPtr += len;
		header.extraDataLen -= len;
		bodyOffset += len;
	}
	for( ;; )
	{
		err = SocketReadData( sock, bodyBuffer, bodySize, &bodyOffset );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		if( err == kConnectionErr )	goto exit;
		require_noerr( err, exit );
		
		FD_SET( sock, &set );
		timeout.tv_sec  = 5;
		timeout.tv_usec = 0;
		n = select( (int)( sock + 1 ), &set, NULL, NULL, &timeout );
		require_action_quiet( n != 0, exit, err = kTimeoutErr );
		err = map_global_value_errno( n == 1, n );
		require_noerr( err, exit );
		require_action( FD_ISSET( sock, &set ), exit, err = kUnknownErr );
	}
	check( bodyOffset == bodySize );
	bodyBuffer[ bodyOffset ] = '\0';
	
	dlog( kLogLevelVerbose, "========== HTTP RESPONSE body ==========\n" );
	dlog( kLogLevelVerbose, "%.*s\n", 8192, bodyBuffer );
	
	// Success!
	
	err = SocketCloseGracefully( sock, 1 );
	check_noerr( err );
	sock = kInvalidSocketRef;
	
	*outData = bodyBuffer;
	bodyBuffer = NULL;
	*outSize = bodySize;
	err = kNoErr;
	
exit:
	ForgetMem( &bodyBuffer );
	ForgetSocket( &sock );
	ForgetMem( &host );
	return( err );
}

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
		
		// Check for interleaved binary data (4 byte header that begins with $). See RFC 2326 section 10.12.
		
		if( ( ( dst - buf ) >= 4 ) && ( buf[ 0 ] == '$' ) )
		{
			end = buf + 4;
			goto foundHeader;
		}
		
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
		
		// Check for interleaved binary data (4 byte header that begins with $). See RFC 2326 section 10.12.
		
		if( ( buf[ 0 ] == '$' ) && ( ( end - buf ) >= 4 ) )
		{
			end = buf + 4;
			goto foundHeader;
		}
		
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

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HTTPNetUtils_Test
//===========================================================================================================================

OSStatus	HTTPNetUtils_Test( void )
{
	OSStatus		err;
	char *			buf;
	size_t			len;
	
	err = HTTPDownloadFile( "http://www.apple.com/", 64000, &buf, &len );
	require_noerr( err, exit );
	printf( "Response:\n%.*s\n", (int) len, buf );
	free( buf );
	
exit:
	printf( "HTTPNetUtils_Test: %s\n", !err ? "passed" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

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
	char					tempStr[ 128 ];
	
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
	
	// Chunked
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( !HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: chunked\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: gzip, chunked\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: custom ; name=value, chunked\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: custom ; name=\"a, b, c, d\" ; x=y, chunked\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: custom;name=\"a, b, c, d\";x=y,chunked\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
	str = 
	"GET / HTTP/1.1\r\n"
	"Accept: */*\r\n"
	"Transfer-Encoding: gzip, custom\r\n"
	"Content-Length: 123\n\n"
	"\r\n";
	require_action( !HTTPIsChunked( str, kSizeCString ), exit, err = -1 );
	
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
	
	err = HTTPHeader_InitRequestF( &header, "HTTP/1.1", "GET", "/%s.%s", "index", "html" );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, kHTTPHeader_ContentLength, "%d", 1234 );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, kHTTPHeader_ContentLength, "%d", 12000 );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, kHTTPHeader_ContentLength, "%d", 200 );
	require_noerr( err, exit );
	
	err = HTTPHeader_Commit( &header );
	require_noerr( err, exit );
	
	err = HTTPHeader_Uncommit( &header );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, "X-Test", "%d", 123 );
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
	
	err = HTTPHeader_SetField( &header, "X-Test", "test", 1234 );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, kHTTPHeader_Server, "TestServer/1.0" );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, "X-Test", "some longer string", 1234 );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, "X-Test", "x", 12000 );
	require_noerr( err, exit );
	
	err = HTTPHeader_SetField( &header, "X-Test", "y", 200 );
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
	
	// TimedNonce
	
	err = HTTPMakeTimedNonce( kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
		kHTTPTimedNoncePrivateKeyPtr, kHTTPTimedNoncePrivateKeyLen, 
		tempStr, sizeof( tempStr ), &len );
	require_noerr( err, exit );
	require_action( strlen( tempStr ) == len, exit, err = -1 );
	
	err = HTTPVerifyTimedNonce( tempStr, strlen( tempStr ), 10, kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
		kHTTPTimedNoncePrivateKeyPtr, kHTTPTimedNoncePrivateKeyLen );
	require_noerr( err, exit );
	
	tempStr[ 30 ] = ( tempStr[ 30 ] == 'N' ) ? 'n' : 'N';
	err = HTTPVerifyTimedNonce( tempStr, strlen( tempStr ), 10, kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
		kHTTPTimedNoncePrivateKeyPtr, kHTTPTimedNoncePrivateKeyLen );
	require_action( err != kNoErr, exit, err = -1 );
	
	sleep( 1 );
	err = HTTPVerifyTimedNonce( tempStr, strlen( tempStr ), 0, kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
		kHTTPTimedNoncePrivateKeyPtr, kHTTPTimedNoncePrivateKeyLen );
	require_action( err == kTimeoutErr, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	printf( "HTTPUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
