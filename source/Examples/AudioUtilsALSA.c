/*
	File:    	AudioUtilsALSA.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	n/a
	
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
	
	Copyright (C) 2012-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MathUtils.h"
#include "TickUtils.h"
#include "ThreadUtils.h"

#include "AudioUtils.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_LINUX )
	#include <alsa/asoundlib.h>
	#include "ALSAMixer.h"
#endif
#if( TARGET_OS_QNX )
	#include <sys/asoundlib.h>
#endif

#if( !AUDIO_STREAM_DLL )
	#include CF_RUNTIME_HEADER
#endif

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

#define kDefaultLatencyMics 10000

#define CAPTURE_AUDIO 0
#define LOG_TIME_STAMPS 0

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
	void *							delegateContext;		// Context for the session delegate
	pthread_t						outputThread;			// Thread processing audio output.
	pthread_t						inputThread;			// Thread processing audio input.
	snd_pcm_t *						inputPCMHandle;			// Handle to the audio engine for reading PCM input.
	snd_pcm_t *						outputPCMHandle;		// Handle to the audio engine for writing PCM output.
	Boolean							prepared;				// True if AudioStreamPrepare has been called (and stop hasn't yet).
	Boolean							stop;					// True if audio should stop.
#if( TARGET_OS_QNX )
	int								inputFD;				// Weak reference to file descriptor for input.  Closed by libasound.
	int								outputFD;				// Weak reference to file descriptor for output. Closed by libasound.
	int								mixerFD;				// Weak reference to file descriptor for mixer.  Closed by libasound.
	int								mixerFDIndex;			// Index of the mixer poll entry.
	snd_mixer_t *					mixerHandle;			// Handle to the audio mixer.
	snd_mixer_group_t				mixerGroup;				// Mixer info.
#endif
	uint8_t *						inputBuffer;			// Buffer between the input callback and the OS audio stack.
	size_t							inputMaxLen;			// Number of bytes the input buffer can hold.
	uint8_t *						outputBuffer;			// Buffer between the output callback and the OS audio stack.
	size_t							outputMaxLen;			// Number of bytes the output buffer can hold.
	uint32_t						inputSampleTime;		// Current number of samples processed for input.
	uint32_t						outputSampleTime;		// Current number of samples processed for input.
	
	AudioStreamInputCallback_f		inputCallbackPtr;		// Function to call to write input audio.
	void *							inputCallbackCtx;		// Context to pass to audio input callback function.
	AudioStreamOutputCallback_f		outputCallbackPtr;		// Function to call to read audio to output.
	void *							outputCallbackCtx;		// Context to pass to audio output callback function.
	Boolean							input;					// Enable audio input.
	AudioStreamBasicDescription		format;					// Format of the audio data.
	AudioStreamType					streamType;
	uint32_t						preferredLatencyMics;	// Max latency the app can tolerate.
	char *							threadName;				// Name to use when creating threads.
	int								threadPriority;			// Priority to run threads.
	Boolean							hasThreadPriority;		// True if a thread priority has been set.
#if( CAPTURE_AUDIO )
	FILE*							audioOutCaptureFile;
#endif
#if( LOG_TIME_STAMPS )
	FILE*                           inputStampsFile;
	FILE*                           outputStampsFile;
#endif
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
//static uint32_t		_AudioStreamGetLatency( AudioStreamImpRef me, OSStatus *outErr );
//static uint32_t		_AudioStreamGetInputLatency( AudioStreamImpRef me, OSStatus *outErr );
//static uint32_t		_AudioStreamGetOutputLatency( AudioStreamImpRef me, OSStatus *outErr );
static void *		_AudioStreamInputThread( void *inArg );
static void *		_AudioStreamOutputThread( void *inArg );

static int			_AudioStreamConfigure( snd_pcm_t * handle, snd_pcm_format_t format, AudioStreamBasicDescription* absd );

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

#if( TARGET_OS_QNX )
// Use static null callbacks for implicit init to zero. Linux vs QNX have different members so can't explicitly init.
static snd_mixer_callbacks_t		kAudioMixerNullCallbacks;
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
	
	check( !me->inputPCMHandle );
	check( !me->outputPCMHandle );
	check( !me->inputBuffer );
	check( !me->outputBuffer );
	ForgetMem( &me->threadName );
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
	
	check( !me->inputPCMHandle );
	check( !me->outputPCMHandle );
	check( !me->inputBuffer );
	check( !me->outputBuffer );
#if( CAPTURE_AUDIO )
	if (me->audioOutCaptureFile)
		fclose(me->audioOutCaptureFile);
#endif
#if( LOG_TIME_STAMPS )
	if (me->inputStampsFile)
		fclose(me->inputStampsFile);
	if (me->outputStampsFile)
		fclose(me->outputStampsFile);
#endif
	ForgetMem( &me->threadName );
	free( me );
	LogCategory_Remove( &log_category_from_name( AudioStream ) );
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
		// $$$ TODO: Actual head-unit implementations can use the audio type to enable certain types of audio processing.
		err = kNotHandledErr;
		goto exit;
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
	
	/*// Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )
	{
		s64 = _AudioStreamGetLatency( me, &err );
		require_noerr( err, exit );
		
		value = CFNumberCreateInt64( s64 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Input Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )//TODO: needs new property
	{
		s64 = _AudioStreamGetInputLatency( me, &err );
		require_noerr( err, exit );
		
		value = CFNumberCreateInt64( s64 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Output Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )//TODO: needs new property
	{
		s64 = _AudioStreamGetOutputLatency( me, &err );
		require_noerr( err, exit );
		
		value = CFNumberCreateInt64( s64 );
		require_action( value, exit, err = kNoMemoryErr );
	}*/
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		value = CFNumberCreateInt64( me->preferredLatencyMics );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		value = CFStringCreateWithCString( NULL, me->threadName ? me->threadName : "", kCFStringEncodingUTF8 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		value = CFNumberCreateInt64( me->threadPriority );
		require_action( value, exit, err = kNoMemoryErr );
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
	char *						cstr;
	double						d;
	
	// Properties may only be set before AudioStreamPrepare is called.
	
	require_action( !me->prepared, exit, err = kStateErr );
	
	if( 0 ) {}
	
	// AudioType

	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		// $$$ TODO: Actual head-unit implementations can use the audio type to enable certain types of audio processing.
#if( DEBUG )
		char stringBuf[64];
		as_dlog( kLogLevelTrace, "AudioType: %s\n", CFGetCString( inValue, stringBuf, sizeof(stringBuf) ) );
#endif
	}
	
	// StreamType
	else if ( CFEqual( inProperty, kAudioStreamProperty_StreamType ) )
	{
		me->streamType = CFGetInt64( inValue, &err );
		as_dlog( kLogLevelTrace, "StreamType: %d\n", me->streamType );
		require_noerr( err, exit );
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		//Buffer sizes are dependent on the format, make sure it can't be changed after Prepare
		if ( me->prepared ) {
			err = kAlreadyInitializedErr;
			goto exit;
		}
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
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		cstr = CFCopyCString( inValue, &err );
		require_noerr( err, exit );
		
		if( me->threadName ) free( me->threadName );
		me->threadName = cstr;
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		me->threadPriority = (int) CFGetInt64( inValue, &err );
		me->hasThreadPriority = ( err == kNoErr );
	}
	
	// Vocoder sample rate
	else if( CFEqual( inProperty, CFSTR( "vocoderSampleRate" ) ) )
	{
		d = CFGetDouble( inValue, &err );
		as_dlog( kLogLevelTrace, "VocoderSampleRate: %f\n", d );
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
	AudioStreamImpRef const me = _AudioStreamGetImp( inStream );
	me->format = *inFormat;
}

//===========================================================================================================================
//	AudioStreamSetDelegateContext
//===========================================================================================================================

void AudioStreamSetDelegateContext( AudioStreamRef inStream, void* inContext )
{
	AudioStreamImpRef const me = _AudioStreamGetImp( inStream );
	me->delegateContext = inContext;
}

//===========================================================================================================================
//	_AudioStreamGetLatency
//===========================================================================================================================

/*#if( TARGET_OS_LINUX )
static uint32_t	_AudioStreamGetLatency( AudioStreamImpRef me, OSStatus *outErr )
{
	uint32_t					result = 0;
	OSStatus					err;
	snd_pcm_uframes_t			bufferSize, periodSize;
    snd_pcm_sframes_t           delay;
	
	require_action( me->outputPCMHandle, exit, err = kNotPreparedErr );
	
	err = snd_pcm_get_params( me->outputPCMHandle, &bufferSize, &periodSize );
	require_noerr( err, exit );

    err = snd_pcm_delay( me->outputPCMHandle , &delay );
	require_noerr( err, exit );
    result = (uint64_t)(delay + periodSize) * kMicrosecondsPerSecond / me->format.mSampleRate;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

static uint32_t	_AudioStreamGetInputLatency( AudioStreamImpRef me, OSStatus *outErr )
{
	uint32_t					result = 0;
	OSStatus					err;
	snd_pcm_uframes_t			bufferSize, periodSize;
    snd_pcm_sframes_t           delay;
	
	require_action( me->inputPCMHandle, exit, err = kNotPreparedErr );
	
	err = snd_pcm_get_params( me->inputPCMHandle, &bufferSize, &periodSize );
	require_noerr( err, exit );
	
    err = snd_pcm_delay( me->inputPCMHandle , &delay );
	require_noerr( err, exit );
    result = (uint64_t)(delay + periodSize) * kMicrosecondsPerSecond / me->format.mSampleRate;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

static uint32_t	_AudioStreamGetOutputLatency( AudioStreamImpRef me, OSStatus *outErr )
{
	uint32_t					result = 0;
	OSStatus					err;
	snd_pcm_uframes_t			bufferSize, periodSize;
    snd_pcm_sframes_t           delay;
	
	require_action( me->outputPCMHandle, exit, err = kNotPreparedErr );
	
	err = snd_pcm_get_params( me->outputPCMHandle, &bufferSize, &periodSize );
	require_noerr( err, exit );
	
    err = snd_pcm_delay( me->outputPCMHandle , &delay );
	require_noerr( err, exit );
    result = (uint64_t)(delay + periodSize) * kMicrosecondsPerSecond / me->format.mSampleRate;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}
#endif

#if( TARGET_OS_QNX )
static uint32_t	_AudioStreamGetLatency( AudioStreamImpRef me, OSStatus *outErr )
{
	uint32_t					result = 0;
	OSStatus					err;
	snd_pcm_channel_setup_t		info;
	
	require_action( me->outputPCMHandle, exit, err = kNotPreparedErr );
	
	memset( &info, 0, sizeof( info ) );
	err = snd_pcm_plugin_setup( me->outputPCMHandle, &info );
	require_noerr( err, exit );
	
	result = info.buf.block.frag_size * info.buf.block.frags_max;
	result = (uint32_t)( ( ( (uint64_t) result ) * kMicrosecondsPerSecond ) / me->format.mSampleRate );
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}
#endif*/

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

#if( TARGET_OS_LINUX )

int _AudioStreamConfigure( snd_pcm_t * handle, snd_pcm_format_t format, AudioStreamBasicDescription* absd ) {
	int err;
	snd_pcm_hw_params_t *params;
	//unsigned int val;
	//snd_pcm_uframes_t size;
	//int dir;
	//printf("Name: %s\n", snd_pcm_name(handle));
	err = snd_pcm_hw_params_malloc(&params);
	if (err == 0) {
		err = snd_pcm_hw_params_any( handle, params);
		if (err == 0) {
			err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
			//if (err)
				//printf("snd_pcm_hw_params_set_access %d\n", err);
			require_noerr( err, exit );
			
			err = snd_pcm_hw_params_set_format(handle, params, format);
			//if (err)
				//printf("snd_pcm_hw_params_set_format %d\n", err);
			require_noerr( err, exit );
			
			err = snd_pcm_hw_params_set_channels(handle, params, absd->mChannelsPerFrame);
			//if (err)
				//printf("snd_pcm_hw_params_set_channels %d\n", err);
			require_noerr( err, exit );

			err = snd_pcm_hw_params_set_rate(handle, params, absd->mSampleRate, 0);
			require_noerr( err, exit );
			//10ms buffer time gives exact buffer sizes for all sample rates we support 480, 441, 320, 240, 160, 80
			//This matters for ALSA SRC, which will round buffer sizes and adjust the effective sample rate to match
			err = snd_pcm_hw_params_set_period_size(handle, params, absd->mSampleRate / 100, 0);
			if (err)
				printf("snd_pcm_hw_params_set_period_size %d\n", err);
			err = snd_pcm_hw_params_set_periods(handle, params, 8, 0);
			//if (err)
				//printf("snd_pcm_hw_params_set_periods %d\n", err);
			err = snd_pcm_hw_params(handle, params);
			if (err)
				as_ulog( kLogLevelNotice, "snd_pcm_hw_params %d\n", err);
		}
exit:
		snd_pcm_hw_params_free(params);
	}
	return err;
}

OSStatus	AudioStreamPrepare( AudioStreamRef inStream )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	OSStatus					err;
	Boolean						be, sign;
	snd_pcm_format_t			format;
	snd_pcm_uframes_t			bufferSize, periodSize;
	
	// Convert the ASBD to snd format.
	
	require_action( me->format.mFormatID == kAudioFormatLinearPCM, exit, err = kFormatErr );
	be   = ( me->format.mFormatFlags & kAudioFormatFlagIsBigEndian )		? true : false;
	sign = ( me->format.mFormatFlags & kAudioFormatFlagIsSignedInteger )	? true : false;
	if( me->format.mBitsPerChannel == 8 )
	{
		if( sign )			format = SND_PCM_FORMAT_S8;
		else				format = SND_PCM_FORMAT_U8;
	}
	else if( me->format.mBitsPerChannel == 16 )
	{
		if( be && sign )	format = SND_PCM_FORMAT_S16_BE;
		else if( be )		format = SND_PCM_FORMAT_U16_BE;
		else if( sign )		format = SND_PCM_FORMAT_S16_LE;
		else				format = SND_PCM_FORMAT_U16_LE;
	}
	else if( me->format.mBitsPerChannel == 24 )
	{
		if( be && sign )	format = SND_PCM_FORMAT_S24_BE;
		else if( be )		format = SND_PCM_FORMAT_U24_BE;
		else if( sign )		format = SND_PCM_FORMAT_S24_LE;
		else				format = SND_PCM_FORMAT_U24_LE;
	}
	else
	{
		dlogassert( "Unsupported audio format: %{asbd}", &me->format );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Open the OS audio engine and configure it.
		
	if( me->input )
	{
		err = snd_pcm_open( &me->inputPCMHandle, "micinput", SND_PCM_STREAM_CAPTURE, 0 );//SND_PCM_NONBLOCK
		require_noerr_quiet( err, exit );
		as_ulog( kLogLevelNotice, "Input name: %s\n", snd_pcm_name(me->inputPCMHandle));
		
		err = _AudioStreamConfigure(me->inputPCMHandle, format, &me->format );
		require_noerr( err, exit );
		
		err = snd_pcm_prepare( me->inputPCMHandle );
		require_noerr( err, exit );
		
		err = snd_pcm_get_params( me->inputPCMHandle, &bufferSize, &periodSize );
		require_noerr( err, exit );
		as_ulog( kLogLevelNotice, "Input buffer size %lu period size %lu\n", bufferSize, periodSize);
		if( bufferSize <= 0 ) bufferSize = 2048;
		
		me->inputMaxLen = bufferSize * me->format.mBytesPerFrame ;
		me->inputBuffer = (uint8_t *) malloc( me->inputMaxLen );
		require_action( me->inputBuffer, exit, err = kNoMemoryErr );
		
#if( LOG_TIME_STAMPS )
		time_t timeSecs;
		struct tm timeComps;
		time(&timeSecs);
		localtime_r(&timeSecs, &timeComps);
		char path[256];
		snprintf(path, sizeof(path), "/tmp/input-stamps-%04d-%02d-%02d %02d:%02d:%02d.txt", timeComps.tm_year + 1900, timeComps.tm_mon, timeComps.tm_mday, timeComps.tm_hour, timeComps.tm_min, timeComps.tm_sec);
		me->inputStampsFile = fopen(path, "w");
		setlinebuf(me->inputStampsFile);
#endif
	}
	
	// Configure output.

	err = snd_pcm_open( &me->outputPCMHandle, me->streamType == kAudioStreamType_AltAudio ? "altaudio" : "mainaudio", SND_PCM_STREAM_PLAYBACK, 0 );//SND_PCM_NONBLOCK
	require_noerr_quiet( err, exit );
	as_ulog( kLogLevelNotice, "Output name: %s\n", snd_pcm_name(me->outputPCMHandle));
	
	err = _AudioStreamConfigure( me->outputPCMHandle, format, &me->format );
	require_noerr( err, exit );
	
	err = snd_pcm_prepare( me->outputPCMHandle );
	require_noerr( err, exit );
	
	err = snd_pcm_get_params( me->outputPCMHandle, &bufferSize, &periodSize );
	require_noerr( err, exit );
	as_ulog( kLogLevelNotice, "Output buffer size %lu period size %lu\n", bufferSize, periodSize);
	if( periodSize <= 0 ) periodSize = 512;
	
	me->outputMaxLen = periodSize * me->format.mBytesPerFrame;
	me->outputBuffer = (uint8_t *) malloc( me->outputMaxLen );
	require_action( me->outputBuffer, exit, err = kNoMemoryErr );

	me->prepared = true;
	err = kNoErr;

#if( CAPTURE_AUDIO || LOG_TIME_STAMPS)
	time_t timeSecs;
	struct tm timeComps;
	time(&timeSecs);
	localtime_r(&timeSecs, &timeComps);
	char path[256];
#endif
#if( CAPTURE_AUDIO )
	snprintf(path, sizeof(path), "/tmp/audio-out-%04d-%02d-%02d %02d:%02d:%02d.raw", timeComps.tm_year + 1900, timeComps.tm_mon, timeComps.tm_mday, timeComps.tm_hour, timeComps.tm_min, timeComps.tm_sec);
	me->audioOutCaptureFile = fopen(path, "w");
#endif
#if( LOG_TIME_STAMPS )
	snprintf(path, sizeof(path), "/tmp/output-stamps-%04d-%02d-%02d %02d:%02d:%02d.txt", timeComps.tm_year + 1900, timeComps.tm_mon, timeComps.tm_mday, timeComps.tm_hour, timeComps.tm_min, timeComps.tm_sec);
	me->outputStampsFile = fopen(path, "w");
	setlinebuf(me->outputStampsFile);
#endif

exit:
	if( err ) AudioStreamStop( inStream, false );
	return( err );
}
#endif

#if( TARGET_OS_QNX )
static OSStatus
	_AudioStreamPreparePlugInParams( 
		AudioStreamImpRef				me, 
		const snd_pcm_channel_info_t *	inInfo, 
		snd_pcm_channel_params_t *		outParams )
{
	OSStatus		err = kNoErr;
	Boolean			be, sign;
	
	memset( outParams, 0, sizeof( *outParams ) );
	outParams->channel	= inInfo->channel;
	outParams->mode		= SND_PCM_MODE_BLOCK;
	
	require_action( me->format.mFormatID == kAudioFormatLinearPCM, exit, err = kFormatErr );
	be   = ( me->format.mFormatFlags & kAudioFormatFlagIsBigEndian )		? true : false;
	sign = ( me->format.mFormatFlags & kAudioFormatFlagIsSignedInteger )	? true : false;
	if( me->format.mBitsPerChannel == 8 )
	{
		if( sign )			outParams->format.format = SND_PCM_SFMT_S8;
		else				outParams->format.format = SND_PCM_SFMT_U8;
	}
	else if( me->format.mBitsPerChannel == 16 )
	{
		if( be && sign )	outParams->format.format = SND_PCM_SFMT_S16_BE;
		else if( be )		outParams->format.format = SND_PCM_SFMT_U16_BE;
		else if( sign )		outParams->format.format = SND_PCM_SFMT_S16_LE;
		else				outParams->format.format = SND_PCM_SFMT_U16_LE;
	}
	else if( me->format.mBitsPerChannel == 24 )
	{
		if( be && sign )	outParams->format.format = SND_PCM_SFMT_S24_BE;
		else if( be )		outParams->format.format = SND_PCM_SFMT_U24_BE;
		else if( sign )		outParams->format.format = SND_PCM_SFMT_S24_LE;
		else				outParams->format.format = SND_PCM_SFMT_U24_LE;
	}
	else
	{
		dlogassert( "Unsupported audio format: %{asbd}", &me->format );
		err = kUnsupportedErr;
		goto exit;
	}
	outParams->format.interleave	= 1;
	outParams->format.rate			= (int32_t) me->format.mSampleRate;
	outParams->format.voices		= me->format.mChannelsPerFrame;
	outParams->start_mode			= SND_PCM_START_FULL;
	outParams->stop_mode			= SND_PCM_STOP_ROLLOVER;
	outParams->buf.block.frag_size	= inInfo->max_fragment_size;
	outParams->buf.block.frags_max	= 1;
	outParams->buf.block.frags_min	= 1;
	
exit:
	return( err );
}

OSStatus	AudioStreamPrepare( AudioStreamRef inStream )
{
	AudioStreamImpRef const			me = _AudioStreamGetImp( inStream );
	OSStatus						err;
	int								card;
	snd_pcm_channel_info_t			captureInfo, playbackInfo;
	snd_pcm_channel_setup_t			setup;
	
	if( me->input )
	{
		err = snd_pcm_open_preferred( &me->inputPCMHandle, &card, NULL, SND_PCM_OPEN_CAPTURE );
		if( err ) me->inputPCMHandle = NULL; // Workaround the library setting this to a bad pointer.
		require_noerr( err, exit );
		snd_pcm_plugin_set_disable( me->inputPCMHandle, PLUGIN_DISABLE_MMAP );
	
		as_ulog( kLogLevelVerbose, "snd_pcm_open_preferred inputHandle %d, card %d for CAPTURE\n", 
			(int) me->inputPCMHandle, (int) card );

		memset( &captureInfo, 0, sizeof( captureInfo ) );
		captureInfo.channel = SND_PCM_CHANNEL_CAPTURE;
		err = snd_pcm_plugin_info( me->inputPCMHandle, &captureInfo );
		require_noerr( err, exit );

		as_ulog( kLogLevelVerbose, "snd_pcm_plugin_info channel %d, max_fragment_size %d for CAPTURE\n", 
			(int) captureInfo.channel, (int) captureInfo.max_fragment_size );
	}

	// Configure output.

	err = snd_pcm_open_preferred( &me->outputPCMHandle, &card, NULL, SND_PCM_OPEN_PLAYBACK );
	if( err ) me->outputPCMHandle = NULL; // Workaround the library setting this to a bad pointer.
	require_noerr( err, exit );
	snd_pcm_plugin_set_disable( me->outputPCMHandle, PLUGIN_DISABLE_MMAP );

	as_ulog( kLogLevelVerbose, "snd_pcm_open_preferred inputHandle %d, card %d for PLAYBACK\n", 
		(int) me->outputPCMHandle, (int) card );

	memset( &playbackInfo, 0, sizeof( playbackInfo ) );
	playbackInfo.channel = SND_PCM_CHANNEL_PLAYBACK;
	err = snd_pcm_plugin_info( me->outputPCMHandle, &playbackInfo );
	require_noerr( err, exit );
	
	as_ulog( kLogLevelVerbose, "snd_pcm_plugin_info channel %d, max_fragment_size %d for PLAYBACK\n", 
		(int) playbackInfo.channel, (int) playbackInfo.max_fragment_size );
	
	if( me->inputPCMHandle )
	{
		snd_pcm_channel_params_t		params;

		err = _AudioStreamPreparePlugInParams( me, &captureInfo, &params );
		require_noerr( err, exit );

		err = snd_pcm_plugin_params( me->inputPCMHandle, &params );
		require_noerr( err, exit );
		
		err = snd_pcm_plugin_prepare( me->inputPCMHandle, captureInfo.channel );
		require_noerr( err, exit );
		
		memset( &me->mixerGroup, 0, sizeof( me->mixerGroup ) );
		memset( &setup, 0, sizeof( setup ) );
		setup.channel	= captureInfo.channel;
		setup.mixer_gid	= &me->mixerGroup.gid;
		err = snd_pcm_plugin_setup( me->inputPCMHandle, &setup );
		require_noerr( err, exit );
		
		me->inputMaxLen = setup.buf.block.frag_size;
		me->inputBuffer = (uint8_t *) malloc( me->inputMaxLen );
		require_action( me->inputBuffer, exit, err = kNoMemoryErr );

		as_ulog( kLogLevelVerbose, "snd_pcm_plugin_setup mixer_device %d, buf.block.frag_size %d for CAPTURE\n", 
			(int) setup.mixer_device, (int) setup.buf.block.frag_size );
	}
	if( me->outputPCMHandle )
	{
		snd_pcm_channel_params_t		params;

		err = _AudioStreamPreparePlugInParams( me, &playbackInfo, &params );
		require_noerr( err, exit );

		err = snd_pcm_plugin_params( me->outputPCMHandle, &params );
		require_noerr( err, exit );
		
		err = snd_pcm_plugin_prepare( me->outputPCMHandle, playbackInfo.channel );
		require_noerr( err, exit );
		
		memset( &me->mixerGroup, 0, sizeof( me->mixerGroup ) );
		memset( &setup, 0, sizeof( setup ) );
		setup.channel	= playbackInfo.channel;
		setup.mixer_gid	= &me->mixerGroup.gid;
		err = snd_pcm_plugin_setup( me->outputPCMHandle, &setup );
		require_noerr( err, exit );
		
		me->outputMaxLen = setup.buf.block.frag_size;
		me->outputBuffer = (uint8_t *) malloc( me->outputMaxLen );
		require_action( me->outputBuffer, exit, err = kNoMemoryErr );

		as_ulog( kLogLevelVerbose, "snd_pcm_plugin_setup mixer_device %d, buf.block.frag_size %d for PLAYBACK\n", 
			(int) setup.mixer_device, (int) setup.buf.block.frag_size );
	}
	
	err = snd_mixer_open( &me->mixerHandle, card, setup.mixer_device );
	require_noerr( err, exit );
	
	me->prepared = true;
	
exit:
	if( err ) AudioStreamStop( inStream, false );
	return( err );
}
#endif

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef inStream )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( inStream );
	OSStatus					err;
	//int							pipeFDs[ 2 ];
#if( TARGET_OS_LINUX )
	//int							inputCount, outputCount, n;
#endif
	//int							fdCount, i;
	
	if( !me->prepared )
	{
		err = AudioStreamPrepare( inStream );
		require_noerr( err, exit );
	}
		
	// Start threads to process audio.
	me->stop = false;
	if( me->inputPCMHandle )
	{
		err = pthread_create( &me->inputThread, NULL, _AudioStreamInputThread, inStream );
		require_noerr( err, exit );
	}
	
	err = pthread_create( &me->outputThread, NULL, _AudioStreamOutputThread, inStream );
	require_noerr( err, exit );
	CFRetain( inStream );
	
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
	OSStatus					err;
	//ssize_t						n;
	
	DEBUG_USE_ONLY( err );
	
	me->stop = true;
	if( me->inputPCMHandle )
	{
		err = pthread_join( me->inputThread, NULL );
		check_noerr( err );
	}
	err = pthread_join( me->outputThread, NULL );
	check_noerr( err );

	ForgetMem( &me->inputBuffer );
	ForgetMem( &me->outputBuffer );
#if( TARGET_OS_QNX )
	if( me->mixerHandle )
	{
		err = snd_mixer_close( me->mixerHandle );
		check_noerr( err );
		me->mixerHandle = NULL;
	}
#endif
	if( me->inputPCMHandle )
	{
		if( inDrain )
		{
			#if( TARGET_OS_LINUX )
				err = snd_pcm_drain( me->inputPCMHandle );
				check_noerr( err );
			#endif
		}
		snd_pcm_close( me->inputPCMHandle );
		me->inputPCMHandle = NULL;
	}
	if( me->outputPCMHandle )
	{
		if( inDrain )
		{
			#if( TARGET_OS_LINUX )
				err = snd_pcm_drain( me->outputPCMHandle );
				check_noerr( err );
			#elif( TARGET_OS_QNX )
				err = snd_pcm_plugin_playback_drain( me->outputPCMHandle );
				check_noerr( err );
			#endif
		}
		snd_pcm_close( me->outputPCMHandle );
		me->outputPCMHandle = NULL;
	}
	me->prepared = false;
	CFRelease( inStream );
}

