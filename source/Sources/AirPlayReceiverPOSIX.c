/*
	File:    	AirPlayReceiverPOSIX.c
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
	
	POSIX platform plugin for AirPlay.
*/

#include "AudioUtils.h"

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "HIDUtils.h"
#include "ScreenUtils.h"

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

// AirPlayReceiverServerPlatformData

typedef struct
{
	uint32_t						systemBufferSizeMicros;
	uint32_t						systemSampleRate;

}	AirPlayReceiverServerPlatformData;

// AirPlayAudioStreamPlatformContext

typedef struct
{
	AirPlayStreamType				type;
	AirPlayAudioFormat				format;
	Boolean							input;
	Boolean							loopback;
	CFStringRef						audioType;
	double							vocoderSampleRate;
	
	AirPlayStreamType				activeType;
	AirPlayReceiverSessionRef		session;
	AudioStreamRef					stream;
	Boolean							started;
	
}	AirPlayAudioStreamPlatformContext;

// AirPlayReceiverSessionPlatformData

typedef struct
{
	AirPlayReceiverSessionRef				session;
	AirPlayAudioStreamPlatformContext		mainAudioCtx;
	AirPlayAudioStreamPlatformContext		altAudioCtx;
	Boolean									sessionStarted;
	
}	AirPlayReceiverSessionPlatformData;

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus	_SetUpStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams );
static void		_TearDownStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams );
static OSStatus	_UpdateStreams( AirPlayReceiverSessionRef inSession );
	static void
		_AudioInputCallBack( 
			uint32_t		inSampleTime, 
			uint64_t		inHostTime, 
			const void *	inBuffer, 
			size_t			inLen, 
			void *			inContext );
static void
	_AudioOutputCallBack( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );

#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
	static CFArrayRef	_HIDCopyDevices( AirPlayReceiverSessionRef inSession, OSStatus *outErr );
	static OSStatus		_HIDStart( AirPlayReceiverSessionRef inSession );
	static void			_HIDStop( AirPlayReceiverSessionRef inSession );

	static CFArrayRef	_CopyDisplayDescriptions( AirPlayReceiverSessionRef inSession, OSStatus *outErr );
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

ulog_define( AirPlayReceiverPlatform, kLogLevelTrace, kLogFlags_Default, "AirPlay",  NULL );
#define atrp_ucat()					&log_category_from_name( AirPlayReceiverPlatform )
#define atrp_ulog( LEVEL, ... )		ulog( atrp_ucat(), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark -
#pragma mark == Server-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverServerPlatformInitialize
//===========================================================================================================================

