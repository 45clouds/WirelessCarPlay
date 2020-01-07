/*
	File:    	APSAudioSessionStub.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
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
	
	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
*/

#include "APSAudioSession.h"
#include "AirPlayCommon.h"

#include <errno.h>
#include <stdlib.h>

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>

#include CF_HEADER
#include LIBDISPATCH_HEADER

///===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( APSAudioSession, kLogLevelTrace, kLogFlags_Default, "APSAudioSession", NULL );
#define as_dlog( LEVEL, ... )		dlogc( &log_category_from_name( APSAudioSession ), (LEVEL), __VA_ARGS__ )
#define as_ulog( LEVEL, ... )		ulog( &log_category_from_name( APSAudioSession ), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AudioSessionSetEventHandler
//===========================================================================================================================

void APSAudioSessionSetEventHandler( APSAudioSessionEventHandler_f inHandler, void *inContext )
{
	(void) inHandler;
	(void) inContext;
	
	// This implementation should remain empty.
}

//===========================================================================================================================
//	_CreateLatencyDictionary
//===========================================================================================================================

static CFDictionaryRef
	_CreateLatencyDictionary(
		AudioStreamType inStreamType,
		CFStringRef inAudioType,
		uint32_t inSampleRate,
		uint32_t inSampleSize,
		uint32_t inChannels,
		uint32_t inInputLatency,
		uint32_t inOutputLatency,
		OSStatus *outErr )
{
	CFDictionaryRef						result = NULL;
	OSStatus							err;
	CFMutableDictionaryRef				latencyDict;
	
	latencyDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( latencyDict, exit, err = kNoMemoryErr );
	
	if( inStreamType != kAudioStreamType_Invalid )	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_Type, inStreamType );
	if( inAudioType )		CFDictionarySetValue( latencyDict, kAPSAudioSessionKey_AudioType, inAudioType );
	if( inSampleRate > 0 )	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_SampleRate, inSampleRate );
	if( inSampleSize > 0 )	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_SampleSize, inSampleSize );
	if( inChannels > 0 )	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_Channels, inChannels );
	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_InputLatencyMicros, inInputLatency );
	CFDictionarySetInt64( latencyDict, kAPSAudioSessionKey_OutputLatencyMicros, inOutputLatency );
	
	result = latencyDict;
	latencyDict = NULL;
	err = kNoErr;
exit:
	CFReleaseNullSafe( latencyDict );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
// APSAudioSessionCopyLatencies
//===========================================================================================================================

