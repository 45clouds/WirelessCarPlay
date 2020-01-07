/*
	File:    	AirPlayReceiverPOSIX.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ‚ÄùPublic 
	Software‚Ä? and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in¬†consideration of your agreement to abide by them, Apple grants
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
	fixes or enhancements to Apple in connection with this software (‚ÄúFeedback‚Ä?, you hereby grant to
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
	
	POSIX platform plugin for AirPlay.
*/

#include <CoreUtils/AudioUtils.h>
#include <CoreUtils/StringUtils.h>
#include <CoreUtils/SystemUtils.h>

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlaySettings.h"
	#include "APSAudioSession.h"

	#include <CoreUtils/HIDUtils.h>
	#include <CoreUtils/ScreenUtils.h>

#if 0
#pragma mark == Structures ==
#endif
#include <glib.h>

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
	
	HIDBrowserRef							hidBrowser;
	dispatch_semaphore_t					hidBrowserStartedSignal;
	
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
static void _HandleAudioSessionEvent( APSAudioSessionEventType inType, CFTypeRef inParam, void *inContext );

	static CFArrayRef	_HIDCopyDevices( AirPlayReceiverSessionRef inSession, OSStatus *outErr );
	static OSStatus		_HIDStart( AirPlayReceiverSessionRef inSession );
	static void			_HIDStop( AirPlayReceiverSessionRef inSession );
	static void			_HIDBrowserEventHandler( HIDBrowserEventType inType, CFTypeRef inParam, void *inContext );
	static void
		_HIDDeviceEventHandler( 
			HIDDeviceRef		inDevice, 
			HIDDeviceEventType	inType, 
			OSStatus			inStatus, 
			const uint8_t *		inPtr, 
			size_t				inLen, 
			void *				inContext );

	static CFArrayRef	_CopyDisplayDescriptions( AirPlayReceiverSessionRef inSession, OSStatus *outErr );

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
	
	// UpdatePrefs
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdatePrefs ) ) )
	{
	}
	
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
//	APSAudioSessionGetCompatibilityInputFormats
//===========================================================================================================================

APSAudioSessionAudioFormat	APSAudioSessionGetCompatibilityInputFormats( AudioStreamType inStreamType )
{
	APSAudioSessionAudioFormat		formats;
	
	formats = 0;
	
	// $$$ TODO: This is where the accessory provides a list of audio input formats it supports in hardware.
	// It is important that, at a minimum, all sample rates required by the specification are included here.
	if( inStreamType == kAudioStreamType_MainAudio ) {
		formats |= 		kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
						kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono |
						kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono;
	}
	
	return( formats );
}

//===========================================================================================================================
//	APSAudioSessionGetCompatibilityOutputFormats
//===========================================================================================================================