OSStatus	AirPlayReceiverServerPlatformInitialize( AirPlayReceiverServerRef inServer )
{
	OSStatus								err;
	AirPlayReceiverServerPlatformData *		platform;
	
	platform = (AirPlayReceiverServerPlatformData *) calloc( 1, sizeof( *platform ) );
	require_action( platform, exit, err = kNoMemoryErr );
	inServer->platformPtr = platform;
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformFinalize
//===========================================================================================================================

void	AirPlayReceiverServerPlatformFinalize( AirPlayReceiverServerRef inServer )
{
	AirPlayReceiverServerPlatformData * const	platform = (AirPlayReceiverServerPlatformData *) inServer->platformPtr;
	
	if( !platform ) return;
	
	free( platform );
	inServer->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformControl
//===========================================================================================================================

OSStatus
	AirPlayReceiverServerPlatformControl( 
		CFTypeRef			inServer, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverServerRef const				server		= (AirPlayReceiverServerRef) inServer;
	OSStatus									err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// Other
	
	else if( server->delegate.control_f )
	{
		err = server->delegate.control_f( server, inCommand, inQualifier, inParams, outParams, server->delegate.context );
		goto exit;
	}
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
//	AirPlayReceiverServerPlatformCopyProperty
//===========================================================================================================================

CFTypeRef
	AirPlayReceiverServerPlatformCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	CFTypeRef							value  = NULL;
	OSStatus							err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// SystemFlags
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_StatusFlags ) ) )
	{
		// Always report an audio link until we can detect it correctly.
		
		value = CFNumberCreateInt64( kAirPlayStatusFlag_AudioLink );
		require_action( value, exit, err = kUnknownErr );
	}
	
	// Other
	
	else if( server->delegate.copyProperty_f )
	{
		value = server->delegate.copyProperty_f( server, inProperty, inQualifier, &err, server->delegate.context );
		goto exit;
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( err )		ForgetCF( &value );
	if( outErr )	*outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformSetProperty
//===========================================================================================================================

OSStatus
	AirPlayReceiverServerPlatformSetProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// Other
	
	else if( server->delegate.setProperty_f )
	{
		err = server->delegate.setProperty_f( server, inProperty, inQualifier, inValue, server->delegate.context );
		goto exit;
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Session-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformInitialize
//===========================================================================================================================

OSStatus	AirPlayReceiverSessionPlatformInitialize( AirPlayReceiverSessionRef inSession )
{
	OSStatus									err;
	AirPlayReceiverSessionPlatformData *		spd;
	
	spd = (AirPlayReceiverSessionPlatformData *) calloc( 1, sizeof( *spd ) );
	require_action( spd, exit, err = kNoMemoryErr );
	spd->session = inSession;
	inSession->platformPtr = spd;
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformFinalize
//===========================================================================================================================

void	AirPlayReceiverSessionPlatformFinalize( AirPlayReceiverSessionRef inSession )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	
	if( !spd ) return;
	
#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
	_HIDStop( inSession );
#endif
	spd->sessionStarted = false;
	_TearDownStreams( inSession, NULL );
	
	free( spd );
	inSession->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformControl
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionPlatformControl( 
		CFTypeRef			inSession, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	double											duration, finalVolume;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// DuckAudio
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_DuckAudio ) ) )
	{
		duration = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayKey_DurationMs ), &err );
		if( err || ( duration < 0 ) ) duration = 500;
		duration /= 1000;
		
		finalVolume = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayProperty_Volume ), &err );
		finalVolume = !err ? DBtoLinear( (float) finalVolume ) : 0.2;
		finalVolume = Clamp( finalVolume, 0.0, 1.0 );
		
		// Notify client of duck command
		if( session->delegate.duckAudio_f )
		{
			atrp_ulog( kLogLevelNotice, "Delegating ducking of audio to %f within %f seconds\n", finalVolume, duration );
			session->delegate.duckAudio_f( session, duration, finalVolume, session->delegate.context );
		}
	}
	
	// UnduckAudio
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UnduckAudio ) ) )
	{
		duration = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayKey_DurationMs ), &err );
		if( err || ( duration < 0 ) ) duration = 500;
		duration /= 1000;
		
		// Notify client of unduck command
		if( session->delegate.unduckAudio_f )
		{
			atrp_ulog( kLogLevelNotice, "Delegating unducking of audio within %f seconds\n", duration );
			session->delegate.unduckAudio_f( session, duration, session->delegate.context );
		}
	}
	
	// SetUpStreams
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_SetUpStreams ) ) )
	{
		err = _SetUpStreams( session, inParams );
		require_noerr( err, exit );
	}
	
	// TearDownStreams
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_TearDownStreams ) ) )
	{
		_TearDownStreams( session, inParams );
	}
	
	// StartSession
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StartSession ) ) )
	{
		spd->sessionStarted = true;
		err = _UpdateStreams( session );
		require_noerr( err, exit );
	}
	
	// Other
	
	else if( session->delegate.control_f )
	{
		err = session->delegate.control_f( session, inCommand, inQualifier, inParams, outParams, session->delegate.context );
		goto exit;
	}
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
//	AirPlayReceiverSessionPlatformCopyProperty
//===========================================================================================================================

