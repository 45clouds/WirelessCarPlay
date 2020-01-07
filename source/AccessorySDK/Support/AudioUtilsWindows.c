/*
	File:    	AudioUtilsWindows.c
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
	
	AudioStream adapter that uses the Windows Wave APIs for audio capture and playback.
*/

#include "AudioUtils.h"

#include <stdlib.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "TickUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#pragma warning( disable:4201 )	// Disable "nonstandard extension used : nameless struct/union" warning for Microsoft headers.
#include "Mmsystem.h"
#pragma warning( default:4201 )	// Re-enable "nonstandard extension used : nameless struct/union" after Microsoft headers.
#pragma comment( lib, "Winmm.lib" )

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

#define kDefaultLatencyMs		50

struct AudioStreamPrivate
{
	CFRuntimeBase					base;					// CF type info. Must be first.
	Boolean							inputEnabled;			// True if to read data from a microphone.
	HWAVEIN							inputHandle;			// Handle for recording audio.
	Boolean							outputEnabled;			// True if to read data from a microphone.
	HWAVEOUT						outputHandle;			// Handle for playing audio.
	WAVEHDR							inputBuffers[ 2 ];		// Buffers input.
	WAVEHDR							outputBuffers[ 2 ];		// Buffers output.
	uint32_t						inputSampleTime;		// Current number of samples processed for input.
	uint32_t						outputSampleTime;		// Current number of samples played.
	Boolean							prepared;				// True if AudioStreamPrepare has been called (and stop hasn't yet).
	Boolean							stop;					// True if audio should stop.
	
	dispatch_source_t				rampTimer;				// Timer for ramping volume.
	double							rampCurrentVolume;		// Current volume during ramping.
	double							rampFinalVolume;		// Final volume to ramp to.
	double							rampGain;				// Volume to add at each step.
	int								rampCurrentStep;		// Current step in the ramping process.
	int								rampTotalSteps;			// Total number of steps in the ramping process.
	
	AudioStreamInputCallback_f		inputCallbackPtr;		// Function to call to write input audio.
	void *							inputCallbackCtx;		// Context to pass to audio input callback function.
	AudioStreamOutputCallback_f		outputCallbackPtr;		// Function to call to read audio to output.
	void *							outputCallbackCtx;		// Context to pass to audio output callback function.
	
	CFStringRef						audioType;				// Type of audio content. See kAudioStreamAudioType_*.
	AudioStreamBasicDescription		format;					// Format of the audio data.
	uint32_t						preferredLatencyMics;	// Max latency the app can tolerate.
};

static void		_AudioStreamGetTypeID( void *inContext );
static void		_AudioStreamFinalize( CFTypeRef inCF );
static double	_AudioStreamGetVolume( AudioStreamRef me, OSStatus *outErr );
static OSStatus	_AudioStreamSetVolume( AudioStreamRef me, double inVolume );
static void		_AudioStreamRampVolumeTimer( void *inContext );
static void		_AudioStreamRampVolumeCanceled( void *inContext );
static void CALLBACK
	_AudioStreamInputCallback( 
		HWAVEIN		inHandle, 
		UINT		inMsg, 
		DWORD_PTR	inContext, 
		DWORD_PTR	inParam1, 
		DWORD_PTR	inParam2 );
static void CALLBACK
	_AudioStreamOutputCallback(
		HWAVEOUT	inHandle,
		UINT		inMsg,
		DWORD_PTR	inContext,
		DWORD_PTR	inParam1,
		DWORD_PTR	inParam2 );

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
//	Logging
//===========================================================================================================================