APSAudioSessionAudioFormat	APSAudioSessionGetCompatibilityOutputFormats( AudioStreamType inStreamType )
{
	APSAudioSessionAudioFormat		formats;
	
	formats = APSAudioSessionGetCompatibilityInputFormats( inStreamType );

	// $$$ TODO: This is where the accessory provides a list of audio output formats it supports in hardware.
	// It is expected that the list of supported audio output formats is a superset of the supported audio
	// input formats.  As with input formats, it is important that, at a minimum, all sample rates required
	// by the specification are included here.
	formats |= 		kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo; 
//		|
//					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
	
	return( formats );
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
	
	// AudioFormats

	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_AudioFormats ) ) )
	{
		AirPlayAudioFormat		formats;
		AirPlayStreamType		streamTypes[] = {
										kAirPlayStreamType_MainAudio, // Must be first for backward compatibility
										kAirPlayStreamType_AltAudio, // Must be second for backward compatibility
										kAirPlayStreamType_MainHighAudio
									};
		CFStringRef				audioTypes[] = {
										CFSTR( kAirPlayAudioType_Compatibility ), // Must be first for backward compatibility
										CFSTR( kAirPlayAudioType_Default ),
										CFSTR( kAirPlayAudioType_Media ),
										CFSTR( kAirPlayAudioType_Telephony ),
										CFSTR( kAirPlayAudioType_SpeechRecognition ),
										CFSTR( kAirPlayAudioType_Alert ),
									};
		uint32_t				streamTypeNdx, audioTypeNdx;
		
		value = CFArrayCreateMutable( NULL, countof( streamTypes ), &kCFTypeArrayCallBacks );
		require_action( value, exit, err = kNoMemoryErr );
		
		// Outer loop must be audioType for backward compatibility 
		for ( audioTypeNdx = 0; audioTypeNdx < countof( audioTypes ); ++audioTypeNdx )
		{
			for( streamTypeNdx = 0; streamTypeNdx < countof( streamTypes ); ++streamTypeNdx )
			{
				if( CFEqual( audioTypes[ audioTypeNdx ], CFSTR( kAirPlayAudioType_Compatibility ) ) )
				{
					// Compatibility type may not have same values for input and output
					if(( streamTypes[ streamTypeNdx ] != kAirPlayStreamType_MainHighAudio)
							&& ( streamTypes[ streamTypeNdx ] != kAirPlayStreamType_AltAudio ))
					{
						// Compatibility is not valid for MainHighAudio
						AirPlayAudioFormat		inputFormats, outputFormats;

						inputFormats = APSAudioSessionGetCompatibilityInputFormats( streamTypes[ streamTypeNdx ] );
						outputFormats = APSAudioSessionGetCompatibilityOutputFormats( streamTypes[ streamTypeNdx ] );

						if( inputFormats || outputFormats )
						{
							CFMutableDictionaryRef	dict;

							dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
							require_action( dict, exit, err = kNoMemoryErr );
							CFArrayAppendValue( (CFMutableArrayRef) value, dict );

							CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_Type ), streamTypes[ streamTypeNdx ] );
							CFDictionarySetValue( dict, CFSTR( kAirPlayKey_AudioType ), audioTypes[ audioTypeNdx ] );

							if( inputFormats && streamTypes[ streamTypeNdx ] == kAirPlayStreamType_MainAudio ) {
								CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioInputFormats ), inputFormats );
							}
							if( outputFormats )  {
								CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioOutputFormats ), outputFormats );
							}

							CFRelease( dict );
						}
					}
				}
				else
				{
					AirPlayAudioFormat		formats;
#ifndef B511 //NAGIVI
					if( streamTypes[ streamTypeNdx ] != kAirPlayStreamType_MainHighAudio)
#endif						
					{
						formats = APSAudioSessionGetSupportedFormats( streamTypes[ streamTypeNdx ], audioTypes[ audioTypeNdx ] );
						if ( formats )
						{
							CFMutableDictionaryRef	dict;

							dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
							require_action( dict, exit, err = kNoMemoryErr );
							CFArrayAppendValue( (CFMutableArrayRef) value, dict );

							CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_Type ), streamTypes[ streamTypeNdx ] );
							CFDictionarySetValue( dict, CFSTR( kAirPlayKey_AudioType ), audioTypes[ audioTypeNdx ] );

							// Since audio types are, by definition, duplex or not, we can use the same values for input and output
							// Input values are not valid for Media and Alert types or Alternate Audio
							if( streamTypes[ streamTypeNdx ] != kAirPlayStreamType_AltAudio &&
									!CFEqual( audioTypes[ audioTypeNdx ], CFSTR( kAirPlayAudioType_Media ) ) &&
									!CFEqual( audioTypes[ audioTypeNdx ], CFSTR( kAirPlayAudioType_Alert ) ) )
							{
								CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioInputFormats ), formats );
							}
