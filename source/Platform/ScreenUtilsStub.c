/*
	File:    	ScreenUtilsStub.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	320.17
	
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
	
	Copyright (C) 2013-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ScreenUtils.h"

#include "CommonServices.h"
#include "CFUtils.h"
#include "TickUtils.h"

#if( !SCREEN_STREAM_DLL )
	#include CF_HEADER
	#include CF_RUNTIME_HEADER
	#include LIBDISPATCH_HEADER
#endif

#define H264_ANNEX_B 1

//===========================================================================================================================
//	ScreenStream
//===========================================================================================================================

#if H264_ANNEX_B
#include "MiscUtils.h"
#endif

#if( SCREEN_STREAM_DLL )
typedef struct ScreenStreamImp *			ScreenStreamImpRef;
struct ScreenStreamImp
#else
typedef struct ScreenStreamPrivate *		ScreenStreamImpRef;
struct ScreenStreamPrivate
#endif
{
#if( !SCREEN_STREAM_DLL )
	CFRuntimeBase		base;					// CF type info. Must be first.
#endif
	void *				delegateContext;		// Context for the session delegate
#if H264_ANNEX_B
	uint8_t *			annexBHeaderPtr;		// Ptr to H.264 Annex-B header.
	size_t				annexBHeaderLen;		// Number of bytes in the H.264 Annex-B header.
	Boolean				annexBHeaderWritten;	// True if we've written the full Annex-B header to the decoder.
	size_t				nalSizeHeader;			// Number of bytes in the size before each NAL unit.
#else
	uint8_t *			avccPtr;
	size_t				avccLen;
#endif
	int					widthPixels;			// Width of the screen in pixels.
	int					heightPixels;			// Height of the screen in pixels.
};

#if( SCREEN_STREAM_DLL )
	#define _ScreenStreamGetImp( STREAM )		( (ScreenStreamImpRef) ScreenStreamGetContext( (STREAM) ) )
#else
	#define _ScreenStreamGetImp( STREAM )		(STREAM)
#endif

#if( !SCREEN_STREAM_DLL )
static void	_ScreenStreamGetTypeID( void *inContext );
static void	_ScreenStreamFinalize( CFTypeRef inCF );

static dispatch_once_t			gScreenStreamInitOnce = 0;
static CFTypeID					gScreenStreamTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kScreenStreamClass = 
{
	0,						// version
	"ScreenStream",			// className
	NULL,					// init
	NULL,					// copy
	_ScreenStreamFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};
#endif

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( ScreenStream, kLogLevelNotice, kLogFlags_Default, "ScreenStream", NULL );
#define ss_dlog( LEVEL, ... )		dlogc( &log_category_from_name( ScreenStream ), (LEVEL), __VA_ARGS__ )
#define ss_ulog( LEVEL, ... )		ulog( &log_category_from_name( ScreenStream ), (LEVEL), __VA_ARGS__ )

#if( !SCREEN_STREAM_DLL )
//===========================================================================================================================
//	ScreenStreamGetTypeID
//===========================================================================================================================

CFTypeID	ScreenStreamGetTypeID( void )
{
	dispatch_once_f( &gScreenStreamInitOnce, NULL, _ScreenStreamGetTypeID );
	return( gScreenStreamTypeID );
}

static void _ScreenStreamGetTypeID( void *inContext )
{
	(void) inContext;
	
	gScreenStreamTypeID = _CFRuntimeRegisterClass( &kScreenStreamClass );
	check( gScreenStreamTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	ScreenStreamCreate
//===========================================================================================================================

OSStatus	ScreenStreamCreate( ScreenStreamRef *outStream )
{
	OSStatus			err;
	ScreenStreamRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (ScreenStreamRef) _CFRuntimeCreateInstance( NULL, ScreenStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	// $$$ TODO: Other initialization goes here.
	// This function is only called when ScreenUtils is compiled into the AirPlay library.
	
	*outStream = me;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ScreenStreamFinalize
//===========================================================================================================================

static void	_ScreenStreamFinalize( CFTypeRef inCF )
{
	ScreenStreamRef const		me = (ScreenStreamRef) inCF;
	
	DEBUG_USE_ONLY( me );
	
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called when ScreenUtils is compiled into the AirPlay library, when the retain count of a ScreenStream 
	// object goes to zero.
	(void) me;

#if H264_ANNEX_B
	ForgetMem( &me->annexBHeaderPtr );
#else
	ForgetMem( &me->avccPtr );
#endif
}
#endif

#if( SCREEN_STREAM_DLL )
//===========================================================================================================================
//	ScreenStreamInitialize
//===========================================================================================================================

OSStatus	ScreenStreamInitialize( ScreenStreamRef inStream )
{
	OSStatus				err;
	ScreenStreamImpRef		me;
	
	require_action( ScreenStreamGetContext( inStream ) == NULL, exit, err = kAlreadyInitializedErr );
	
	me = (ScreenStreamImpRef) calloc( 1, sizeof( *me ) );
	require_action( me, exit, err = kNoMemoryErr );
	
	// $$$ TODO: Other initialization goes here.
	// This function is called (instead of ScreenStreamCreate()) when ScreenUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the initialization code should look very similar
	// to that in ScreenStreamCreate().
	
	ScreenStreamSetContext( inStream, me );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ScreenStreamFinalize
//===========================================================================================================================

void	ScreenStreamFinalize( ScreenStreamRef inStream )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	
	if( !me ) return;
	
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called (instead of _ScreenStreamFinalize()) when ScreenUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the finalization code should look very similar to that in
	// _ScreenStreamFinalize().
	// It is automatically invoked, when the retain count of an ScreenStream object goes to zero.

#if H264_ANNEX_B
	ForgetMem( &me->annexBHeaderPtr );
#else
	ForgetMem( &me->avccPtr );
#endif
	free( me );
	ScreenStreamSetContext( inStream, NULL );
}
#endif

//===========================================================================================================================
//	ScreenStreamSetDelegateContext
//===========================================================================================================================

void ScreenStreamSetDelegateContext( ScreenStreamRef inStream, void* inContext )
{
	ScreenStreamImpRef const me = _ScreenStreamGetImp( inStream );
	me->delegateContext = inContext;
}

//===========================================================================================================================
//	ScreenStreamSetWidthHeight
//===========================================================================================================================

void ScreenStreamSetWidthHeight( ScreenStreamRef inStream, uint32_t width, uint32_t height )
{
	ScreenStreamImpRef const me = _ScreenStreamGetImp( inStream );
	me->widthPixels = width;
	me->heightPixels = height;
}

//===========================================================================================================================
//	ScreenStreamSetAVCC
//===========================================================================================================================

OSStatus ScreenStreamSetAVCC( ScreenStreamRef inStream, const uint8_t* avccPtr, size_t avccLen )
{
	OSStatus err = kNoErr;
	
	ScreenStreamImpRef const me = _ScreenStreamGetImp( inStream );
#if H264_ANNEX_B
	uint8_t *			headerPtr;
	size_t				headerLen;
	size_t				nalSizeHeader;
	
	err = H264ConvertAVCCtoAnnexBHeader( avccPtr, avccLen, NULL, 0, &headerLen, NULL, NULL );
	require_noerr( err, exit );
	
	headerPtr = (uint8_t *) malloc( headerLen );
	require_action( headerPtr, exit, err = kNoMemoryErr );
	err = H264ConvertAVCCtoAnnexBHeader( avccPtr, avccLen, headerPtr, headerLen, &headerLen, &nalSizeHeader, NULL );
	if( err )
		free( headerPtr );
	require_noerr( err, exit );
	
	if( me->annexBHeaderPtr )
		free( me->annexBHeaderPtr );
	me->annexBHeaderPtr		= headerPtr;
	me->annexBHeaderLen		= headerLen;
	me->nalSizeHeader		= nalSizeHeader;
	me->annexBHeaderWritten = false;
#else
	if ( avccLen != me->avccLen || memcmp( avccPtr, me->avccPtr, avccLen) != 0 )
	{
		ForgetMem( &me->avccPtr );
		uint8_t* newPtr = malloc( avccLen );
		require_action( newPtr, exit, err = kNoMemoryErr );
		memcpy( newPtr, avccPtr, avccLen );
		me->avccPtr = newPtr;
		me->avccLen = avccLen;
	}
#endif
exit:
	return( err );
}

//===========================================================================================================================
//	ScreenStreamStart
//===========================================================================================================================

OSStatus	ScreenStreamStart( ScreenStreamRef inStream )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	OSStatus						err;
	
	// $$$ TODO: This is where the video processing chain should be started.
	// Once this function returns, ScreenStreamProcessData() will be called continuously, providing H.264 bit-stream data
	// to be decoded and displayed.
	(void) me;
	
	err = kNoErr;
	ss_ulog( kLogLevelNotice, "Screen stream started\n" );
	
	if( err )
	{
		ss_ulog( kLogLevelError, "### Start screen stream failed: %#m\n", err );
		ScreenStreamStop( inStream );
	}
	return( err );
}

//===========================================================================================================================
//	ScreenStreamStop
//===========================================================================================================================

void	ScreenStreamStop( ScreenStreamRef inStream )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );

	// $$$ TODO: This is where the video processing chain should be stopped.
	// This function is responsible for releasing any resources allocated in ScreenStreamStart().

#if H264_ANNEX_B
	ForgetMem( &me->annexBHeaderPtr );
#else
	ForgetMem( &me->avccPtr );
#endif
	ss_ulog( kLogLevelNotice, "Screen stream stopped\n" );
}

#if H264_ANNEX_B
//===========================================================================================================================
//	_ScreenStreamDecode
//===========================================================================================================================

static OSStatus	_ScreenStreamDecode( ScreenStreamImpRef me, const uint8_t *inPtr, size_t inLen )
{
	(void)me;
	(void)inPtr;
	(void)inLen;

	// $$$ TODO: Decode H.264 Annex B data
	// This function currently expects the frame to be decoded (but not necessarily displayed) synchronously; that is,
	// when this function returns, there should be no expectation that the memory pointed to by inPtr will remain valid.
	return( kNoErr );
}
#endif

//===========================================================================================================================
//	ScreenStreamProcessData
//===========================================================================================================================

OSStatus
	ScreenStreamProcessData( 
		ScreenStreamRef				inStream, 
		const uint8_t *				inData,
		size_t						inLen, 
		uint64_t					inDisplayTicks, 
		CFDictionaryRef				inOptions, 
		ScreenStreamCompletion_f	inCompletion, 
		void *						inContext )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	OSStatus						err;
	
	(void) inDisplayTicks;
	(void) inOptions;
	
#if H264_ANNEX_B
	const uint8_t *					src;
	const uint8_t *					end;
	const uint8_t *					nalPtr;
	size_t							nalLen;
	const uint8_t					startCodePrefix[4] = { 0x00, 0x00, 0x00, 0x01 };//H.264 zero_byte and start_code_prefix_one_3bytes, see H.264 Annex B, B.1.1

	if( !me->annexBHeaderWritten )
	{
		require_action( me->annexBHeaderPtr, exit, err = kNotPreparedErr );
		err = _ScreenStreamDecode( me, me->annexBHeaderPtr, me->annexBHeaderLen );
		require_noerr( err, exit );
		me->annexBHeaderWritten = true;
	}
	
	if ( me->nalSizeHeader == sizeof( startCodePrefix ) )
	{
		src = inData;
		end = src + inLen;
		const uint8_t * start = inData;
		while( ( err = H264GetNextNALUnit( src, end, me->nalSizeHeader, &nalPtr, &nalLen, &src ) ) == kNoErr )
		{
			memcpy( (uint8_t *)start, startCodePrefix, sizeof( startCodePrefix ) );
			start = (uint8_t *)src;
		}
		if ( err == kEndingErr )
			err = _ScreenStreamDecode( me, inData, inLen );
	}
	else
	{
		uint8_t * pData = malloc( inLen * 4 );//Worst case a buffer full of nothing but single byte lengths of 0x00 will be expanded to 4 times the size when replaced with 0x00000001
		require_action( pData, exit, err = kNoMemoryErr );
		
		size_t uIndex = 0;
		src = inData;
		end = src + inLen;
		while( ( err = H264GetNextNALUnit( src, end, me->nalSizeHeader, &nalPtr, &nalLen, &src ) ) == kNoErr )
		{
			memcpy( pData + uIndex, startCodePrefix, sizeof( startCodePrefix ) );
			uIndex += sizeof( startCodePrefix );
			memcpy( pData + uIndex, nalPtr, nalLen );
			uIndex += nalLen;
		}
		if ( err == kEndingErr )
			err = _ScreenStreamDecode( me, pData, uIndex );
		free(pData);
	}
	require_noerr( err, exit );
#else
	// $$$ TODO: Decode an H.264 frame
	// This function currently expects the frame to be decoded (but not necessarily displayed) synchronously; that is,
	// when this function returns, there should be no expectation that the memory pointed to by inData will remain valid.
	(void) inData;
	(void) inLen;
	(void) me;
#endif

	err = kNoErr;
	
exit:;
	if( inCompletion ) inCompletion( inContext );
	if( err ) ss_ulog( kLogLevelError, "### Screen stream process data failed: %#m\n", err );
	return( err );
}