CFTypeRef
	AirPlayReceiverSessionPlatformCopyProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	CFTypeRef										value = NULL;
	
	(void) inFlags;
	(void) spd;
	
	if( session->delegate.copyProperty_f )
	{
		value = session->delegate.copyProperty_f( session, inProperty, inQualifier, &err, session->delegate.context );
	}
	else
	{
		err = kNotHandledErr;
	}
	
#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
	if( err != kNoErr ) {
		
		// Displays
		
		if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Displays ) ) )
		{
			value = _CopyDisplayDescriptions( session, &err );
		}
	
		// HIDDevices
	
		else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_HIDDevices ) ) )
		{
			value = _HIDCopyDevices( session, &err );
		}
		else
		{
			err = kNotHandledErr;
		}
	}
#endif
	
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformSetProperty
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionPlatformSetProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverSessionRef const					session	= (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	
	(void) spd;
	(void) inFlags;
	
	if( 0 ) {}
	
	// Other
	
	else if( session->delegate.setProperty_f )
	{
		err = session->delegate.setProperty_f( session, inProperty, inQualifier, inValue, session->delegate.context );
		goto exit;
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_SetUpStreams
//===========================================================================================================================

static OSStatus	_SetUpStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	CFArrayRef										streams;
	CFIndex											i, n;
	CFDictionaryRef									streamDesc;
	AirPlayStreamType								streamType;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	CFStringRef										cfstr;
	CFDictionaryRef									dict;
	
	streams = CFDictionaryGetCFArray( inParams, CFSTR( kAirPlayKey_Streams ), NULL );
	n = streams ? CFArrayGetCount( streams ) : 0;
	for( i = 0; i < n; ++i )
	{
		streamDesc = CFArrayGetCFDictionaryAtIndex( streams, i, &err );
		require_noerr( err, exit );
		
		streamType = (AirPlayStreamType) CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		switch( streamType )
		{
			case kAirPlayStreamType_MainAudio:		streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_MainHighAudio:	streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_AltAudio:		streamCtx = &spd->altAudioCtx;  break;
			case kAirPlayStreamType_Screen:			continue;
			default: atrp_ulog( kLogLevelNotice, "### Unsupported stream type %d\n", streamType ); continue;
		}
		streamCtx->type				= streamType;
		streamCtx->format			= CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_AudioFormat ), NULL );
		if( streamCtx->format == kAirPlayAudioFormat_Invalid ) streamCtx->format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
		streamCtx->input			= CFDictionaryGetBoolean( streamDesc, CFSTR( kAirPlayKey_Input ), &err );
		streamCtx->loopback			= CFDictionaryGetBoolean( streamDesc, CFSTR( kAirPlayKey_AudioLoopback ), NULL );
		
		cfstr = CFDictionaryGetCFString( streamDesc, CFSTR( kAirPlayKey_AudioType ), NULL );
		ReplaceCF( &streamCtx->audioType, cfstr );
		
		dict = CFDictionaryGetCFDictionary( streamDesc, CFSTR( kAirPlayKey_VocoderInfo ), NULL );
		if( dict )
		{
			streamCtx->vocoderSampleRate = CFDictionaryGetDouble( dict, CFSTR( kAirPlayVocoderInfoKey_SampleRate ), NULL );
		}
	}
	
	err = _UpdateStreams( inSession );
	require_noerr( err, exit );

#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
	if( IsValidSocket( inSession->eventSock ) )
	{
		err = _HIDStart( inSession );
		require_noerr( err, exit );
	}
#endif
	
exit:
	if( err ) _TearDownStreams( inSession, inParams );
	return( err );
}

//===========================================================================================================================
//	_TearDownStreams
//===========================================================================================================================

