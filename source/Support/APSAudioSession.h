/*
	File:    	APSAudioSession.h
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
	
	Copyright (C) 2010-2014 Apple Inc. All Rights Reserved.
*/
/*!
	@header		AudioSession API
	@discussion	Provides APIs for audio session.
*/

#ifndef	__APSAudioSession_h__
#define	__APSAudioSession_h__

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/AudioUtils.h>
#include <CoreUtils/CFUtils.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark -
#pragma mark == APSAudioSession ==
#endif

//===========================================================================================================================
/*!	@group		APSAudioSession
	@abstract	Utilities for dealing with global audio state.
*/

typedef uint64_t		APSAudioSessionAudioFormat;
#define kAPSAudioSessionAudioFormat_Invalid						0
#define kAPSAudioSessionAudioFormat_PCM_8KHz_16Bit_Mono			( 1 << 2 )	// 0x00000004
#define kAPSAudioSessionAudioFormat_PCM_8KHz_16Bit_Stereo		( 1 << 3 )	// 0x00000008
#define kAPSAudioSessionAudioFormat_PCM_16KHz_16Bit_Mono		( 1 << 4 )	// 0x00000010
#define kAPSAudioSessionAudioFormat_PCM_16KHz_16Bit_Stereo		( 1 << 5 )	// 0x00000020
#define kAPSAudioSessionAudioFormat_PCM_24KHz_16Bit_Mono		( 1 << 6 )	// 0x00000040
#define kAPSAudioSessionAudioFormat_PCM_24KHz_16Bit_Stereo		( 1 << 7 )	// 0x00000080
#define kAPSAudioSessionAudioFormat_PCM_32KHz_16Bit_Mono		( 1 << 8 )	// 0x00000100
#define kAPSAudioSessionAudioFormat_PCM_32KHz_16Bit_Stereo		( 1 << 9 )	// 0x00000200
#define kAPSAudioSessionAudioFormat_PCM_44KHz_16Bit_Mono		( 1 << 10 )	// 0x00000400
#define kAPSAudioSessionAudioFormat_PCM_44KHz_16Bit_Stereo		( 1 << 11 )	// 0x00000800
#define kAPSAudioSessionAudioFormat_PCM_44KHz_24Bit_Mono		( 1 << 12 )	// 0x00001000
#define kAPSAudioSessionAudioFormat_PCM_44KHz_24Bit_Stereo		( 1 << 13 )	// 0x00002000
#define kAPSAudioSessionAudioFormat_PCM_48KHz_16Bit_Mono		( 1 << 14 )	// 0x00004000
#define kAPSAudioSessionAudioFormat_PCM_48KHz_16Bit_Stereo		( 1 << 15 )	// 0x00008000
#define kAPSAudioSessionAudioFormat_PCM_48KHz_24Bit_Mono		( 1 << 16 )	// 0x00010000
#define kAPSAudioSessionAudioFormat_PCM_48KHz_24Bit_Stereo		( 1 << 17 )	// 0x00020000

// The following compressed formats are used to indicate support for decoding/encoding of
// audio data via the AudioConverter interface.  An AudioConverter instance will be used
// to decode the compressed audio to (or encode from) the equivalent PCM format prior to
// the samples being delivered to (or read from) the corresponding AudioStream object. It
// is therefore required that the corresponding PCM format be specified in addition to
// each of the compressed formats returned from APSAudioSessionGetSupportedFormats.
#define kAPSAudioSessionAudioFormat_AAC_LC_44KHz_Stereo			( 1 << 22 )	// 0x00400000
#define kAPSAudioSessionAudioFormat_AAC_LC_48KHz_Stereo			( 1 << 23 )	// 0x00800000
#define kAPSAudioSessionAudioFormat_AAC_ELD_44KHz_Stereo		( 1 << 24 )	// 0x01000000
#define kAPSAudioSessionAudioFormat_AAC_ELD_48KHz_Stereo		( 1 << 25 )	// 0x02000000
#define kAPSAudioSessionAudioFormat_AAC_ELD_16KHz_Mono			( 1 << 26 ) // 0x04000000
#define kAPSAudioSessionAudioFormat_AAC_ELD_24KHz_Mono			( 1 << 27 ) // 0x08000000
#define kAPSAudioSessionAudioFormat_OPUS_16KHz_Mono				( 1 << 28 ) // 0x10000000
#define kAPSAudioSessionAudioFormat_OPUS_24KHz_Mono				( 1 << 29 ) // 0x20000000
#define kAPSAudioSessionAudioFormat_OPUS_48KHz_Mono				( 1 << 30 ) // 0x40000000
	
// The keys below are used to describe a list of latencies prior to AudioStream setup.
// We need these latencies due to the restrictions of the audio stack on iOS.
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
	
// [Number] Stream type. See AudioStreamType.
#define kAPSAudioSessionKey_Type					CFSTR( "type" )
	
// [String] Type of audio content (e.g. telephony, media, etc.). See kAudioStreamAudioType_*.
#define kAPSAudioSessionKey_AudioType				CFSTR( "audioType" )
	
// [Number] Number of audio channels (e.g. 2 for stereo).
#define kAPSAudioSessionKey_Channels				CFSTR( "ch" )

