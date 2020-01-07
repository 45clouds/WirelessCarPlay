/*
	File:    	AudioUtilsDLL.c
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
	
	AudioStream adapter that delegates functionality to a DLL.
	
	This defaults to loading the DLL from "libAudioStream.so".
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DAUDIO_STREAM_DLL_PATH\"/some/other/path/libAudioStream.so\"
*/

#include "AudioUtils.h"

#include <dlfcn.h>
#include <stdlib.h>

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

#if( defined( AUDIO_STREAM_DLL_PATH ) )
	#define kAudioStreamDLLPath					AUDIO_STREAM_DLL_PATH
#else
	#define kAudioStreamDLLPath					"libAudioStream.so"
#endif

#define FIND_SYM( NAME )	(NAME ## _f)(uintptr_t) dlsym( me->dllHandle, # NAME );

struct AudioStreamPrivate
{
	CFRuntimeBase							base;		// CF type info. Must be first.
	void *									context;	// Context for DLLs.
	void *									dllHandle;	// Handle to the DLL implementing the internals.
	AudioStreamInitialize_f					initialize_f;
	AudioStreamFinalize_f					finalize_f;
	AudioStreamSetInputCallback_f			setInputCallback_f;
	AudioStreamSetOutputCallback_f			setOutputCallback_f;
	_AudioStreamCopyProperty_f				copyProperty_f;
	_AudioStreamSetProperty_f				setProperty_f;
	AudioStreamRampVolume_f					rampVolume_f;
	AudioStreamPrepare_f					prepare_f;
	AudioStreamStart_f						start_f;
	AudioStreamStop_f						stop_f;
};

static void		_AudioStreamGetTypeID( void *inContext );
static void		_AudioStreamFinalize( CFTypeRef inCF );