//							CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioInputFormats ), formats );
							CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioOutputFormats ), formats );

							CFRelease( dict );
						}
					}
				}
			}
		}
	}	
	// AudioLatencies
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_AudioLatencies ) ) )
	{
		value = APSAudioSessionCopyLatencies( &err );
		require_noerr( err, exit );
		atrp_ulog( kLogLevelVerbose, "AudioLatencies = %@\n", value );
	}
	
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

	APSAudioSessionSetEventHandler( _HandleAudioSessionEvent, inSession );

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
	
	_HIDStop( inSession );
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
		
		// Duck main audio if started
		if( spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			atrp_ulog( kLogLevelNotice, "Ducking audio to %f within %f seconds\n", finalVolume, duration );
			AudioStreamRampVolume( spd->mainAudioCtx.stream, finalVolume, duration, session->server->queue );
		}

		// Notify client of duck command as well
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
		
		// Unduck main audio if started
		if( spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			atrp_ulog( kLogLevelNotice, "Unducking audio within %f seconds\n", duration );
			AudioStreamRampVolume( spd->mainAudioCtx.stream, 1.0, duration, session->server->queue );
		}
		
		// Notify client of unduck command as well
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
	
	if( 0 ) {}
	
	// Displays
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Displays ) ) )
	{
		value = _CopyDisplayDescriptions( session, &err );
		require_noerr_quiet( err, exit );
	}
	
	// HIDDevices
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_HIDDevices ) ) )
	{
		value = _HIDCopyDevices( session, &err );
		require_noerr( err, exit );
	}
	
	// Other
	
	else if( session->delegate.copyProperty_f )
	{
		value = session->delegate.copyProperty_f( session, inProperty, inQualifier, &err, session->delegate.context );
		goto exit;
	}
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
	
	if( IsValidSocket( inSession->eventSock ) && !spd->hidBrowser ) // $$$ TO DO: Need flag to enable HID.
	{
		err = _HIDStart( inSession );
		require_noerr( err, exit );
	}
	
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
	if( spd == 	NULL)
	{
		return;
	}
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
		_HIDStop( inSession );
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
	Boolean const									useMain	= ( spd->mainAudioCtx.type != kAirPlayStreamType_Invalid );
	Boolean const									useAlt	= ( spd->altAudioCtx.type  != kAirPlayStreamType_Invalid );
	OSStatus										err;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	AudioStreamBasicDescription						asbd;
	
	if( useMain || useAlt )
	{
		AirPlayReceiverServerPlatformData * const platform = (AirPlayReceiverServerPlatformData *) inSession->server->platformPtr;
		uint32_t systemBufferSizeMicros = platform->systemBufferSizeMicros;

		if( ( systemBufferSizeMicros == 0 ) &&
		    ( ( spd->mainAudioCtx.type == kAirPlayStreamType_MainAudio || spd->mainAudioCtx.type == kAirPlayStreamType_MainHighAudio ) || useAlt ) )
		{
			// Reduce HAL buffer size to 5 ms. Below that may cause HAL overloads.
			systemBufferSizeMicros = 5000;
		}
		
		APSAudioSessionEnsureSetup( spd->mainAudioCtx.input, platform->systemSampleRate, systemBufferSizeMicros );
	}
	
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
		
		if( streamCtx->input ) AudioStreamSetInputCallback( streamCtx->stream, _AudioInputCallBack, streamCtx );
		AudioStreamSetOutputCallback( streamCtx->stream, _AudioOutputCallBack, streamCtx );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_StreamType, streamCtx->activeType );
		AudioStreamPropertySetCString( streamCtx->stream, kAudioStreamProperty_ThreadName, "AirPlayAudioMain", kSizeCString );
		AudioStreamPropertySetInt64( streamCtx->stream, kAudioStreamProperty_ThreadPriority, kAirPlayThreadPriority_AudioReceiver );

		err = AudioStreamPropertySetBoolean( streamCtx->stream, kAudioStreamProperty_Input, streamCtx->input );
		require_noerr( err, exit );
		err = _AudioStreamSetProperty( streamCtx->stream, kAudioStreamProperty_AudioType, streamCtx->audioType );
		require_noerr( err, exit );
		AudioStreamPropertySetBoolean( streamCtx->stream, kAudioStreamProperty_VarispeedEnabled, (streamCtx->type == kAirPlayStreamType_GeneralAudio) );
		
		err = AirPlayAudioFormatToPCM( streamCtx->format, &asbd );
		require_noerr( err, exit );
		err = AudioStreamPropertySetBytes( streamCtx->stream, kAudioStreamProperty_Format, &asbd, sizeof( asbd ) );
		require_noerr( err, exit );
		
		if( streamCtx->vocoderSampleRate > 0 )
			AudioStreamPropertySetDouble( streamCtx->stream, CFSTR( "vocoderSampleRate" ), streamCtx->vocoderSampleRate );
		
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
		err = AudioStreamPropertySetBytes( streamCtx->stream, kAudioStreamProperty_Format, &asbd, sizeof( asbd ) );
		require_noerr( err, exit );
		
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
	
	if( !useMain && !useAlt )	APSAudioSessionEnsureTornDown();
	
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