//===========================================================================================================================
//	_AudioStreamThread
//===========================================================================================================================

#if( TARGET_OS_LINUX )
static snd_pcm_sframes_t _GetAvailDelayWithTimestamp(snd_pcm_t *handle, uint64_t *timestamp, snd_pcm_sframes_t *delay)
{
    int err;
    snd_pcm_status_t *status;
    
    snd_pcm_status_alloca(&status);
	err = snd_pcm_status(handle, status);
	if (err < 0)
		return err;
#ifdef USE_UPTICKS_FOR_AUDIO_TS
	*timestamp = UpTicks();
#else
	struct timespec ts;
	snd_pcm_status_get_htstamp(status, &ts);
    *timestamp = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#endif
    *delay = snd_pcm_status_get_delay(status);
    return snd_pcm_status_get_avail(status);
}

static void * _AudioStreamInputThread( void *inArg )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( (AudioStreamRef) inArg );
	int							result;
	snd_pcm_sframes_t			frames;
    snd_pcm_sframes_t			delay;
	uint64_t					hostTime;
	OSStatus					err;
	uint8_t *					ptr;
	ssize_t						n;
	Boolean						restart;

	SetThreadName( "AudioStreamInputThread" );
	if( me->hasThreadPriority ) SetCurrentThreadPriority( me->threadPriority );
	err = snd_pcm_start( me->inputPCMHandle );
	check_noerr( err );
	do {
		result = snd_pcm_wait( me->inputPCMHandle, 20 );
		if (me->stop)
			break;
		//if (result == 0)
			//printf("%d %d\n", result, snd_pcm_state(me->inputPCMHandle));
		if (result) {
			restart = false;
			if (result < 0) {
				if (result == -EPIPE) {
					err = snd_pcm_recover( me->inputPCMHandle, result, 1 );//snd_pcm_drain
					restart = true;
				}	else {
					err = snd_pcm_recover( me->inputPCMHandle, result, 1 );
					if (err == kNoErr)
						err = snd_pcm_start( me->inputPCMHandle );
				}
				as_ulog( kLogLevelNotice, "### ALSA input wait error %#m -> %#m\n", (OSStatus) result, err );
			}
		retryRead:
			frames = snd_pcm_avail(me->inputPCMHandle);//Some input devices are having problems with just snd_pcm_status
			_GetAvailDelayWithTimestamp( me->inputPCMHandle, &hostTime, &delay);
			//if (result < 0)
				//printf("%d %ld %ld %d %ld\n", result, frames, delay, snd_pcm_state(me->inputPCMHandle), snd_pcm_avail(me->inputPCMHandle));
			if( frames < 0 )
			{
				if (frames == -EPIPE) {
					err = snd_pcm_recover( me->inputPCMHandle, result, 1 );//snd_pcm_drain
					restart = true;
					goto retryRead;
				} else {
					err = snd_pcm_recover( me->inputPCMHandle, result, 1);
					if (err == kNoErr)
						err = snd_pcm_start( me->inputPCMHandle );
				}
				as_ulog( kLogLevelNotice, "### ALSA input avail error %#m -> %#m\n", (OSStatus) frames, err );
			}
			else if ( frames > 0 )
			{
				ptr = me->inputBuffer;
				do {
					n = snd_pcm_readi( me->inputPCMHandle, ptr, frames );
				} while (n == -EAGAIN);
				if (n == -EPIPE) {
					err = snd_pcm_recover( me->inputPCMHandle, result, 1 );//snd_pcm_drain
					restart = true;
					goto retryRead;
				}
				if( n <= 0 )
				{
					err = snd_pcm_recover( me->inputPCMHandle, result, 1 );
					if (err == kNoErr)
						err = snd_pcm_start( me->inputPCMHandle );
					as_ulog( kLogLevelNotice, "### ALSA read error %#m -> %#m\n", (OSStatus) n, err );
				}
				else
				{
					if (restart) {
						snd_pcm_start( me->inputPCMHandle );
						restart = false;
					}
                    hostTime -= (uint64_t)delay * 1000000000 / me->format.mSampleRate;
		#if( LOG_TIME_STAMPS )
					fprintf(me->inputStampsFile, "%d %ld %u %llu\n", n, delay, me->inputSampleTime, hostTime);
		#endif
					me->inputCallbackPtr( me->inputSampleTime, hostTime, ptr, (size_t)( n * me->format.mBytesPerFrame ), me->inputCallbackCtx );
					me->inputSampleTime += n;
				}
			}
			if (restart)
				snd_pcm_start( me->inputPCMHandle );
		}
	} while (1);
	return NULL;
}