static void	_TearDownStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	CFArrayRef										streams;
	CFIndex											i, n;
	CFDictionaryRef									streamDesc;
	AirPlayStreamType								streamType;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	
	streams = inParams ? CFDictionaryGetCFArray( inParams, CFSTR( kAirPlayKey_Streams ), NULL ) : NULL;
	n = streams ? CFArrayGetCount( streams ) : 0;
	for( i = 0; i < n; ++i )
	{
		streamDesc = CFArrayGetCFDictionaryAtIndex( streams, i, &err );
		require_noerr( err, exit );
		
		streamType = (AirPlayStreamType) CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		switch( streamType )
		{
			case kAirPlayStreamType_MainAudio:		streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_MainHighAudio:	streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_AltAudio:		streamCtx = &spd->altAudioCtx;  break;
			case kAirPlayStreamType_Screen:			continue;
			default: atrp_ulog( kLogLevelNotice, "### Unsupported stream type %d\n", streamType ); continue;
		}
		streamCtx->type = kAirPlayStreamType_Invalid;
	}
	if( n == 0 )
	{
		spd->mainAudioCtx.type = kAirPlayStreamType_Invalid;
		spd->altAudioCtx.type  = kAirPlayStreamType_Invalid;
#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
		_HIDStop( inSession );
#endif
	}
	_UpdateStreams( inSession );
		
exit:
	return;
}

//===========================================================================================================================
//	_UpdateStreams
//===========================================================================================================================