CFArrayRef	APSAudioSessionCopyLatencies( OSStatus *outErr )
{
	CFArrayRef							result = NULL;
	OSStatus							err;
	
	// $$$ TODO: obtain audio latencies for all audio formats and audio types supported by the underlying hardware.
	// Audio latencies are reported as an ordered array of dictionaries (from least restrictive to the most restrictive).
	// Each dictionary contains the following keys:
	//		[kAPSAudioSessionKey_Type] - if not specified, then latencies are good for all stream types
	//		[kAPSAudioSessionKey_AudioType] - if not specified, then latencies are good for all audio types
	//		[kAPSAudioSessionKey_SampleRate] - if not specified, then latencies are good for all sample rates
	//		[kAPSAudioSessionKey_SampleSize] - if not specified, then latencies are good for all sample sizes
	//		[kAPSAudioSessionKey_Channels] - if not specified, then latencies are good for all channel counts
	//		[kAPSAudioSessionKey_CompressionType] - if not specified, then latencies are good for all compression types
	//		kAPSAudioSessionKey_InputLatencyMicros
	//		kAPSAudioSessionKey_OutputLatencyMicros
	
	CFMutableArrayRef					audioLatenciesArray;
	CFDictionaryRef						dict = NULL;
	
	audioLatenciesArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( audioLatenciesArray, exit, err = kNoMemoryErr );

	// MainAudio catch all - set 0 latencies for now - $$$ TODO set real latencies
	// R25 don't know audiotype
#if 0	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		NULL,										// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
#endif
	// MainAudio default latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		kAudioStreamAudioType_Default,				// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// MainAudio media latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		kAudioStreamAudioType_Media,				// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// MainAudio telephony latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		kAudioStreamAudioType_Telephony,			// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// MainAudio SpeechRecognition latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		kAudioStreamAudioType_SpeechRecognition,	// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// Main Audio alert latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainAudio,					// inStreamType
		kAudioStreamAudioType_Alert,				// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );

	// AltAudio catch all latencies - set 0 latencies for now - $$$ TODO set real latencies
	// R25 don't know audiotype
#if 0	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_AltAudio,					// inStreamType
		NULL,										// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
#endif	
	// AltAudio Media latencies - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_AltAudio,					// inStreamType
		kAudioStreamAudioType_Default,				// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// MainHighAudio Media latencies (wireless only) - set 0 latencies for now - $$$ TODO set real latencies
	
	dict = _CreateLatencyDictionary(
		kAudioStreamType_MainHighAudio,				// inStreamType
		kAudioStreamAudioType_Media,				// inAudioType
		0,											// inSampleRate,
		0,											// inSampleSize,
		0,											// inChannels,
		0,											// inInputLatency,
		0,											// inOutputLatency,
		&err );
	require_noerr( err, exit );
	
	CFArrayAppendValue( audioLatenciesArray, dict );
	ForgetCF( &dict );
	
	// $$$ TODO add more latencies dictionaries as needed
	
	result = audioLatenciesArray;
	audioLatenciesArray = NULL;
	err = kNoErr;

exit:
	CFReleaseNullSafe( dict );
	CFReleaseNullSafe( audioLatenciesArray );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	APSAudioSessionEnsureSetup
//===========================================================================================================================

void
	APSAudioSessionEnsureSetup(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros )
{
	(void) inHasInput;
	(void) inPreferredSystemSampleRate;
	(void) inPreferredSystemBufferSizeMicros;

	// This implementation should remain empty.
}

//===========================================================================================================================
//	APSAudioSessionEnsureTornDown
//===========================================================================================================================

void APSAudioSessionEnsureTornDown( void )
{
	// This implementation should remain empty.
}

//===========================================================================================================================
//	APSAudioSessionGetSupportedFormats
//===========================================================================================================================

APSAudioSessionAudioFormat	APSAudioSessionGetSupportedFormats( AudioStreamType inStreamType, CFStringRef inAudioType )
{
	APSAudioSessionAudioFormat		formats;
	
	(void) inStreamType;
	(void) inAudioType;
	
	// $$$ TODO: This is where the accessory provides a list of audio formats it supports in hardware for the given stream
	// and audio type. It is important that, at a minimum, all sample rates required by the specification are included here.
	
	switch( inStreamType )
	{
		case kAudioStreamType_MainAudio:
			if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Compatibility ) ) )
				formats =
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo |
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono;
			else if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Default ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono |
					kAirPlayAudioFormat_OPUS_24KHz_Mono |
					kAirPlayAudioFormat_OPUS_16KHz_Mono;
#else
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono; 
#endif
					//kAirPlayAudioFormat_AAC_ELD_24KHz_Mono |
					//kAirPlayAudioFormat_AAC_ELD_16KHz_Mono;
			else if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Media ) ) )
				formats =
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
//					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
			else if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Telephony ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono	|
					kAirPlayAudioFormat_OPUS_24KHz_Mono |
					kAirPlayAudioFormat_OPUS_16KHz_Mono;
#else					
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono |
					kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono; 
#endif
					//kAirPlayAudioFormat_AAC_ELD_24KHz_Mono |
					//kAirPlayAudioFormat_AAC_ELD_16KHz_Mono;
			else if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_SpeechRecognition ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono |
					kAirPlayAudioFormat_OPUS_24KHz_Mono;
#else	
					kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono;
#endif
					//kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;

			else if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Alert ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
					kAirPlayAudioFormat_OPUS_48KHz_Mono;
#else					
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
#endif
//					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_OPUS_48KHz_Mono;
					//kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo |
					//kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo;
			else
				formats = 0;
			break;
			
		case kAudioStreamType_MainHighAudio:
			if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Media ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo |
					kAirPlayAudioFormat_AAC_LC_48KHz_Stereo |
					kAirPlayAudioFormat_AAC_LC_44KHz_Stereo;
#else	
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo; 
#endif
			else
				formats = 0;
			break;
			
		case kAudioStreamType_AltAudio:
			if( CFEqual( inAudioType, CFSTR( kAirPlayAudioType_Default ) ) )
				formats =
#ifdef NAGIVI
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
					kAirPlayAudioFormat_OPUS_48KHz_Mono;
#else	
					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
#endif					
//					kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo |
//					kAirPlayAudioFormat_OPUS_48KHz_Mono;
					//kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo |
					//kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo;
			else
				formats = 0;
			break;
			
		default:
			formats = 0;
			break;
	}
	
	return( formats );
}