static void * _AudioStreamOutputThread( void *inArg )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( (AudioStreamRef) inArg );
	int							result;
	snd_pcm_sframes_t			frames;
    snd_pcm_sframes_t			delay;
	uint64_t					hostTime;
	OSStatus					err;
	size_t const				bytesPerUnit = me->format.mBytesPerFrame;
	uint8_t *					ptr;
	snd_pcm_sframes_t			n;

	SetThreadName( "AudioStreamOutputThread" );
	if( me->hasThreadPriority ) SetCurrentThreadPriority( me->threadPriority );
	//snd_pcm_format_set_silence( format, buffer, frames * channels );
	bzero(me->outputBuffer, me->outputMaxLen);
	frames = me->outputMaxLen / me->format.mBytesPerFrame;
	me->outputSampleTime += frames;
	while( frames > 0 )
	{
		n = snd_pcm_writei( me->outputPCMHandle, me->outputBuffer, frames );
		if( n <= 0 )
		{
			err = snd_pcm_recover( me->outputPCMHandle, n, 1 );
			as_ulog( kLogLevelNotice, "### ALSA initial write error %#m -> %#m\n", (OSStatus) n, err );
		}
		frames -= n;
	}
	do {
		result = snd_pcm_wait( me->outputPCMHandle, 20 );
		if (me->stop)
			break;
		if (result) {
			if (result < 0) {
				err = snd_pcm_recover( me->outputPCMHandle, result, 1 );
				as_ulog( kLogLevelNotice, "### ALSA input wait error %#m -> %#m\n", (OSStatus) result, err );
			}
			frames = _GetAvailDelayWithTimestamp( me->outputPCMHandle, &hostTime, &delay);
			if( frames < 0 )
			{
				err = snd_pcm_recover( me->outputPCMHandle, frames, 1 );
				as_ulog( kLogLevelNotice, "### ALSA output avail error %#m -> %#m\n", (OSStatus) frames, err );
			}
			else if ( frames > 0 )
			{
				
				ptr = me->outputBuffer;
				frames = Min( frames, (snd_pcm_sframes_t)( me->outputMaxLen / bytesPerUnit ) );
                hostTime += (uint64_t)delay * 1000000000 / me->format.mSampleRate;
	#if( LOG_TIME_STAMPS )
				fprintf(me->outputStampsFile, "%ld %ld %u %llu\n", frames, delay, me->outputSampleTime, hostTime);
	#endif
				me->outputCallbackPtr( me->outputSampleTime, hostTime, ptr, (size_t)( frames * bytesPerUnit ), me->outputCallbackCtx );
				me->outputSampleTime += frames;
#if( CAPTURE_AUDIO )
				fwrite( ptr, (size_t)( frames * bytesPerUnit ), 1, me->audioOutCaptureFile );
#endif
				
				while( frames > 0 )
				{
					n = snd_pcm_writei( me->outputPCMHandle, ptr, frames );
					if( n <= 0 )
					{
						err = snd_pcm_recover( me->outputPCMHandle, n, 1 );
						as_ulog( kLogLevelNotice, "### ALSA write error %#m -> %#m\n", (OSStatus) n, err );
						continue;
					}
					ptr += ( n * bytesPerUnit );
					frames -= n;
				}
			}
		}
	} while (1);
	return NULL;
}