//===========================================================================================================================
//	_HandleAudioSessionEvent
//===========================================================================================================================

static void _HandleAudioSessionEvent( APSAudioSessionEventType inType, CFTypeRef inParam, void *inContext )
{
	AirPlayReceiverSessionRef const					session	= (AirPlayReceiverSessionRef) inContext;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;

	(void) inParam;

	switch( inType )
	{
		case kAPSAudioSessionEventAudioInterrupted:
			// Restart if we've been interrupted.
			spd->mainAudioCtx.started = false;
			spd->altAudioCtx.started  = false;
			_UpdateStreams( session );
			break;
		
		case kAPSAudioSessionEventAudioServicesWereReset:
			// Rebuild streams.
		
			AudioStreamForget( &spd->mainAudioCtx.stream );
			ForgetCF( &spd->mainAudioCtx.audioType );
			spd->mainAudioCtx.started = false;
			
			AudioStreamForget( &spd->altAudioCtx.stream );
			ForgetCF( &spd->altAudioCtx.audioType );
			spd->altAudioCtx.started = false;

			_UpdateStreams( session );
			break;

		default:
			dlogassert( "Bad event type: %u", inType );
			break;
	}
}

#if 0
#pragma mark -
#pragma mark == HID ==
#endif

//===========================================================================================================================
//	_HIDCopyDevices
//===========================================================================================================================