ulog_define( AudioStream, kLogLevelTrace, kLogFlags_Default, "AudioStream", NULL );
#define as_dlog( LEVEL, ... )		dlogc( &log_category_from_name( AudioStream ), (LEVEL), __VA_ARGS__ )
#define as_ulog( LEVEL, ... )		ulog( &log_category_from_name( AudioStream ), (LEVEL), __VA_ARGS__ )

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
	
	me->outputEnabled = true;

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
	
	check( !me->inputHandle );
	check( !me->outputHandle );
	check( !me->rampTimer );
}

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void	AudioStreamSetInputCallback( AudioStreamRef me, AudioStreamInputCallback_f inFunc, void *inContext )
{
	me->inputCallbackPtr = inFunc;
	me->inputCallbackCtx = inContext;
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void	AudioStreamSetOutputCallback( AudioStreamRef me, AudioStreamOutputCallback_f inFunc, void *inContext )
{
	me->outputCallbackPtr = inFunc;
	me->outputCallbackCtx = inContext;
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef	_AudioStreamCopyProperty( CFTypeRef inObject, CFStringRef inProperty, OSStatus *outErr )
{
	AudioStreamRef const		me		= (AudioStreamRef) inObject;
	CFTypeRef					value	= NULL;
	OSStatus					err;
	double						d;
	
	if( 0 ) {}
	
	// AudioType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		value = me->audioType;
		require_action_quiet( value, exit, err = kNotFoundErr );
		CFRetain( value );
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		value = CFDataCreate( NULL, (const uint8_t *) &me->format, (CFIndex) sizeof( me->format ) );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Input

	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
		value = me->inputEnabled ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )
	{
		value = CFNumberCreateInt64( ( 2 * kDefaultLatencyMs ) * 1000 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		value = CFNumberCreateInt64( me->preferredLatencyMics );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Volume

	else if( CFEqual( inProperty, kAudioStreamProperty_Volume ) )
	{
		d = _AudioStreamGetVolume( me, &err );
		require_noerr( err, exit );
		
		value = CFNumberCreate( NULL, kCFNumberDoubleType, &d );
		require_action( value, exit, err = kUnknownErr );
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

OSStatus	_AudioStreamSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue )
{
	AudioStreamRef const		me = (AudioStreamRef) inObject;
	OSStatus					err;
	double						d;
	
	if( 0 ) {}
	
	// AudioType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		require_action( CFIsType( inValue, CFString ), exit, err = kTypeErr );
		ReplaceCF( &me->audioType, inValue );
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
		me->inputEnabled = CFGetBoolean( inValue, NULL );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		me->preferredLatencyMics = CFGetUInt32( inValue, &err );
		require_noerr( err, exit );
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		// We don't use a custom thread nor do we have control over the threads used by CoreAudio so do nothing here.
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		// We don't use a custom thread nor do we have control over the threads used by CoreAudio so do nothing here.
	}
	
	// Volume

	else if( CFEqual( inProperty, kAudioStreamProperty_Volume ) )
	{
		d = CFGetDouble( inValue, &err );
		require_noerr( err, exit );
		
		err = _AudioStreamSetVolume( me, d );
		require_noerr( err, exit );
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
//	_AudioStreamGetVolume
//===========================================================================================================================

static double	_AudioStreamGetVolume( AudioStreamRef me, OSStatus *outErr )
{
	double			volume = 0;
	OSStatus		err;
	DWORD			tmp;
	
	require_action( me->outputHandle, exit, err = kNotPreparedErr );
	err = waveOutGetVolume( me->outputHandle, &tmp );
	require_noerr( err, exit );
	
	volume = ( (double)( tmp & 0xFFFF ) ) / 0xFFFF;
	
exit:
	if( outErr ) *outErr = err;
	return( volume );
}

//===========================================================================================================================
//	_AudioStreamSetVolume
//===========================================================================================================================

static OSStatus	_AudioStreamSetVolume( AudioStreamRef me, double inVolume )
{
	OSStatus		err;
	DWORD			volume;
	
	require_action( me->outputHandle, exit, err = kNotPreparedErr );
	volume = (DWORD)( inVolume * 0xFFFF );	// Scale 0.0-1.0 to 0-0xFFFF for left volume.
	volume |= ( volume << 16 );				// Copy left volume to right volume so they are the same.
	err = waveOutSetVolume( me->outputHandle, volume );
	require_noerr( err, exit );
	
exit:
	return( err );
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
	OSStatus		err;
	uint64_t		nanos;
	
	require_action( me->outputHandle, exit, err = kNotInUseErr );
	dispatch_source_forget( &me->rampTimer );
	
	me->rampCurrentVolume	= _AudioStreamGetVolume( me, NULL );
	me->rampFinalVolume		= inFinalVolume;
	me->rampGain			= ( inFinalVolume - me->rampCurrentVolume ) / 16;
	me->rampCurrentStep		= 0;
	me->rampTotalSteps		= 16;
	nanos					= (uint64_t)( ( inDurationSecs * kNanosecondsPerSecond ) / 16 );
	as_dlog( kLogLevelTrace, "Ramping volume from %f to %f in %d steps over %f seconds\n", 
		me->rampCurrentVolume, inFinalVolume, 16, inDurationSecs );
	
	me->rampTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inQueue ? inQueue : dispatch_get_main_queue() );
	require_action( me->rampTimer, exit, err = kUnknownErr );
	CFRetain( me );
	dispatch_set_context( me->rampTimer, me );
	dispatch_source_set_event_handler_f( me->rampTimer, _AudioStreamRampVolumeTimer );
	dispatch_source_set_cancel_handler_f( me->rampTimer, _AudioStreamRampVolumeCanceled );
	dispatch_source_set_timer( me->rampTimer, dispatch_time( DISPATCH_TIME_NOW, nanos ), nanos, 5 * kNanosecondsPerMillisecond );
	dispatch_resume( me->rampTimer );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioStreamRampVolumeTimer
//===========================================================================================================================

static void	_AudioStreamRampVolumeTimer( void *inContext )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	Boolean						done;
	
	me->rampCurrentVolume += me->rampGain;
	done = ( ( ++me->rampCurrentStep >= me->rampTotalSteps ) || ( me->rampCurrentVolume == me->rampFinalVolume ) );
	_AudioStreamSetVolume( me, done ? me->rampFinalVolume : me->rampCurrentVolume );
	as_dlog( kLogLevelVerbose, "Ramping volume at %d of %d, volume %f of %f\n", 
		me->rampCurrentStep, me->rampTotalSteps, me->rampCurrentVolume, me->rampFinalVolume );
	if( done )
	{
		as_dlog( kLogLevelTrace, "Ramped volume to %f\n", me->rampFinalVolume );
		dispatch_source_forget( &me->rampTimer );
	}
}

//===========================================================================================================================
//	_AudioStreamRampVolumeCanceled
//===========================================================================================================================

static void	_AudioStreamRampVolumeCanceled( void *inContext )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	
	if( ( me->rampCurrentStep != me->rampTotalSteps ) && ( me->rampCurrentVolume != me->rampFinalVolume ) )
	{
		as_dlog( kLogLevelTrace, "Ramp volume canceled at %d of %d, volume %f of %f\n", 
			me->rampCurrentStep, me->rampTotalSteps, me->rampCurrentVolume, me->rampFinalVolume );
	}
	CFRelease( me );
}

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus	AudioStreamPrepare( AudioStreamRef me )
{
	OSStatus			err;
	WAVEFORMATEX		format;
	size_t				len;
	WAVEHDR *			hdr;
	
	// Open the audio device.
	
	memset( &format, 0, sizeof( format ) );
	format.wFormatTag		= WAVE_FORMAT_PCM;
	format.nChannels		= me->format.mChannelsPerFrame;
	format.nSamplesPerSec	= me->format.mSampleRate;
	format.nAvgBytesPerSec	= me->format.mSampleRate * me->format.mBytesPerPacket;
	format.nBlockAlign		= me->format.mBytesPerPacket;
	format.wBitsPerSample	= me->format.mBitsPerChannel;
	format.cbSize			= 0;
	if( me->inputEnabled )
	{
		err = waveInOpen( &me->inputHandle, WAVE_MAPPER, &format, (DWORD_PTR) _AudioStreamInputCallback, (DWORD_PTR) me, 
			CALLBACK_FUNCTION );
		require_noerr( err, exit );
	}
	if( me->outputEnabled )
	{
		err = waveOutOpen( &me->outputHandle, WAVE_MAPPER, &format, (DWORD_PTR) _AudioStreamOutputCallback, (DWORD_PTR) me, 
			CALLBACK_FUNCTION | WAVE_ALLOWSYNC );
		require_noerr( err, exit );
	}
	
	// Set up buffers.
	
	len = ( me->format.mSampleRate * kDefaultLatencyMs * me->format.mBytesPerPacket ) / 1000;
	if( me->inputHandle )
	{
		hdr = &me->inputBuffers[ 0 ];
		hdr->lpData = (LPSTR) calloc( 1, len );
		require_action( hdr->lpData, exit, err = kNoMemoryErr );
		hdr->dwBufferLength = (DWORD) len;
		err = waveInPrepareHeader( me->inputHandle, hdr, sizeof( *hdr ) );
		require_noerr( err, exit );
	
		hdr = &me->inputBuffers[ 1 ];
		hdr->lpData = (LPSTR) calloc( 1, len );
		require_action( hdr->lpData, exit, err = kNoMemoryErr );
		hdr->dwBufferLength = (DWORD) len;
		err = waveInPrepareHeader( me->inputHandle, hdr, sizeof( *hdr ) );
		require_noerr( err, exit );
	}
	if( me->outputHandle )
	{
		hdr = &me->outputBuffers[ 0 ];
		hdr->lpData = (LPSTR) calloc( 1, len );
		require_action( hdr->lpData, exit, err = kNoMemoryErr );
		hdr->dwBufferLength = (DWORD) len;
		err = waveOutPrepareHeader( me->outputHandle, hdr, sizeof( *hdr ) );
		require_noerr( err, exit );
	
		hdr = &me->outputBuffers[ 1 ];
		hdr->lpData = (LPSTR) calloc( 1, len );
		require_action( hdr->lpData, exit, err = kNoMemoryErr );
		hdr->dwBufferLength = (DWORD) len;
		err = waveOutPrepareHeader( me->outputHandle, hdr, sizeof( *hdr ) );
		require_noerr( err, exit );
	}
	me->prepared = true;
	err = kNoErr;
	
exit:
	if( err ) AudioStreamStop( me, false );
	return( err );
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef me )
{
	OSStatus		err;
	
	if( !me->prepared )
	{
		err = AudioStreamPrepare( me );
		require_noerr( err, exit );
	}
	me->stop = false;
	me->inputSampleTime = 0;
	me->outputSampleTime = 0;
	
	if( me->inputHandle )
	{
		err = waveInAddBuffer( me->inputHandle, &me->inputBuffers[ 0 ], sizeof( me->inputBuffers[ 0 ] ) );
		require_noerr( err, exit );
		
		err = waveInAddBuffer( me->inputHandle, &me->inputBuffers[ 1 ], sizeof( me->inputBuffers[ 1 ] ) );
		require_noerr( err, exit );
		
		err = waveInStart( me->inputHandle );
		require_noerr( err, exit );
	}
	if( me->outputHandle )
	{
		// Write silence to start the audio device. Completions will re-fill with real data.
		
		err = waveOutWrite( me->outputHandle, &me->outputBuffers[ 0 ], sizeof( me->outputBuffers[ 0 ] ) );
		require_noerr( err, exit );
		
		err = waveOutWrite( me->outputHandle, &me->outputBuffers[ 1 ], sizeof( me->outputBuffers[ 1 ] ) );
		require_noerr( err, exit );
	}
	
exit:
	if( err ) AudioStreamStop( me, false );
	return( err);
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void	AudioStreamStop( AudioStreamRef me, Boolean inDrain )
{
	OSStatus		err;
	WAVEHDR *		hdr;
	
	DEBUG_USE_ONLY( err );

	me->stop = true;
	if( me->inputHandle )
	{
		err = waveInReset( me->inputHandle );
		check_noerr( err );
		
		hdr = &me->inputBuffers[ 0 ];
		if( hdr->dwFlags & WHDR_PREPARED )
		{
			err = waveInUnprepareHeader( me->inputHandle, hdr, sizeof( *hdr ) );
			check_noerr( err );
		}
		ForgetMem( &hdr->lpData );
		
		hdr = &me->inputBuffers[ 1 ];
		if( hdr->dwFlags & WHDR_PREPARED )
		{
			err = waveInUnprepareHeader( me->inputHandle, hdr, sizeof( *hdr ) );
			check_noerr( err );
		}
		ForgetMem( &hdr->lpData );
		
		err = waveInClose( me->inputHandle );
		check_noerr( err );
		me->inputHandle = NULL;
	}
	if( me->outputHandle )
	{
		err = waveOutReset( me->outputHandle );
		check_noerr( err );
		
		hdr = &me->outputBuffers[ 0 ];
		if( hdr->dwFlags & WHDR_PREPARED )
		{
			err = waveOutUnprepareHeader( me->outputHandle, hdr, sizeof( *hdr ) );
			check_noerr( err );
		}
		ForgetMem( &hdr->lpData );
		
		hdr = &me->outputBuffers[ 1 ];
		if( hdr->dwFlags & WHDR_PREPARED )
		{
			err = waveOutUnprepareHeader( me->outputHandle, hdr, sizeof( *hdr ) );
			check_noerr( err );
		}
		ForgetMem( &hdr->lpData );
		
		err = waveOutClose( me->outputHandle );
		check_noerr( err );
		me->outputHandle = NULL;
	}
	me->prepared = false;
}

//===========================================================================================================================
//	_AudioStreamInputCallback
//===========================================================================================================================

static void CALLBACK
	_AudioStreamInputCallback( 
		HWAVEIN		inHandle, 
		UINT		inMsg, 
		DWORD_PTR	inContext, 
		DWORD_PTR	inParam1, 
		DWORD_PTR	inParam2 )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	WAVEHDR * const				hdr = (WAVEHDR *) inParam1;
	OSStatus					err;
	
	(void) inParam2;
	
	if( ( inMsg != WIM_DATA ) || me->stop ) return;
	
	me->inputCallbackPtr( me->inputSampleTime, UpTicks(), hdr->lpData, hdr->dwBufferLength, me->inputCallbackCtx );
	me->inputSampleTime += ( hdr->dwBufferLength / me->format.mBytesPerFrame );
	
	err = waveInAddBuffer( me->inputHandle, hdr, sizeof( *hdr ) );
	check_noerr( err );
}

//===========================================================================================================================
//	_AudioStreamOutputCallback
//===========================================================================================================================

static void CALLBACK
	_AudioStreamOutputCallback(
		HWAVEOUT	inHandle,
		UINT		inMsg,
		DWORD_PTR	inContext,
		DWORD_PTR	inParam1,
		DWORD_PTR	inParam2 )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	WAVEHDR * const				hdr = (WAVEHDR *) inParam1;
	OSStatus					err;
	
	(void) inParam2;
	
	if( ( inMsg != WOM_DONE ) || me->stop ) return;
	
	me->outputCallbackPtr( me->outputSampleTime, UpTicks(), hdr->lpData, hdr->dwBufferLength, me->outputCallbackCtx );
	me->outputSampleTime += ( hdr->dwBufferLength / me->format.mBytesPerFrame );
	
	err = waveOutWrite( inHandle, hdr, sizeof( *hdr ) );
	check_noerr( err );
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