static const CFRuntimeClass		kAudioStreamClass = 
{
	0,						// version
	"AudioStream",			// className
	NULL,					// init
	NULL,					// copy
	_AudioStreamFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t		gAudioStreamInitOnce	= 0;
static CFTypeID				gAudioStreamTypeID		= _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	AudioStreamGetTypeID
//===========================================================================================================================

CFTypeID	AudioStreamGetTypeID( void )
{
	dispatch_once_f( &gAudioStreamInitOnce, NULL, _AudioStreamGetTypeID );
	return( gAudioStreamTypeID );
}

static void _AudioStreamGetTypeID( void *inContext )
{
	(void) inContext;
	
	gAudioStreamTypeID = _CFRuntimeRegisterClass( &kAudioStreamClass );
	check( gAudioStreamTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	AudioStreamCreate
//===========================================================================================================================

OSStatus	AudioStreamCreate( AudioStreamRef *outStream )
{
	OSStatus			err;
	AudioStreamRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AudioStreamRef) _CFRuntimeCreateInstance( NULL, AudioStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	// Note: this uses RTLD_NODELETE to avoid re-initialization issues with global log categories if the DLL is unloaded 
	// and reloaded. Log categories we know about are removed on finalize, but DLL developers may not be as thorough.
	
	me->dllHandle = dlopen( kAudioStreamDLLPath, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE );
	require_action( me->dllHandle, exit, err = kPathErr );
	
	me->initialize_f			= FIND_SYM( AudioStreamInitialize );
	me->finalize_f				= FIND_SYM( AudioStreamFinalize );
	me->setInputCallback_f		= FIND_SYM( AudioStreamSetInputCallback );
	me->setOutputCallback_f		= FIND_SYM( AudioStreamSetOutputCallback );
	me->copyProperty_f			= FIND_SYM( _AudioStreamCopyProperty );
	me->setProperty_f			= FIND_SYM( _AudioStreamSetProperty );
	me->rampVolume_f			= FIND_SYM( AudioStreamRampVolume );
	me->prepare_f				= FIND_SYM( AudioStreamPrepare );
	me->start_f					= FIND_SYM( AudioStreamStart );
	me->stop_f					= FIND_SYM( AudioStreamStop );
	
	if( me->initialize_f )
	{
		err = me->initialize_f( me );
		require_noerr( err, exit );
	}
	
	*outStream = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_AudioStreamFinalize
//===========================================================================================================================

static void	_AudioStreamFinalize( CFTypeRef inCF )
{
	AudioStreamRef const		me = (AudioStreamRef) inCF;
	
	if( me->finalize_f ) me->finalize_f( me );
	if( me->dllHandle )
	{
		dlclose( me->dllHandle );
		me->dllHandle = NULL;
	}
}

//===========================================================================================================================
//	AudioStreamGetContext
//===========================================================================================================================

void *	AudioStreamGetContext( AudioStreamRef me )
{
	return( me->context );
}

//===========================================================================================================================
//	AudioStreamSetContext
//===========================================================================================================================

void	AudioStreamSetContext( AudioStreamRef me, void *inContext )
{
	me->context = inContext;
}

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void	AudioStreamSetInputCallback( AudioStreamRef me, AudioStreamInputCallback_f inFunc, void *inContext )
{
	if( me->setInputCallback_f ) me->setInputCallback_f( me, inFunc, inContext );
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void	AudioStreamSetOutputCallback( AudioStreamRef me, AudioStreamOutputCallback_f inFunc, void *inContext )
{
	if( me->setOutputCallback_f ) me->setOutputCallback_f( me, inFunc, inContext );
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef	_AudioStreamCopyProperty( CFTypeRef inObject, CFStringRef inProperty, OSStatus *outErr )
{
	AudioStreamRef const		me = (AudioStreamRef) inObject;
	
	if( me->copyProperty_f ) return( me->copyProperty_f( inObject, inProperty, outErr  ) );
	if( outErr ) *outErr = kUnsupportedErr;
	return( NULL );
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus	_AudioStreamSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue )
{
	AudioStreamRef const		me = (AudioStreamRef) inObject;
	
	return( me->setProperty_f ? me->setProperty_f( inObject, inProperty, inValue ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioStreamRampVolume
//===========================================================================================================================

OSStatus
	AudioStreamRampVolume( 
		AudioStreamRef		me, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue )
{
	return( me->rampVolume_f ? me->rampVolume_f( me, inFinalVolume, inDurationSecs, inQueue ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus	AudioStreamPrepare( AudioStreamRef me )
{
	return( me->prepare_f ? me->prepare_f( me ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef me )
{
	return( me->start_f ? me->start_f( me ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void	AudioStreamStop( AudioStreamRef me, Boolean inDrain )
{
	if( me->stop_f ) me->stop_f( me, inDrain );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AudioStreamTest
//===========================================================================================================================

#if( !EXCLUDE_UNIT_TESTS )

static void
	_AudioStreamTestInput( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext );

static void
	_AudioStreamTestOutput( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );

OSStatus	AudioStreamTest( Boolean inInput )
{
	OSStatus						err;
	SineTableRef					sineTable = NULL;
	AudioStreamRef					me = NULL;
	AudioStreamBasicDescription		asbd;
	size_t							written = 0;
	
	err = SineTable_Create( &sineTable, 44100, 800 );
	require_noerr( err, exit );
	
	err = AudioStreamCreate( &me );
	require_noerr( err, exit );
	
	if( inInput )
	{
		AudioStreamSetInputCallback( me, _AudioStreamTestInput, &written );
		_AudioStreamSetProperty( me, kAudioStreamProperty_Input, kCFBooleanTrue );
	}
	else
	{
		AudioStreamSetOutputCallback( me, _AudioStreamTestOutput, sineTable );
	}
	
	ASBD_FillPCM( &asbd, 44100, 16, 16, 2 );
	err = AudioStreamPropertySetBytes( me, kAudioStreamProperty_Format, &asbd, sizeof( asbd ) );
	require_noerr( err, exit );
	
	err = AudioStreamPropertySetInt64( me, kAudioStreamProperty_PreferredLatency, 100000 );
	require_noerr( err, exit );
	
	err = AudioStreamStart( me );
	require_noerr( err, exit );
	
	sleep( 5 );
	
	if( inInput )
	{
		require_action( written > 0, exit, err = kReadErr );
	}
	
exit:
	AudioStreamForget( &me );
	if( sineTable ) SineTable_Delete( sineTable );
	printf( "AudioStreamTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

static void
	_AudioStreamTestInput( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext )
{
	size_t * const		writtenPtr = (size_t *) inContext;
	
	(void) inSampleTime;
	(void) inHostTime;
	(void) inBuffer;
	
	*writtenPtr += inLen;
}

static void
	_AudioStreamTestOutput( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext )
{
	SineTableRef const		sineTable = (SineTableRef) inContext;
	
	(void) inSampleTime;
	(void) inHostTime;
	
	SineTable_GetSamples( sineTable, 0, (int)( inLen / 4 ), inBuffer );
}

#endif // !EXCLUDE_UNIT_TESTS
