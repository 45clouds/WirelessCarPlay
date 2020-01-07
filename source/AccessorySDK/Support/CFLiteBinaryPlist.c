/*
	File:    	CFLiteBinaryPlist.c
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
*/

#include "CFLiteBinaryPlist.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "MiscUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <limits.h>
	#include <stdarg.h>
	#include <stddef.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#include CF_HEADER

#if( !defined( CFCOMPAT_HAS_UNICODE_SUPPORT ) )
	#define CFCOMPAT_HAS_UNICODE_SUPPORT		0
#endif
#if( CFCOMPAT_HAS_UNICODE_SUPPORT )
	#include "utfconv.h"
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kMaxDepth								32

#define	kCFLHeaderSignaturePtr					"CFB1"
#define	kCFLHeaderSignatureLen					sizeof_string( kCFLHeaderSignaturePtr )
#define	kCFLTrailerSignaturePtr					"END!"
#define	kCFLTrailerSignatureLen					sizeof_string( kCFLTrailerSignaturePtr )

#define	kCFLBinaryPlistMarkerNull				0x00
#define	kCFLBinaryPlistMarkerNullTerminator		0x01
#define	kCFLBinaryPlistMarkerFalse				0x08
#define	kCFLBinaryPlistMarkerTrue				0x09
#define	kCFLBinaryPlistMarkerFill				0x0F
#define	kCFLBinaryPlistMarkerInt				0x10
#define	kCFLBinaryPlistMarkerReal				0x20
#define	kCFLBinaryPlistMarkerDateBase			0x30
#define	kCFLBinaryPlistMarkerDateInteger		0x30
#define	kCFLBinaryPlistMarkerDateFloat			0x33
#define	kCFLBinaryPlistMarkerData				0x40
#define	kCFLBinaryPlistMarkerASCIIString		0x50
#define	kCFLBinaryPlistMarkerUnicodeString		0x60
#define	kCFLBinaryPlistMarkerUTF8String			0x70
#define	kCFLBinaryPlistMarkerUID				0x80
#define	kCFLBinaryPlistMarkerSmallInteger		0x90
#define	kCFLBinaryPlistMarkerArray				0xA0
#define	kCFLBinaryPlistMarkerDictionary			0xD0

//===========================================================================================================================
//	Types
//===========================================================================================================================

// CFBinaryPlistContext

typedef struct
{
	CFMutableDataRef				data;
	union
	{
		uint16_t					u16[ 128 ];
		uint8_t						u8[ 256 ];
		
	}	tempBuf;
	CFMutableArrayRef				array;
	CFMutableDictionaryRef			uniqueDict;
	CFIndex							uniqueCount;
	size_t							bytesWritten;
	uint8_t							objectRefSize;
	uint8_t							offsetIntSize;
	size_t							offsetTableOffset;
	uint32_t						flags;
	CFBinaryPlistStreamedWrite_f	writer_f;
	void *							writer_ctx;
	
}	CFBinaryPlistContext;

#define CFBinaryPlistContextInit( CTX ) \
	do \
	{ \
		(CTX)->data					= NULL; \
		(CTX)->array				= NULL; \
		(CTX)->uniqueDict			= NULL; \
		(CTX)->uniqueCount			= 0; \
		(CTX)->bytesWritten			= 0; \
		(CTX)->objectRefSize		= 0; \
		(CTX)->offsetIntSize		= 0; \
		(CTX)->offsetTableOffset	= 0; \
		(CTX)->flags				= 0; \
		(CTX)->writer_f				= NULL; \
		(CTX)->writer_ctx			= NULL; \
		\
	}	while( 0 )

#define CFBinaryPlistContextFree( CTX ) \
	do \
	{ \
		ForgetCF( &(CTX)->data ); \
		ForgetCF( &(CTX)->array ); \
		ForgetCF( &(CTX)->uniqueDict ); \
		\
	}	while( 0 )

// CFBinaryPlistStreamedReadContext

typedef struct
{
	CFBinaryPlistStreamedFlags		flags;
	CFDataRef						data;
	const uint8_t *					base;
	CFMutableDictionaryRef			uniqueDict;
	CFIndex							uniqueCount;
	int32_t							depth;
	
}	CFBinaryPlistStreamedReadContext;

// CFBinaryPlistDictionaryApplierContext

typedef struct
{
	CFBinaryPlistContext *		plistCtx;
	CFDictionaryRef				dict;
	OSStatus					err;
	
}	CFBinaryPlistDictionaryApplierContext;

// CFBinaryPlistTrailer

typedef struct
{
	uint8_t			unused[ 5 ];
	uint8_t			sortVersion;
	uint8_t			offsetIntSize;
	uint8_t			objectRefSize;
	uint64_t		numObjects;
	uint64_t		topObject;
	uint64_t		offsetTableOffset;
	
}	CFBinaryPlistTrailer;

check_compile_time( offsetof( CFBinaryPlistTrailer, unused )			==  0 );
check_compile_time( offsetof( CFBinaryPlistTrailer, sortVersion )		==  5 );
check_compile_time( offsetof( CFBinaryPlistTrailer, offsetIntSize )		==  6 );
check_compile_time( offsetof( CFBinaryPlistTrailer, objectRefSize )		==  7 );
check_compile_time( offsetof( CFBinaryPlistTrailer, numObjects )		==  8 );
check_compile_time( offsetof( CFBinaryPlistTrailer, topObject )			== 16 );
check_compile_time( offsetof( CFBinaryPlistTrailer, offsetTableOffset )	== 24 );
check_compile_time( sizeof(   CFBinaryPlistTrailer )					== 32 );

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void		_GlobalEnsureInitialized( void );

static OSStatus	_WriteStreamed( CFBinaryPlistContext *ctx, CFTypeRef inObj );
static OSStatus	_WriteStreamedObject( CFBinaryPlistContext *inContext, CFTypeRef inObj );
static void		_WriteStreamedDictionaryApplier( const void *inKey, const void *inValue, void *inContext );
static OSStatus	_WriteStreamedUpdateUID( CFBinaryPlistContext *ctx, CFTypeRef inObj, Boolean *outSkip );
CF_RETURNS_RETAINED
static CFTypeRef
	_ReadStreamed( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t *						inPtr, 
		const uint8_t * const				inEnd, 
		OSStatus *							outErr );
static OSStatus
	_ReadStreamedObject( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t **					ioPtr, 
		const uint8_t *						inEnd, 
		CFTypeRef *							outObj );
static OSStatus
	_ReadStreamedUIDObject( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t **					ioPtr, 
		const uint8_t *						inEnd, 
		uint8_t								inLen, 
		CFTypeRef *							outObj );

static void		_FlattenPlist( CFBinaryPlistContext *ctx, CFTypeRef inObj );
static void		_FlattenArray( const void *inValue, void *inContext );
static void		_FlattenDictionaryKey( const void *inKey, const void *inValue, void *inContext );
static void		_FlattenDictionaryValue( const void *inKey, const void *inValue, void *inContext );
static Boolean	_ObjectsExactlyEqual( const void *a, const void *b );
static void		_WriteV0ArrayValue( const void *inValue, void *inContext );
static void		_WriteV0DictionaryKey( const void *inKey, const void *inValue, void *inContext );
static void		_WriteV0DictionaryValue( const void *inKey, const void *inValue, void *inContext );
static OSStatus	_WriteV0Object( CFBinaryPlistContext *inContext, CFTypeRef inObj );
static OSStatus	_WriteV0String( CFBinaryPlistContext *ctx, CFStringRef inStr );

CF_RETURNS_RETAINED
static CFTypeRef
	_ReadV0Object( 
		CFBinaryPlistContext *	ctx, 
		const uint8_t *			inSrc, 
		const uint8_t *			inEnd, 
		size_t					inOffset, 
		OSStatus *				outErr );
static OSStatus
	_ReadRefOffset( 
		CFBinaryPlistContext *	ctx, 
		const uint8_t *			inSrc, 
		const uint8_t *			inEnd, 
		const uint8_t **		ioPtr, 
		size_t *				outOffset );

static OSStatus
	_ReadInteger( 
		const uint8_t **	ioPtr, 
		const uint8_t *		inEnd, 
		uint64_t *			outValue, 
		Boolean				inAllowSmall, 
		Boolean				inLittleEndian );
static OSStatus
	_ReadSizedInteger( 
		const uint8_t **	ioPtr, 
		const uint8_t *		inEnd, 
		size_t				inLen, 
		uint64_t *			outValue, 
		Boolean				inLittleEndian );
static OSStatus	_WriteBytes( CFBinaryPlistContext *inContext, const void *inData, size_t inSize );
static OSStatus	_WriteInteger( CFBinaryPlistContext *ctx, uint64_t inValue, Boolean inAllowSmall, Boolean inLittleEndian );
static OSStatus	_WriteNumber( CFBinaryPlistContext *ctx, CFNumberRef inNum, Boolean inAllowSmall, Boolean inLittleEndian );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static CFTypeID		gCFArrayType		= (CFTypeID) -1;
static CFTypeID		gCFBooleanType		= (CFTypeID) -1;
static CFTypeID		gCFDataType			= (CFTypeID) -1;
static CFTypeID		gCFDateType			= (CFTypeID) -1;
static CFTypeID		gCFDictionaryType	= (CFTypeID) -1;
static CFTypeID		gCFNumberType		= (CFTypeID) -1;
static CFTypeID		gCFStringType		= (CFTypeID) -1;

//===========================================================================================================================
//	_GlobalEnsureInitialized
//===========================================================================================================================

static void	_GlobalEnsureInitialized( void )
{
	if( gCFStringType == ( (CFTypeID) -1 ) )
	{
		gCFArrayType		= CFArrayGetTypeID();
		gCFBooleanType		= CFBooleanGetTypeID();
		gCFDataType			= CFDataGetTypeID();
		gCFDateType			= CFDateGetTypeID();
		gCFDictionaryType	= CFDictionaryGetTypeID();
		gCFNumberType		= CFNumberGetTypeID();
		gCFStringType		= CFStringGetTypeID();
	}
}

#if 0
#pragma mark -
#pragma mark == Streamed ==
#endif

//===========================================================================================================================
//	CFBinaryPlistStreamedCreateData
//===========================================================================================================================

CFDataRef	CFBinaryPlistStreamedCreateData( CFTypeRef inObj, OSStatus *outErr )
{
	return( CFBinaryPlistStreamedCreateDataEx( inObj, kCFBinaryPlistStreamedFlags_Default, outErr ) );
}

//===========================================================================================================================
//	CFBinaryPlistStreamedCreateDataEx
//===========================================================================================================================

CFDataRef	CFBinaryPlistStreamedCreateDataEx( CFTypeRef inObj, CFBinaryPlistStreamedFlags inFlags, OSStatus *outErr )
{
	CFDataRef					data = NULL;
	OSStatus					err;
	CFBinaryPlistContext		ctx;
	
	_GlobalEnsureInitialized();
	CFBinaryPlistContextInit( &ctx );
	
	ctx.data = CFDataCreateMutable( NULL, 0 );
	require_action( ctx.data, exit, err = kNoMemoryErr );
	
	ctx.flags = inFlags;
	
	err = _WriteStreamed( &ctx, inObj );
	require_noerr( err, exit );
	
	data = ctx.data;
	ctx.data = NULL;
	
exit:
	CFBinaryPlistContextFree( &ctx );
	if( outErr ) *outErr = err;
	return( data );
}

//===========================================================================================================================
//	CFBinaryPlistStreamedWriteBytes
//===========================================================================================================================

OSStatus
	CFBinaryPlistStreamedWriteBytes( 
		const void *					inData, 
		size_t							inLen, 
		CFBinaryPlistStreamedWrite_f	inCallback, 
		void *							inContext )
{
	OSStatus					err;
	CFBinaryPlistContext		ctx;
	uint8_t						marker;
	
	_GlobalEnsureInitialized();
	CFBinaryPlistContextInit( &ctx );
	ctx.writer_f	= inCallback;
	ctx.writer_ctx	= inContext;
	
	if( inLen < 15 ) marker = (uint8_t)( kCFLBinaryPlistMarkerData | inLen );
	else			 marker = kCFLBinaryPlistMarkerData | 0xF;
	err = _WriteBytes( &ctx, &marker, 1 );
	require_noerr( err, exit );
	
	if( inLen >= 15 )
	{
		err = _WriteInteger( &ctx, inLen, true, true );
		require_noerr( err, exit );
	}
	
	err = _WriteBytes( &ctx, inData, inLen );
	require_noerr( err, exit );
	
exit:
	CFBinaryPlistContextFree( &ctx );
	return( err );
}

//===========================================================================================================================
//	CFBinaryPlistStreamedWriteObject
//===========================================================================================================================

OSStatus
	CFBinaryPlistStreamedWriteObject( 
		CFTypeRef						inObj, 
		CFBinaryPlistStreamedFlags		inFlags, 
		CFBinaryPlistStreamedWrite_f	inCallback, 
		void *							inContext )
{
	OSStatus					err;
	CFBinaryPlistContext		ctx;
	
	_GlobalEnsureInitialized();
	CFBinaryPlistContextInit( &ctx );
	ctx.flags		= inFlags;
	ctx.writer_f	= inCallback;
	ctx.writer_ctx	= inContext;
	err = _WriteStreamed( &ctx, inObj );
	CFBinaryPlistContextFree( &ctx );
	return( err );
}

//===========================================================================================================================
//	_WriteStreamed
//===========================================================================================================================