static OSStatus	_UpdateStreams( AirPlayReceiverSessionRef inSession )
{
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	AudioStreamBasicDescription						asbd;
	
	// Update main audio stream.
	
	streamCtx = &spd->mainAudioCtx;
	if( ( streamCtx->type != kAirPlayStreamType_Invalid ) && !streamCtx->stream )
	{
		atrp_ulog( kLogLevelNotice, "Main audio setting up %s for %@, input %s, loopback %s\n",
			AirPlayAudioFormatToString( streamCtx->format ), 
			streamCtx->audioType ? streamCtx->audioType : CFSTR( kAirPlayAudioType_Default ),
			streamCtx->input			? "yes" : "no",
			streamCtx->loopback			? "yes" : "no" );
		
		streamCtx->activeType = streamCtx->type;
		streamCtx->session = inSession;
		err = AudioStreamCreate( &streamCtx->stream );
		//TODO check what to do with inSession->server->audioStreamOptions; they are not used in AudioStreamCreate
		require_noerr( err, exit );
		AudioStreamSetDelegateContext( streamCtx->stream, inSession->delegate.context );
		
		if( streamCtx->input ) AudioStreamSetInputCallback( streamCtx->stream, _AudioInputCallBack, streamCtx );
		AudioStreamSetOutputCallback( streamCtx->stream, _AudioOutputCallBack, streamCtx );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_StreamType, streamCtx->activeType );
		AudioStreamPropertySetCString( streamCtx->stream, kAudioStreamProperty_ThreadName, "AirPlayAudioMain", kSizeCString );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_ThreadPriority, kAirPlayThreadPriority_AudioReceiver );

		err = AudioStreamPropertySetBoolean( streamCtx->stream, kAudioStreamProperty_Input, streamCtx->input );
		require_noerr( err, exit );
		err = _AudioStreamSetProperty( streamCtx->stream, kAudioStreamProperty_AudioType, streamCtx->audioType );
		require_noerr( err, exit );
		
		err = AirPlayAudioFormatToPCM( streamCtx->format, &asbd );
		require_noerr( err, exit );
		AudioStreamSetFormat(streamCtx->stream, &asbd);
		
		if( streamCtx->vocoderSampleRate > 0 )
			AudioStreamPropertySetDouble( streamCtx->stream, kAudioStreamProperty_VocoderSampleRate, streamCtx->vocoderSampleRate );
		
		err = AudioStreamPrepare( streamCtx->stream );
		require_noerr( err, exit );
	}
	else if( ( streamCtx->type == kAirPlayStreamType_Invalid ) && streamCtx->stream )
	{
		AudioStreamForget( &streamCtx->stream );
		ForgetCF( &streamCtx->audioType );
		streamCtx->started = false;
		streamCtx->session = NULL;
		streamCtx->vocoderSampleRate = 0;
		streamCtx->activeType = kAirPlayStreamType_Invalid;
		atrp_ulog( kLogLevelNotice, "Main audio torn down\n" );
	}
	
	// Update alt audio.
	
	streamCtx = &spd->altAudioCtx;
	if( ( streamCtx->type != kAirPlayStreamType_Invalid ) && !streamCtx->stream )
	{
		atrp_ulog( kLogLevelNotice, "Alt audio setting up %s\n", AirPlayAudioFormatToString( streamCtx->format ) );
		
		streamCtx->activeType = streamCtx->type;
		streamCtx->session = inSession;
		err = AudioStreamCreate( &streamCtx->stream );
		require_noerr( err, exit );
		
		AudioStreamSetOutputCallback( streamCtx->stream, _AudioOutputCallBack, streamCtx );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_StreamType, kAirPlayStreamType_AltAudio );
		AudioStreamPropertySetCString( streamCtx->stream, kAudioStreamProperty_ThreadName, "AirPlayAudioAlt", kSizeCString );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_ThreadPriority, kAirPlayThreadPriority_AudioReceiver );
		
		err = AirPlayAudioFormatToPCM( streamCtx->format, &asbd );
		require_noerr( err, exit );
		AudioStreamSetFormat(streamCtx->stream, &asbd);
		
		err = AudioStreamPrepare( streamCtx->stream );
		require_noerr( err, exit );
	}
	else if( ( streamCtx->type == kAirPlayStreamType_Invalid ) && streamCtx->stream )
	{
		AudioStreamForget( &streamCtx->stream );
		ForgetCF( &streamCtx->audioType );
		streamCtx->started = false;
		streamCtx->session = NULL;
		streamCtx->vocoderSampleRate = 0;
		streamCtx->activeType = kAirPlayStreamType_Invalid;
		atrp_ulog( kLogLevelNotice, "Alt audio torn down\n" );
	}
	
	// If audio has started, make sure all the streams are started.
	
	if( spd->sessionStarted )
	{
		streamCtx = &spd->mainAudioCtx;
		if( streamCtx->stream && !streamCtx->started )
		{
			err = AudioStreamStart( streamCtx->stream );
			if( err ) atrp_ulog( kLogLevelWarning, "### Main audio start failed: %#m\n", err );
			streamCtx->started = true;
		}
		
		streamCtx = &spd->altAudioCtx;
		if( streamCtx->stream && !streamCtx->started )
		{
			err = AudioStreamStart( streamCtx->stream );
			if( err ) atrp_ulog( kLogLevelWarning, "### Alt audio start failed: %#m\n", err );
			streamCtx->started = true;
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioInputCallBack
//===========================================================================================================================

static void
	_AudioInputCallBack( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext )
{
	AirPlayAudioStreamPlatformContext * const		streamCtx = (AirPlayAudioStreamPlatformContext *) inContext;
	OSStatus										err;
	
	require_quiet( !streamCtx->loopback, exit );
	
	err = AirPlayReceiverSessionWriteAudio( streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime, 
		inBuffer, inLen );
	require_noerr( err, exit );
	
exit:
	return;
}

//===========================================================================================================================
//	_AudioOutputCallBack
//===========================================================================================================================

static void
	_AudioOutputCallBack( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext )
{
	AirPlayAudioStreamPlatformContext * const		streamCtx = (AirPlayAudioStreamPlatformContext *) inContext;
	OSStatus										err;
	
	err = AirPlayReceiverSessionReadAudio( streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime, 
		inBuffer, inLen );
	require_noerr( err, exit );
	
	if( streamCtx->input && streamCtx->loopback )
	{
		err = AirPlayReceiverSessionWriteAudio( streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime, 
			inBuffer, inLen );
		require_noerr( err, exit );
	}
	
exit:
	return;
}

#if 0
#pragma mark -
#pragma mark == HID ==
#endif

#if defined( LEGACY_REGISTER_SCREEN_HID)
//===========================================================================================================================
//	_HIDCopyDevices
//===========================================================================================================================

static CFArrayRef	_HIDCopyDevices( AirPlayReceiverSessionRef inSession, OSStatus *outErr )
{
	CFArrayRef										result		= NULL;
	CFMutableArrayRef								descriptions;
	CFArrayRef										devices		= NULL;
	ScreenRef										mainScreen	= NULL;
	CFIndex											i, n;
	OSStatus										err;
	CFTypeRef										obj;

	(void)inSession;
	descriptions = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( descriptions, exit, err = kNoMemoryErr );
	
	mainScreen = ScreenCopyMain( &err );
	require_noerr( err, exit );
	
	devices = HIDCopyDevices( &err );
	require_noerr( err, exit );

	n = devices ? CFArrayGetCount( devices ) : 0;
	for( i = 0; i < n; ++i )
	{
		HIDDeviceRef				device;
		CFMutableDictionaryRef		description;
		
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( devices, i );
		description = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( description, skip, err = kNoMemoryErr );
		
		obj = HIDDeviceCopyID( device );
		require_action( obj, skip, err = kNoMemoryErr );
		CFDictionarySetValue( description, CFSTR( kAirPlayKey_UUID ), obj );
		CFRelease( obj );
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_ReportDescriptor, NULL, &err );
		require_noerr( err, skip );
		CFDictionarySetValue( description, CFSTR( kAirPlayKey_HIDDescriptor ), obj );
		CFRelease( obj );
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_CountryCode, NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( description, CFSTR( kAirPlayKey_HIDCountryCode ), obj );
			CFRelease( obj );
		}
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_Name, NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( description, CFSTR( kAirPlayKey_Name ), obj );
			CFRelease( obj );
		}
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_ProductID, NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( description, CFSTR( kAirPlayKey_HIDProductID ), obj );
			CFRelease( obj );
		}
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_SampleRate, NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( description, CFSTR( kAirPlayKey_SampleRate ), obj );
			CFRelease( obj );
		}
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_VendorID, NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( description, CFSTR( kAirPlayKey_HIDVendorID ), obj );
			CFRelease( obj );
		}
		
		if( mainScreen )
		{
			obj = ScreenCopyProperty( mainScreen, kScreenProperty_UUID, NULL, NULL );
			if( obj )
			{
				CFDictionarySetValue( description, CFSTR( kAirPlayKey_DisplayUUID ), obj );
				CFRelease( obj );
			}
		}
		
		CFArrayAppendValue( descriptions, description );
		err = kNoErr;
		
	skip:
		ForgetCF( &description );
		if( err ) atrp_ulog( kLogLevelNotice, "### Report HID device %d of %d failed: %#m\n", (int) i, (int) n, err );
	}
	
	result = descriptions;
	descriptions = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( mainScreen );
	CFReleaseNullSafe( devices );
	CFReleaseNullSafe( descriptions );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_HIDStart
