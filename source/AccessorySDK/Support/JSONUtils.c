/*
	File:    	JSONUtils.c
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
	
	Copyright (C) 2009-2015 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "JSONUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "PrintFUtils.h"
#include "StringUtils.h"

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#define JSON_UTILS_USE_SYSTEM		1
#else
	#define JSON_UTILS_USE_SYSTEM		0
#endif

#if( JSON_UTILS_USE_SYSTEM )
	#include <Foundation/NSJSONSerialization.h>
#else
	#include <ctype.h>

	static OSStatus	_ReadObject( const char **ioPtr, const char *inEnd, CFTypeRef *outObj );
	static OSStatus	_ReadQuotedString( const char **ioPtr, const char *inEnd, CFStringRef *outStr );
#endif

//===========================================================================================================================
//	CFCreateWithJSONBytes
//===========================================================================================================================

#if( JSON_UTILS_USE_SYSTEM )
CFPropertyListRef
	CFCreateWithJSONBytes( 
		const void *	inPtr, 
		size_t			inLen, 
		uint32_t		inFlags, 
		CFTypeID		inType, 
		OSStatus *		outErr )
{
	CFPropertyListRef			result = NULL, obj = NULL;
	OSStatus					err;
	NSData *					data;
	NSJSONReadingOptions		options;
	Boolean						isMutable;
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inPtr );
	if( inLen > 0 )
	{
		data = [[NSData alloc] initWithBytes:inPtr length:inLen];
		require_action( data, exit, err = kNoMemoryErr );
		
		@autoreleasepool
		{
			options = NSJSONReadingAllowFragments;
			if( inFlags & kCFPropertyListMutableContainers )
			{
				options |= NSJSONReadingMutableContainers;
			}
			if( inFlags & kCFPropertyListMutableContainersAndLeaves )
			{
				options |= NSJSONReadingMutableContainers;
				options |= NSJSONReadingMutableLeaves;
			}
			obj = (CFPropertyListRef) CFBridgingRetain( [NSJSONSerialization JSONObjectWithData:data options:options error:nil] );
			arc_safe_release( data );
		}
		require_action_quiet( obj, exit, err = kFormatErr );
		require_action_quiet( !inType || ( CFGetTypeID( obj ) == inType ), exit, err = kTypeErr );
	}
	else
	{
		isMutable = ( inFlags & ( kCFPropertyListMutableContainers | kCFPropertyListMutableContainersAndLeaves ) ) ? false : true;
		if( inType == CFDictionaryGetTypeID() )
		{
			if( isMutable )	obj = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			else			obj = CFDictionaryCreate( NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( obj, exit, err = kNoMemoryErr );
		}
		else if( inType == CFArrayGetTypeID() )
		{
			if( isMutable )	obj = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			else			obj = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
			require_action( obj, exit, err = kNoMemoryErr );
		}
		else
		{
			err = kUnsupportedDataErr;
			goto exit;
		}
	}
	result = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	if( outErr ) *outErr = err;
	return( result );
}
#else
CFPropertyListRef
	CFCreateWithJSONBytes( 
		const void *	inPtr, 
		size_t			inLen, 
		uint32_t		inFlags, 
		CFTypeID		inType, 
		OSStatus *		outErr )
{
	CFTypeRef			result = NULL, obj = NULL;
	OSStatus			err;
	const char *		ptr = (const char *) inPtr;
	Boolean				isMutable;
	
	if( inLen == kSizeCString ) inLen = strlen( ptr );
	if( inLen > 0 )
	{
		err = _ReadObject( &ptr, ptr + inLen, &obj );
		require_noerr_quiet( err, exit );
		require_action_quiet( !inType || ( CFGetTypeID( obj ) == inType ), exit, err = kTypeErr );
	}
	else
	{
		isMutable = ( inFlags & ( kCFPropertyListMutableContainers | kCFPropertyListMutableContainersAndLeaves ) ) ? false : true;
		if( inType == CFDictionaryGetTypeID() )
		{
			if( isMutable )	obj = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			else			obj = CFDictionaryCreate( NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( obj, exit, err = kNoMemoryErr );
		}
		else if( inType == CFArrayGetTypeID() )
		{
			if( isMutable )	obj = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			else			obj = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
			require_action( obj, exit, err = kNoMemoryErr );
		}
		else
		{
			err = kUnsupportedDataErr;
			goto exit;
		}
	}
	result = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_ReadObject
//===========================================================================================================================

static OSStatus	_ReadObject( const char **ioPtr, const char *inEnd, CFTypeRef *outObj )
{
	OSStatus					err;
	const char *				src;
	char						c;
	CFMutableDictionaryRef		dict;
	CFMutableArrayRef			array;
	CFStringRef					key;
	CFTypeRef					value;
	size_t						len;
	
	src		= *ioPtr;
	dict	= NULL;
	array	= NULL;
	key		= NULL;
	value	= NULL;
	
	while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
	require_action_quiet( src < inEnd, exit, err = kMalformedErr );
	
	c = *src;
	if( c == '{' ) // Object (dictionary)
	{
		dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( dict, exit, err = kNoMemoryErr );
		
		++src;
		for( ;; )
		{
			while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
		
			if( *src == '}' )
			{
				++src;
				break;
			}
			
			err = _ReadQuotedString( &src, inEnd, &key );
			require_noerr_quiet( err, exit );
			
			while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
			require_action_quiet( *src == ':', exit, err = kMalformedErr );
			for( ++src; ( src < inEnd ) && isspace_safe( *src ); ++src ) {}
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
			
			err = _ReadObject( &src, inEnd, &value );
			require_noerr_quiet( err, exit );
			
			CFDictionarySetValue( dict, key, value );
			CFRelease( key );
			CFRelease( value );
			key   = NULL;
			value = NULL;
			
			while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
			if( *src == ',' ) ++src;
		}
		
		*outObj = dict;
		dict = NULL;
	}
	else if( c == '[' ) // Array
	{
		array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( array, exit, err = kNoMemoryErr );
		
		++src;
		for( ;; )
		{
			while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
			
			if( *src == ']' )
			{
				++src;
				break;
			}
			
			err = _ReadObject( &src, inEnd, &value );
			require_noerr_quiet( err, exit );
			
			CFArrayAppendValue( array, value );
			CFRelease( value );
			value = NULL;
			
			while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
			require_action_quiet( src < inEnd, exit, err = kMalformedErr );
			if( *src == ',' ) ++src;
		}
		
		*outObj = array;
		array = NULL;
	}
	else if( c == '\"' ) // String
	{
		CFStringRef		tempCFStr;
		
		err = _ReadQuotedString( &src, inEnd, &tempCFStr );
		require_noerr_quiet( err, exit );
		*outObj = tempCFStr;
	}
	else if( ( c == '-' ) || isdigit_safe( c ) ) // Number
	{
		const char *		startPtr;
		const char *		fracPtr;
		char				tempStr[ 128 ];
		int					n;
		int64_t				xi;
		double				xd;
		
		startPtr = src;
		if( c == '-' ) ++src;
		while(   ( src < inEnd ) && isdigit_safe( *src ) ) ++src;
		fracPtr = src;
		if(      ( src < inEnd ) && ( *src == '.' ) ) ++src;
		while(   ( src < inEnd ) && isdigit_safe( *src ) ) ++src;
		if(      ( src < inEnd ) && ( tolower_safe( *src ) == 'e' ) ) ++src;
		if(      ( src < inEnd ) && ( *src == '+' ) ) ++src;
		else if( ( src < inEnd ) && ( *src == '-' ) ) ++src;
		while(   ( src < inEnd ) && isdigit_safe( *src ) ) ++src;
		
		len = (size_t)( src - startPtr );
		require_action_quiet( len < sizeof( tempStr ), exit, err = kSizeErr );
		memcpy( tempStr, startPtr, len );
		tempStr[ len ] = '\0';
		
		if( ( src - fracPtr ) > 0 ) // Floating point.
		{
			n = sscanf( tempStr, "%lf", &xd );
			require_action_quiet( n == 1, exit, err = kFormatErr );
			
			*outObj = CFNumberCreate( NULL, kCFNumberDoubleType, &xd );
			require_action( *outObj, exit, err = kNoMemoryErr );
		}
		else
		{
			n = SNScanF( tempStr, len, "%lld", &xi );
			require_action_quiet( n == 1, exit, err = kFormatErr );
			
			*outObj = CFNumberCreate( NULL, kCFNumberSInt64Type, &xi );
			require_action( *outObj, exit, err = kNoMemoryErr );
		}
	}
	else
	{
		len = (size_t)( inEnd - src );
		if( ( len >= 4 ) && ( memcmp( src, "true", 4 ) == 0 ) )
		{
			src += 4;
			*outObj = kCFBooleanTrue;
		}
		else if( ( len >= 5 ) && ( memcmp( src, "false", 5 ) == 0 ) )
		{
			src += 5;
			*outObj = kCFBooleanFalse;
		}
		else if( ( len >= 4 ) && ( memcmp( src, "null", 4 ) == 0 ) )
		{
			src += 4;
			*outObj = kCFNull;
		}
		else
		{
			err = kMalformedErr;
			goto exit;
		}
	}
	err = kNoErr;
	
exit:
	if( dict )	CFRelease( dict );
	if( array )	CFRelease( array );
	if( key )	CFRelease( key );
	if( value )	CFRelease( value );
	*ioPtr = src;
	return( err );
}

//===========================================================================================================================
//	_ReadQuotedString
//===========================================================================================================================

static OSStatus	_ReadQuotedString( const char **ioPtr, const char *inEnd, CFStringRef *outStr )
{
	const char *			src = *ioPtr;
	char					c;
	OSStatus				err;
	CFMutableStringRef		str;
	char					buf[ 64 ];
	char *					dst;
	char *					lim;
	
	str = CFStringCreateMutable( NULL, 0 );
	require_action( str, exit, err = kNoMemoryErr );
	
	while( ( src < inEnd ) && isspace_safe( *src ) ) ++src;
	require_action_quiet( src < inEnd, exit, err = kMalformedErr );
	require_action_quiet( *src == '"', exit, err = kMalformedErr );
	++src;
	
	dst = buf;
	lim = dst + ( sizeof( buf ) - 1 );
	while( src < inEnd )
	{
		if( dst == lim )
		{
			*dst = '\0';
			CFStringAppendCString( str, buf, kCFStringEncodingUTF8 );
			dst = buf;
		}
		
		c = *src++;
		if( c == '\\' ) // Escape
		{
			require_action_quiet( src < inEnd, exit, err = kUnderrunErr );
			c = *src++;
			if(      c == 'b' ) c = '\b';
			else if( c == 'f' ) c = '\f';
			else if( c == 'n' ) c = '\n';
			else if( c == 'r' ) c = '\r';
			else if( c == 't' ) c = '\t';
			#if( !CFLITE_ENABLED )
			else if( c == 'u' ) // \uxxxx Unicode escape.
			{
				uint16_t	u16;
				int			i;
				
				if( dst != buf )
				{
					*dst = '\0';
					CFStringAppendCString( str, buf, kCFStringEncodingUTF8 );
					dst = buf;
				}
				
				require_action_quiet( ( inEnd - src ) >= 4, exit, err = kUnderrunErr );
				u16 = 0;
				for( i = 0; i < 4; ++i )
				{
					c = *src++;
					if(      ( c >= '0' ) && ( c <= '9' ) ) u16 = (uint16_t)( ( u16 << 4 ) |        ( c - '0' ) );
					else if( ( c >= 'a' ) && ( c <= 'f' ) ) u16 = (uint16_t)( ( u16 << 4 ) | ( 10 + ( c - 'a' ) ) );
					else if( ( c >= 'A' ) && ( c <= 'F' ) ) u16 = (uint16_t)( ( u16 << 4 ) | ( 10 + ( c - 'A' ) ) );
					else { err = kRangeErr; goto exit; }
				}
				CFStringAppendCharacters( str, &u16, 1 );
				continue;
			}
			#endif
			else {} // Other escapes fall through to use the character after the backslash.
		}
		else if( c == '"' )
		{
			break;
		}
		*dst++ = c;
	}
	if( dst != buf )
	{
		*dst = '\0';
		CFStringAppendCString( str, buf, kCFStringEncodingUTF8 );
	}
	
	*outStr = str;
	str = NULL;
	err = kNoErr;
	
exit:
	if( str ) CFRelease( str );
	*ioPtr = src;
	return( err );
}
#endif // JSON_UTILS_USE_SYSTEM

#if 0
#pragma mark -
#endif

#if( JSON_UTILS_USE_SYSTEM )
//===========================================================================================================================
//	CFCreateJSONData
//===========================================================================================================================

CFDataRef	CFCreateJSONData( CFPropertyListRef inPlist, JSONFlags inFlags, OSStatus *outErr )
{
	OSStatus					err;
	CFDataRef					data;
	NSJSONWritingOptions		options;
	
	@autoreleasepool
	{
		options = (NSJSONWritingOptions) NSJSONReadingAllowFragments; // Until <radar:15529724> is fixed.
		if( !( inFlags & kJSONFlags_Condensed ) ) options |= NSJSONWritingPrettyPrinted;
		data = (CFDataRef) CFBridgingRetain( [NSJSONSerialization dataWithJSONObject:(__bridge id) inPlist options:options 
			error:nil] );
		require_action_quiet( data, exit, err = kUnsupportedDataErr );
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( data );
}
#else
typedef struct
{
	CFMutableDataRef		data;
	JSONFlags				flags;
	char					stringBuf[ 256 ];
	
}	CFJSONContext;

static OSStatus	_WriteJSONObject( CFJSONContext *ctx, CFPropertyListRef inObj, size_t inIndent, int inSameLine );
static OSStatus	_WriteBytes( CFJSONContext *ctx, const void *inData, size_t inSize );
static OSStatus	_WriteIndent( CFJSONContext *ctx, size_t inIndent );

static CFTypeID		gCFArrayType		= (CFTypeID) -1;
static CFTypeID		gCFBooleanType		= (CFTypeID) -1;
static CFTypeID		gCFDataType			= (CFTypeID) -1;
static CFTypeID		gCFDateType			= (CFTypeID) -1;
static CFTypeID		gCFDictionaryType	= (CFTypeID) -1;
static CFTypeID		gCFNullType			= (CFTypeID) -1;
static CFTypeID		gCFNumberType		= (CFTypeID) -1;
static CFTypeID		gCFStringType		= (CFTypeID) -1;

//===========================================================================================================================
//	CFCreateJSONData
//===========================================================================================================================

CFDataRef	CFCreateJSONData( CFPropertyListRef inPlist, JSONFlags inFlags, OSStatus *outErr )
{
	OSStatus			err;
	CFJSONContext		ctx;
	
	if( gCFStringType == ( (CFTypeID) -1 ) )
	{
		gCFArrayType		= CFArrayGetTypeID();
		gCFBooleanType		= CFBooleanGetTypeID();
		gCFDataType			= CFDataGetTypeID();
		gCFDateType			= CFDateGetTypeID();
		gCFDictionaryType	= CFDictionaryGetTypeID();
		gCFNullType			= CFNullGetTypeID();
		gCFNumberType		= CFNumberGetTypeID();
		gCFStringType		= CFStringGetTypeID();
	}
	
	ctx.data = CFDataCreateMutable( NULL, 0 );
	require_action( ctx.data, exit, err = kNoMemoryErr );
	
	ctx.flags = inFlags;
	err = _WriteJSONObject( &ctx, inPlist, 0, 0 );
	require_noerr( err, exit );
	
	if( !( inFlags & kJSONFlags_Condensed ) )
	{
		err = _WriteBytes( &ctx, "\n", 1 );
		require_noerr( err, exit );
	}
	
exit:
	if( outErr ) *outErr = err;
	return( ctx.data );
}

//===========================================================================================================================
//	_WriteJSONObject
//===========================================================================================================================

static OSStatus	_WriteJSONObject( CFJSONContext *ctx, CFPropertyListRef inObj, size_t inIndent, int inSameLine )
{
	int const			condensed	= ( ctx->flags & kJSONFlags_Condensed ) != 0;
	int const			escapeSlash	= ( ctx->flags & kJSONFlags_EscapeSlash ) != 0;
	OSStatus			err;
	CFTypeID			type;
	CFRange				range;
	CFIndex				i;
	CFIndex				n;
	CFIndex				size;
	CFIndex				used;
	CFTypeRef			obj;
	CFTypeRef *			keysAndValues;
	CFIndex				nTotal;
	const char *		src;
	const char *		end;
	const char *		ptr;
	char				c;
	char				cstr[ 2 ];
	
	keysAndValues = NULL;
	
	if( ( inIndent > 0 ) && !inSameLine )
	{
		err = _WriteIndent( ctx, inIndent );
		require_noerr( err, exit );
	}
	
	type = CFGetTypeID( inObj );
	
	// String
	
	if( type == gCFStringType )
	{
		err = _WriteBytes( ctx, "\"", 1 );
		require_noerr( err, exit );
		
		range.location = 0;
		range.length   = CFStringGetLength( (CFStringRef) inObj );
		while( range.length > 0 )
		{
			size = (CFIndex) sizeof( ctx->stringBuf );
			n = CFStringGetBytes( (CFStringRef) inObj, range, kCFStringEncodingUTF8, 0, false, 
				(uint8_t *) ctx->stringBuf, size, &used );
			require_action( n > 0, exit, err = kUnsupportedErr );
			
			c = '\0';
			src = ctx->stringBuf;
			end = src + used;
			while( src != end )
			{
				// Escape JSON escape sequences.
				
				ptr = src;
				if( escapeSlash )	while( ( src != end ) && ( ( c = *src ) != '"' ) && ( c != '\\' ) && ( c != '/' ) ) ++src;
				else				while( ( src != end ) && ( ( c = *src ) != '"' ) && ( c != '\\' ) ) ++src;
				
				err = _WriteBytes( ctx, ptr, (size_t)( src - ptr ) );
				require_noerr( err, exit );
				
				if( src != end )
				{
					cstr[ 0 ] = '\\';
					cstr[ 1 ] = c;
					err = _WriteBytes( ctx, cstr, 2 );
					require_noerr( err, exit );
					++src;
				}
			}
			
			range.location	+= n;
			range.length	-= n;
		}
		
		err = _WriteBytes( ctx, "\"", 1 );
		require_noerr( err, exit );
	}
	
	// Number
	
	else if( type == gCFNumberType )
	{
		if( CFNumberIsFloatType( (CFNumberRef) inObj ) )
		{
			double		d;
			
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &d );
			n = snprintf( ctx->stringBuf, sizeof( ctx->stringBuf ), "%f", d );
		}
		else
		{
			int64_t			s64;
			
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &s64 );
			n = SNPrintF( ctx->stringBuf, sizeof( ctx->stringBuf ), "%lld", s64 );
		}
		err = _WriteBytes( ctx, ctx->stringBuf, (size_t) n );
		require_noerr( err, exit );
	}
	
	// Boolean
	
	else if( type == gCFBooleanType )
	{
		const char *		tempPtr;
		
		if( inObj == kCFBooleanTrue ) { tempPtr = "true";  n = 4; }
		else						  { tempPtr = "false"; n = 5; }
		err = _WriteBytes( ctx, tempPtr, (size_t) n );
		require_noerr( err, exit );
	}
	
	// Dictionary
	
	else if( type == gCFDictionaryType )
	{
		if( condensed ) err = _WriteBytes( ctx, "{", 1 );
		else			err = _WriteBytes( ctx, "{\n", 2 );
		require_noerr( err, exit );
		
		n = CFDictionaryGetCount( (CFDictionaryRef) inObj );
		if( n > 0 )
		{
			nTotal = n * 2;
			keysAndValues = (CFTypeRef *) malloc( ( (size_t) nTotal ) * sizeof( *keysAndValues ) );
			require_action( keysAndValues, exit, err = kNoMemoryErr );
			CFDictionaryGetKeysAndValues( (CFDictionaryRef) inObj, &keysAndValues[ 0 ], &keysAndValues[ n ] );
			
			if( !condensed ) ++inIndent;
			for( i = 0; i < n; ++i )
			{
				err = _WriteJSONObject( ctx, keysAndValues[ i ], inIndent, 0 );
				require_noerr( err, exit );
				
				if( condensed ) err = _WriteBytes( ctx, ":", 1 );
				else			err = _WriteBytes( ctx, " : ", 3 );
				require_noerr( err, exit );
				
				err = _WriteJSONObject( ctx, keysAndValues[ n + i ], inIndent, 1 );
				require_noerr( err, exit );
				
				if( i < ( n - 1 ) )
				{
					err = _WriteBytes( ctx, ",", 1 );
					require_noerr( err, exit );
				}
				if( !condensed )
				{
					err = _WriteBytes( ctx, "\n", 1 );
					require_noerr( err, exit );
				}
			}
			if( !condensed ) --inIndent;
			
			free( (void *) keysAndValues );
			keysAndValues = NULL;
		}
		
		if( inIndent > 0 )
		{
			err = _WriteIndent( ctx, inIndent );
			require_noerr( err, exit );
		}
		err = _WriteBytes( ctx, "}", 1 );
		require_noerr( err, exit );
	}
	
	// Array
	
	else if( type == gCFArrayType )
	{
		if( condensed ) err = _WriteBytes( ctx, "[", 1 );
		else			err = _WriteBytes( ctx, "[\n", 2 );
		require_noerr( err, exit );
		
		n = CFArrayGetCount( (CFArrayRef) inObj );
		if( !condensed ) ++inIndent;
		for( i = 0; i < n; ++i )
		{
			obj = CFArrayGetValueAtIndex( (CFArrayRef) inObj, i );
			err = _WriteJSONObject( ctx, obj, inIndent, 0 );
			require_noerr( err, exit );
			
			if( i < ( n - 1 ) )
			{
				err = _WriteBytes( ctx, ",", 1 );
				require_noerr( err, exit );
			}
			if( !condensed )
			{
				err = _WriteBytes( ctx, "\n", 1 );
				require_noerr( err, exit );
			}
		}
		if( !condensed ) --inIndent;
		
		if( inIndent > 0 )
		{
			err = _WriteIndent( ctx, inIndent );
			require_noerr( err, exit );
		}
		err = _WriteBytes( ctx, "]", 1 );
		require_noerr( err, exit );
	}
	
	// Null
	
	else if( type == gCFNullType )
	{
		err = _WriteBytes( ctx, "null", 4 );
		require_noerr( err, exit );
	}
	
	// Data
	
	else if( type == gCFDataType )
	{
		// $$$ TO DO: figure out how to handle binary data.
		
		dlogassert( "CFData not supported" );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Date
	
	else if( type == gCFDateType )
	{
		dlogassert( "CFDate not supported" );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Unsupported object.
	
	else
	{
		dlogassert( "unknown object type: %d", type );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( keysAndValues ) free( (void *) keysAndValues );
	return( err );
}

//===========================================================================================================================
//	_WriteBytes
//===========================================================================================================================

static OSStatus	_WriteBytes( CFJSONContext *ctx, const void *inData, size_t inSize )
{
	CFDataAppendBytes( ctx->data, (const UInt8 *) inData, (CFIndex) inSize );
	return( kNoErr );
}

//===========================================================================================================================
//	_WriteIndent
//===========================================================================================================================

static OSStatus	_WriteIndent( CFJSONContext *ctx, size_t inIndent )
{
	size_t		len;
	size_t		i;
	
	for( ; inIndent > 0; inIndent -= len )
	{
		len = Min( inIndent, sizeof( ctx->stringBuf ) );
		for( i = 0; i < len; ++i ) ctx->stringBuf[ i ] = '\t';
		CFDataAppendBytes( ctx->data, (uint8_t *) ctx->stringBuf, (CFIndex) len );
	}
	return( kNoErr );
}
#endif // JSON_UTILS_USE_SYSTEM

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	JSONUtils_Test
//===========================================================================================================================

OSStatus	_JSONUtils_TestOne( const char *inJSON, Boolean inPrint );

OSStatus	JSONUtils_Test( Boolean inPrint )
{
	OSStatus			err;
	const char *		str;
	
	// Test 1
	
	str = 
	"{\n"
	"	\"glossary\": {\n"
	"		\"title\": \"example glossary\",\n"
	"		\"GlossDiv\": {\n"
	"			\"title\": \"S\",\n"
	"			\"GlossList\": {\n"
	"				\"GlossEntry\": {\n"
	"					\"ID\": \"SGML\",\n"
	"					\"SortAs\": \"SGML\",\n"
	"					\"GlossTerm\": \"Standard Generalized Markup Language\",\n"
	"					\"Acronym\": \"SGML\",\n"
	"					\"Abbrev\": \"ISO 8879:1986\",\n"
	"					\"GlossDef\": {\n"
	"						\"para\": \"A meta-markup language, used to create markup languages such as DocBook.\",\n"
	"						\"GlossSeeAlso\": [\"GML\", \"XML\"]\n"
	"					},\n"
	"					\"GlossSee\": \"markup\"\n"
	"				}\n"
	"			}\n"
	"		}\n"
	"	}\n"
	"}\n";
	
	err = _JSONUtils_TestOne( str, inPrint );
	require_noerr( err, exit );
	
	// Test 2
	
	str = 
	"{\n"
	"  \"Image\": {\n"
	"	  \"Width\":  800,\n"
	"	  \"Height\": 600,\n"
	"	  \"Title\":  \"View from \\\"15th\\\" Floor\",\n"
	"	  \"Thumbnail\": {\n"
	"		  \"Url\":    \"http://www.example.com/image/481989943\",\n"
	"		  \"Height\": 125,\n"
	"		  \"Width\":  \"100\"\n"
	"	  },\n"
	"	  \"IDs\": [116, 943, 234, 38793]\n"
	"	}\n"
	"}\n";
	
	err = _JSONUtils_TestOne( str, inPrint );
	require_noerr( err, exit );
	
	// Test 3
	
	str = 
	"[\n"
	"   {\n"
	"	  \"precision\": \"zip\",\n"
	"	  \"Latitude\":  37.7668,\n"
	"	  \"Longitude\": -122.3959,\n"
	"	  \"Address\":   \"\",\n"
	"	  \"City\":      \"SAN FRANCISCO \\\\ a // \",\n"
	"	  \"State\":     \"CA\",\n"
	"	  \"Zip\":       \"94107\",\n"
	"	  \"Country\":   \"US\"\n"
	"   },\n"
	"   {\n"
	"	  \"precision\": \"zip\",\n"
	"	  \"Latitude\":  37.371991,\n"
	"	  \"Longitude\": -122.026020,\n"
	"	  \"Address\":   \"\",\n"
	"	  \"City\":      \"SUNNYVALE\",\n"
	"	  \"State\":     \"CA\",\n"
	"	  \"Zip\":       \"94085\",\n"
	"	  \"Country\":   \"US\",\n"
	"	  \"Null\":      null\n"
	"   }\n"
	"]\n";
	
	err = _JSONUtils_TestOne( str, inPrint );
	require_noerr( err, exit );
	
exit:
	printf( "JSONUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

OSStatus	_JSONUtils_TestOne( const char *inJSON, Boolean inPrint )
{
	OSStatus		err;
	CFTypeRef		obj1 = NULL;
	CFTypeRef		obj2 = NULL;
	CFDataRef		data;
	
	if( inPrint )
	{
		FPrintF( stderr, "ORIGINAL JSON\n" );
		FPrintF( stderr, "-------------\n" );
		FPrintF( stderr, "%s\n", inJSON );
		
		FPrintF( stderr, "JSON -> CF\n" );
		FPrintF( stderr, "----------\n" );
	}
	obj1 = CFCreateWithJSONBytes( inJSON, kSizeCString, 0, 0, &err );
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%@\n", obj1 );
	
	if( inPrint )
	{
		FPrintF( stderr, "CF -> JSON\n" );
		FPrintF( stderr, "----------\n" );
	}
	data = CFCreateJSONData( obj1, kJSONFlags_None, &err );
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%.*s\n", (int) CFDataGetLength( data ), CFDataGetBytePtr( data ) );
	
	obj2 = CFCreateWithJSONBytes( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), 0, 0, &err );
	CFRelease( data );
	require_noerr( err, exit );
	require_action( CFEqual( obj1, obj2 ), exit, err = -1 );
	
	if( inPrint )
	{
		FPrintF( stderr, "CF -> JSON (condensed)\n" );
		FPrintF( stderr, "----------------------\n" );
	}
	data = CFCreateJSONData( obj1, kJSONFlags_Condensed, &err );
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%.*s\n", (int) CFDataGetLength( data ), CFDataGetBytePtr( data ) );
	
	CFRelease( obj2 );
	obj2 = CFCreateWithJSONBytes( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), 0, 0, &err );
	CFRelease( data );
	require_noerr( err, exit );
	require_action( CFEqual( obj1, obj2 ), exit, err = -1 );
	
	if( inPrint ) FPrintF( stderr, "\n\n" );
	
exit:
	CFReleaseNullSafe( obj1 );
	CFReleaseNullSafe( obj2 );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