static OSStatus	_WriteStreamed( CFBinaryPlistContext *ctx, CFTypeRef inObj )
{
	CFBinaryPlistStreamedFlags const		flags = ctx->flags;
	OSStatus								err;
	
	if( flags & kCFBinaryPlistStreamedFlag_Unique )
	{
		CFDictionaryKeyCallBacks		dictCallbacks;
		
		dictCallbacks			= kCFTypeDictionaryKeyCallBacks;
		dictCallbacks.equal		= _ObjectsExactlyEqual;
		dictCallbacks.retain	= NULL;
		dictCallbacks.release	= NULL;
		ctx->uniqueDict = CFDictionaryCreateMutable( NULL, 0, &dictCallbacks, NULL );
		require_action( ctx->uniqueDict, exit, err = kNoMemoryErr );
	}
	if( flags & kCFBinaryPlistStreamedFlag_Header )
	{
		err = _WriteBytes( ctx, kCFLHeaderSignaturePtr, kCFLHeaderSignatureLen );
		require_noerr( err, exit );
	}
	if( flags & ( kCFBinaryPlistStreamedFlag_Begin | kCFBinaryPlistStreamedFlag_End | kCFBinaryPlistStreamedFlag_Body ) )
	{
		err = _WriteStreamedObject( ctx, inObj );
		require_noerr( err, exit );
	}
	if( flags & kCFBinaryPlistStreamedFlag_Trailer )
	{
		err = _WriteBytes( ctx, kCFLTrailerSignaturePtr, kCFLTrailerSignatureLen );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteStreamedObject
//===========================================================================================================================

static OSStatus	_WriteStreamedObject( CFBinaryPlistContext *ctx, CFTypeRef inObj )
{
	CFBinaryPlistStreamedFlags const		flags	= ctx->flags;
	CFTypeID const							type	= CFGetTypeID( inObj );
	OSStatus								err;
	uint8_t									marker;
	CFRange									range;
	CFIndex									i, n, size, used;
	const char *							cptr;
	CFTypeRef								obj;
	uint8_t									buf[ 9 ];
	Boolean									skip;
	
	// String
	
	if( type == gCFStringType )
	{
		// When uniquing, check if non-empty strings have already been written and write a UID instead.
		
		if( ctx->uniqueDict && ( CFStringGetLength( (CFStringRef) inObj ) > 0 ) )
		{
			err = _WriteStreamedUpdateUID( ctx, inObj, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		// Write UTF-8 bytes with a NUL terminator.
		
		marker = kCFLBinaryPlistMarkerUTF8String;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		
		cptr = CFStringGetCStringPtr( (CFStringRef) inObj, kCFStringEncodingUTF8 );
		if( cptr )
		{
			err = _WriteBytes( ctx, cptr, strlen( cptr ) + 1 );
			require_noerr( err, exit );
		}
		else
		{
			range.location	= 0;
			range.length	= CFStringGetLength( (CFStringRef) inObj );
			while( range.length > 0 )
			{
				size = (CFIndex) sizeof( ctx->tempBuf );
				n = CFStringGetBytes( (CFStringRef) inObj, range, kCFStringEncodingUTF8, 0, false, ctx->tempBuf.u8, size, &used );
				require_action( n > 0, exit, err = kUnsupportedErr );
				
				err = _WriteBytes( ctx, ctx->tempBuf.u8, (size_t) used );
				require_noerr( err, exit );
				range.location	+= n;
				range.length	-= n;
			}
			
			err = _WriteBytes( ctx, "", 1 );
			require_noerr( err, exit );
		}
	}
	
	// Number
	
	else if( type == gCFNumberType )
	{
		err = _WriteNumber( ctx, (CFNumberRef) inObj, true, true );
		require_noerr( err, exit );
	}
	
	// Boolean
	
	else if( type == gCFBooleanType )
	{
		marker = ( inObj == kCFBooleanTrue ) ? kCFLBinaryPlistMarkerTrue : kCFLBinaryPlistMarkerFalse;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
	}
	
	// Data
	
	else if( type == gCFDataType )
	{
		// When uniquing, check if data objects > 1 byte have already been written and write a UID instead.
		
		n = CFDataGetLength( (CFDataRef) inObj );
		if( ( n > 1 ) && ctx->uniqueDict )
		{
			err = _WriteStreamedUpdateUID( ctx, inObj, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		// For lengths  < 0xF, the count is stored in the low 4 bits of the marker.
		// For lengths >= 0xF, 0xF is stored in the low 4 bits of the marker and an integer count follows.
		
		if( n < 15 ) marker = (uint8_t)( kCFLBinaryPlistMarkerData | n );
		else		 marker = kCFLBinaryPlistMarkerData | 0xF;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		
		if( n >= 15 )
		{
			err = _WriteInteger( ctx, (uint64_t) n, true, true );
			require_noerr( err, exit );
		}
		
		err = _WriteBytes( ctx, CFDataGetBytePtr( (CFDataRef) inObj ), (size_t) n );
		require_noerr( err, exit );
	}
	
	// Date
	
	else if( type == gCFDateType )
	{
		if( ctx->uniqueDict )
		{
			err = _WriteStreamedUpdateUID( ctx, inObj, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		buf[ 0 ] = kCFLBinaryPlistMarkerDateFloat;
		WriteLittleFloat64( &buf[ 1 ], CFDateGetAbsoluteTime( (CFDateRef) inObj ) );
		err = _WriteBytes( ctx, buf, 9 );
		require_noerr( err, exit );
	}
	
	// Dictionary
	
	else if( type == gCFDictionaryType )
	{
		if( flags & kCFBinaryPlistStreamedFlag_Begin )
		{
			marker = kCFLBinaryPlistMarkerDictionary;
			err = _WriteBytes( ctx, &marker, 1 );
			require_noerr( err, exit );
		}
		if( flags & kCFBinaryPlistStreamedFlag_Body )
		{
			CFDictionaryRef const		dict = (CFDictionaryRef) inObj;
			
			n = CFDictionaryGetCount( dict );
			if( n <= 16 )
			{
				const void *		keys[ 16 ];
				const void *		values[ 16 ];
				
				CFDictionaryGetKeysAndValues( dict, keys, values );
				for( i = 0; i < n; ++i )
				{
					err = _WriteStreamedObject( ctx, keys[ i ] );
					require_noerr( err, exit );
					
					err = _WriteStreamedObject( ctx, values[ i ] );
					require_noerr( err, exit );
				}
			}
			else
			{
				CFBinaryPlistDictionaryApplierContext		dictApplierCtx;
				
				dictApplierCtx.plistCtx	= ctx;
				dictApplierCtx.dict		= dict;
				dictApplierCtx.err		= kNoErr;
				CFDictionaryApplyFunction( dictApplierCtx.dict, _WriteStreamedDictionaryApplier, &dictApplierCtx );
				require_noerr_action( dictApplierCtx.err, exit, err = dictApplierCtx.err );
			}
		}
		if( flags & kCFBinaryPlistStreamedFlag_End )
		{
			marker = kCFLBinaryPlistMarkerNullTerminator;
			err = _WriteBytes( ctx, &marker, 1 );
			require_noerr( err, exit );
		}
	}
	
	// Array
	
	else if( type == gCFArrayType )
	{
		if( flags & kCFBinaryPlistStreamedFlag_Begin )
		{
			marker = kCFLBinaryPlistMarkerArray;
			err = _WriteBytes( ctx, &marker, 1 );
			require_noerr( err, exit );
		}
		if( flags & kCFBinaryPlistStreamedFlag_Body )
		{
			n = CFArrayGetCount( (CFArrayRef) inObj );
			for( i = 0; i < n; ++i )
			{
				obj = CFArrayGetValueAtIndex( (CFArrayRef) inObj, i );
				err = _WriteStreamedObject( ctx, obj );
				require_noerr( err, exit );
			}
		}
		if( flags & kCFBinaryPlistStreamedFlag_End )
		{
			marker = kCFLBinaryPlistMarkerNullTerminator;
			err = _WriteBytes( ctx, &marker, 1 );
			require_noerr( err, exit );
		}
	}
	
	// Null
	
	else if( inObj == ( (CFTypeRef) kCFNull ) )
	{
		marker = kCFLBinaryPlistMarkerNull;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
	}
	
	// Unknown
	
	else
	{
		dlogassert( "Unknown object type: %u", type );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteStreamedDictionaryApplier
//===========================================================================================================================

static void	_WriteStreamedDictionaryApplier( const void *inKey, const void *inValue, void *inContext )
{
	CFBinaryPlistDictionaryApplierContext * const		ctx = (CFBinaryPlistDictionaryApplierContext *) inContext;
	OSStatus											err;
	
	require_noerr_action_quiet( ctx->err, exit, err = kNoErr );
	
	err = _WriteStreamedObject( ctx->plistCtx, inKey );
	require_noerr( err, exit );
	
	err = _WriteStreamedObject( ctx->plistCtx, inValue );
	require_noerr( err, exit );
	
exit:
	if( err ) ctx->err = err;
}

//===========================================================================================================================
//	_WriteStreamedUpdateUID
//===========================================================================================================================

static OSStatus	_WriteStreamedUpdateUID( CFBinaryPlistContext *ctx, CFTypeRef inObj, Boolean *outSkip )
{
	OSStatus		err;
	uintptr_t		uid;
	uint8_t			buf[ 9 ];
	size_t			len;
	
	uid = (uintptr_t) CFDictionaryGetValue( ctx->uniqueDict, inObj );
	if( uid )
	{
		uid -= 1;
		if( uid <= 0xFF )
		{
			buf[ 0 ] = kCFLBinaryPlistMarkerUID | 0;
			buf[ 1 ] = (uint8_t) uid;
			len = 2;
		}
		else if( uid <= 0xFFFF )
		{
			buf[ 0 ] = kCFLBinaryPlistMarkerUID | 1;
			WriteLittle16( &buf[ 1 ], uid );
			len = 3;
		}
		else if( uid <= 0xFFFFFF )
		{
			buf[ 0 ] = kCFLBinaryPlistMarkerUID | 2;
			WriteLittle24( &buf[ 1 ], uid );
			len = 4;
		}
		else if( uid <= UINT32_C( 0xFFFFFFFF ) )
		{
			buf[ 0 ] = kCFLBinaryPlistMarkerUID | 3;
			WriteLittle32( &buf[ 1 ], uid );
			len = 5;
		}
		else
		{
			dlogassert( "UID too big" );
			err = kRangeErr;
			goto exit;
		}
		err = _WriteBytes( ctx, buf, len );
		require_noerr( err, exit );
		
		*outSkip = true;
	}
	else
	{
		CFDictionaryAddValue( ctx->uniqueDict, inObj, (const void *)(uintptr_t) ++ctx->uniqueCount );
		*outSkip = false;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFBinaryPlistStreamedCreateWithBytes
//===========================================================================================================================

CFTypeRef	CFBinaryPlistStreamedCreateWithBytes( const void *inPtr, size_t inLen, OSStatus *outErr )
{
	CFBinaryPlistStreamedFlags		flags;
	
	flags = kCFBinaryPlistStreamedFlag_Header | kCFBinaryPlistStreamedFlag_Trailer | kCFBinaryPlistStreamedFlag_Body;
	return( CFBinaryPlistStreamedCreateWithBytesEx( inPtr, inLen, flags, outErr ) );
}

//===========================================================================================================================
//	CFBinaryPlistStreamedCreateWithBytesEx
//===========================================================================================================================

CFTypeRef
	CFBinaryPlistStreamedCreateWithBytesEx( 
		const void *				inPtr, 
		size_t						inLen, 
		CFBinaryPlistStreamedFlags	inFlags, 
		OSStatus *					outErr )
{
	CFBinaryPlistStreamedReadContext		ctx;
	
	ctx.flags		= inFlags;
	ctx.data		= NULL;
	ctx.base		= (const uint8_t *) inPtr;
	ctx.uniqueDict	= NULL;
	ctx.uniqueCount	= 0;
	ctx.depth		= 0;
	return( _ReadStreamed( &ctx, ctx.base, ctx.base + inLen, outErr ) );
}

//===========================================================================================================================
//	CFBinaryPlistStreamedCreateWithData
//===========================================================================================================================

CFTypeRef	CFBinaryPlistStreamedCreateWithData( CFDataRef inData, CFBinaryPlistStreamedFlags inFlags, OSStatus *outErr )
{
	CFBinaryPlistStreamedReadContext		ctx;
	
	ctx.flags		= inFlags;
	ctx.data		= inData;
	ctx.base		= CFDataGetBytePtr( inData );
	ctx.uniqueDict	= NULL;
	ctx.uniqueCount	= 0;
	ctx.depth		= 0;
	return( _ReadStreamed( &ctx, ctx.base, ctx.base + CFDataGetLength( inData ), outErr ) );
}

//===========================================================================================================================
//	_ReadStreamed
//===========================================================================================================================

static CFTypeRef
	_ReadStreamed( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t *						inPtr, 
		const uint8_t * const				inEnd, 
		OSStatus *							outErr )
{
	CFTypeRef		result	= NULL;
	CFTypeRef		obj		= NULL;
	OSStatus		err;
	
	_GlobalEnsureInitialized();
	
	if( ctx->flags & kCFBinaryPlistStreamedFlag_Unique )
	{
		CFDictionaryValueCallBacks		dictCallbacks;
		
		dictCallbacks			= kCFTypeDictionaryValueCallBacks;
		dictCallbacks.equal		= _ObjectsExactlyEqual;
		dictCallbacks.retain	= NULL;
		dictCallbacks.release	= NULL;
		ctx->uniqueDict = CFDictionaryCreateMutable( NULL, 0, NULL, &dictCallbacks );
		require_action( ctx->uniqueDict, exit, err = kNoMemoryErr );
	}
	if( ctx->flags & kCFBinaryPlistStreamedFlag_Header )
	{
		require_action_quiet( ( inEnd - inPtr ) >= ( (ptrdiff_t) kCFLHeaderSignatureLen ), exit, err = kSizeErr );
		require_action_quiet( memcmp( inPtr, kCFLHeaderSignaturePtr, kCFLHeaderSignatureLen ) == 0, exit, err = kSignatureErr );
		inPtr += kCFLHeaderSignatureLen;
	}
	if( ctx->flags & kCFBinaryPlistStreamedFlag_Body )
	{
		err = _ReadStreamedObject( ctx, &inPtr, inEnd, &obj );
		require_noerr_quiet( err, exit );
		require_action_quiet( obj, exit, err = kMalformedErr );
	}
	if( ctx->flags & kCFBinaryPlistStreamedFlag_Trailer )
	{
		require_action_quiet( ( inEnd - inPtr ) >= ( (ptrdiff_t) kCFLTrailerSignatureLen ), exit, err = kSizeErr );
		require_action_quiet( memcmp( inPtr, kCFLTrailerSignaturePtr, kCFLTrailerSignatureLen ) == 0, exit, err = kSignatureErr );
	}
	result	= obj;
	obj		= NULL;
	err		= kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	ForgetCF( &ctx->uniqueDict );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_ReadStreamedObject
//===========================================================================================================================

static OSStatus
	_ReadStreamedObject( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t **					ioPtr, 
		const uint8_t *						inEnd, 
		CFTypeRef *							outObj )
{
	OSStatus					err;
	const uint8_t *				p;
	const uint8_t *				r;
	uint8_t						marker;
	Value64						v64;
	CFMutableArrayRef			array	= NULL;
	CFMutableDictionaryRef		dict	= NULL;
	CFTypeRef					key		= NULL;
	CFTypeRef					value	= NULL;
	CFNumberRef					num;
	CFDateRef					date;
	
	p = *ioPtr;
	require_action_quiet( p < inEnd, exit, err = kUnderrunErr );
	
	marker = *p++;
	switch( marker & 0xF0 )
	{
		case 0: // 0 is the upper 4 bits for several object types so just use it directly.
			switch( marker )
			{
				case kCFLBinaryPlistMarkerNull:				*outObj = kCFNull;			break;
				case kCFLBinaryPlistMarkerNullTerminator:	*outObj = NULL;				break;
				case kCFLBinaryPlistMarkerFalse:			*outObj = kCFBooleanFalse;	break;
				case kCFLBinaryPlistMarkerTrue:				*outObj = kCFBooleanTrue;	break;
				default: err = kTypeErr; goto exit;
			}
			break;
		
		case kCFLBinaryPlistMarkerInt:
			
			// Read the integer as 2^nnnn bytes where bits nnnn come from the lower 4 bits of the marker.
			
			marker &= 0x0F;
			if( marker < 4 )
			{
				err = _ReadSizedInteger( &p, inEnd, 1 << marker, &v64.u64, true );
				require_noerr_quiet( err, exit );
				
				num = CFNumberCreate( NULL, kCFNumberSInt64Type, &v64.u64 );
				require_action( num, exit, err = kNoMemoryErr );
			}
			else if( marker == 4 )
			{
				uint128_compat		u128;
				
				require_action_quiet( ( inEnd - p ) >= 16, exit, err = kSizeErr );
				u128.hi = ReadLittle64( p ); p += 8;
				u128.lo = ReadLittle64( p ); p += 8;
				num = CFNumberCreate( NULL, kCFNumberSInt128Type_compat, &u128 );
				require_action( num, exit, err = kNoMemoryErr );
			}
			else
			{
				err = kCountErr;
				goto exit;
			}
			*outObj = num;
			
			if( ( marker > 0 ) && ctx->uniqueDict )
			{
				CFDictionaryAddValue( ctx->uniqueDict, (const void *)(uintptr_t) ctx->uniqueCount++, num );
			}
			break;
		
		case kCFLBinaryPlistMarkerReal:
			
			// The low 4 bits of the marker are log2( byteCount ) so 2^2 = 4-byte Float32, 2^3 = 8-byte Float64.
			
			switch( marker & 0x0F )
			{
				case 2:
					require_action_quiet( ( inEnd - p ) >= 4, exit, err = kSizeErr );
					v64.f32[ 0 ] = ReadLittleFloat32( p );
					num = CFNumberCreate( NULL, kCFNumberFloat32Type, &v64.f32[ 0 ] );
					require_action( num, exit, err = kNoMemoryErr );
					p += 4;
					*outObj = num;
					break;
				
				case 3:
					require_action_quiet( ( inEnd - p ) >= 8, exit, err = kSizeErr );
					v64.f64 = ReadLittleFloat64( p );
					num = CFNumberCreate( NULL, kCFNumberFloat64Type, &v64.f64 );
					require_action( num, exit, err = kNoMemoryErr );
					p += 8;
					*outObj = num;
					break;
				
				default:
					err = kSizeErr;
					goto exit;
			}
			if( ctx->uniqueDict )
			{
				CFDictionaryAddValue( ctx->uniqueDict, (const void *)(uintptr_t) ctx->uniqueCount++, num );
			}
			break;
		
		case kCFLBinaryPlistMarkerDateBase:
			
			// Date is a little endian 64-bit IEEE float stored seconds with an epoch of 2001-01-01 00:00:00.
			
			require_action_quiet( marker == kCFLBinaryPlistMarkerDateFloat, exit, err = kTypeErr );
			require_action_quiet( ( inEnd - p ) >= 8, exit, err = kSizeErr );
			date = CFDateCreate( NULL, ReadLittleFloat64( p ) );
			require_action( date, exit, err = kNoMemoryErr );
			p += 8;
			*outObj = date;
			
			if( ctx->uniqueDict )
			{
				CFDictionaryAddValue( ctx->uniqueDict, (const void *)(uintptr_t) ctx->uniqueCount++, date );
			}
			break;
		
		case kCFLBinaryPlistMarkerData:
		{
			CFDataRef		data;
			
			// For values < 0xF, the low 4 bits of the marker are the count.
			// For values >= 0xF, the low 4 bits of the marker are 0xF and an integer count follows.
			
			v64.u64 = marker & 0x0F;
			if( v64.u64 == 0xF )
			{
				err = _ReadInteger( &p, inEnd, &v64.u64, true, true );
				require_noerr_quiet( err, exit );
				require_action_quiet( v64.u64 <= 0x7FFFFFFF, exit, err = kRangeErr );
			}
			require_action_quiet( (ptrdiff_t) v64.u64 <= ( inEnd - p ) , exit, err = kUnderrunErr );
			
			#if( TARGET_OS_DARWIN || CFLITE_ENABLED ) // Subdata optimization not available everywhere.
			if( ctx->data && ( ctx->flags & kCFBinaryPlistStreamedFlag_NoCopy ) )
			{
				CFRange		range;
				
				range.location	= (CFIndex)( p - ctx->base );
				range.length	= (CFIndex) v64.u64;
				data = CFDataCreateSubdataWithRangeNoCopy( ctx->data, range, &err );
				require_noerr( err, exit );
			}
			else
			#endif
			{
				data = CFDataCreate( NULL, p, (CFIndex) v64.u64 );
				require_action( data, exit, err = kNoMemoryErr );
			}
			p += v64.u64;
			*outObj = data;
			
			if( ( v64.u64 > 1 ) && ctx->uniqueDict )
			{
				CFDictionaryAddValue( ctx->uniqueDict, (const void *)(uintptr_t) ctx->uniqueCount++, data );
			}
			break;
		}
		
		case kCFLBinaryPlistMarkerUTF8String:
		{
			CFStringRef		str;
			
			// UTF-8 strings are stored as NUL-terminated bytes so read until NUL
			
			for( r = p; ( r < inEnd ) && ( *r != '\0' ); ++r ) {}
			require_action_quiet( r < inEnd, exit, err = kUnderrunErr );
			
			str = CFStringCreateWithCString( NULL, (const char *) p, kCFStringEncodingUTF8 );
			require_action( str, exit, err = kMalformedErr );
			*outObj = str;
			
			if( ( *p != '\0' ) && ctx->uniqueDict )
			{
				CFDictionaryAddValue( ctx->uniqueDict, (const void *)(uintptr_t) ctx->uniqueCount++, str );
			}
			p = r + 1;
			break;
		}
		
		case kCFLBinaryPlistMarkerSmallInteger:
			
			// Small integers store the value directly in the low 4 bits of the marker.
			
			v64.s8[ 0 ] = marker & 0x0F;
			num = CFNumberCreate( NULL, kCFNumberSInt8Type, &v64.s8[ 0 ] );
			require_action( num, exit, err = kNoMemoryErr );
			*outObj = num;
			break;
		
		case kCFLBinaryPlistMarkerArray:
			
			// Arrays are objects serialized sequentially with a null terminator object at the end.
			
			array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			require_action( array, exit, err = kNoMemoryErr );
			
			for( ;; )
			{
				require_action_quiet( ctx->depth < kMaxDepth, exit, err = kOverrunErr );
				++ctx->depth;
				err = _ReadStreamedObject( ctx, &p, inEnd, &value );
				--ctx->depth;
				require_noerr_quiet( err, exit );
				if( !value ) break;
				
				CFArrayAppendValue( array, value );
				CFRelease( value );
				value = NULL;
			}
			*outObj = array; array = NULL;
			break;
		
		case kCFLBinaryPlistMarkerDictionary:
			
			// Dictionaries are key/value object pairs serialized sequentially with a null terminator key at the end.
			
			dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( dict, exit, err = kNoMemoryErr );
			
			for( ;; )
			{
				require_action_quiet( ctx->depth < kMaxDepth, exit, err = kOverrunErr );
				++ctx->depth;
				err = _ReadStreamedObject( ctx, &p, inEnd, &key );
				--ctx->depth;
				require_noerr_quiet( err, exit );
				if( !key ) break;
				
				++ctx->depth;
				err = _ReadStreamedObject( ctx, &p, inEnd, &value );
				--ctx->depth;
				require_noerr_quiet( err, exit );
				require_action_quiet( value, exit, err = kUnderrunErr );
				
				CFDictionarySetValue( dict, key, value );
				CFRelease( key );	key		= NULL;
				CFRelease( value );	value	= NULL;
			}
			
			*outObj = dict; dict = NULL;
			break;
		
		case kCFLBinaryPlistMarkerUID:
			err = _ReadStreamedUIDObject( ctx, &p, inEnd, marker & 0x0F, outObj );
			require_noerr_quiet( err, exit );
			break;
		
		default:
			err = kUnsupportedErr;
			goto exit;
	}
	*ioPtr = p;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( key );
	CFReleaseNullSafe( value );
	CFReleaseNullSafe( array );
	CFReleaseNullSafe( dict );
	return( err );
}

//===========================================================================================================================
//	_ReadStreamedUIDObject
//===========================================================================================================================

static OSStatus
	_ReadStreamedUIDObject( 
		CFBinaryPlistStreamedReadContext *	ctx, 
		const uint8_t **					ioPtr, 
		const uint8_t *						inEnd, 
		uint8_t								inLen, 
		CFTypeRef *							outObj )
{
	const uint8_t *		ptr = *ioPtr;
	OSStatus			err;
	uintptr_t			uid;
	CFTypeRef			obj;
	
	require_action_quiet( ( inEnd - ptr ) >= inLen, exit, err = kSizeErr );
	switch( inLen )
	{
		case 0:	uid = Read8( ptr );			ptr += 1; break;
		case 1: uid = ReadLittle16( ptr );	ptr += 2; break;
		case 2: uid = ReadLittle24( ptr );	ptr += 3; break;
		case 3: uid = ReadLittle32( ptr );	ptr += 4; break;
		default: err = kRangeErr; goto exit;
	}
	
	require_action_quiet( ctx->uniqueDict, exit, err = kUnsupportedDataErr );
	obj = CFDictionaryGetValue( ctx->uniqueDict, (const void *) uid );
	require_action_quiet( obj, exit, err = kIDErr );
	CFRetain( obj );
	
	*ioPtr	= ptr;
	*outObj	= obj;
	err		= kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Version 0 ==
#endif

//===========================================================================================================================
//	CFBinaryPlistV0CreateData
//===========================================================================================================================

CFDataRef	CFBinaryPlistV0CreateData( CFTypeRef inObj, OSStatus *outErr )
{
	return( CFBinaryPlistV0CreateDataEx( inObj, 0, outErr ) );
}

CFDataRef	CFBinaryPlistV0CreateDataEx( CFTypeRef inObj, uint32_t inFlags, OSStatus *outErr )
{
	CFDataRef						result = NULL;
	OSStatus						err;
	CFBinaryPlistContext			ctx;
	CFDictionaryKeyCallBacks		dictCallbacks;
	CFBinaryPlistTrailer			trailer;
	uint64_t *						offsets = NULL;
	CFIndex							i;
	CFTypeRef						obj;
	uint8_t							buf[ 8 ];
	
	_GlobalEnsureInitialized();
	CFBinaryPlistContextInit( &ctx );
	ctx.flags = inFlags;
	
	// Flatten the plist to an array of unique objects.
	
	ctx.data = CFDataCreateMutable( NULL, 0 );
	require_action( ctx.data, exit, err = kNoMemoryErr );
	
	ctx.array = CFArrayCreateMutable( NULL, 0, NULL );
	require_action( ctx.array, exit, err = kNoMemoryErr );
	
	dictCallbacks			= kCFTypeDictionaryKeyCallBacks;
	dictCallbacks.equal		= _ObjectsExactlyEqual;
	dictCallbacks.retain	= NULL;
	dictCallbacks.release	= NULL;
	ctx.uniqueDict = CFDictionaryCreateMutable( NULL, 0, &dictCallbacks, NULL );
	require_action( ctx.uniqueDict, exit, err = kNoMemoryErr );
	
	_FlattenPlist( &ctx, inObj );
	check( ctx.uniqueCount > 0 );
	
	// Write the header and object table.
	
	err = _WriteBytes( &ctx, "bplist00", 8 );
	require_noerr( err, exit );
	
	memset( &trailer, 0, sizeof( trailer ) );
	trailer.numObjects		= hton64( ctx.uniqueCount );
	trailer.objectRefSize	= MinPowerOf2BytesForValue( (uint64_t) ctx.uniqueCount );
	ctx.objectRefSize		= trailer.objectRefSize;
	
	offsets = (uint64_t *) malloc( ( (size_t) ctx.uniqueCount ) * sizeof( *offsets ) );
	require_action( offsets, exit, err = kNoMemoryErr );
	for( i = 0; i < ctx.uniqueCount; ++i )
	{
		offsets[ i ] = ctx.bytesWritten;
		obj = CFArrayGetValueAtIndex( ctx.array, i );
		err = _WriteV0Object( &ctx, obj );
		require_noerr( err, exit );
	}
	
	// Write the offsets table and trailer.
	
	trailer.offsetTableOffset	= hton64( ctx.bytesWritten );
	trailer.offsetIntSize		= MinPowerOf2BytesForValue( ctx.bytesWritten );
	for( i = 0; i < ctx.uniqueCount; ++i )
	{
		WriteBig64( buf, offsets[ i ] );
		err = _WriteBytes( &ctx, buf + ( sizeof( *offsets ) - trailer.offsetIntSize ), trailer.offsetIntSize );
		require_noerr( err, exit );
	}
	err = _WriteBytes( &ctx, &trailer, sizeof( trailer ) );
	require_noerr( err, exit );
	
	result = ctx.data;
	ctx.data = NULL;
	
exit:
	FreeNullSafe( offsets );
	CFBinaryPlistContextFree( &ctx );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_FlattenPlist
//===========================================================================================================================

static void	_FlattenPlist( CFBinaryPlistContext *ctx, CFTypeRef inObj )
{
	CFTypeID		type;
	CFIndex			oldCount;
	
	// Add the object and if the count doesn't change, it means the object was already there so we can skip it.
	// This doesn't unique arrays or dictionaries because it's slow and they have poor hash functions.
	
	oldCount = CFDictionaryGetCount( ctx->uniqueDict );
	CFDictionaryAddValue( ctx->uniqueDict, inObj, (const void *)(uintptr_t) ctx->uniqueCount );
	
	type = CFGetTypeID( inObj );
	if( ( type == gCFStringType )  || 
		( type == gCFNumberType )  ||
		( type == gCFBooleanType ) || 
		( type == gCFDataType )    || 
		( type == gCFDateType ) )
	{
		if( CFDictionaryGetCount( ctx->uniqueDict ) == oldCount )
		{
			return;
		}
	}
	CFArrayAppendValue( ctx->array, inObj );
	++ctx->uniqueCount;
	
	if( type == gCFDictionaryType )
	{
		CFDictionaryApplyFunction( (CFDictionaryRef) inObj, _FlattenDictionaryKey, ctx );
		CFDictionaryApplyFunction( (CFDictionaryRef) inObj, _FlattenDictionaryValue, ctx );
	}
	else if( type == gCFArrayType )
	{
		CFArrayApplyFunction( (CFArrayRef) inObj, CFRangeMake( 0, CFArrayGetCount( (CFArrayRef) inObj ) ), _FlattenArray, ctx );
	}
}

static void	_FlattenArray( const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	
	_FlattenPlist( ctx, inValue );
}

static void	_FlattenDictionaryKey( const void *inKey, const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	
	(void) inValue;
	
	_FlattenPlist( ctx, inKey );
}

static void	_FlattenDictionaryValue( const void *inKey, const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	
	(void) inKey;
	
	_FlattenPlist( ctx, inValue );
}

//===========================================================================================================================
//	_ObjectsExactlyEqual
//
//	This is needed because we need exact matches to avoid lossy roundtrip conversions:
//
//	1. CFEqual will treat kCFBooleanFalse == CFNumber(0) and kCFBooleanTrue == CFNumber(1).
//	2. CFEqual will treat CFNumber( 1.0 ) == CFNumber( 1 ).
//===========================================================================================================================

static Boolean	_ObjectsExactlyEqual( const void *a, const void *b )
{
	CFTypeID const		aType = CFGetTypeID( a );
	CFTypeID const		bType = CFGetTypeID( b );
	
	if( ( aType == bType ) && CFEqual( a, b ) )
	{
		if( aType != gCFNumberType )
		{
			return( true );
		}
		if( CFNumberIsFloatType( (CFNumberRef) a ) == CFNumberIsFloatType( (CFNumberRef) b ) )
		{
			return( true );
		}
	}
	return( false );
}

//===========================================================================================================================
//	_WriteV0Object
//===========================================================================================================================

static OSStatus	_WriteV0Object( CFBinaryPlistContext *ctx, CFTypeRef inObj )
{
	OSStatus		err;
	CFTypeID		type;
	uint8_t			marker;
	Value64			v;
	CFIndex			count;
	
	type = CFGetTypeID( inObj );
	if( type == gCFStringType )
	{
		err = _WriteV0String( ctx, (CFStringRef) inObj );
		require_noerr( err, exit );
	}
	else if( type == gCFNumberType )
	{
		err = _WriteNumber( ctx, (CFNumberRef) inObj, false, false );
		require_noerr( err, exit );
	}
	else if( type == gCFBooleanType )
	{
		marker = ( inObj == kCFBooleanTrue ) ? kCFLBinaryPlistMarkerTrue : kCFLBinaryPlistMarkerFalse;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
	}
	else if( type == gCFDataType )
	{
		count = CFDataGetLength( (CFDataRef) inObj );
		marker = (uint8_t)( kCFLBinaryPlistMarkerData | ( ( count < 15 ) ? count : 0xF ) );
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		if( count >= 15 )
		{
			err = _WriteInteger( ctx, (uint64_t) count, false, false );
			require_noerr( err, exit );
		}
		err = _WriteBytes( ctx, CFDataGetBytePtr( (CFDataRef) inObj ), (size_t) count );
		require_noerr( err, exit );
	}
	else if( type == gCFDictionaryType )
	{
		count = CFDictionaryGetCount( (CFDictionaryRef) inObj );
		marker = (uint8_t)( kCFLBinaryPlistMarkerDictionary | ( ( count < 15 ) ? count : 0xF ) );
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		if( count >= 15 )
		{
			err = _WriteInteger( ctx, (uint64_t) count, false, false );
			require_noerr( err, exit );
		}
		CFDictionaryApplyFunction( (CFDictionaryRef) inObj, _WriteV0DictionaryKey, ctx );
		CFDictionaryApplyFunction( (CFDictionaryRef) inObj, _WriteV0DictionaryValue, ctx );
	}
	else if( type == gCFArrayType )
	{
		count = CFArrayGetCount( (CFArrayRef) inObj );
		marker = (uint8_t)( kCFLBinaryPlistMarkerArray | ( ( count < 15 ) ? count : 0xF ) );
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		if( count >= 15 )
		{
			err = _WriteInteger( ctx, (uint64_t) count, false, false );
			require_noerr( err, exit );
		}
		CFArrayApplyFunction( (CFArrayRef) inObj, CFRangeMake( 0, count ), _WriteV0ArrayValue, ctx );
	}
	else if( type == gCFDateType )
	{
		marker = kCFLBinaryPlistMarkerDateFloat;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
		
		v.f64 = CFDateGetAbsoluteTime( (CFDateRef) inObj );
		v.u64 = hton64( v.u64 );
		err = _WriteBytes( ctx, &v.u64, 8 );
		require_noerr( err, exit );
	}
	else if( inObj == ( (CFTypeRef) kCFNull ) )
	{
		marker = kCFLBinaryPlistMarkerNull;
		err = _WriteBytes( ctx, &marker, 1 );
		require_noerr( err, exit );
	}
	else
	{
		dlogassert( "Unsupported object type: %u", type );
		err = kUnsupportedDataErr;
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteV0ArrayValue
//===========================================================================================================================

static void	_WriteV0ArrayValue( const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	uint32_t							refnum;
	uint8_t								buf[ 4 ];
	
	refnum = (uint32_t)(uintptr_t) CFDictionaryGetValue( ctx->uniqueDict, inValue );
	WriteBig32( buf, refnum );
	_WriteBytes( ctx, buf + ( 4 - ctx->objectRefSize ), ctx->objectRefSize );
}

//===========================================================================================================================
//	_WriteV0DictionaryKey
//===========================================================================================================================

static void	_WriteV0DictionaryKey( const void *inKey, const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	uint32_t							refnum;
	uint8_t								buf[ 4 ];
	
	(void) inValue;
	
	refnum = (uint32_t)(uintptr_t) CFDictionaryGetValue( ctx->uniqueDict, inKey );
	WriteBig32( buf, refnum );
	_WriteBytes( ctx, buf + ( 4 - ctx->objectRefSize ), ctx->objectRefSize );
}

//===========================================================================================================================
//	_WriteV0DictionaryValue
//===========================================================================================================================

static void	_WriteV0DictionaryValue( const void *inKey, const void *inValue, void *inContext )
{
	CFBinaryPlistContext * const		ctx = (CFBinaryPlistContext *) inContext;
	uint32_t							refnum;
	uint8_t								buf[ 4 ];
	
	(void) inKey;
	
	refnum = (uint32_t)(uintptr_t) CFDictionaryGetValue( ctx->uniqueDict, inValue );
	WriteBig32( buf, refnum );
	_WriteBytes( ctx, buf + ( 4 - ctx->objectRefSize ), ctx->objectRefSize );
}

//===========================================================================================================================
//	_WriteV0String
//===========================================================================================================================

static OSStatus	_WriteV0String( CFBinaryPlistContext *ctx, CFStringRef inStr )
{
	OSStatus			err;
	const uint8_t *		src;
	uint8_t *			utf8;
	uint8_t *			utf8Buf = NULL;
#if( ( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES ) || CFCOMPAT_HAS_UNICODE_SUPPORT )
	uint16_t *			utf16Buf = NULL;
#endif
	size_t				len, i, count;
	uint8_t				marker;
	CFIndex				nBytes;
	CFRange				range;
	
	src = (const uint8_t *) CFStringGetCStringPtr( inStr, kCFStringEncodingUTF8 );
	if( src )
	{
		len = strlen( (const char *) src );
	}
	else
	{
		range = CFRangeMake( 0, CFStringGetLength( inStr ) );
		nBytes = CFStringGetMaximumSizeForEncoding( range.length, kCFStringEncodingUTF8 );
		if( nBytes < (CFIndex) sizeof( ctx->tempBuf ) )
		{
			utf8 = ctx->tempBuf.u8;
		}
		else
		{
			utf8Buf = (uint8_t *) malloc( (size_t)( nBytes + 1 ) );
			require_action( utf8Buf, exit, err = kNoMemoryErr );
			utf8 = utf8Buf;
		}
		range.location = CFStringGetBytes( inStr, range, kCFStringEncodingUTF8, 0, false, utf8, nBytes, &nBytes );
		require_action( range.location == range.length, exit, err = kUnknownErr );
		utf8[ nBytes ] = '\0';
		src = utf8;
		len = (size_t) nBytes;
	}
	
	// Check if the string is only ASCII. If it's then we can write it out directly.
	
	for( i = 0; ( i < len ) && !( src[ i ] & 0x80 ); ++i ) {}
	if( i == len )
	{
		marker = kCFLBinaryPlistMarkerASCIIString;
		count  = len;
	}
	else if( ctx->flags & kCFBinaryPlistFlag_UTF8Strings )
	{
		marker = kCFLBinaryPlistMarkerUTF8String;
		count  = len;
	}
	else
	{
		#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
			uint16_t *		utf16;
			
			ForgetMem( &utf8Buf );
			range = CFRangeMake( 0, CFStringGetLength( inStr ) );
			nBytes = CFStringGetMaximumSizeForEncoding( range.length, kCFStringEncodingUTF16BE );
			if( nBytes <= (CFIndex) sizeof( ctx->tempBuf ) )
			{
				utf16 = ctx->tempBuf.u16;
			}
			else
			{
				utf16Buf = (uint16_t *) malloc( (size_t) nBytes );
				require_action( utf16Buf, exit, err = kNoMemoryErr );
				utf16 = utf16Buf;
			}
			range.location = CFStringGetBytes( inStr, range, kCFStringEncodingUTF16BE, 0, false, (uint8_t *) utf16, nBytes, &nBytes );
			require_action( range.location == range.length, exit, err = kUnknownErr );
			
			marker = kCFLBinaryPlistMarkerUnicodeString;
			src    = (const uint8_t *) utf16;
			len    = (size_t) nBytes;
			count  = len / 2;
		#elif( CFCOMPAT_HAS_UNICODE_SUPPORT )
			err = utf8_decodestr_copy( src, len, &utf16Buf, &len, 0, UTF_BIG_ENDIAN );
			require_noerr( err, exit );
			
			marker = kCFLBinaryPlistMarkerUnicodeString;
			src    = (const uint8_t *) utf16Buf;
			count  = len / 2;
		#else
			dlogassert( "UTF-16 required, but conversion code stripped out" );
			err = kUnsupportedDataErr;
			goto exit;
		#endif
	}
	
	marker |= ( ( count < 15 ) ? count : 0xF );
	err = _WriteBytes( ctx, &marker, 1 );
	require_noerr( err, exit );
	if( count >= 15 )
	{
		err = _WriteInteger( ctx, count, false, false );
		require_noerr( err, exit );
	}
	err = _WriteBytes( ctx, src, len );
	require_noerr( err, exit );
	
exit:
	FreeNullSafe( utf8Buf );
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	FreeNullSafe( utf16Buf );
#elif( CFCOMPAT_HAS_UNICODE_SUPPORT )
	if( utf16Buf ) utffree( utf16Buf );
#endif
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFBinaryPlistV0CreateWithData
//===========================================================================================================================

CFPropertyListRef	CFBinaryPlistV0CreateWithData( const void *inPtr, size_t inLen, OSStatus *outErr )
{
	const uint8_t *					src   = (const uint8_t *) inPtr;
	const uint8_t *					end   = src + inLen;
	CFPropertyListRef				plist = NULL;
	CFBinaryPlistContext			ctx;
	OSStatus						err;
	CFDictionaryValueCallBacks		dictCallbacks;
	CFBinaryPlistTrailer			trailer;
	uint64_t						offset;
	const uint8_t *					ptr;
	
	CFBinaryPlistContextInit( &ctx );
	
	// Sanity check the header/trailer up front to speed up batch checking of arbitrary files.
	
	require_action_quiet( src < end, exit, err = kSizeErr );
	require_action_quiet( inLen > ( 8 + sizeof( trailer ) ), exit, err = kSizeErr );
	require_action_quiet( memcmp( src, "bplist00", 8 ) == 0, exit, err = kFormatErr );
	memcpy( &trailer, end - sizeof( trailer ), sizeof( trailer ) );
	end -= sizeof( trailer );
	
	require_action_quiet( 
		( trailer.offsetIntSize == 1 ) || 
		( trailer.offsetIntSize == 2 ) ||
		( trailer.offsetIntSize == 4 ) ||
		( trailer.offsetIntSize == 8 ), exit, err = kMalformedErr );
	require_action_quiet( 
		( trailer.objectRefSize == 1 ) || 
		( trailer.objectRefSize == 2 ) ||
		( trailer.objectRefSize == 4 ) ||
		( trailer.objectRefSize == 8 ), exit, err = kMalformedErr );
	
	trailer.numObjects = ntoh64( trailer.numObjects );
	require_action_quiet( trailer.numObjects > 0, exit, err = kCountErr );
	
	trailer.topObject = ntoh64( trailer.topObject );
	require_action_quiet( trailer.topObject < trailer.numObjects, exit, err = kRangeErr );
	
	trailer.offsetTableOffset = ntoh64( trailer.offsetTableOffset );
	require_action_quiet( trailer.offsetTableOffset >= 9, exit, err = kMalformedErr );
	require_action_quiet( trailer.offsetTableOffset < ( inLen - sizeof( trailer ) ), exit, err = kMalformedErr );
	require_action_quiet( trailer.numObjects <= 
		( ( (size_t)( end - ( src + trailer.offsetTableOffset ) ) ) / trailer.offsetIntSize ), exit, err = kCountErr );
	
	offset = trailer.offsetTableOffset + ( trailer.topObject * trailer.offsetIntSize );
	require_action_quiet( offset < ( (uint64_t)( end - src ) ), exit, err = kRangeErr );
	ptr = src + offset;
	err = _ReadSizedInteger( &ptr, end, trailer.offsetIntSize, &offset, false );
	require_noerr_quiet( err, exit );
	
	// Read the root object (and any objects it contains).
	
	_GlobalEnsureInitialized();
	ctx.uniqueCount			= (CFIndex) trailer.numObjects;
	ctx.offsetIntSize		= trailer.offsetIntSize;
	ctx.objectRefSize		= trailer.objectRefSize;
	ctx.offsetTableOffset	= (size_t) trailer.offsetTableOffset;
	
	dictCallbacks = kCFTypeDictionaryValueCallBacks;
	dictCallbacks.equal = _ObjectsExactlyEqual;
	ctx.uniqueDict = CFDictionaryCreateMutable( NULL, 0, NULL, &dictCallbacks );
	require_action( ctx.uniqueDict, exit, err = kNoMemoryErr );
	
	plist = _ReadV0Object( &ctx, src, end, (size_t) offset, &err );
	require_noerr_quiet( err, exit );
	
exit:
	CFBinaryPlistContextFree( &ctx );
	if( outErr ) *outErr = err;
	return( plist );
}

//===========================================================================================================================
//	_ReadV0Object
//===========================================================================================================================

static CFTypeRef
	_ReadV0Object( 
		CFBinaryPlistContext *	ctx, 
		const uint8_t *			inSrc, 
		const uint8_t *			inEnd, 
		size_t					inOffset, 
		OSStatus *				outErr )
{
	CFTypeRef					obj = NULL;
	OSStatus					err;
	const uint8_t *				ptr;
	uint8_t						marker;
	uint64_t					count;
	uint32_t					u32;
	Value64						v64;
	uint128_compat				u128;
	size_t						offset;
	CFMutableArrayRef			array = NULL;
	CFMutableDictionaryRef		dict = NULL;
	CFTypeRef					key = NULL, value = NULL;
	const uint8_t *				keyPtr;
	const uint8_t *				valuePtr;
	
	require_action_quiet( inOffset < ( (size_t)( inEnd - inSrc ) ), exit, err = kRangeErr );
	
	obj = CFDictionaryGetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset );
	if( obj )
	{
		CFRetain( obj );
		err = kNoErr;
		goto exit;
	}
	
	ptr = inSrc + inOffset;
	marker = *ptr++;
	switch( marker & 0xF0 )
	{
		case 0:
			switch( marker )
			{
				case kCFLBinaryPlistMarkerNull:		obj = CFRetain( kCFNull );			break;
				case kCFLBinaryPlistMarkerFalse:	obj = CFRetain( kCFBooleanFalse );	break;
				case kCFLBinaryPlistMarkerTrue:		obj = CFRetain( kCFBooleanTrue );	break;
				default: err = kTypeErr; goto exit;
			}
			break;
		
		case kCFLBinaryPlistMarkerInt:
			u32 = 1 << ( marker & 0x0F );
			require_action_quiet( u32 <= 16, exit, err = kCountErr );
			require_action_quiet( u32 <= ( (size_t)( inEnd - ptr ) ), exit, err = kSizeErr );
			if( u32 > 8 )
			{
				for( u128.hi = 0; u32 > 8; --u32 ) u128.hi = ( u128.hi << 8 ) | *ptr++;
				for( u128.lo = 0; u32 > 0; --u32 ) u128.lo = ( u128.lo << 8 ) | *ptr++;
				obj = CFNumberCreate( NULL, kCFNumberSInt128Type_compat, &u128 );
			}
			else
			{
				for( v64.u64 = 0; u32 > 0; --u32 ) v64.u64 = ( v64.u64 << 8 ) | *ptr++;
				obj = CFNumberCreate( NULL, kCFNumberSInt64Type, &v64.u64 );
			}
			require_action( obj, exit, err = kNoMemoryErr );
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerReal:
			switch( marker & 0x0F )
			{
				case 2:
					require_action_quiet( ( inEnd - ptr ) >= 4, exit, err = kSizeErr );
					v64.u32[ 0 ] = ReadBig32( ptr );
					obj = CFNumberCreate( NULL, kCFNumberFloat32Type, &v64.f32 );
					require_action( obj, exit, err = kNoMemoryErr );
					break;
				
				case 3:
					require_action_quiet( ( inEnd - ptr ) >= 8, exit, err = kSizeErr );
					v64.u64 = ReadBig64( ptr );
					obj = CFNumberCreate( NULL, kCFNumberFloat64Type, &v64.f64 );
					require_action( obj, exit, err = kNoMemoryErr );
					break;
				
				default:
					err = kSizeErr;
					goto exit;
			}
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerDateBase:
			require_action_quiet( marker == kCFLBinaryPlistMarkerDateFloat, exit, err = kTypeErr );
			require_action_quiet( ( inEnd - ptr ) >= 8, exit, err = kSizeErr );
			v64.u64 = ReadBig64( ptr );
			obj = CFDateCreate( NULL, v64.f64 );
			require_action( obj, exit, err = kNoMemoryErr );
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerData:
			count = marker & 0x0F;
			if( count == 0xF )
			{
				err = _ReadInteger( &ptr, inEnd, &count, false, false );
				require_noerr_quiet( err, exit );
			}
			require_action_quiet( count <= ( (size_t)( inEnd - ptr ) ), exit, err = kSizeErr );
			
			obj = CFDataCreate( NULL, ptr, (CFIndex) count );
			require_action( obj, exit, err = kNoMemoryErr );
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerASCIIString:
		case kCFLBinaryPlistMarkerUTF8String:
			count = marker & 0x0F;
			if( count == 0xF )
			{
				err = _ReadInteger( &ptr, inEnd, &count, false, false );
				require_noerr_quiet( err, exit );
			}
			require_action_quiet( count <= ( (size_t)( inEnd - ptr ) ), exit, err = kSizeErr );
			
			obj = CFStringCreateWithBytes( NULL, ptr, (CFIndex) count, 
				( ( marker & 0xF0 ) == kCFLBinaryPlistMarkerASCIIString ) ? kCFStringEncodingASCII : kCFStringEncodingUTF8, 
				false );
			require_action( obj, exit, err = kNoMemoryErr );
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerUnicodeString:
			count = marker & 0x0F;
			if( count == 0xF )
			{
				err = _ReadInteger( &ptr, inEnd, &count, false, false );
				require_noerr_quiet( err, exit );
			}
			count *= 2;
			require_action_quiet( count <= ( (size_t)( inEnd - ptr ) ), exit, err = kSizeErr );
			
			obj = CFStringCreateWithBytes( NULL, ptr, (CFIndex) count, kCFStringEncodingUTF16BE, false );
			require_action( obj, exit, err = kNoMemoryErr );
			CFDictionarySetValue( ctx->uniqueDict, (const void *)(uintptr_t) inOffset, obj );
			break;
		
		case kCFLBinaryPlistMarkerArray:
			count = marker & 0x0F;
			if( count == 0xF )
			{
				err = _ReadInteger( &ptr, inEnd, &count, false, false );
				require_noerr_quiet( err, exit );
			}
			
			array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			require_action( array, exit, err = kNoMemoryErr );
			
			while( count-- > 0 )
			{
				err = _ReadRefOffset( ctx, inSrc, inEnd, &ptr, &offset );
				require_noerr_quiet( err, exit );
				value = _ReadV0Object( ctx, inSrc, inEnd, offset, &err );
				require_noerr_quiet( err, exit );
				
				CFArrayAppendValue( array, value );
				CFRelease( value );
				value = NULL;
			}
			obj = array;
			array = NULL;
			break;
		
		case kCFLBinaryPlistMarkerDictionary:
			count = marker & 0x0F;
			if( count == 0xF )
			{
				err = _ReadInteger( &ptr, inEnd, &count, false, false );
				require_noerr_quiet( err, exit );
			}
			require_action_quiet( count <= ( ( (size_t)( inEnd - ptr ) ) / ctx->objectRefSize ), exit, err = kCountErr );
			
			dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( dict, exit, err = kNoMemoryErr );
			
			keyPtr   = ptr;
			valuePtr = ptr + ( count * ctx->objectRefSize );
			while( count-- > 0 )
			{
				err = _ReadRefOffset( ctx, inSrc, inEnd, &keyPtr, &offset );
				require_noerr_quiet( err, exit );
				key = _ReadV0Object( ctx, inSrc, inEnd, offset, &err );
				require_noerr_quiet( err, exit );
				
				err = _ReadRefOffset( ctx, inSrc, inEnd, &valuePtr, &offset );
				require_noerr_quiet( err, exit );
				value = _ReadV0Object( ctx, inSrc, inEnd, offset, &err );
				require_noerr_quiet( err, exit );
				
				CFDictionarySetValue( dict, key, value );
				CFRelease( key );   key   = NULL;
				CFRelease( value ); value = NULL;
			}
			obj = dict;
			dict = NULL;
			break;
		
		default:
			err = kTypeErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( array );
	CFReleaseNullSafe( dict );
	CFReleaseNullSafe( key );
	CFReleaseNullSafe( value );
	if( outErr ) *outErr = err;
	return( obj );
}

//===========================================================================================================================
//	_ReadRefOffset
//===========================================================================================================================

static OSStatus
	_ReadRefOffset( 
		CFBinaryPlistContext *	ctx, 
		const uint8_t *			inSrc, 
		const uint8_t *			inEnd, 
		const uint8_t **		ioPtr, 
		size_t *				outOffset )
{
	const uint8_t *		ptr = *ioPtr;
	OSStatus			err;
	uint64_t			refnum;
	const uint8_t *		offsetPtr;
	uint64_t			offset;
	
	require_action_quiet( ctx->objectRefSize < ( (size_t)( inEnd - ptr ) ), exit, err = kUnderrunErr );
	switch( ctx->objectRefSize )
	{
		case 1: refnum = Read8( ptr ); break;
		case 2: refnum = ReadBig16( ptr ); break;
		case 4: refnum = ReadBig32( ptr ); break;
		case 8: refnum = ReadBig64( ptr ); break;
		default: err = kInternalErr; goto exit;
	}
	*ioPtr = ptr + ctx->objectRefSize;
	
	require_action_quiet( refnum < ( (uint64_t) ctx->uniqueCount ), exit, err = kRangeErr );
	offsetPtr = inSrc + ctx->offsetTableOffset + ( refnum * ctx->offsetIntSize );
	switch( ctx->offsetIntSize )
	{
		case 1: offset = Read8( offsetPtr ); break;
		case 2: offset = ReadBig16( offsetPtr ); break;
		case 4: offset = ReadBig32( offsetPtr ); break;
		case 8: offset = ReadBig64( offsetPtr ); break;
		default: err = kInternalErr; goto exit;
	}
	require_action_quiet( offset <= SIZE_MAX, exit, err = kSizeErr );
	*outOffset = (size_t) offset;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Common ==
#endif

//===========================================================================================================================
//	_ReadInteger
//===========================================================================================================================

static OSStatus
	_ReadInteger( 
		const uint8_t **	ioPtr, 
		const uint8_t *		inEnd, 
		uint64_t *			outValue, 
		Boolean				inAllowSmall, 
		Boolean				inLittleEndian )
{
	OSStatus			err;
	const uint8_t *		ptr;
	uint8_t				marker;
	
	ptr = *ioPtr;
	require_action_quiet( ptr < inEnd, exit, err = kUnderrunErr );
	marker = *ptr++;
	if( inAllowSmall && ( ( marker & 0xF0 ) == kCFLBinaryPlistMarkerSmallInteger ) )
	{
		// Small integers store the value directly in the low 4 bits of the marker.
		
		*outValue = marker & 0x0F;
	}
	else if( ( marker & 0xF0 ) == kCFLBinaryPlistMarkerInt )
	{
		// Read the integer as 2^nnnn bytes where bits nnnn come from the lower 4 bits of the marker.
		
		err = _ReadSizedInteger( &ptr, inEnd, 1 << ( marker & 0x0F ), outValue, inLittleEndian );
		require_noerr_quiet( err, exit );
	}
	else
	{
		err = kTypeErr;
		goto exit;
	}
	*ioPtr = ptr;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ReadSizedInteger
//===========================================================================================================================

static OSStatus
	_ReadSizedInteger( 
		const uint8_t **	ioPtr, 
		const uint8_t *		inEnd, 
		size_t				inLen, 
		uint64_t *			outValue, 
		Boolean				inLittleEndian )
{
	const uint8_t *		ptr = *ioPtr;
	OSStatus			err;
	
	require_action_quiet( ( (size_t)( inEnd - ptr ) ) >= inLen, exit, err = kSizeErr );
	switch( inLen )
	{
		case 1:
			*outValue = Read8( ptr );
			break;
		
		case 2:
			*outValue = inLittleEndian ? ReadLittle16( ptr ) : ReadBig16( ptr );
			break;
		
		case 4:
			*outValue = inLittleEndian ? ReadLittle32( ptr ) : ReadBig32( ptr );
			break;
		
		case 8:
			*outValue = inLittleEndian ? ReadLittle64( ptr ) : ReadBig64( ptr );
			break;
		
		default:
			err = kCountErr;
			goto exit;
	}
	*ioPtr = ptr + inLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteBytes
//===========================================================================================================================

static OSStatus	_WriteBytes( CFBinaryPlistContext *ctx, const void *inData, size_t inSize )
{
	OSStatus		err;
	
	if( ctx->data )
	{
		CFDataAppendBytes( ctx->data, (const UInt8 *) inData, (CFIndex) inSize );
	}
	else if( ctx->writer_f )
	{
		err = ctx->writer_f( inData, inSize, ctx->writer_ctx );
		require_noerr_quiet( err, exit );
	}
	else
	{
		err = kNotPreparedErr;
		goto exit;
	}
	ctx->bytesWritten += inSize;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteInteger
//===========================================================================================================================

static OSStatus	_WriteInteger( CFBinaryPlistContext *ctx, uint64_t inValue, Boolean inAllowSmall, Boolean inLittleEndian )
{
	OSStatus		err;
	uint8_t			buf[ 9 ];
	size_t			len;
	
	if( inAllowSmall && ( inValue <= 15 ) )
	{
		buf[ 0 ] = (uint8_t)( kCFLBinaryPlistMarkerSmallInteger | inValue );
		len = 1;
	}	
	else if( inValue <= UINT64_C( 0xFF ) )
	{
		buf[ 0 ] = kCFLBinaryPlistMarkerInt | 0;
		buf[ 1 ] = (uint8_t) inValue;
		len = 2;
	}
	else if( inValue <= UINT64_C( 0xFFFF ) )
	{
		buf[ 0 ] = kCFLBinaryPlistMarkerInt | 1;
		if( inLittleEndian )	WriteLittle16( &buf[ 1 ], inValue );
		else					WriteBig16( &buf[ 1 ], inValue );
		len = 3;
	}
	else if( inValue <= UINT64_C( 0xFFFFFFFF ) )
	{
		buf[ 0 ] = kCFLBinaryPlistMarkerInt | 2;
		if( inLittleEndian )	WriteLittle32( &buf[ 1 ], inValue );
		else					WriteBig32( &buf[ 1 ], inValue );
		len = 5;
	}
	else
	{
		buf[ 0 ] = kCFLBinaryPlistMarkerInt | 3;
		if( inLittleEndian )	WriteLittle64( &buf[ 1 ], inValue );
		else					WriteBig64( &buf[ 1 ], inValue );
		len = 9;
	}
	
	err = _WriteBytes( ctx, buf, len );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_WriteNumber
//===========================================================================================================================

static OSStatus	_WriteNumber( CFBinaryPlistContext *ctx, CFNumberRef inNum, Boolean inAllowSmall, Boolean inLittleEndian )
{
	OSStatus		err;
	Value64			v;
	uint8_t			buf[ 17 ];
	size_t			len;
	Boolean			skip;
	
	if( CFNumberIsFloatType( inNum ) )
	{
		if( ctx->flags & kCFBinaryPlistStreamedFlag_Unique )
		{
			err = _WriteStreamedUpdateUID( ctx, inNum, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		if( CFNumberGetByteSize( inNum ) <= ( (CFIndex) sizeof( Float32 ) ) )
		{
			CFNumberGetValue( inNum, kCFNumberFloat32Type, &v.f32[ 0 ] );
			
			buf[ 0 ] = kCFLBinaryPlistMarkerReal | 2; // 2 for 2^2 = 4 byte Float32.
			if( inLittleEndian )	WriteLittleFloat32( &buf[ 1 ], v.f32[ 0 ] );
			else					WriteBigFloat32( &buf[ 1 ], v.f32[ 0 ] );
			len = 5;
		}
		else
		{
			CFNumberGetValue( inNum, kCFNumberFloat64Type, &v.f64 );
			
			buf[ 0 ] = kCFLBinaryPlistMarkerReal | 3; // 2 for 2^3 = 8 byte Float64.
			if( inLittleEndian )	WriteLittleFloat64( &buf[ 1 ], v.f64 );
			else					WriteBigFloat64( &buf[ 1 ], v.f64 );
			len = 9;
		}
		err = _WriteBytes( ctx, buf, len );
		require_noerr( err, exit );
	}
	else if( CFNumberGetType( inNum ) == kCFNumberSInt128Type_compat )
	{
		int128_compat		u128;
		
		if( ctx->flags & kCFBinaryPlistStreamedFlag_Unique )
		{
			err = _WriteStreamedUpdateUID( ctx, inNum, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		CFNumberGetValue( inNum, kCFNumberSInt128Type_compat, &u128 );
		
		buf[ 0 ] = kCFLBinaryPlistMarkerInt | 4;
		if( inLittleEndian )
		{
			WriteLittle64( &buf[ 1 ], u128.hi );
			WriteLittle64( &buf[ 9 ], u128.lo );
		}
		else
		{
			WriteBig64( &buf[ 1 ], u128.hi );
			WriteBig64( &buf[ 9 ], u128.lo );
		}
		
		err = _WriteBytes( ctx, buf, 17 );
		require_noerr( err, exit );
	}
	else
	{
		CFNumberGetValue( inNum, kCFNumberSInt64Type, &v.u64 );
		
		if( ( ctx->flags & kCFBinaryPlistStreamedFlag_Unique ) && ( v.u64 > 0xFF ) )
		{
			err = _WriteStreamedUpdateUID( ctx, inNum, &skip );
			require_noerr( err, exit );
			require_quiet( !skip, exit );
		}
		
		err = _WriteInteger( ctx, v.u64, inAllowSmall, inLittleEndian );
		require_noerr( err, exit );
	}
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	CFLiteBinaryPlistTest
//===========================================================================================================================

static const uint8_t		kV0Test1[] = // TopObject not at offset 8 (bybiplist1.0 after signature).
{
	0x62, 0x70, 0x6C, 0x69, 0x73, 0x74, 0x30, 0x30, 0x62, 0x79, 0x62, 0x69, 0x70, 0x6C, 0x69, 0x73, 
	0x74, 0x31, 0x2E, 0x30, 0x00, 0xD2, 0x01, 0x02, 0x03, 0x04, 0x56, 0x54, 0x6F, 0x70, 0x69, 0x63, 
	0x73, 0x57, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0xA3, 0x05, 0x06, 0x07, 0xD7, 0x08, 0x09, 
	0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x0F, 0x0F, 0x10, 0x11, 0x04, 0x12, 0x5B, 0x54, 0x6F, 0x70, 
	0x69, 0x63, 0x50, 0x6F, 0x6C, 0x69, 0x63, 0x79, 0x57, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x49, 0x44, 
	0x5A, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x50, 0x68, 0x61, 0x73, 0x65, 0x5F, 0x10, 0x10, 0x54, 0x6F, 
	0x70, 0x69, 0x63, 0x44, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x5E, 0x54, 
	0x6F, 0x70, 0x69, 0x63, 0x4D, 0x61, 0x78, 0x4C, 0x65, 0x6E, 0x67, 0x74, 0x68, 0x5B, 0x54, 0x6F, 
	0x70, 0x69, 0x63, 0x53, 0x6F, 0x75, 0x72, 0x63, 0x65, 0x5D, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x49, 
	0x6E, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6C, 0x10, 0x01, 0x5F, 0x10, 0x11, 0x66, 0x31, 0x3A, 0x30, 
	0x31, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x31, 0x11, 0x04, 0x20, 
	0x10, 0x64, 0xD7, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x13, 0x13, 0x14, 0x10, 0x15, 0x16, 
	0x17, 0x10, 0x02, 0x10, 0x03, 0x11, 0x01, 0x28, 0x5F, 0x10, 0x11, 0x30, 0x30, 0x3A, 0x30, 0x30, 
	0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x32, 0x10, 0x96, 0xD7, 0x08, 
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x13, 0x14, 0x18, 0x10, 0x19, 0x1A, 0x1B, 0x10, 0x09, 0x11, 
	0x08, 0x40, 0x5F, 0x10, 0x11, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 
	0x3A, 0x30, 0x30, 0x3A, 0x30, 0x33, 0x10, 0xD2, 0x5F, 0x10, 0x11, 0x30, 0x30, 0x3A, 0x30, 0x30, 
	0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x31, 0x00, 0x15, 0x00, 0x1A, 
	0x00, 0x21, 0x00, 0x29, 0x01, 0x08, 0x00, 0x2D, 0x00, 0xB2, 0x00, 0xDE, 0x00, 0x3C, 0x00, 0x48, 
	0x00, 0x50, 0x00, 0x5B, 0x00, 0x6E, 0x00, 0x7D, 0x00, 0x89, 0x00, 0x97, 0x00, 0x99, 0x00, 0xAD, 
	0x00, 0xB0, 0x00, 0xC1, 0x00, 0xC3, 0x00, 0xC5, 0x00, 0xC8, 0x00, 0xDC, 0x00, 0xED, 0x00, 0xEF, 
	0x00, 0xF2, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x1C
};

static const uint8_t		kV0Test2[] = // TopObject not at offset 8 (NUL byte after signature).
{
	0x62, 0x70, 0x6C, 0x69, 0x73, 0x74, 0x30, 0x30, 0x00, 0xD2, 0x01, 0x02, 0x03, 0x04, 0x56, 0x54, 
	0x6F, 0x70, 0x69, 0x63, 0x73, 0x57, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0xA3, 0x05, 0x06, 
	0x07, 0xD7, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x0F, 0x0F, 0x10, 0x11, 0x04, 0x12, 
	0x5B, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x50, 0x6F, 0x6C, 0x69, 0x63, 0x79, 0x57, 0x54, 0x6F, 0x70, 
	0x69, 0x63, 0x49, 0x44, 0x5A, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x50, 0x68, 0x61, 0x73, 0x65, 0x5F, 
	0x10, 0x10, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x44, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x61, 0x74, 0x69, 
	0x6F, 0x6E, 0x5E, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x4D, 0x61, 0x78, 0x4C, 0x65, 0x6E, 0x67, 0x74, 
	0x68, 0x5B, 0x54, 0x6F, 0x70, 0x69, 0x63, 0x53, 0x6F, 0x75, 0x72, 0x63, 0x65, 0x5D, 0x54, 0x6F, 
	0x70, 0x69, 0x63, 0x49, 0x6E, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6C, 0x10, 0x01, 0x5F, 0x10, 0x11, 
	0x66, 0x31, 0x3A, 0x30, 0x31, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 
	0x31, 0x11, 0x04, 0x20, 0x10, 0x64, 0xD7, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x13, 0x13, 
	0x14, 0x10, 0x15, 0x16, 0x17, 0x10, 0x02, 0x10, 0x03, 0x11, 0x01, 0x28, 0x5F, 0x10, 0x11, 0x30, 
	0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x32, 
	0x10, 0x96, 0xD7, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x13, 0x14, 0x18, 0x10, 0x19, 0x1A, 
	0x1B, 0x10, 0x09, 0x11, 0x08, 0x40, 0x5F, 0x10, 0x11, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 
	0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x33, 0x10, 0xD2, 0x5F, 0x10, 0x11, 0x30, 
	0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x3A, 0x30, 0x31, 
	0x00, 0x09, 0x00, 0x0E, 0x00, 0x15, 0x00, 0x1D, 0x00, 0xFC, 0x00, 0x21, 0x00, 0xA6, 0x00, 0xD2, 
	0x00, 0x30, 0x00, 0x3C, 0x00, 0x44, 0x00, 0x4F, 0x00, 0x62, 0x00, 0x71, 0x00, 0x7D, 0x00, 0x8B, 
	0x00, 0x8D, 0x00, 0xA1, 0x00, 0xA4, 0x00, 0xB5, 0x00, 0xB7, 0x00, 0xB9, 0x00, 0xBC, 0x00, 0xD0, 
	0x00, 0xE1, 0x00, 0xE3, 0x00, 0xE6, 0x00, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10
};

static const uint8_t		kV0Test3[] = // Simple examples of most types.
{
	0x62, 0x70, 0x6C, 0x69, 0x73, 0x74, 0x30, 0x30, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
	0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x58, 0x64, 0x61, 0x74, 0x65, 0x49, 0x74, 
	0x65, 0x6D, 0x58, 0x72, 0x65, 0x61, 0x6C, 0x49, 0x74, 0x65, 0x6D, 0x59, 0x61, 0x72, 0x72, 0x61, 
	0x79, 0x49, 0x74, 0x65, 0x6D, 0x5B, 0x75, 0x6E, 0x69, 0x63, 0x6F, 0x64, 0x65, 0x49, 0x74, 0x65, 
	0x6D, 0x58, 0x64, 0x61, 0x74, 0x61, 0x49, 0x74, 0x65, 0x6D, 0x5A, 0x6E, 0x75, 0x6D, 0x62, 0x65, 
	0x72, 0x49, 0x74, 0x65, 0x6D, 0x58, 0x62, 0x6F, 0x6F, 0x6C, 0x49, 0x74, 0x65, 0x6D, 0x5A, 0x73, 
	0x74, 0x72, 0x69, 0x6E, 0x67, 0x49, 0x74, 0x65, 0x6D, 0x33, 0x41, 0xB2, 0x1D, 0xE7, 0x52, 0x62, 
	0xAC, 0xC9, 0x23, 0x3F, 0xDE, 0x14, 0x7A, 0xE1, 0x47, 0xAE, 0x14, 0xA1, 0x0C, 0x55, 0x69, 0x74, 
	0x65, 0x6D, 0x30, 0x68, 0x00, 0x61, 0x00, 0x62, 0x00, 0x63, 0x21, 0x2C, 0x00, 0x64, 0x00, 0x65, 
	0x00, 0x66, 0x21, 0x33, 0x40, 0x13, 0xFF, 0xDC, 0x79, 0x0D, 0x90, 0x3F, 0x00, 0x00, 0x09, 0x58, 
	0x48, 0x69, 0x20, 0x74, 0x68, 0x65, 0x72, 0x65, 0x08, 0x19, 0x22, 0x2B, 0x35, 0x41, 0x4A, 0x55, 
	0x5E, 0x69, 0x72, 0x7B, 0x7D, 0x83, 0x94, 0x95, 0x9E, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8
};

static OSStatus	TestCFLiteBinaryPlistStreamedWrite( void );
static OSStatus	TestCFLiteBinaryPlistStreamedUniquing( void );

OSStatus	CFLiteBinaryPlistTest( void )
{
	OSStatus						err;
	CFBinaryPlistStreamedFlags		flags;
	CFMutableDictionaryRef			plist;
	CFMutableDictionaryRef			plist2	= NULL;
	CFNumberRef						num		= NULL;
	int								x, x2;
	double							d;
	CFDataRef						data	= NULL;
	CFDataRef						data2	= NULL;
	CFStringRef						str;
	CFMutableArrayRef				array	= NULL;
	CFMutableDictionaryRef			dict	= NULL;
	CFDateRef						date;
	char							cstr[ 260 ];
	size_t							i;
	uint8_t							buf[ 32 ];
	CFTypeRef						obj;
	
	// Empty Test
	
	plist = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( plist, exit, err = kNoMemoryErr );
	
	data = CFBinaryPlistStreamedCreateData( plist, &err );
	require_noerr( err, exit );
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithBytes( CFDataGetBytePtr( data ), 
		(size_t) CFDataGetLength( data ), &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
	flags = kCFBinaryPlistStreamedFlags_ReadDefault | kCFBinaryPlistStreamedFlag_NoCopy;
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithData( data, flags, &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &data );
	ForgetCF( &plist2 );
	
	data = CFBinaryPlistV0CreateData( plist, &err );
	require_noerr( err, exit );
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( CFDataGetBytePtr( data ), 
		(size_t) CFDataGetLength( data ), &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	plist2 = (CFMutableDictionaryRef) CFPropertyListCreateWithData( NULL, data, 0, NULL, NULL );
	require_action( plist2, exit, err = kResponseErr );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
#endif
	ForgetCF( &data );
	
	// Normal Test
	
	CFDictionarySetValue( plist, CFSTR( "false" ), kCFBooleanFalse );
	CFDictionarySetValue( plist, CFSTR( "true" ), kCFBooleanTrue );
	
	x = 1234567;
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &x );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number" ), num );
	ForgetCF( &num );
	
	x = -123;
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &x );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number2" ), num );
	ForgetCF( &num );
	
#if( CFLITE_ENABLED )
{
	int128_compat		s128;
	
	s128.hi = (int64_t) UINT64_C( 0xFFFFFFFFFFFFFFFF );
	s128.lo = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt128Type_compat, &s128 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number3" ), num );
	ForgetCF( &num );
	
	s128.hi = -1;
	s128.lo = (uint64_t) INT64_C( -123 );
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt128Type_compat, &s128 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number4" ), num );
	ForgetCF( &num );
}
#endif
	
	x = 0;
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &x );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number5" ), num );
	ForgetCF( &num );
	
	x = 15;
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &x );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "number6" ), num );
	ForgetCF( &num );
	
	d = 123.456;
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberDoubleType, &d );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "real" ), num );
	ForgetCF( &num );
	
	data2 = CFDataCreate( kCFAllocatorDefault, (const uint8_t *) "\x01\x02\x03\x04\x05", 5 );
	require_action( data2, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "data" ), data2 );
	ForgetCF( &data2 );
	
	memset( buf, '2', 14 );
	data2 = CFDataCreate( kCFAllocatorDefault, buf, 14 );
	require_action( data2, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "data2" ), data2 );
	ForgetCF( &data2 );
	
	memset( buf, '3', 15 );
	data2 = CFDataCreate( kCFAllocatorDefault, buf, 15 );
	require_action( data2, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "data3" ), data2 );
	ForgetCF( &data2 );
	
	memset( buf, '4', 16 );
	data2 = CFDataCreate( kCFAllocatorDefault, buf, 16 );
	require_action( data2, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "data4" ), data2 );
	ForgetCF( &data2 );
	
	str = CFStringCreateWithCString( kCFAllocatorDefault, "test string", kCFStringEncodingUTF8 );
	require_action( str, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "string" ), str );
	CFRelease( str );
	
	str = CFStringCreateWithCString( kCFAllocatorDefault, "test string (こんにちは)", kCFStringEncodingUTF8 );
	require_action( str, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "string2" ), str );
	CFRelease( str );
	
	for( i = 0; i < ( sizeof( cstr ) - 1 ); ++i )
	{
		cstr[ i ] = kAlphaNumericCharSet[ i % sizeof_string( kAlphaNumericCharSet ) ];
	}
	cstr[ i ] = '\0';
	str = CFStringCreateWithCString( kCFAllocatorDefault, cstr, kCFStringEncodingUTF8 );
	require_action( str, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "string3" ), str );
	CFRelease( str );
	
	for( i = 0; i < ( sizeof( cstr ) - 4 ); ++i )
	{
		cstr[ i ] = kAlphaNumericCharSet[ i % sizeof_string( kAlphaNumericCharSet ) ];
	}
	memcpy( &cstr[ i ], "\xe3\x81\xa1", 3 ); i += 3;
	cstr[ i ] = '\0';
	str = CFStringCreateWithCString( kCFAllocatorDefault, cstr, kCFStringEncodingUTF8 );
	require_action( str, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "string4" ), str );
	CFRelease( str );
	
	array = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	CFArrayAppendValue( array, kCFBooleanFalse );
	CFArrayAppendValue( array, CFSTR( "test string" ) );
	CFDictionarySetValue( plist, CFSTR( "array" ), array );
	ForgetCF( &array );
	
	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "false" ), kCFBooleanFalse );
	CFDictionarySetValue( dict, CFSTR( "string" ), CFSTR( "test string" ) );
	CFDictionarySetValue( plist, CFSTR( "dictionary" ), dict );
	ForgetCF( &dict );
	
	date = CFDateCreateWithComponents( kCFAllocatorDefault, 2006, 3, 9, 23, 56, 35 );
	require_action( date, exit, err = kNoMemoryErr );
	CFDictionarySetValue( plist, CFSTR( "date" ), date );
	CFRelease( date );
	
	// V0 Basic
	
	data = CFBinaryPlistV0CreateData( plist, &err );
	require_noerr( err, exit );
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( CFDataGetBytePtr( data ), 
		(size_t) CFDataGetLength( data ), &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	plist2 = (CFMutableDictionaryRef) CFPropertyListCreateWithData( NULL, data, 0, NULL, NULL );
	require_action( plist2, exit, err = kResponseErr );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
#endif
	ForgetCF( &data );
	
	data = CFBinaryPlistV0CreateDataEx( plist, kCFBinaryPlistFlag_UTF8Strings, &err );
	require_noerr( err, exit );
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( CFDataGetBytePtr( data ), 
		(size_t) CFDataGetLength( data ), &err );
	ForgetCF( &data );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
	// V0 Test 1
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( kV0Test1, sizeof( kV0Test1 ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( plist2, CFDictionary ), exit, err = kTypeErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist2, CFSTR( "Address" ) );
	require_action( str && CFIsType( str, CFString ), exit, err = kTypeErr );
	require_action( CFEqual( str, CFSTR( "00:00:00:00:00:01" ) ), exit, err = kMismatchErr );
	
	obj = CFDictionaryGetValue( plist2, CFSTR( "Topics" ) );
	require_action( obj && CFIsType( obj, CFArray ), exit, err = kTypeErr );
	require_action( CFArrayGetCount( (CFArrayRef) obj ) == 3, exit, err = kCountErr );
	
	ForgetCF( &plist2 );
	
	// V0 Test 2
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( kV0Test2, sizeof( kV0Test2 ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( plist2, CFDictionary ), exit, err = kTypeErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist2, CFSTR( "Address" ) );
	require_action( str && CFIsType( str, CFString ), exit, err = kTypeErr );
	require_action( CFEqual( str, CFSTR( "00:00:00:00:00:01" ) ), exit, err = kMismatchErr );
	
	obj = CFDictionaryGetValue( plist2, CFSTR( "Topics" ) );
	require_action( obj && CFIsType( obj, CFArray ), exit, err = kTypeErr );
	require_action( CFArrayGetCount( (CFArrayRef) obj ) == 3, exit, err = kCountErr );
	
	ForgetCF( &plist2 );
	
	// V0 Test 3
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistV0CreateWithData( kV0Test3, sizeof( kV0Test3 ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( plist2, CFDictionary ), exit, err = kTypeErr );
	
	obj = CFDictionaryGetValue( plist2, CFSTR( "boolItem" ) );
	require_action( obj && CFIsType( obj, CFBoolean ), exit, err = kTypeErr );
	require_action( CFEqual( obj, kCFBooleanTrue ), exit, err = kMismatchErr );
	
	obj = CFDictionaryGetValue( plist2, CFSTR( "dataItem" ) );
	require_action( obj && CFIsType( obj, CFData ), exit, err = kTypeErr );
	require_action( CFDataGetLength( (CFDataRef) obj ) == 0, exit, err = kMismatchErr );
	
	obj = CFDictionaryGetValue( plist2, CFSTR( "stringItem" ) );
	require_action( obj && CFIsType( obj, CFString ), exit, err = kTypeErr );
	require_action( CFEqual( obj, CFSTR( "Hi there" ) ), exit, err = kMismatchErr );
	
	ForgetCF( &plist2 );
	
	// Streamed 1
	
	data = CFBinaryPlistStreamedCreateData( plist, &err );
	require_noerr( err, exit );
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithBytes( CFDataGetBytePtr( data ), 
		(size_t) CFDataGetLength( data ), &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
	flags = kCFBinaryPlistStreamedFlags_ReadDefault | kCFBinaryPlistStreamedFlag_NoCopy;
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithData( data, flags, &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, plist2 ), exit, err = kResponseErr );
	ForgetCF( &data );
	ForgetCF( &plist2 );
	
	// Streamed 2
	
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithBytesEx( "\x70" "test", 6, 
		kCFBinaryPlistStreamedFlag_Body, &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist2, CFSTR( "test" ) ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x70" "test", 6 );
	require_action( data, exit, err = kNoMemoryErr );
	flags = kCFBinaryPlistStreamedFlag_Body | kCFBinaryPlistStreamedFlag_NoCopy;
	plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithData( data, flags, &err );
	ForgetCF( &data );
	require_noerr( err, exit );
	require_action( CFEqual( plist2, CFSTR( "test" ) ), exit, err = kResponseErr );
	ForgetCF( &plist2 );
	
	// Small integer test.
	
	memcpy( buf, "CFB1" "\x90" "END!", 9 );
	for( x = 0; x <= 30; ++x )
	{
		num = CFNumberCreate( NULL, kCFNumberIntType, &x );
		require_action( num, exit, err = kResponseErr );
		data = CFBinaryPlistStreamedCreateData( num, &err );
		require_noerr( err, exit );
		
		if( ( x >= 0 ) && ( x <= 15 ) )
		{
			buf[ 4 ] = (uint8_t )( 0x90 | x );
			require_action( MemEqual( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), buf, 9 ), exit, err = kResponseErr );
		}
		
		plist2 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithBytes( CFDataGetBytePtr( data ), 
			(size_t) CFDataGetLength( data ), &err );
		require_noerr( err, exit );
		require_action( CFIsType( plist2, CFNumber ), exit, err = kResponseErr );
		require_action( CFEqual( plist2, num ), exit, err = kResponseErr );
		
		x2 = -1;
		CFNumberGetValue( (CFNumberRef) plist2, kCFNumberIntType, &x2 );
		require_action( x == x2, exit, err = kResponseErr );
		
		ForgetCF( &num );
		ForgetCF( &plist2 );
		ForgetCF( &data );
	}
	
	err = TestCFLiteBinaryPlistStreamedWrite();
	require_noerr( err, exit );
	
	err = TestCFLiteBinaryPlistStreamedUniquing();
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( array );
	CFReleaseNullSafe( data );
	CFReleaseNullSafe( data2 );
	CFReleaseNullSafe( dict );
	CFReleaseNullSafe( num );
	CFReleaseNullSafe( plist );
	CFReleaseNullSafe( plist2 );
	printf( "CFLiteBinaryPlistTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	TestCFLiteBinaryPlistStreamedWrite
//===========================================================================================================================

static const uint8_t		kTestStreamedPlist1[] =
{
	0x43, 0x46, 0x42, 0x31, 0xD0, 0x90, 0x70, 0x6E, 0x75, 0x6D, 0x2D, 0x30, 0x2D, 0x6B, 0x65, 0x79, 
	0x00, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x6E, 0x65, 0x67, 0x2D, 0x31, 0x32, 0x33, 
	0x00, 0x13, 0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 
	0x72, 0x2D, 0x62, 0x69, 0x67, 0x33, 0x32, 0x00, 0x12, 0xFF, 0xFF, 0xFF, 0x7F, 0x70, 0x6E, 0x75, 
	0x6D, 0x62, 0x65, 0x72, 0x2D, 0x6D, 0x69, 0x6E, 0x36, 0x34, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x80, 0x70, 0x64, 0x61, 0x74, 0x61, 0x2D, 0x31, 0x34, 0x00, 0x4E, 0x00, 0x11, 
	0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0x70, 0x73, 0x74, 0x72, 
	0x69, 0x6E, 0x67, 0x2D, 0x65, 0x6D, 0x70, 0x74, 0x79, 0x00, 0x70, 0x00, 0x70, 0x64, 0x61, 0x74, 
	0x61, 0x2D, 0x31, 0x35, 0x00, 0x4F, 0x9F, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 
	0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x31, 0x35, 
	0x00, 0x9F, 0x9A, 0x70, 0x6E, 0x75, 0x6D, 0x2D, 0x31, 0x30, 0x2D, 0x6B, 0x65, 0x79, 0x00, 0x70, 
	0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x30, 0x00, 0x90, 0x70, 0x73, 0x74, 0x72, 0x69, 0x6E, 
	0x67, 0x2D, 0x61, 0x62, 0x63, 0x00, 0x70, 0x61, 0x62, 0x63, 0x00, 0x70, 0x64, 0x61, 0x74, 0x61, 
	0x2D, 0x31, 0x36, 0x00, 0x4F, 0x10, 0x10, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 
	0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x70, 0x62, 0x6F, 0x6F, 0x6C, 0x65, 0x61, 0x6E, 0x2D, 
	0x74, 0x72, 0x75, 0x65, 0x00, 0x09, 0x10, 0x64, 0x70, 0x6E, 0x75, 0x6D, 0x2D, 0x31, 0x30, 0x30, 
	0x2D, 0x6B, 0x65, 0x79, 0x00, 0x9F, 0x70, 0x6E, 0x75, 0x6D, 0x2D, 0x31, 0x35, 0x2D, 0x6B, 0x65, 
	0x79, 0x00, 0x70, 0x64, 0x61, 0x74, 0x65, 0x00, 0x33, 0x00, 0x00, 0x00, 0xC3, 0xF2, 0xE8, 0xBA, 
	0x41, 0x70, 0x64, 0x61, 0x74, 0x61, 0x2D, 0x38, 0x00, 0x48, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 
	0x66, 0x77, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x6D, 0x61, 0x78, 0x36, 0x34, 0x00, 
	0x13, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 
	0x2D, 0x35, 0x00, 0x95, 0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x31, 0x34, 0x00, 0x9E, 
	0x70, 0x6E, 0x75, 0x6D, 0x62, 0x65, 0x72, 0x2D, 0x31, 0x36, 0x00, 0x10, 0x10, 0x70, 0x6E, 0x65, 
	0x73, 0x74, 0x65, 0x64, 0x2D, 0x61, 0x72, 0x72, 0x61, 0x79, 0x00, 0xA0, 0x70, 0x61, 0x72, 0x72, 
	0x61, 0x79, 0x2D, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x00, 0x12, 0x40, 0x42, 0x0F, 0x00, 0x09, 
	0xD0, 0x70, 0x61, 0x72, 0x72, 0x61, 0x79, 0x2D, 0x64, 0x69, 0x63, 0x74, 0x2D, 0x6B, 0x65, 0x79, 
	0x31, 0x00, 0x70, 0x61, 0x72, 0x72, 0x61, 0x79, 0x2D, 0x64, 0x69, 0x63, 0x74, 0x2D, 0x76, 0x61, 
	0x6C, 0x75, 0x65, 0x31, 0x00, 0x70, 0x61, 0x72, 0x72, 0x61, 0x79, 0x2D, 0x64, 0x69, 0x63, 0x74, 
	0x2D, 0x6B, 0x65, 0x79, 0x32, 0x00, 0x70, 0x61, 0x72, 0x72, 0x61, 0x79, 0x2D, 0x64, 0x69, 0x63, 
	0x74, 0x2D, 0x76, 0x61, 0x6C, 0x75, 0x65, 0x32, 0x00, 0x01, 0x01, 0x70, 0x62, 0x6F, 0x6F, 0x6C, 
	0x65, 0x61, 0x6E, 0x2D, 0x66, 0x61, 0x6C, 0x73, 0x65, 0x00, 0x08, 0x70, 0x64, 0x61, 0x74, 0x61, 
	0x2D, 0x30, 0x00, 0x40, 0x01, 0x45, 0x4E, 0x44, 0x21
};

typedef struct
{
	uint8_t		buf[ 512 ];
	size_t		len;
	
}	TestCFLiteBinaryPlistStreamedWriteContext;

static OSStatus	_StreamedCallback( const void *inData, size_t inLen, void *inContext );

static OSStatus	TestCFLiteBinaryPlistStreamedWrite( void )
{
	TestCFLiteBinaryPlistStreamedWriteContext		ctx;
	OSStatus										err;
	CFMutableDictionaryRef							dict;
	CFTypeRef										plist	= NULL;
	CFTypeRef										plist2	= NULL;
	CFArrayRef										array	= NULL;
	CFNumberRef										num;
	CFDateRef										date;
	
	// Full plist test.
	
	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "boolean-true" ), kCFBooleanTrue );
	CFDictionarySetValue( dict, CFSTR( "boolean-false" ), kCFBooleanFalse );
	CFDictionarySetData( dict, CFSTR( "data-0" ), "", 0 );
	CFDictionarySetData( dict, CFSTR( "data-8" ), "\x00\x11\x22\x33\x44\x55\x66\x77", 8 );
	CFDictionarySetData( dict, CFSTR( "data-14" ), "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD", 14 );
	CFDictionarySetData( dict, CFSTR( "data-15" ), "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE", 15 );
	CFDictionarySetData( dict, CFSTR( "data-16" ), "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF", 16 );
	CFDictionarySetInt64( dict, CFSTR( "number-0" ), 0 );
	CFDictionarySetInt64( dict, CFSTR( "number-5" ), 5 );
	CFDictionarySetInt64( dict, CFSTR( "number-14" ), 14 );
	CFDictionarySetInt64( dict, CFSTR( "number-15" ), 15 );
	CFDictionarySetInt64( dict, CFSTR( "number-16" ), 16 );
	CFDictionarySetInt64( dict, CFSTR( "number-neg-123" ), -123 );
	CFDictionarySetInt64( dict, CFSTR( "number-big32" ), INT_MAX );
	CFDictionarySetInt64( dict, CFSTR( "number-min64" ), INT64_MIN );
	CFDictionarySetInt64( dict, CFSTR( "number-max64" ), INT64_MAX );
	CFDictionarySetValue( dict, CFSTR( "string-empty" ), CFSTR( "" ) );
	CFDictionarySetValue( dict, CFSTR( "string-abc" ), CFSTR( "abc" ) );
	
	date = CFDateCreateWithComponents( NULL, 2015, 4, 23, 9, 28, 35 );
	require_action( date, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "date" ), date );
	CFRelease( date );
	
	num = CFNumberCreateInt64( 0 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, num, CFSTR( "num-0-key" ) );
	CFRelease( num );
	
	num = CFNumberCreateInt64( 10 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, num, CFSTR( "num-10-key" ) );
	CFRelease( num );
	
	num = CFNumberCreateInt64( 15 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, num, CFSTR( "num-15-key" ) );
	CFRelease( num );
	
	num = CFNumberCreateInt64( 100 );
	require_action( num, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, num, CFSTR( "num-100-key" ) );
	CFRelease( num );
	
	err = CFPropertyListAppendFormatted( NULL, dict, 
		"%kO="
		"["
			"%O"
			"%i"
			"%O"
			"{"
				"%kO=%O"
				"%kO=%O"
			"}"
		"]", 
		CFSTR( "nested-array" ), 
			CFSTR( "array-string" ), 
			1000000, 
			kCFBooleanTrue, 
				CFSTR( "array-dict-key1" ), CFSTR( "array-dict-value1" ), 
				CFSTR( "array-dict-key2" ), CFSTR( "array-dict-value2" ) );
	require_noerr( err, exit );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( dict, 
		kCFBinaryPlistStreamedFlag_Header	| 
		kCFBinaryPlistStreamedFlag_Trailer	|
		kCFBinaryPlistStreamedFlag_Begin	|
		kCFBinaryPlistStreamedFlag_End	|
		kCFBinaryPlistStreamedFlag_Body, 
		_StreamedCallback, &ctx );
	require_noerr( err, exit );
	
	plist = CFBinaryPlistStreamedCreateWithBytes( ctx.buf, ctx.len, &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, dict ), exit, err = kMismatchErr );
	
	plist2 = CFBinaryPlistStreamedCreateWithBytes( kTestStreamedPlist1, sizeof( kTestStreamedPlist1 ), &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist2, dict ), exit, err = kMismatchErr );
	
	ForgetCF( &dict );
	ForgetCF( &plist );
	ForgetCF( &plist2 );
	
	// Dictionary test.
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, "{}", &err );
	require_noerr( err, exit );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_Begin, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\xD0", 1 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_Header | kCFBinaryPlistStreamedFlag_Begin, 
		_StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "CFB1" "\xD0", 5 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_End, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x01", 1 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_Trailer | kCFBinaryPlistStreamedFlag_End, 
		_StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x01" "END!", 5 ), exit, err = kMismatchErr );
	
	ForgetCF( &dict );
	
	// Array test.
	
	array = (CFArrayRef) CFCreateF( &err, "[]", &err );
	require_noerr( err, exit );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_Begin, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\xA0", 1 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_Header | kCFBinaryPlistStreamedFlag_Begin, 
		_StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "CFB1" "\xA0", 5 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_End, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x01", 1 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_Trailer | kCFBinaryPlistStreamedFlag_End, 
		_StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x01" "END!", 5 ), exit, err = kMismatchErr );
	
	ForgetCF( &array );
	
	// Raw bytes test.
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteBytes( "", 0, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x40", 1 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteBytes( "a", 1, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x41" "a", 2 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteBytes( "12345678901234", 14, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x4E" "123456789012345", 15 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteBytes( "123456789012345", 15, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x4F" "\x9F" "123456789012345", 17 ), exit, err = kMismatchErr );
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteBytes( "123456789012345678", 18, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	require_action( MemEqual( ctx.buf, ctx.len, "\x4F" "\x10" "\x12" "123456789012345678", 21 ), exit, err = kMismatchErr );
	
	// Begin/write/end tests.
	
	ctx.len = 0;
	err = CFBinaryPlistStreamedWriteObject( NULL, kCFBinaryPlistStreamedFlag_Header, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	dict = (CFMutableDictionaryRef) CFCreateF( &err, "{}", &err );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_Begin, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteObject( CFSTR( "array" ), kCFBinaryPlistStreamedFlag_Body, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	array = (CFArrayRef) CFCreateF( &err, "[]", &err );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_Begin, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	
	err = CFBinaryPlistStreamedWriteBytes( "a", 1, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteBytes( "xyz", 3, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteBytes( "12", 2, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	
	err = CFBinaryPlistStreamedWriteObject( array, kCFBinaryPlistStreamedFlag_End, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteObject( dict, kCFBinaryPlistStreamedFlag_End, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	err = CFBinaryPlistStreamedWriteObject( NULL, kCFBinaryPlistStreamedFlag_Trailer, _StreamedCallback, &ctx );
	require_noerr( err, exit );
	
	ForgetCF( &array );
	ForgetCF( &dict );
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, 
		"{"
			"%kO="
			"["
				"%D"
				"%D"
				"%D"
			"]"
		"}", 
		CFSTR( "array" ),
			"a",	1, 
			"xyz",	3, 
			"12",	2 );
	require_noerr( err, exit );
	
	plist = CFBinaryPlistStreamedCreateWithBytes( ctx.buf, ctx.len, &err );
	require_noerr( err, exit );
	require_action( CFEqual( plist, dict ), exit, err = kMismatchErr );
	
	ForgetCF( &dict );
	ForgetCF( &plist );
	
exit:
	CFReleaseNullSafe( array );
	CFReleaseNullSafe( plist );
	CFReleaseNullSafe( plist2 );
	CFReleaseNullSafe( dict );
	return( err );
}

//===========================================================================================================================
//	_StreamedCallback
//===========================================================================================================================

static OSStatus	_StreamedCallback( const void *inData, size_t inLen, void *inContext )
{
	TestCFLiteBinaryPlistStreamedWriteContext * const		ctx = (TestCFLiteBinaryPlistStreamedWriteContext *) inContext;
	OSStatus												err;
	
	require_action( ( sizeof( ctx->buf ) - ctx->len ) >= inLen, exit, err = kSizeErr );
	memcpy( &ctx->buf[ ctx->len ], inData, inLen );
	ctx->len += inLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TestCFLiteBinaryPlistStreamedUniquing
//===========================================================================================================================

static OSStatus	TestCFLiteBinaryPlistStreamedUniquing( void )
{
	CFBinaryPlistStreamedFlags		flags	= kCFBinaryPlistStreamedFlags_Default | kCFBinaryPlistStreamedFlag_Unique;
	OSStatus						err;
	CFMutableDictionaryRef			dict	= NULL;
	CFMutableDictionaryRef			dict2;
	CFMutableDictionaryRef			dict3;
	CFDateRef						date;
	CFMutableArrayRef				array;
	CFDataRef						data	= NULL;
	CFDataRef						data2	= NULL;
	Boolean							good;
	
	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );
	
	CFDictionarySetValue( dict, CFSTR( "key false" ), kCFBooleanFalse );
	CFDictionarySetValue( dict, CFSTR( "key true" ), kCFBooleanTrue );
	CFDictionarySetInt64( dict, CFSTR( "key small int 0" ), 0 );
	CFDictionarySetInt64( dict, CFSTR( "key small int 10" ), 10 );
	CFDictionarySetInt64( dict, CFSTR( "key int 200" ), 200 );
	CFDictionarySetInt64( dict, CFSTR( "key int 1234" ), 1234 );
	CFDictionarySetInt64( dict, CFSTR( "key int 123456" ), 123456 );
	CFDictionarySetInt64( dict, CFSTR( "key int 12345678901" ), INT64_C( 12345678901 ) );
	CFDictionarySetDouble( dict, CFSTR( "key double 3.1415" ), 3.1415 );
	CFDictionarySetDouble( dict, CFSTR( "key double 1234.567" ), 1234.567 );
	
	date = CFDateCreateWithComponents( NULL, 2015, 5, 7, 8, 57, 01 );
	require_action( date, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "key date 2015-05-07 08:57:01" ), date );
	CFRelease( date );
	
	date = CFDateCreateWithComponents( NULL, 2002, 3, 4, 5, 6, 7 );
	require_action( date, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "key date 2002-03-04 05:06:07" ), date );
	CFRelease( date );
	
	CFDictionarySetValue( dict, CFSTR( "key string empty" ), CFSTR( "" ) );
	CFDictionarySetValue( dict, CFSTR( "key string a" ), CFSTR( "a" ) );
	CFDictionarySetValue( dict, CFSTR( "key string ab" ), CFSTR( "ab" ) );
	CFDictionarySetValue( dict, CFSTR( "key string abcdefghijklmnopqrstuvwyxz" ), CFSTR( "abcdefghijklmnopqrstuvwyxz" ) );
	CFDictionarySetValue( dict, CFSTR( "key string ☃" ), CFSTR( "☃" ) );
	
	CFDictionarySetData( dict, CFSTR( "key data empty" ), "", 0 );
	CFDictionarySetData( dict, CFSTR( "key data 00" ), "\x00", 1 );
	CFDictionarySetData( dict, CFSTR( "key data 00 11 22 33 44 55 66 77 88 99" ), "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99", 10 );
	
	array = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	CFArrayAppendValue( array, CFSTR( "value string xyz" ) );
	CFArrayAppendValue( array, CFSTR( "value string 1234" ) );
	CFDictionarySetValue( dict, CFSTR( "key array strings: xyz, 1234" ), array );
	CFRelease( array );
	
	dict2 = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict2, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict2, CFSTR( "key string querty" ), CFSTR( "querty" ) );
	CFDictionarySetValue( dict2, CFSTR( "key string keyboard" ), CFSTR( "keyboard" ) );
	CFDictionarySetValue( dict, CFSTR( "key dict strings: querty, keyboard" ), dict2 );
	CFRelease( dict2 );
	
	// Test with no dups.
	
	data = CFBinaryPlistStreamedCreateDataEx( dict, flags, &err );
	require_noerr( err, exit );
	
	dict3 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithData( data, flags, &err );
	ForgetCF( &data );
	require_noerr( err, exit );
	good = CFEqual( dict3, dict );
	CFRelease( dict3 );
	require_action( good, exit, err = kMismatchErr );
	
	// Test with dups.
	
	CFDictionarySetValue( dict, CFSTR( "key dup false" ), kCFBooleanFalse );
	CFDictionarySetValue( dict, CFSTR( "key dup true" ), kCFBooleanTrue );
	CFDictionarySetInt64( dict, CFSTR( "key dup small int 0" ), 0 );
	CFDictionarySetInt64( dict, CFSTR( "key dup int 200" ), 200 );
	CFDictionarySetInt64( dict, CFSTR( "key dup int 1234" ), 1234 );
	CFDictionarySetDouble( dict, CFSTR( "key dup double 3.1415" ), 3.1415 );
	CFDictionarySetData( dict, CFSTR( "key dup data empty" ), "", 0 );
	CFDictionarySetData( dict, CFSTR( "key dup data 00" ), "\x00", 1 );
	CFDictionarySetData( dict, CFSTR( "key dup data 00 11 22 33 44 55 66 77 88 99" ), "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99", 10 );
	
	date = CFDateCreateWithComponents( NULL, 2015, 5, 7, 8, 57, 01 );
	require_action( date, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( "key dup date 2015-05-07 08:57:01" ), date );
	CFRelease( date );
	
	CFDictionarySetValue( dict, CFSTR( "key dup string empty" ), CFSTR( "" ) );
	CFDictionarySetValue( dict, CFSTR( "key dup string a" ), CFSTR( "a" ) );
	CFDictionarySetValue( dict, CFSTR( "key dup string ab" ), CFSTR( "ab" ) );
	CFDictionarySetValue( dict, CFSTR( "key dup string 2 ab" ), CFSTR( "ab" ) );
	CFDictionarySetValue( dict, CFSTR( "key dup string 3 ab" ), CFSTR( "ab" ) );
	CFDictionarySetValue( dict, CFSTR( "key string querty" ), CFSTR( "dup key string querty" ) );
	
	data = CFBinaryPlistStreamedCreateDataEx( dict, flags, &err );
	require_noerr( err, exit );
	
	dict3 = (CFMutableDictionaryRef) CFBinaryPlistStreamedCreateWithData( data, flags, &err );
	ForgetCF( &data );
	require_noerr( err, exit );
	good = CFEqual( dict3, dict );
	CFRelease( dict3 );
	require_action( good, exit, err = kMismatchErr );
	
	// Test that uniquing version is smaller.
	
	data = CFBinaryPlistStreamedCreateDataEx( dict, flags, &err );
	require_noerr( err, exit );
	
	data2 = CFBinaryPlistStreamedCreateData( dict, &err );
	require_noerr( err, exit );
	
	good = ( CFDataGetLength( data ) < CFDataGetLength( data2 ) ) ? true : false;
	ForgetCF( &data );
	ForgetCF( &data2 );
	require_action( good, exit, err = kMismatchErr );
	
exit:
	CFReleaseNullSafe( dict );
	CFReleaseNullSafe( data );
	CFReleaseNullSafe( data2 );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == Format ==
#endif

/*===========================================================================================================================
	Binary Property List Format
	---------------------------
	
	HEADER
		signature "CFB1" for version 1 streamed binary plists.
		OR signature "bplist00" for version 0 binary plists (normal CF).
	
	ROOT OBJECT
		Object Formats (marker byte optionally followed by additional info)
		Streamed binary plists use little endian.
		Version 0 binary plists use big endian.
		
		null	0000 0000							// Null object.
		null	0000 0001							// Null terminator object.
		bool	0000 1000							// false.
		bool	0000 1001							// true.
		fill	0000 1111							// fill byte.
		int		0001 nnnn	...						// 2^nnnn bytes.
		real	0010 nnnn	...						// nnnn=2: 4-byte IEEE float. nnnn=3: 8-byte IEEE double.
		date	0011 0011	...						// Seconds from 2001-01-01 00:00:00. Negative is before then.
		data	0100 nnnn	[count]	...				// nnnn < 15: nnnn bytes, nnnn == 15: int count then <count> bytes.
		string	0101 nnnn	[count]	...				// ASCII string, nnnn is # of chars, else 1111 then int count then bytes.
		string	0110 nnnn	[count]	...				// Unicode string, nnnn is # of chars, else 1111 then int count then UTF-16.
		string	0111 0000	...						// UTF-8 bytes with a NUL terminator.
		uid		1000 nnnn	<UID>					// nnnn+1 is byte count then unsigned UID bytes.
		int		1001 xxxx							// Small integer: xxxx is 0-15.
		array	1010 0000	objects					// Value objects with a null terminator object.
		array	1010 nnnn	[int]	objects			// nnnn is count, unless '1111', then int count follows,
		dict	1101 0000	key/value object pairs	// Key/value object pairs with a null terminator object.
		dict	1101 nnnn	[int]	keys, values	// nnnn is count, unless '1111', then int count follows.
	
	OFFSET TABLE (for version 0 binary plists)
		list of ints, byte size of which is given in trailer
		-- these are the byte offsets into the file
		-- number of these is in the trailer
	
	TRAILER
		signature ("END!") for streamed binary plists.
		OR for version 0 binary plists:
			byte size of offset ints in offset table
			byte size of object refs in arrays and dicts
			number of offsets in offset table (also is number of objects)
			element # in offset table which is top level object
			offset table offset
	
	UNIQUING
		For streamed binary plists, uniquing writes the first instance of an object directly then writes a UID for 
		subsequent instances of the same object. UID's start at 0 and count each object that supports uniquing. To avoid 
		cases where a UID is larger than the object itself and to minimize the size of each UID, only the following 
		objects are uniqued:
		
		- Data objects > 1 byte.
		- Date objects.
		- Number objects that use more than 1 byte when serialized. This is integers > 0xFF and reals.
		- Non-empty strings.
===========================================================================================================================*/