//===========================================================================================================================

static OSStatus	_HIDStart( AirPlayReceiverSessionRef inSession )
{
	HIDSetSession( inSession );

	atrp_ulog( kLogLevelTrace, "HID started\n" );
	
	return( kNoErr );
}

//===========================================================================================================================
//	_HIDStop
//===========================================================================================================================

static void	_HIDStop( AirPlayReceiverSessionRef inSession )
{
	(void)inSession;

	atrp_ulog( kLogLevelTrace, "HID stopping\n" );
	HIDSetSession( NULL );
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

//===========================================================================================================================
//	_CopyDisplayDescriptions
//===========================================================================================================================

static CFArrayRef	_CopyDisplayDescriptions( AirPlayReceiverSessionRef inSession, OSStatus *outErr )
{
	CFArrayRef					result = NULL;
	OSStatus					err;
	CFMutableArrayRef			displays;
	CFMutableDictionaryRef		displayInfo = NULL;
	
	displays = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( displays, exit, err = kNoMemoryErr );
	
	displayInfo = AirPlayReceiverSessionScreen_CopyDisplaysInfo( inSession->screenSession, &err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( displays, displayInfo );
	result = displays;
	displays = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( displayInfo );
	CFReleaseNullSafe( displays );
	if( outErr ) *outErr = err;
	return( result );
}

#endif