#endif // TARGET_OS_LINUX

#if( TARGET_OS_QNX )
static void *	_AudioStreamThread( void *inArg )
{
	AudioStreamImpRef const		me = _AudioStreamGetImp( (AudioStreamRef) inArg );
	ssize_t						n;
	OSStatus					err;
	size_t const				bytesPerUnit = me->format.mBytesPerFrame;
	uint64_t					hostTime;
	uint32_t					inputSampleTime = 0;
	uint32_t					outputSampleTime = 0;
	
	SetThreadName( me->threadName ? me->threadName : "AudioStreamThread" );
	if( me->hasThreadPriority ) SetCurrentThreadPriority( me->threadPriority );
	
	for( ;; )
	{
		n = poll( me->pollFDArray, me->pollFDCount, -1 );
		err = poll_errno( n );
		if( err == EINTR ) continue;
		if( err ) { dlogassert( "poll() error: %#m", err ); usleep( 1000 ); continue; }
		if( me->pollFDArray[ 0 ].revents & POLLIN ) break; // Only event is quit so stop anytime pipe is readable.
		hostTime = UpTicks();
		
		// Process input audio.
		
		if( me->inputPCMHandle && ( me->pollFDArray[ me->inputFDIndex ].revents & POLLIN ) )
		{
			n = snd_pcm_plugin_read( me->inputPCMHandle, me->inputBuffer, me->inputMaxLen );
			if( n > 0 )
			{
				me->inputCallbackPtr( inputSampleTime, hostTime, me->inputBuffer, (size_t) n, me->inputCallbackCtx );
				inputSampleTime += ( n / bytesPerUnit );
			}
			else
			{
				as_ulog( kLogLevelNotice, "### ALSA read error %#m\n", (OSStatus) n );
			}
		}
		
		// Process output audio.
		
		if( me->outputPCMHandle && ( me->pollFDArray[ me->outputFDIndex ].revents & POLLOUT ) )
		{
			me->outputCallbackPtr( outputSampleTime, hostTime, me->outputBuffer, me->outputMaxLen, me->outputCallbackCtx );
			n = snd_pcm_plugin_write( me->outputPCMHandle, me->outputBuffer, me->outputMaxLen );
			if( n < ( (ssize_t) me->outputMaxLen ) )
			{
				as_ulog( kLogLevelNotice, "### ALSA write error %#m\n", (OSStatus) n );
			}
			if( n > 0 ) outputSampleTime += ( n / bytesPerUnit );
		}
		
		// Process mixer events.
		
		if( me->pollFDArray[ me->mixerFDIndex ].revents & POLLIN )
		{
			snd_mixer_read( me->mixerHandle, &kAudioMixerNullCallbacks );
		}
	}
	return( NULL );
}
#endif

#if 0
#pragma mark -
#endif