// [Number] Type of compression used. See kAPSAudioSessionKeyCompressionType_* constants.
#define kAPSAudioSessionKey_CompressionType			CFSTR( "ct" )
	#define kAPSAudioSessionKeyCompressionType_PCM			( 1 << 0 ) // 0x01: Uncompressed PCM.
	#define kAPSAudioSessionKeyCompressionType_AAC_LC		( 1 << 2 ) // 0x04: AAC Low Complexity (AAC-LC).
	#define kAPSAudioSessionKeyCompressionType_AAC_ELD		( 1 << 3 ) // 0x08: AAC Enhanced Low Delay (AAC-ELD).
	#define kAPSAudioSessionKeyCompressionType_OPUS			( 1 << 5 ) // 0x20: Opus.
	
// [Number] Number of samples per second (e.g. 44100).
#define kAPSAudioSessionKey_SampleRate				CFSTR( "sr" )

// [Number] Bit size of each audio sample (e.g. "16").
#define	kAPSAudioSessionKey_SampleSize				CFSTR( "ss" )
	
// [Number] Input latency in microseconds.
#define kAPSAudioSessionKey_InputLatencyMicros		CFSTR( "inputLatencyMicros" )

// [Number] Output latency in microseconds.
#define kAPSAudioSessionKey_OutputLatencyMicros		CFSTR( "outputLatencyMicros" )
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionSetEventHandler
	@abstract	Sets the function to call when an event arrives.
*/
typedef uint32_t	APSAudioSessionEventType;
#define kAPSAudioSessionEventAudioServicesWereReset	1 // Underlying audio services were reset.
#define kAPSAudioSessionEventAudioInterrupted			2 // Interruption occured in the audio services.

#define APSAudioSessionEventToString( X ) ( \
	( (X) == kAPSAudioSessionEventAudioServicesWereReset )	? "Reset"		: \
	( (X) == kAPSAudioSessionEventAudioInterrupted )		? "Interrupted"	: \
															  "?" )

typedef void ( *APSAudioSessionEventHandler_f )( APSAudioSessionEventType inType, CFTypeRef inParam, void *inContext );
void	APSAudioSessionSetEventHandler( APSAudioSessionEventHandler_f inHandler, void *inContext );
typedef void ( *AudioSessionSetEventHandler_f )( APSAudioSessionEventHandler_f inHandler, void *inContext );

// [Boolean] Read-only.  Returns kCFBooleanTrue if the session supports varispeed.
#define kAPSAudioSessionProperty_SupportsVarispeed					CFSTR( "supportsVarispeed" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionCopyProperty
	@abstract	Copies a property of the underlying audio session and hardware.
*/
CFTypeRef	APSAudioSessionCopyProperty( CFStringRef inPropertyName, OSStatus *outErr );
typedef CFTypeRef ( *APSAudioSessionCopyProperty_f )( CFStringRef inPropertyName, OSStatus *outErr );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionCopyLatencies
	@abstract	Copies audio latencies for all stream types, audio types and audio formats supported by the underlying hardware.
*/
CFArrayRef	APSAudioSessionCopyLatencies( OSStatus *outErr );
typedef CFArrayRef ( *APSAudioSessionCopyLatencies_f )( OSStatus *outErr );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionEnsureSetup
	@abstract	Ensure that audio services are initialized.
*/
void
	APSAudioSessionEnsureSetup(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros );
typedef void
	( *APSAudioSessionEnsureSetup_f )(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionEnsureTornDown
	@abstract	Signal that any audio services required by AirPlay are no longer required.
*/
void	APSAudioSessionEnsureTornDown( void );
typedef void ( *APSAudioSessionEnsureTornDown_f )( void );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionGetSupportedFormats
	@abstract	Gets the set of formats that are supported by the underlying hardware for the requested stream and audio type.
*/
APSAudioSessionAudioFormat	APSAudioSessionGetSupportedFormats( AudioStreamType inStreamType, CFStringRef inAudioType );
typedef APSAudioSessionAudioFormat ( *APSAudioSessionGetSupportedFormats_f )( AudioStreamType inStreamType, CFStringRef inAudioType );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionGetCompatibilityInputFormats
	@abstract	Gets the set of input formats for the "compatibility" type that are supported by the underlying hardware
	for the requested stream.
*/
APSAudioSessionAudioFormat	APSAudioSessionGetCompatibilityInputFormats( AudioStreamType inStreamType );
typedef APSAudioSessionAudioFormat ( *APSAudioSessionGetCompatibilityInputFormats_f )( AudioStreamType inStreamType );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APSAudioSessionGetCompatibilityOutputFormats
	@abstract	Gets the set of output formats for the "compatibility" type that are supported by the underlying hardware
	for the requested stream.
*/
APSAudioSessionAudioFormat	APSAudioSessionGetCompatibilityOutputFormats( AudioStreamType inStreamType );
typedef APSAudioSessionAudioFormat ( *APSAudioSessionGetCompatibilityOutputFormats_f )( AudioStreamType inStreamType );

#ifdef __cplusplus
}
#endif

#endif	// __APSAudioSession_h__
