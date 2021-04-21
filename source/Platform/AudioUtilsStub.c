/*
	File:    	AudioUtilsStub.c
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "AudioUtils.h"

#include <errno.h>
#include <stdlib.h>

#include "CommonServices.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#if( !AUDIO_STREAM_DLL )
	#include CF_RUNTIME_HEADER
#endif

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

#if( AUDIO_STREAM_DLL )
typedef struct AudioStreamImp *			AudioStreamImpRef;
struct AudioStreamImp
#else
typedef struct AudioStreamPrivate *		AudioStreamImpRef;
struct AudioStreamPrivate
#endif
{
#if( !AUDIO_STREAM_DLL )
	CFRuntimeBase					base;					// CF type info. Must be first.
#endif
	Boolean							prepared;				// True if AudioStreamPrepare has been called (and stop hasn't yet).
	
	void *							delegateContext;		// Context for the session delegate
	AudioStreamInputCallback_f		inputCallbackPtr;		// Function to call to write input audio.
	void *							inputCallbackCtx;		// Context to pass to audio input callback function.
	AudioStreamOutputCallback_f		outputCallbackPtr;		// Function to call to read audio to output.
	void *							outputCallbackCtx;		// Context to pass to audio output callback function.
	Boolean							input;					// Enable audio input.
	AudioStreamBasicDescription		format;					// Format of the audio data.
	uint32_t						preferredLatencyMics;	// Max latency the app can tolerate.
	uint32_t						streamType;				// AirPlay Stream type (e.g. main, alt).
};

#if( AUDIO_STREAM_DLL )
	#define _AudioStreamGetImp( STREAM )		( (AudioStreamImpRef) AudioStreamGetContext( (STREAM) ) )
#else
	#define _AudioStreamGetImp( STREAM )		(STREAM)
#endif

#if( !AUDIO_STREAM_DLL )
	static void		_AudioStreamGetTypeID( void *inContext );
	static void		_AudioStreamFinalize( CFTypeRef inCF );
#endif

#if( !AUDIO_STREAM_DLL )
static dispatch_once_t			gAudioStreamInitOnce = 0;
static CFTypeID					gAudioStreamTypeID = _kCFRuntimeNotATypeID;
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
#endif

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( AudioStream, kLogLevelTrace, kLogFlags_Default, "AudioStream", NULL );
#define as_dlog( LEVEL, ... )		dlogc( &log_category_from_name( AudioStream ), (LEVEL), __VA_ARGS__ )
#define as_ulog( LEVEL, ... )		ulog( &log_category_from_name( AudioStream ), (LEVEL), __VA_ARGS__ )

#if( !AUDIO_STREAM_DLL )
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

OSStatus	AudioStreamCreate( CFDictionaryRef inOptions, AudioStreamRef *outStream )
{
	OSStatus			err;
	AudioStreamRef		me;
	size_t				extraLen;
	
	(void) inOptions;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AudioStreamRef) _CFRuntimeCreateInstance( NULL, AudioStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	// $$$ TODO: Other initialization goes here.
	// This function is only called when AudioUtils is compiled into the AirPlay library.
	
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
	
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called when AudioUtils is compiled into the AirPlay library, when the retain count of an AudioStream 
	// object goes to zero.
	(void) me;
}
#endif // !AUDIO_STREAM_DLL

#if( AUDIO_STREAM_DLL )
//===========================================================================================================================
//	AudioStreamInitialize
//===========================================================================================================================

OSStatus	AudioStreamInitialize( AudioStreamRef inStream )
{
	OSStatus				err;
	AudioStreamImpRef		me;
		
	require_action( AudioStreamGetContext( inStream ) == NULL, exit, err = kAlreadyInitializedErr );
	
	me = (AudioStreamImpRef) calloc( 1, sizeof( *me ) );
	require_action( me, exit, err = kNoMemoryErr );
	
	// $$$ TODO: Other initialization goes here.
	// This function is called (instead of AudioStreamCreate()) when AudioUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the initialization code should look very similar
	// to that in AudioStreamCreate().
	
	AudioStreamSetContext( inStream, me );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AudioStreamFinalize
//===========================================================================================================================

void	AudioStreamFinalize( AudioStreamRef inStream )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	
	if( !me ) return;
	
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called (instead of _AudioStreamFinalize()) when AudioUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the finalization code should look very similar to that in
	// _AudioStreamFinalize().
	// It is automatically invoked, when the retain count of an AudioStream object goes to zero.

	free( me );
	AudioStreamSetContext( inStream, NULL );
}
#endif

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void	AudioStreamSetInputCallback( AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void *inContext )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	
	me->inputCallbackPtr = inFunc;
	me->inputCallbackCtx = inContext;
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void	AudioStreamSetOutputCallback( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	
	me->outputCallbackPtr = inFunc;
	me->outputCallbackCtx = inContext;
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef
	_AudioStreamCopyProperty( 
		CFTypeRef		inObject,
		CFStringRef		inProperty, 
		OSStatus *		outErr )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( (AudioStreamRef) inObject );
	OSStatus					err;
	CFTypeRef					value = NULL;
	
	if( 0 ) {}
	
	// AudioType
	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		// $$$ TODO: Return the current audio type.
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		value = CFDataCreate( NULL, (const uint8_t *) &me->format, sizeof( me->format ) );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Input

	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
		value = me->input ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		value = CFNumberCreateInt64( me->preferredLatencyMics );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// StreamType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_StreamType ) )
	{
		value = CFNumberCreateInt64( me->streamType );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		// $$$ TODO: If your implementation uses a helper thread, return its name here.
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		// $$$ TODO: If your implementation uses a helper thread, return its priority here.
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus
	_AudioStreamSetProperty( 
		CFTypeRef		inObject,
		CFStringRef		inProperty, 
		CFTypeRef		inValue )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( (AudioStreamRef) inObject );
	OSStatus					err;
	
	// Properties may only be set before AudioStreamPrepare is called.
	
	require_action( !me->prepared, exit, err = kStateErr );
	
	if( 0 ) {}
	
	// AudioType

	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		// $$$ TODO: Use the audio type to enable certain types of audio processing.
		// For example, if the audio type is "telephony", echo cancellation should be enabled;
		// if the audio type is "speech recognition", non-linear processing algorithms should be disabled.
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		CFGetData( inValue, &me->format, sizeof( me->format ), NULL, &err );
		require_noerr( err, exit );
	}
	
	// Input

	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
		me->input = CFGetBoolean( inValue, NULL );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		me->preferredLatencyMics = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, exit );
	}
	
	// StreamType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_StreamType ) )
	{
		me->streamType = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, exit );
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		// $$$ TODO: If your implementation uses a helper thread, set the name of the thread to the string passed in
		// to this property.  See SetThreadName().
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		// $$$ TODO: If your implementation uses a helper thread, set the priority of the thread to the string passed in
		// to this property.  See SetCurrentThreadPriority().
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AudioStreamSetFormat
//===========================================================================================================================

void AudioStreamSetFormat( AudioStreamRef inStream, const AudioStreamBasicDescription* inFormat )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	me->format = *inFormat;
}

//===========================================================================================================================
//	AudioStreamSetDelegateContext
//===========================================================================================================================

void AudioStreamSetDelegateContext( AudioStreamRef inStream, void* inContext )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	me->delegateContext = inContext;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus	AudioStreamPrepare( AudioStreamRef inStream )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	OSStatus					err;
	
	// $$$ TODO: This is where the audio processing chain should be set up based on the properties previously set on the
	// AudioStream object:
	//	me->format specifies the sample rate, channel count, and bit-depth.
	//	me->input specifies whether or not the processing chain should be set up to record audio from the accessory's
	//	          microphone(s).
	// Audio output should always be prepared.
	// If the audio processing chain is successfully set up, me->prepared should be set to true.
	
	me->prepared = true;
	err = kNoErr;
	
	if( err ) AudioStreamStop( inStream, false );
	return( err );
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef inStream )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	OSStatus					err;
	
	if( !me->prepared )
	{
		err = AudioStreamPrepare( inStream );
		require_noerr( err, exit );
	}
	
	// $$$ TODO: This is where the audio processing chain should be started.
	//
	// me->outputCallbackPtr should be invoked periodically to retrieve a continuous stream of samples to be output.
	// When calling me->outputCallbackPtr(), a buffer is provided for the caller to write into.  Equally important
	// is the inSampleTime and inHostTime arguments.  It is important that accurate { inSampleTime, inHostTime } pairs
	// be provided to the caller.  inSampleTime should be a (reasonably) current running total of the number of samples
	// that have hit the speaker since AudioStreamStart() was called.  inHostTime is the system time, in units of ticks,
	// corresponding to inSampleTime (see TickUtils.h).  This information will be returned to the controller and is
	// a key piece in allowing the controller to derive the relationship between the controller's system clock and the
	// accessory's audio (DAC) clock for A/V sync.
	//
	// If input has been requested (me->input == true), then me->inputCallbackPtr should also be invoked periodically
	// to provide a continuous stream of samples from the accessory's microphone (possibly with some processing, depending
	// on the audio type, see kAudioStreamProperty_AudioType).  If no audio samples are available for whatever reason,
	// the me->inputCallbackPtr should be called with a buffer of zeroes.

	err = kNoErr;
	
exit:
	if( err ) AudioStreamStop( inStream, false );
	return( err );
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void	AudioStreamStop( AudioStreamRef inStream, Boolean inDrain )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );

	// $$$ TODO: This is where the audio processing chain should be stopped, and the audio processing chain torn down.
	// When AudioStreamStop() returns, the object should return to the state similar to before AudioStreamPrepare()
	// was called, so this function is responsible for undoing any resource allocation performed in AudioStreamPrepare().
	(void) inDrain;

	me->prepared = false;
}

#if 0
#pragma mark -
#endif