static CFArrayRef	_HIDCopyDevices( AirPlayReceiverSessionRef inSession, OSStatus *outErr )
{
	AirPlayReceiverSessionPlatformData * const		spd			= (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	CFArrayRef										result		= NULL;
	CFMutableArrayRef								descriptions;
	CFArrayRef										devices		= NULL;
	ScreenRef										mainScreen	= NULL;
	CFIndex											i, n;
	OSStatus										err;
	CFTypeRef										obj;
	
	descriptions = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( descriptions, exit, err = kNoMemoryErr );
	
	mainScreen = ScreenCopyMain( NULL );
	
	if( spd->hidBrowser )
	{
		devices = (CFArrayRef) HIDBrowserCopyProperty( spd->hidBrowser, kHIDBrowserProperty_Devices, NULL, &err );
		require_noerr( err, exit );
	}
	n = devices ? CFArrayGetCount( devices ) : 0;
	for( i = 0; i < n; ++i )
	{
		HIDDeviceRef				device;
		CFMutableDictionaryRef		description;
		
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( devices, i );
		description = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( description, skip, err = kNoMemoryErr );
		
		obj = HIDDeviceCopyProperty( device, kHIDDeviceProperty_UUID, NULL, &err );
		require_noerr( err, skip );
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
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	
	dispatch_forget( &spd->hidBrowserStartedSignal );
	spd->hidBrowserStartedSignal = dispatch_semaphore_create( 0 );
	require_action( spd->hidBrowserStartedSignal, exit, err = kNoMemoryErr );

	HIDBrowserForget( &spd->hidBrowser );
	err = HIDBrowserCreate( &spd->hidBrowser );
	require_noerr( err, exit );
	
	HIDBrowserSetProperty( spd->hidBrowser, kHIDBrowserProperty_HIDRaw, NULL, kCFBooleanFalse );
	HIDBrowserSetEventHandler( spd->hidBrowser, _HIDBrowserEventHandler, inSession );
	err = HIDBrowserStart( spd->hidBrowser );
	require_noerr( err, exit );
	CFRetain( inSession );
	
	dispatch_semaphore_wait( spd->hidBrowserStartedSignal, DISPATCH_TIME_FOREVER );
	
	atrp_ulog( kLogLevelTrace, "HID started\n" );
	
exit:
	if( err )
	{
		atrp_ulog( kLogLevelWarning, "### HID start failed: %#m\n", err );
		_HIDStop( inSession );
	}
	return( err );
}

//===========================================================================================================================
//	_HIDStop
//===========================================================================================================================

static void	_HIDStop( AirPlayReceiverSessionRef inSession )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	
	if( spd->hidBrowser )
	{
		atrp_ulog( kLogLevelTrace, "HID stopping\n" );
		HIDBrowserStopDevices( spd->hidBrowser );
		HIDBrowserForget( &spd->hidBrowser );
	}

	dispatch_forget( &spd->hidBrowserStartedSignal );
}

//===========================================================================================================================
//	_HIDBrowserEventHandler
//===========================================================================================================================

static void	_HIDBrowserEventHandler( HIDBrowserEventType inType, CFTypeRef inParam, void *inContext )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inContext;
	AirPlayReceiverSessionPlatformData *			spd;
	OSStatus										err;
	HIDDeviceRef									device;
	
	(void) inContext;
	DEBUG_USE_ONLY( err );
	
	if( inType == kHIDBrowserEventStarted )
	{
		spd = (AirPlayReceiverSessionPlatformData *) session->platformPtr;
		dispatch_semaphore_signal( spd->hidBrowserStartedSignal );
	}
	else if( inType == kHIDBrowserEventAttached )
	{
		device = (HIDDeviceRef) inParam;
		HIDDeviceSetEventHandler( device, _HIDDeviceEventHandler, session );
		err = HIDDeviceStart( device );
		check_noerr( err );
		if( !err ) CFRetain( session );
	}
	else if( inType == kHIDBrowserEventStopped )
	{
		atrp_ulog( kLogLevelTrace, "HID stopped\n" );
		CFRelease( session );
	}
}

//===========================================================================================================================
//	_HIDDeviceEventHandler
//===========================================================================================================================

static void
	_HIDDeviceEventHandler( 
		HIDDeviceRef		inDevice, 
		HIDDeviceEventType	inType, 
		OSStatus			inStatus, 
		const uint8_t *		inPtr, 
		size_t				inLen, 
		void *				inContext )
{
	AirPlayReceiverSessionRef const		session	= (AirPlayReceiverSessionRef) inContext;
	CFTypeRef							uuid	= NULL;
	OSStatus							err;
	
	if( inType == kHIDDeviceEventReport )
	{
		CFMutableDictionaryRef		request;
		
		if( inStatus )
		{
			err = ( inStatus == kEndingErr ) ? kNoErr : inStatus;
			HIDDeviceStop( inDevice );
			goto exit;
		}
		
		uuid = HIDDeviceCopyProperty( inDevice, kHIDDeviceProperty_UUID, NULL, &err );
		require_noerr( err, exit );
		
		atrp_ulog( kLogLevelChatty, "HID report from %@: %.3H\n", uuid, inPtr, (int) inLen, (int) inLen );
		
		request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( request, exit, err = kNoMemoryErr );
		CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_HIDSendReport ) );
		CFDictionarySetValue( request, CFSTR( kAirPlayKey_UUID ), uuid );
		CFDictionarySetData( request, CFSTR( kAirPlayKey_HIDReport ), inPtr, inLen );
		
		err = AirPlayReceiverSessionSendCommand( session, request, NULL, NULL );
		CFRelease( request );
		require_noerr( err, exit );
	}
	else if( inType == kHIDDeviceEventStopped )
	{
		atrp_ulog( kLogLevelTrace, "HID device stopped\n" );
		CFRelease( session );
	}
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( uuid );
	if( err ) atrp_ulog( kLogLevelNotice, "### HID device event failed: %#m\n", err );
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
