/*
	File:    	AirPlayCommon.h
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayCommon_h__
#define	__AirPlayCommon_h__

#include <CoreUtils/CommonServices.h>

#if 0
#pragma mark == Configuration ==
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

//#if( 1 && !defined( AIRPLAY_CONFIG_FILE_PATH ) )
//	#define AIRPLAY_CONFIG_FILE_PATH		"/etc/airplay.conf"
//#endif

#define AIRPLAY_CONFIG_FILE_PATH getenv("MH_CARPLAY_CONFIG_FILE_PATH")?getenv("MH_CARPLAY_CONFIG_FILE_PATH") : "/etc/carplay.ini"

// AIRPLAY_HTTP_SERVER_LEGACY: 1=Support legacy HTTP server port. 0=Only one, kAirPlayFixedPort_MediaControl(7000) port is supported.

#if( !defined( AIRPLAY_HTTP_SERVER_LEGACY ) )
		#define AIRPLAY_HTTP_SERVER_LEGACY	0
#endif

// AIRPLAY_THREADED_MAIN: 1=Run AirPlay's "main" function in its own thread instead of exporting main (for non-process OS's).

#if( !defined( AIRPLAY_THREADED_MAIN ) )
		#define AIRPLAY_THREADED_MAIN		0
#endif

// AIRTUNES_DYNAMIC_PORTS: 1=Bind to dynamic ports. 0=bind to fixed ports.

#if( !defined( AIRTUNES_DYNAMIC_PORTS ) )
	#define AIRTUNES_DYNAMIC_PORTS		1
#endif

#if 0
#pragma mark -
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kAirPlayConnectTimeoutSecs				10
#define kAirPlayConnectTimeoutNanos				( kAirPlayConnectTimeoutSecs * UINT64_C_safe( kNanosecondsPerSecond ) )
#define kAirPlayControlKeepAliveIntervalSecs	10
#define kAirPlayDataTimeoutSecs					30
#define kAirPlayStoppingDataTimeoutSecs			3

#define kAirPlayFixedPort_RTSPControl			5000 // TCP port for RTSP control.
#define kAirPlayFixedPort_RTSPEvents			5001 // TCP port for RTSP events.
#define kAirPlayFixedPort_DACP					5010 // TCP port for DACP events.
#define kAirPlayFixedPort_KeepAlive				5020 // UDP port for keep alive mechanism
#define kAirPlayFixedPort_RTPAudio				6000 // TCP or UDP port for RTP audio data.
#define	kAirPlayFixedPort_RTCPLegacy			6001 // Old UDP port for RTCP packets (time announces, retransmits). Only for old devices.
#define	kAirPlayFixedPort_TimeSyncLegacy		6002 // Old UDP port for time synchronization. Only for old devices.
#define kAirPlayFixedPort_RTPAltAudio			6003 // UDP port for receiving RTP alt audio data.
#define	kAirPlayFixedPort_RTCPClient			6010 // UDP port for receiving retransmit requests.
#define	kAirPlayFixedPort_RTCPServer			6011 // UDP port for receiving time announcements and retransmit responses.
#define	kAirPlayFixedPort_TimeSyncClient		6020 // UDP port for receiving time sync responses.
#define	kAirPlayFixedPort_TimeSyncServer		6021 // UDP port for receiving time sync requests.
#define kAirPlayFixedPort_RTPScreen				6030 // TCP port for receiving RTP screen data.
#define kAirPlayFixedPort_PerfDataClient		6100 // Base TCP or UDP port for receiving ACKs from a performance testing server.
#define kAirPlayFixedPort_PerfDataServer		6200 // Base TCP or UDP port for receiving data from a performance testing client.
#define	kAirPlayFixedPort_MediaControl			7000 // TCP port for control and events for photos, slideshow, video, etc.
#define	kAirPlayFixedPort_MediaData				7001 // TCP port for video file data.
#define	kAirPlayFixedPort_ScreenNTPServer		7010 // UDP port for screen time synchronization requests.
#define	kAirPlayFixedPort_ScreenNTPClient		7011 // UDP port for screen time synchronization responses.
#define	kAirPlayFixedPort_ScreenData			7100 // TCP port for screen data (H.264 frames, meta data, etc.).

#if( AIRTUNES_DYNAMIC_PORTS )
	#define	kAirPlayPort_DACP					0
	#define kAirPlayPort_KeepAlive				0
	#define	kAirPlayPort_MediaControl			kAirPlayFixedPort_MediaControl
	#define	kAirPlayPort_MediaData				kAirPlayFixedPort_MediaData
	#define	kAirPlayPort_RTCPClient				0
	#define	kAirPlayPort_RTCPServer				0
	#define	kAirPlayPort_RTPAudio				0
	#define	kAirPlayPort_RTPScreen				0
	#define kAirPlayPort_RTPAltAudio			0
	#define	kAirPlayPort_RTSPControl			kAirPlayFixedPort_RTSPControl
	#define	kAirPlayPort_RTSPEvents				0
	#define	kAirPlayPort_ScreenData				kAirPlayFixedPort_ScreenData
	#define	kAirPlayPort_ScreenNTPClient		kAirPlayFixedPort_ScreenNTPClient
	#define	kAirPlayPort_ScreenNTPServer		kAirPlayFixedPort_ScreenNTPServer
	#define	kAirPlayPort_TimeSyncClient			0
	#define	kAirPlayPort_TimeSyncServer			0
#else
	#define	kAirPlayPort_DACP					kAirPlayFixedPort_DACP
	#define kAirPlayPort_KeepAlive				kAirPlayFixedPort_KeepAlive
	#define	kAirPlayPort_MediaControl			kAirPlayFixedPort_MediaControl
	#define	kAirPlayPort_MediaData				kAirPlayFixedPort_MediaData
	#define	kAirPlayPort_RTCPClient				kAirPlayFixedPort_RTCPClient
	#define	kAirPlayPort_RTCPServer				kAirPlayFixedPort_RTCPServer
	#define	kAirPlayPort_RTPAudio				kAirPlayFixedPort_RTPAudio
	#define	kAirPlayPort_RTPScreen				kAirPlayFixedPort_RTPScreen
	#define kAirPlayPort_RTPAltAudio			kAirPlayFixedPort_RTPAltAudio
	#define	kAirPlayPort_RTSPControl			kAirPlayFixedPort_RTSPControl
	#define	kAirPlayPort_RTSPEvents				kAirPlayFixedPort_RTSPEvents
	#define	kAirPlayPort_ScreenData				kAirPlayFixedPort_ScreenData
	#define	kAirPlayPort_ScreenNTPClient		kAirPlayFixedPort_ScreenNTPClient
	#define	kAirPlayPort_ScreenNTPServer		kAirPlayFixedPort_ScreenNTPServer
	#define	kAirPlayPort_TimeSyncClient			kAirPlayFixedPort_TimeSyncClient
	#define	kAirPlayPort_TimeSyncServer			kAirPlayFixedPort_TimeSyncServer
#endif

#define kAirTunesAESKeyLen						16

#define kAirPlayHTTPAuthenticationUsername		"AirPlay"
#define kAirPlayHTTPAuthenticationRealm			"AirPlay"
#define kAirPlayHTTPHeader_ClientName			"X-Apple-Client-Name"		// Name of the client device.
#define kAirPlayHTTPHeader_DeviceID				"X-Apple-Device-ID"			// Primary MAC address as a 64-bit hex value (e.g. 0x112233aabbcc).
#define kAirPlayHTTPHeader_Durations			"X-Apple-Durations"			// Semicolon-separated list of <name>=<milliseconds> durations.
#define kAirPlayHTTPHeader_HomeKitPairing		"X-Apple-HKP"				// Indicates if HomeKit pairing should be used.
#define kAirPlayHTTPHeader_EncryptionType		"X-Apple-ET"				// AirPlayEncryptionType to use.
#define kAirPlayHTTPHeader_PairDerive			"X-Apple-PD"				// Indicates if keys should be derived from pairing info.
#define kAirPlayHTTPHeader_ProtocolVersion		"X-Apple-ProtocolVersion"	// Version of the protocol.
#define kAirPlayHTTPHeader_AbsoluteTime			"X-Apple-AbsoluteTime"		// Current CFAbsoluteTime, seconds only
#define kAirTunesHTTPVersionStr					"RTSP/1.0"
#define kAirPlayUUIDNameSpace					( (const uint8_t *) "\xBB\x40\x80\x51\xCC\x46\x40\x0B\x80\x68\xE9\x0D\xCB\xE8\xFB\x2B" )

// Audio

#define kAirPlayAudioBufferMainAltWiredMs	  10 // 10 ms over wired.
#define kAirPlayAudioBufferMainAltWiFiMs	  80 // 80 ms over WiFi.
#define kAirPlayAudioBufferMainHighMs	    1000 // 1000 ms.
#define kAirPlayAudioLatencyOther		   88200 // 2 seconds @ 44100 Hz.
#define kAirPlayAudioLatencyScreen		    3750 // 85 ms @ 44100 Hz.
#define kAirPlayAudioLatencyThreshold	   13230 // 300 ms @ 44100 Hz. Latency <= this uses redundant audio.
#define kAirPlayScreenLatencyWiredMs	      25 // 25 ms over wired.
#define kAirPlayScreenLatencyWiFiMs		      75 // 75 ms over WiFi.

#define kAirPlayAudioBitrateLowLatencyUpTo24KHz	   48000 // 48 kbps.
#define kAirPlayAudioBitrateLowLatencyUpTo32KHz	   64000 // 64 kbps.
#define kAirPlayAudioBitrateLowLatencyUpTo48KHz	   96000 // 96 kbps.

#define kAirPlayAudioBitrateHighLatency	  256000 // 256 kbps.

#define kAirPlaySamplesPerPacket_PCM		 352 // Max samples per packet for PCM.
#define kAirTunesSampleRate				   44100 // 44100 Hz.
#define AirTunesSamplesToMs( X )			( ( ( 1000 * (X) ) + ( kAirTunesSampleRate / 2 ) ) / kAirTunesSampleRate )
#define AirTunesMsToSamples( X )			( ( (X) * kAirTunesSampleRate ) / 1000 )
#define kAirTunesMaxSkewAdjustRate			 441 // Max number of samples to insert/remove per second for skew compensation,
#define kAirTunesPlayoutDelay			   11025 // 250 ms delay to sync with AirPort Express'es 250 ms buffer.
#define	kAirTunesBitsPerSample				  16 // 16-bit samples.
#define	kAirTunesChannelCount				   2 // Left + Right Channels
#define	kAirTunesBytesPerUnit				   4 // 2 bytes per channel and 2 channels per unit = 4 bytes.

#define kAirTunesInvalidVolumeDB			-999
#define kAirTunesSilenceVolumeDB			-144
#define kAirTunesMinVolumeDB				 -30
#define kAirTunesMaxVolumeDB				   0
#define kAirTunesDisabledVolumeDB			   1 // Special flag meaning "volume control is disabled" (line level).

#define	kAirTunesMaxPacketSizeUDP			1472 // Enet MTU (1500) - IPv4 header (20) - UDP header (8) = 1472 (1460 RTP payload).
												 // Enet MTU (1500) - IPv6 header (40) - UDP header (8) = 1452 (1440 RTP payload).
												 // 12 byte RTP header + 1460 max payload (IPv4) or 1440 max payload (IPv6). 
#define	kAirTunesMaxPayloadSizeUDP			1440 // Enet MTU (1500) - IPv6 header (40) - UDP header (8) - RTP header (12) = 1440.

// Max Apple Lossless size is: samplesPerFrame * channelCount * ((10 + sampleSize) / 8) + 1. For example:
//
// AirTunes TCP: (4096 * 2 * ((10 + 16) / 8)) + 1 = 24577 and when rounded up to a power of 2 -> 32KB
// AirTunes UDP: ( 352 * 2 * ((10 + 16) / 8)) + 1 =  2113 and when rounded up to a power of 2 ->  4KB

#define AppleLosslessBufferSize( SAMPLES_PER_FRAME, CHANNEL_COUNT, BITS_PER_SAMPLE ) \
	( ( (SAMPLES_PER_FRAME) * (CHANNEL_COUNT) * ( ( 10 + (BITS_PER_SAMPLE) ) / 8 ) ) + 1 )

#if( !defined( AirPlayIsBusyError ) )
	#define AirPlayIsBusyError( X ) ( \
		( (X) == HTTPStatusToOSStatus( kHTTPStatus_NotEnoughBandwidth ) ) || \
		( (X) == kHTTPStatus_NotEnoughBandwidth ) || \
		( (X) == kAlreadyInUseErr ) )
#endif

// AirPlayAudioJackStatus

#define kAirPlayAudioJackStatus_Disconnected			"disconnected"				// Nothing plugged in
#define kAirPlayAudioJackStatus_ConnectedAnalog			"connected; type=analog"	// Analog cable plugged in.
#define kAirPlayAudioJackStatus_ConnectedDigital		"connected; type=digital"	// Digital/optical cable plugged in.
#define kAirPlayAudioJackStatus_ConnectedUnknown		"connected"					// Unknown cable plugged in or can't detect cable.

// AirPlayCompressionType

typedef uint32_t	AirPlayCompressionType;
#define kAirPlayCompressionType_Undefined		0
#define kAirPlayCompressionType_PCM				( 1 << 0 ) // 0x01: Uncompressed PCM.
#define kAirPlayCompressionType_ALAC			( 1 << 1 ) // 0x02: Apple Lossless (ALAC).
#define kAirPlayCompressionType_AAC_LC			( 1 << 2 ) // 0x04: AAC Low Complexity (AAC-LC).
#define kAirPlayCompressionType_AAC_ELD			( 1 << 3 ) // 0x08: AAC Enhanced Low Delay (AAC-ELD).
#define kAirPlayCompressionType_H264			( 1 << 4 ) // 0x10: H.264 video.
#define kAirPlayCompressionType_OPUS			( 1 << 5 ) // 0x20: Opus.

#define AirPlayCompressionTypeToString( X ) ( \
	( (X) == kAirPlayCompressionType_PCM )		? "PCM"		: \
	( (X) == kAirPlayCompressionType_ALAC )		? "ALAC"	: \
	( (X) == kAirPlayCompressionType_AAC_LC )	? "AAC-LC"	: \
	( (X) == kAirPlayCompressionType_AAC_ELD )	? "AAC-ELD"	: \
	( (X) == kAirPlayCompressionType_H264 )		? "H.264"	: \
	( (X) == kAirPlayCompressionType_OPUS )		? "Opus"	: \
												  "?" )

// AirPlayDiscoveryMode

typedef uint32_t AirPlayDiscoveryMode;
#define kAirPlayDiscoveryMode_None				0 // No browse.
#define kAirPlayDiscoveryMode_Presence			1 // Browse until at least one instance of the given service is found.
#define kAirPlayDiscoveryMode_Detailed			2 // Full browse to find all instances of the given service. 

// AirPlayDisplayFeatures

typedef uint32_t	AirPlayDisplayFeatures;
#define kAirPlayDisplayFeatures_Knobs					( 1 << 1 ) // 0x02: Supports interacting via knobs.
#define kAirPlayDisplayFeatures_LowFidelityTouch		( 1 << 2 ) // 0x04: Supports interacting via low fidelity touch.
#define kAirPlayDisplayFeatures_HighFidelityTouch		( 1 << 3 ) // 0x08: Supports interacting via high fidelity touch.
#define kAirPlayDisplayFeatures_Touchpad				( 1 << 4 ) // 0x10: Supports interacting via touchpad.

typedef uint32_t	AirPlayDisplayPrimaryInputDevice;
#define kAirPlayDisplayPrimaryInputDeviceUndeclared		0
#define kAirPlayDisplayPrimaryInputDeviceTouch			1
#define kAirPlayDisplayPrimaryInputDeviceTouchpad		2
#define kAirPlayDisplayPrimaryInputDeviceKnobs			3

// AirPlayEncryptionType

typedef uint32_t	AirPlayEncryptionType;
#define kAirPlayEncryptionType_Undefined				0
#define kAirPlayEncryptionType_None						( 1 << 0 ) // 0x01: If set, encryption is not required.
#define kAirPlayEncryptionType_MFi_SAPv1				( 1 << 4 ) // 0x10: If set, 128-bit AES key encrypted with MFi-SAPv1 is supported.

#define AirPlayEncryptionTypeToString( X ) \
	( ( (X) == kAirPlayEncryptionType_None )				? "None"	: \
	  ( (X) == kAirPlayEncryptionType_MFi_SAPv1 )			? "MFi"		: \
															  "?" ) 

// Prefixes used for deriving stream-specific keys and IVs:
//
//		Key = Bytes 0-15 of SHA-512("AirPlayStreamKey" + <"read" | "write"> + <stream ID> + <session key>)
//		IV  = Bytes 0-15 of SHA-512("AirPlayStreamIV"  + <"read" | "write"> + <stream ID> + <session key>)

#define kAirPlayEncryptionStreamPrefix_Key			"AirPlayStreamKey"
#define kAirPlayEncryptionStreamPrefix_IV			"AirPlayStreamIV"
#define kAirPlayTLS_PSK_Salt						"Pair-TLS-PSK"

// AirPlaySessionType

typedef uint32_t	AirPlaySessionType;
#define kAirPlaySessionType_Screen			0
#define kAirPlaySessionType_Audio			1

// AirPlayStreamType

typedef uint32_t	AirPlayStreamType;
#define kAirPlayStreamType_Invalid			0 // Reserved for an invalid type.
#define kAirPlayStreamType_GeneralAudio		96	// RTP payload type for general audio output (2 second latency). UDP.
#define kAirPlayStreamType_MainAudio		100 // RTP payload type for low-latency audio input/output. UDP.
#define kAirPlayStreamType_AltAudio			101 // RTP payload type for low-latency UI sounds, alerts, etc. output. UDP.
#define kAirPlayStreamType_MainHighAudio	102 // RTP payload type for high-latency audio output. UDP.
#define kAirPlayStreamType_Screen			110 // RTP payload type for H.264 screen output. TCP.
#define kAirPlayStreamType_Playback			120 // AirPlay Video commands

#define AirPlayStreamTypeToString( X ) \
	( ( (X) == kAirPlayStreamType_GeneralAudio )	? "GeneralAudio"	: \
	  ( (X) == kAirPlayStreamType_MainAudio )		? "MainAudio"		: \
	  ( (X) == kAirPlayStreamType_MainHighAudio )	? "MainHighAudio"	: \
	  ( (X) == kAirPlayStreamType_AltAudio )		? "AltAudio"		: \
	  ( (X) == kAirPlayStreamType_Screen )			? "Screen"			: \
													  "?" )

#define AirPlayStreamTypeToCFString( X ) \
	( ( (X) == kAirPlayStreamType_GeneralAudio )	? CFSTR( "GeneralAudio" )	: \
	  ( (X) == kAirPlayStreamType_MainAudio )		? CFSTR( "MainAudio" )		: \
	  ( (X) == kAirPlayStreamType_MainHighAudio )	? CFSTR( "MainHighAudio" )	: \
	  ( (X) == kAirPlayStreamType_AltAudio )		? CFSTR( "AltAudio" )		: \
	  ( (X) == kAirPlayStreamType_Screen )			? CFSTR( "Screen" )			: \
													  CFSTR( "?" ) )

// AirPlayStatusFlags

typedef uint32_t	AirPlayStatusFlags;
#define kAirPlayStatusFlag_None						0
#define kAirPlayStatusFlag_ProblemsExist			( 1 << 0 ) // 0x0001: Problem has been detected.
#define kAirPlayStatusFlag_Unconfigured				( 1 << 1 ) // 0x0002: Device is not configured.
#define kAirPlayStatusFlag_AudioLink				( 1 << 2 ) // 0x0004: Audio cable is attached.
	#define kAirPlayStatusFlag_PairPIN				( 1 << 9 ) // 0x0200: PIN required to pair.

// WiFi Statistics Reporting

#define kAirPlayWifiStatUpdateIntervalSeconds		3

// Constants for deriving session encryption keys.

#define kAirPlayPairingControlKeySaltPtr			"Control-Salt"
#define kAirPlayPairingControlKeySaltLen			sizeof_string( kAirPlayPairingControlKeySaltPtr )
#define kAirPlayPairingControlKeyReadInfoPtr		"Control-Read-Encryption-Key"
#define kAirPlayPairingControlKeyReadInfoLen		sizeof_string( kAirPlayPairingControlKeyReadInfoPtr )
#define kAirPlayPairingControlKeyWriteInfoPtr		"Control-Write-Encryption-Key"
#define kAirPlayPairingControlKeyWriteInfoLen		sizeof_string( kAirPlayPairingControlKeyWriteInfoPtr )

#define kAirPlayPairingEventsKeySaltPtr				"Events-Salt"
#define kAirPlayPairingEventsKeySaltLen				sizeof_string( kAirPlayPairingEventsKeySaltPtr )
#define kAirPlayPairingEventsKeyReadInfoPtr			"Events-Read-Encryption-Key"
#define kAirPlayPairingEventsKeyReadInfoLen			sizeof_string( kAirPlayPairingEventsKeyReadInfoPtr )
#define kAirPlayPairingEventsKeyWriteInfoPtr		"Events-Write-Encryption-Key"
#define kAirPlayPairingEventsKeyWriteInfoLen		sizeof_string( kAirPlayPairingEventsKeyWriteInfoPtr )

#define kAirPlayPairingDataStreamKeySaltPtr			"DataStream-Salt"
#define kAirPlayPairingDataStreamKeySaltLen			sizeof_string( kAirPlayPairingDataStreamKeySaltPtr )
#define kAirPlayPairingDataStreamKeyInputInfoPtr	"DataStream-Input-Encryption-Key"
#define kAirPlayPairingDataStreamKeyInputInfoLen	sizeof_string( kAirPlayPairingDataStreamKeyInputInfoPtr )
#define kAirPlayPairingDataStreamKeyOutputInfoPtr	"DataStream-Output-Encryption-Key"
#define kAirPlayPairingDataStreamKeyOutputInfoLen	sizeof_string( kAirPlayPairingDataStreamKeyOutputInfoPtr )

// Constants for vehicle information

/*---------------------------------------------------------------------------------------------------------------------------
	ETC: Indicates that an electronic toll collection device is installed in the vehicle (an ETC icon will be shown). [CFDictionary]
	
	Keys
	------------
	kAirPlayKey_Active	-- Whether or not ETC is currently active in the vehicle (determines whether gray or color icon is shown).
 */
#define kAirPlayVehicleInformation_ETC			"ElectronicTollCollection"

#if 0
#pragma mark -
#pragma mark == Audio Formats ==
#endif

//===========================================================================================================================
//	Audio Formats
//===========================================================================================================================

typedef uint64_t		AirPlayAudioFormat;
#define kAirPlayAudioFormat_Invalid						0
#define kAirPlayAudioFormat_Reserved1					( 1 << 0 )	// 0x00000001
#define kAirPlayAudioFormat_Reserved2					( 1 << 1 )	// 0x00000002
#define kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono			( 1 << 2 )	// 0x00000004
#define kAirPlayAudioFormat_PCM_8KHz_16Bit_Stereo		( 1 << 3 )	// 0x00000008
#define kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono		( 1 << 4 )	// 0x00000010
#define kAirPlayAudioFormat_PCM_16KHz_16Bit_Stereo		( 1 << 5 )	// 0x00000020
#define kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono		( 1 << 6 )	// 0x00000040
#define kAirPlayAudioFormat_PCM_24KHz_16Bit_Stereo		( 1 << 7 )	// 0x00000080
#define kAirPlayAudioFormat_PCM_32KHz_16Bit_Mono		( 1 << 8 )	// 0x00000100
#define kAirPlayAudioFormat_PCM_32KHz_16Bit_Stereo		( 1 << 9 )	// 0x00000200
#define kAirPlayAudioFormat_PCM_44KHz_16Bit_Mono		( 1 << 10 )	// 0x00000400
#define kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo		( 1 << 11 )	// 0x00000800
#define kAirPlayAudioFormat_PCM_44KHz_24Bit_Mono		( 1 << 12 )	// 0x00001000
#define kAirPlayAudioFormat_PCM_44KHz_24Bit_Stereo		( 1 << 13 )	// 0x00002000
#define kAirPlayAudioFormat_PCM_48KHz_16Bit_Mono		( 1 << 14 )	// 0x00004000
#define kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo		( 1 << 15 )	// 0x00008000
#define kAirPlayAudioFormat_PCM_48KHz_24Bit_Mono		( 1 << 16 )	// 0x00010000
#define kAirPlayAudioFormat_PCM_48KHz_24Bit_Stereo		( 1 << 17 )	// 0x00020000
#define kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo		( 1 << 18 )	// 0x00040000
#define kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo		( 1 << 19 )	// 0x00080000
#define kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo		( 1 << 20 )	// 0x00100000
#define kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo		( 1 << 21 )	// 0x00200000
#define kAirPlayAudioFormat_AAC_LC_44KHz_Stereo			( 1 << 22 )	// 0x00400000
#define kAirPlayAudioFormat_AAC_LC_48KHz_Stereo			( 1 << 23 )	// 0x00800000
#define kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo		( 1 << 24 )	// 0x01000000
#define kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo		( 1 << 25 )	// 0x02000000
#define kAirPlayAudioFormat_AAC_ELD_16KHz_Mono			( 1 << 26 ) // 0x04000000
#define kAirPlayAudioFormat_AAC_ELD_24KHz_Mono			( 1 << 27 ) // 0x08000000
#define kAirPlayAudioFormat_OPUS_16KHz_Mono				( 1 << 28 ) // 0x10000000
#define kAirPlayAudioFormat_OPUS_24KHz_Mono				( 1 << 29 ) // 0x20000000
#define kAirPlayAudioFormat_OPUS_48KHz_Mono				( 1 << 30 ) // 0x40000000

#define kAirPlayAudioFormatMask_PCM						( kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_8KHz_16Bit_Stereo		| \
														  kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_16KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_24KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_32KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_32KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_44KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_44KHz_24Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_44KHz_24Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_48KHz_16Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_PCM_48KHz_24Bit_Mono		| \
														  kAirPlayAudioFormat_PCM_48KHz_24Bit_Stereo	)

#define kAirPlayAudioFormatMask_ALAC					( kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo	| \
														  kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo	| \
														  kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo	)

#define kAirPlayAudioFormatMask_AAC_LC					( kAirPlayAudioFormat_AAC_LC_44KHz_Stereo		| \
														  kAirPlayAudioFormat_AAC_LC_48KHz_Stereo		)

#define kAirPlayAudioFormatMask_AAC_ELD					( kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo		| \
														  kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo		| \
														  kAirPlayAudioFormat_AAC_ELD_16KHz_Mono		| \
														  kAirPlayAudioFormat_AAC_ELD_24KHz_Mono		)

#define kAirPlayAudioFormatMask_OPUS					( kAirPlayAudioFormat_OPUS_16KHz_Mono			| \
														  kAirPlayAudioFormat_OPUS_24KHz_Mono			| \
														  kAirPlayAudioFormat_OPUS_48KHz_Mono           )


#define AirPlayAudioFormatToString( X ) ( \
	( (X) == kAirPlayAudioFormat_Invalid )					? "Invalid"			: \
	( (X) == kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono )		? "PCM/8000/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_8KHz_16Bit_Stereo )	? "PCM/8000/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono )		? "PCM/16000/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_16KHz_16Bit_Stereo )	? "PCM/16000/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono )		? "PCM/24000/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_24KHz_16Bit_Stereo )	? "PCM/24000/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_32KHz_16Bit_Mono )		? "PCM/32000/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_32KHz_16Bit_Stereo )	? "PCM/32000/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_44KHz_16Bit_Mono )		? "PCM/44100/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo )	? "PCM/44100/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_44KHz_24Bit_Mono )		? "PCM/44100/24/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_44KHz_24Bit_Stereo )	? "PCM/44100/24/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_48KHz_16Bit_Mono )		? "PCM/48000/16/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo )	? "PCM/48000/16/2"	: \
	( (X) == kAirPlayAudioFormat_PCM_48KHz_24Bit_Mono )		? "PCM/48000/24/1"	: \
	( (X) == kAirPlayAudioFormat_PCM_48KHz_24Bit_Stereo )	? "PCM/48000/24/2"	: \
	( (X) == kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo )	? "ALAC/44100/16/2"	: \
	( (X) == kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo )	? "ALAC/44100/24/2"	: \
	( (X) == kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo )	? "ALAC/48000/16/2"	: \
	( (X) == kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo )	? "ALAC/48000/24/2"	: \
	( (X) == kAirPlayAudioFormat_AAC_LC_44KHz_Stereo )		? "AAC-LC/44100/2"	: \
	( (X) == kAirPlayAudioFormat_AAC_LC_48KHz_Stereo )		? "AAC-LC/48000/2"	: \
	( (X) == kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo )		? "AAC-ELD/44100/2"	: \
	( (X) == kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo )		? "AAC-ELD/48000/2"	: \
	( (X) == kAirPlayAudioFormat_AAC_ELD_16KHz_Mono )		? "AAC-ELD/16000/1"	: \
	( (X) == kAirPlayAudioFormat_AAC_ELD_24KHz_Mono )		? "AAC-ELD/24000/1"	: \
	( (X) == kAirPlayAudioFormat_OPUS_16KHz_Mono )			? "OPUS/16000/1"	: \
	( (X) == kAirPlayAudioFormat_OPUS_24KHz_Mono )			? "OPUS/24000/1"	: \
	( (X) == kAirPlayAudioFormat_OPUS_48KHz_Mono )			? "OPUS/48000/1"	: \
															  "Unknown" )

#if 0
#pragma mark -
#pragma mark == Bonjour ==
#endif

//===========================================================================================================================
//	Bonjour
//===========================================================================================================================

// _airplay._tcp

#define kAirPlayBonjourServiceType					"_airplay._tcp."
#define kAirPlayBonjourServiceDomain				"local."

#define kAirPlayTXTKey_DeviceID						"deviceid"	// [String] Globally unique ID of the device. MUST be persistent across reboots.
#define kAirPlayTXTKey_Features						"features"	// [Number][AirPlayFeatures] Supported features.
#define kAirPlayTXTKey_Flags						"flags"		// [Hex integer][AirPlayStatusFlags] System flags.
#define kAirPlayTXTKey_FirmwareVersion				"fv"		// [String] Firmware Source Version (e.g. 1.2.3).
#define kAirPlayTXTKey_Model						"model"		// [String] Model of device (e.g. Device1,1). MUST be globally unique.
#define kAirPlayTXTKey_PublicHKID					"pi"		// [String] HomeKit identifier string - usually MAC address, but not necessarily
#define kAirPlayTXTKey_PublicKey					"pk"		// [Hex string] 32-byte Ed25519 public key.
#define kAirPlayTXTKey_ProtocolVersion				"protovers"	// [String] Protocol version string (e.g. "1.0"). Missing means 1.0.
#define kAirPlayTXTKey_Seed							"seed"		// [Integer] Config seed number. Increment on any software config change.
#define kAirPlayTXTKey_SourceVersion				"srcvers"	// [String] Source version string (e.g. "101.7").

#if 0
#pragma mark -
#pragma mark == Commands ==
#endif

//===========================================================================================================================
//	Commands
//===========================================================================================================================

// HTTP resource path for sending commands from an AirPlay client to an AirPlay server.
// Request payload:  Optional binary plist. See kAirPlayCommand_* for command-specific details.
// Response payload: Optional binary plist.
#define kAirPlayCommandPath					"/command"

/*---------------------------------------------------------------------------------------------------------------------------
	ActivationChanged: Tells the server that the device's activation state has changed.
	
	No request keys.
	No response keys.
 */
#define kAirPlayCommand_ActivationChanged	"activationChanged"

/*---------------------------------------------------------------------------------------------------------------------------
	ConfigChanged: Tells the server that the system's configuration has changed (e.g. network configuration changed).
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_ConfigChanged		"configChanged"

/*---------------------------------------------------------------------------------------------------------------------------
	UpdateAdvertising: Command to update advertising on the AirPlay receiver.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_UpdateAdvertising		"updateAdvertising"

/*---------------------------------------------------------------------------------------------------------------------------
	SetConfig: Sets the configuration to use for the accessory (e.g. WiFi SSID, name, password, etc.).
	
	The configuration should be validated and saved off, but not applied until the subsequent ApplyConfig command.
	Applying the configuration before ApplyConfig could cause a loss of connectivity and prevent the controller from 
	receiving the response. This would appear to the controller as a failure to set the configuration. Waiting for the 
	ApplyConfig command (which is not sent over the network, but handled internally) ensures a reliable configuration.
	
	Request keys
	------------
	kAirPlayKey_AdminPassword	-- Optional password to be required to administer the accessory.
	kAirPlayKey_Name			-- Optional name to give the accessory.
	kAirPlayKey_PlayPassword	-- Optional password to be required to play to the accessory.
	kAirPlayKey_WiFiPSK			-- PSK for joining a WPA-protected network.
	kAirPlayKey_WiFiSSID		-- SSID the accessory should join.
	
	Response keys
	-------------
	kAirPlayKey_Params			-- Optional parameters returned by the accessory (e.g. Apple App Store info).
	kAirPlayKey_Status			-- Result of setting the configuration.
*/
#define kAirPlayCommand_SetConfig			"setConfig"

/*---------------------------------------------------------------------------------------------------------------------------
	ApplyConfig: Applies the configuration previously set with the SetConfig command.
	
	Note: this command is never sent over the network. It's only used internally for reliable configuration.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_ApplyConfig			"applyConfig"

/*---------------------------------------------------------------------------------------------------------------------------
	DuckAudio: Ramps volume down (e.g. Fade down music to play a voice prompt).
	
	Request keys
	------------
	[kAirPlayKey_Decibels]		-- Relative decibel reduction after duck (e.g. -3 means 3 dB less than pre-Duck volume).
	[kAirPlayKey_DurationMs]	-- Number of milliseconds the ramp down should last.
	
	No response keys.
*/
#define kAirPlayCommand_DuckAudio			"duckAudio"

/*---------------------------------------------------------------------------------------------------------------------------
	UnduckAudio: Ramps volume back to the pre-duck level (e.g. restore music volume after an audible alert).
	
	Request keys
	------------
	kAirPlayKey_DurationMs	-- Number of milliseconds the ramp up should last.
	
	No response keys.
*/
#define kAirPlayCommand_UnduckAudio			"unduckAudio"

/*---------------------------------------------------------------------------------------------------------------------------
	FlushAudio: Flush any unplayed audio data. Used when pause is pressed, when the user seeks to a different spot, etc.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_FlushAudio			"flushAudio"

/*---------------------------------------------------------------------------------------------------------------------------
	DisableBluetooth: Disable Bluetooth connectivity to specified device.
	
	Request keys
	------------
	kAirPlayKey_DeviceID -- MAC address of Bluetooth device to disable connectivity to.
	
	No response keys.
*/
#define kAirPlayCommand_DisableBluetooth	"disableBluetooth"

/*---------------------------------------------------------------------------------------------------------------------------
	UpdateFeedback: Provides any input feedback from the controller and returns any output feedback to the controller.
	
	No request keys
	
	Response keys
	-------------
	kAirPlayKey_Streams			-- Array of streams with feedback.
		kAirPlayKey_Type		-- Type of stream.
		kAirPlayKey_SampleRate	-- Estimated sample rate
*/
#define kAirPlayCommand_UpdateFeedback		"updateFeedback"

/*---------------------------------------------------------------------------------------------------------------------------
	HIDSendReport: Sends a HID input report from accessory to controller.
	
	Request keys
	------------
	kAirPlayKey_HIDReport	-- Report to send.
	kAirPlayKey_UUID		-- UUID of the HID device to send the report from.
	
	No response keys.
*/
#define kAirPlayCommand_HIDSendReport		"hidSendReport"

/*---------------------------------------------------------------------------------------------------------------------------
	HIDSetReport: Sets HID report on a HID device. The HID device is specified by UUID.
	
	Request keys
	------------
	kAirPlayKey_HIDReport	-- Report to set.
	kAirPlayKey_UUID		-- UUID of the HID device to set the report on.
	
	No response keys.
*/
#define kAirPlayCommand_SetHIDReport		"hidSetReport"

/*---------------------------------------------------------------------------------------------------------------------------
	HIDCopyInputMode: Copies HID input mode from a HID device. The HID device is specified by UUID.
	
	Request keys
	------------
	kAirPlayKey_UUID			-- UUID of the HID device to set the report on.
	
	Response keys
	-------------
	kAirPlayKey_HIDInputMode	-- Input mode.
*/
#define kAirPlayCommand_HIDCopyInputMode		"hidCopyInputMode"

/*---------------------------------------------------------------------------------------------------------------------------
	HIDSetInputMode: Sets HID input mode on a HID device. The HID device is specified by UUID.
	
	Request keys
	------------
	kAirPlayKey_HIDInputMode	-- Input mode to set.
	kAirPlayKey_UUID			-- UUID of the HID device to set the input mode on.
	
	No response keys.
*/
#define kAirPlayCommand_HIDSetInputMode		"hidSetInputMode"

/*---------------------------------------------------------------------------------------------------------------------------
	iAPSendMessage: Sends an iAP message to the receiver.
 
	Request keys
	------------
	kAirPlayKey_Data			-- iAP message data to send.
 
	No response keys.
*/
#define kAirPlayCommand_iAPSendMessage		"iAPSendMessage"

/*---------------------------------------------------------------------------------------------------------------------------
	ForceKeyFrame: Tells the server to request a key frame from the sender. Used when the decoder crashes, etc.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_ForceKeyFrame		"forceKeyFrame"

/*---------------------------------------------------------------------------------------------------------------------------
	GetLogs: Generate an archive of currently available logs on the receiver and provide a path to the generated archive.
 
	No request keys.
 
	Response keys
	-------------
	kAirPlayKey_Path						-- Path to the generated log archive
*/
#define kAirPlayCommand_GetLogs				"getLogs"

/*---------------------------------------------------------------------------------------------------------------------------
	ChangeModes: For the accessory to requests mode changes (e.g. take main audio).
	
	Request keys
	------------
	kAirPlayKey_AppStates					-- App states to change. Each dictionary entry contains the following keys:
		kAirPlayKey_AppStateID				-- ID of app state to change.
		[kAirPlayKey_State]					-- For non-speech app states, this is whether the app state is true/false.
		[kAirPlayKey_SpeechMode]			-- For speech, this is the speech mode to change to.
	
	kAirPlayKey_Resources					-- Resources to change. Each dictionary entry contains the following keys:
		kAirPlayKey_ResourceID				-- ID of resource to change.
		kAirPlayKey_TransferType			-- Type of change to make (e.g. take, borrow, etc.).
		[kAirPlayKey_TransferPriority]		-- If "take" or "borrow", priority of request (e.g. nice-to-have, etc.).
		[kAirPlayKey_TakeConstraint]		-- If "take", constraint for peer to take. Missing otherwise.
		[kAirPlayKey_BorrowConstraint]		-- If "take", constraint for peer to borrow. Missing otherwise.
		[kAirPlayKey_UnborrowConstraint]	-- If "borrow", constraint for peer to unborrow. Missing otherwise.
	
	Response keys
	-------------
	kAirPlayKey_Params		-- Latest modes property. See the ModesChanged command for keys, etc.
	kAirPlayKey_Status		-- Result of perform the mode change. If this is non-zero, the change failed.
*/
#define kAirPlayCommand_ChangeModes			"changeModes"

/*---------------------------------------------------------------------------------------------------------------------------
	ModesChanged: For the controller to indicate to the accessory that modes have changed (e.g. now on a phone call).
	
	Request keys
	------------
	kAirPlayKey_AppStates 			-- Changed app states. Each dictionary entry contains the following keys:
		kAirPlayKey_AppStateID		-- App state that changed.
		kAirPlayKey_Entity			-- Entity that now owns the app state.
		[kAirPlayKey_SpeechMode]	-- For speech, this the new speech mode.
	
	kAirPlayKey_Resources 			-- Changed resources. Each dictionary entry contains the following keys:
		kAirPlayKey_ResourceID		-- Resource that changed.
		kAirPlayKey_Entity			-- Entity that now owns the resource.
	
	kAirPlayKey_ReasonStr			-- Optional human readable reason the change occurred.
	
	No response keys.
*/
#define kAirPlayCommand_ModesChanged		"modesChanged"

/*---------------------------------------------------------------------------------------------------------------------------
	PrefsChanged: Tells the server that the prefs have changed and it should re-read them.
	
	No request keys.
	No response keys.
*/
#define kAirPlayEvent_PrefsChanged			"prefsChanged"

/*---------------------------------------------------------------------------------------------------------------------------
	UpdatePrefs: Hook for the platform to update any platform-specific prefs it might have.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_UpdatePrefs			"updatePrefs"

/*---------------------------------------------------------------------------------------------------------------------------
	RequestSiri: Requests that Siri be invoked with a specified action.
	
	Request keys
	------------
	kAirPlayKey_SiriAction -- Siri action to invoke (e.g. prewarm, buttowndown, etc.).
	
	No response keys.
*/
#define kAirPlayCommand_RequestSiri			"requestSiri"

/*---------------------------------------------------------------------------------------------------------------------------
	RequestUI: Requests UI to be shown on the other side (e.g. for accessory to ask the controller to show the maps app).
	
	Request keys
	------------
	kAirPlayKey_URL -- Optional URL of the app to launch.
	
	No response keys.
*/
#define kAirPlayCommand_RequestUI			"requestUI"

/*---------------------------------------------------------------------------------------------------------------------------
	SetNightMode: Indicates whether or not apps should alter their display for night mode.
	
	Request keys
	------------
	kAirPlayKey_NightMode -- Boolean indicting whether or not to enable night mode.

	No response keys.
 */
#define kAirPlayCommand_SetNightMode		"setNightMode"

/*---------------------------------------------------------------------------------------------------------------------------
 	SetLimitedUI: Indicates whether or not apps should enable UI limitations.
 
	Request keys
	------------
	kAirPlayKey_LimitedUI -- Boolean indicating whether or not display certain UI elements should be limited.
 
 	No response keys.
 */
#define kAirPlayCommand_SetLimitedUI		"setLimitedUI"

/*---------------------------------------------------------------------------------------------------------------------------
	StartServer
	For the server object, this starts listening for pref changes and may start or stop based on the prefs.
	For the platform object, this tells it that server is enabled and gives the platform to perform any active setup.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_StartServer			"startServer"

/*---------------------------------------------------------------------------------------------------------------------------
	StopServer
	For the server object, this stops all activity on the server (including the listening for pref changes).
	For the platform object, this tells it that server has fully stopped.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_StopServer			"stopServer"

/*---------------------------------------------------------------------------------------------------------------------------
	SessionDied: Tells the server that a session has died (i.e. communication is no longer working).
	
	Qualifier is the AirPlayReceiverSessionRef that died.
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_SessionDied			"sessionDied"

/*---------------------------------------------------------------------------------------------------------------------------
	StartSession: Tells the platform that a session has started so it can do any active setup.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_StartSession		"startSession"

/*---------------------------------------------------------------------------------------------------------------------------
	StopSession: Tells the platform that a session has stopped so it should stop any active session-specific operations.
	
	No request keys.
	No response keys.
*/
#define kAirPlayCommand_StopSession			"stopSession"

/*---------------------------------------------------------------------------------------------------------------------------
	UpdateVehicleInformation: For the controller to indicate to the accessory that vehicle information has changed.
	
	Request keys
	------------
	kAirPlayKey_VehicleInformation	-- New vehicle information.
	
	No response keys.
*/
#define kAirPlayCommand_UpdateVehicleInformation	"updateVehicleInformation"

/*---------------------------------------------------------------------------------------------------------------------------
	StartedPlayingOverP2PSolo: Tells the server that a P2P Solo playback session started.
	
	No response keys.
 */
#define kAirPlayEvent_StartedPlayingOverP2PSolo		"startedPlayingOverP2PSolo"

/*---------------------------------------------------------------------------------------------------------------------------
	StoppedPlayingOverP2PSolo: Tells the server that a P2P Solo playback session stopped.
	
	No response keys.
 */
#define kAirPlayEvent_StoppedPlayingOverP2PSolo		"stoppedPlayingOverP2PSolo"

/*---------------------------------------------------------------------------------------------------------------------------
	SetUpStreams: Sets up one or more streams on the platform. May be call multiple times to set up different streams.
	
	Request keys
	------------
	kAirPlayKey_Streams -- Array of stream descriptions for the streams to set up and their parameters.
	
	Response keys
	-------------
	kAirPlayKey_Streams -- Array of stream descriptions providing info about each set up stream.
*/
#define kAirPlayCommand_SetUpStreams		"setUpStreams"

/*---------------------------------------------------------------------------------------------------------------------------
	TearDownStreams: Tears down one or more streams on the platform. May be call multiple times to tear down different streams.
	
	Note: This may tear down streams at different times and in a different order than they were set up. For example, it
	may set up main audio, screen, and HID streams then a minute later tear down only main audio then a minute after that 
	it may tear down main audio and HID. So this shouldn't make any assumptions about how streams were set up.
	
	Request keys
	------------
	kAirPlayKey_Streams -- Array of stream descriptions for the streams to tear down.
	
	No response keys.
*/
#define kAirPlayCommand_TearDownStreams		"tearDownStreams"

/*---------------------------------------------------------------------------------------------------------------------------
	UpdateTimestamps: Debug hook for receiving a dictionary containing timestamp info.
	
	Request keys
	------------
	"frameCount"	-- Total number of frames processed so far. Also acts as the frame number.
	"metrics"		-- Array of timestamp deltas in milliseconds. See kAirPlayProperty_TimestampInfo for labels.
	
	No response keys.
*/
#define kAirPlayCommand_UpdateTimestamps	"updateTimestamps"

#if 0
#pragma mark -
#pragma mark == Features ==
#endif

//===========================================================================================================================
//	Features
//===========================================================================================================================

typedef uint64_t	AirPlayFeatures;

	#define kAirPlayFeature_Screen						( 1   <<  7 ) // 0x0000000080: Screen mirroring.
	#define kAirPlayFeature_Rotate						( 1   <<  8 ) // 0x0000000100: Can rotate during screen mirroring.
#define kAirPlayFeature_Audio							( 1   <<  9 ) // 0x0000000200: Audio.
#define kAirPlayFeature_RedundantAudio					( 1   << 11 ) // 0x0000000800: Redundant audio packets to handle loss.
#define kAirPlayFeature_AudioPCM						( 1   << 18 ) // 0x0000040000: Uncompressed PCM audio data.
	#define kAirPlayFeature_AudioAAC_LC					( 1   << 20 ) // 0x0000100000: AAC-LC audio compression.
#define kAirPlayFeature_AudioUnencrypted				( 1   << 22 ) // 0x0000400000: Unencrypted audio.
#define kAirPlayFeature_AudioAES_128_MFi_SAPv1			( 1   << 26 ) // 0x0004000000: Audio: 128-bit AES key encrypted with MFi-SAPv1.
	#define kAirPlayFeature_Pairing						( 1   << 27 ) // 0x0008000000: Pairing support.
#define kAirPlayFeature_UnifiedBonjour					( 1   << 30 ) // 0x0040000000: _airplay._tcp and _raop._tcp are unified (_raop._tcp for compatibility only).
#define kAirPlayFeature_Reserved						( 1   << 31 ) // 0x0080000000: Reserved to avoid client bugs treating this as a sign bit.
	#define kAirPlayFeature_Car				  ( UINT64_C( 1 ) << 32 ) // 0x0100000000: Car support.
	#define kAirPlayFeature_CarPlayControl	  ( UINT64_C( 1 ) << 37 ) // 0x2000000000: Supports CarPlayControl protocol for initiating connections.
	#define kAirPlayFeature_HKPairingAndEncrypt	( UINT64_C( 1 ) << 38 ) // 0x4000000000: Control channel encryption.

#if 0
#pragma mark -
#pragma mark == Extended Features ==
#endif

//===========================================================================================================================
//	Extended Features
//===========================================================================================================================

	// Specifies that the accessory supports receiving information about the telephony voice coder during stream setup
	#define kAirPlayExtendedFeature_VocoderInfo					"vocoderInfo"

	// Specifies that the accessory supports enhanced Car UI requests (such as AC_BACK handling)
	#define kAirPlayExtendedFeature_EnhancedRequestCarUI		"enhancedRequestCarUI"

#if 0
#pragma mark -
#pragma mark == Keys ==
#endif

//===========================================================================================================================
//	Keys
//===========================================================================================================================

// [Boolean] Whether or not something is active.
#define kAirPlayKey_Active				"active"

// [String] IP address, DNS name, etc.
#define kAirPlayKey_Address				"address"

// [String] Password used to change settings on the accessory (e.g. configure the network, etc.).
#define kAirPlayKey_AdminPassword		"adminPassword"

// [Number:AirPlayAppStateID] ID of an app state (e.g. phone call). See kAirPlayAppStateID_*.
#define kAirPlayKey_AppStateID			"appStateID"

// [Array] Array of app states for mode changes, etc.
#define kAirPlayKey_AppStates			"appStates"

// [Number] Format of the audio. See kAirPlayAudioFormat_*.
#define kAirPlayKey_AudioFormat			"audioFormat"

// [Number] Bit mask of supported audio input formats. See kAirPlayAudioFormat_*.
#define kAirPlayKey_AudioInputFormats	"audioInputFormats"

// [Number] Bit mask of supported audio output formats. See kAirPlayAudioFormat_*.
#define kAirPlayKey_AudioOutputFormats	"audioOutputFormats"

// APAudioSourceRef
#define kAirPlayKey_AudioSource			"audioSource"

// [String] Type of audio content (e.g. telephony, media, etc.). See kAirPlayAudioType_*.
#define kAirPlayKey_AudioType			"audioType"
	#define kAirPlayAudioType_Compatibility			"compatibility"
	#define kAirPlayAudioType_Default				"default"
	#define kAirPlayAudioType_Media					"media"
	#define kAirPlayAudioType_Telephony				"telephony"
	#define kAirPlayAudioType_SpeechRecognition		"speechRecognition"
	#define	kAirPlayAudioType_Alert					"alert"

// [Number] Desired milliseconds of audio latency.
#define kAirPlayKey_AudioLatencyMs		"audioLatencyMs"

// [Boolean] Loopback output to input on the AirPlay receiver.
#define kAirPlayKey_AudioLoopback		"audioLoopback"

// [Dictionary] Vendor specific audio stream configuration data. Keys are vendor specific.
#define kAirPlayKey_AudioStreamOptions	"audioStreamOptions"

// [String] Unique 10 character string assigned by Apple to an app via the Provisioning Portal (e.g. "24D4XFAF43").
#define kAirPlayKey_BundleSeedID		"bundleSeedID"

// [Number] Number of audio channels (e.g. 2 for stereo).
#define kAirPlayKey_Channels			"ch"

// [Number:AirPlayCompressionType] Type of compression used. See kAirPlayCompressionType_* constants.
#define kAirPlayKey_CompressionType		"ct"

// [String] Command to perform when embedded within another command. See kAirPlayCommand_* constants.
#define kAirPlayKey_Command				"command"

// [Number:AirPlayConstraint] Constraint for changing ownership of a resource. See kAirPlayConstraint_*.
#define kAirPlayKey_TakeConstraint		"takeConstraint"
#define kAirPlayKey_BorrowConstraint	"borrowConstraint"
#define kAirPlayKey_UnborrowConstraint	"unborrowConstraint"

// [Data] Generic/Opaque data blob.
#define kAirPlayKey_Data				"data"

// [Number] Decibel attenuation level. 0 dB is no attenuation. -144 dB is silence.
#define kAirPlayKey_Decibels			"dB"

// [String or Number] Globally unique device ID (e.g. "11:22:33:44:55"). May be a 64-bit scalar version.
#define kAirPlayKey_DeviceID			"deviceID"

// [Number:AirPlayDisplayFeatures] Features of a particular display.
#define kAirPlayKey_DisplayFeatures				kAirPlayKey_Features

// [Number:AirPlayDisplayFeatures] Primary input device when multiple input devices are supported.
#define kAirPlayKey_PrimaryInputDevice          "primaryInputDevice"

// [Array of Dictionary] Displays available.
#define kAirPlayKey_Displays				"displays"
#define kAirPlayKey_Display					"display"

// [String] UUID of the display. This is used for cases where a non-display is linked to a display.
#define kAirPlayKey_DisplayUUID			"displayUUID"

// [Number] Number of milliseconds for an operation.
#define kAirPlayKey_DurationMs			"durationMs"

// [Data] Raw EDID from a display.
#define kAirPlayKey_EDID				"edid"

// [Data] Encryption initialization vector/nonce.
#define kAirPlayKey_EncryptionIV		"eiv"

// [Data] Encryption key. This key must always be encrypted with a session key.
#define kAirPlayKey_EncryptionKey		"ekey"

// [Number:AirPlayEncryptionType] Type of encryption used. See kAirPlayEncryptionType_* constants.
#define kAirPlayKey_EncryptionType		"et"

// [Number:AirPlayEntity] Entity for an app state or resource. Usually indicates the owner.
#define kAirPlayKey_Entity				"entity"

// [Integer:AirPlayFeatures] Support feature flags. See kAirPlayFeature_* constants.
#define kAirPlayKey_Features			"features"

// [String] Firmware revision of the accessory.
#define kAirPlayKey_FirmwareRevision	"firmwareRevision"

// [Number] Max frames per second supported for screen rendering. Defaults to 60.
#define kAirPlayKey_MaxFPS				"maxFPS"

// [String] Hardware revision of the accessory.
#define kAirPlayKey_HardwareRevision	"hardwareRevision"

// [Number] Height in pixels.
#define kAirPlayKey_HeightPixels		"heightPixels"

// [Number] Height in millimeters.
#define kAirPlayKey_HeightPhysical		"heightPhysical"

// [Number] USB-style HID country code.
#define kAirPlayKey_HIDCountryCode		"hidCountryCode"

// [Data] USB-formatted HID descriptor.
#define kAirPlayKey_HIDDescriptor		"hidDescriptor"

// [Array of Dictionary] HID devices.
#define kAirPlayKey_HIDDevices			"hidDevices"
#define kAirPlayKey_HIDDevice			"hidDevice"

// [Number] HID input mode.
#define kAirPlayKey_HIDInputMode		"hidInputMode"

// [Array of BCP-47 language code strings] Languages supported by the accessory's character recognizer.
#define kAirPlayKey_HIDLanguages		"hidLanguages"

// [Number] USB-style HID product ID.
#define kAirPlayKey_HIDProductID		"hidProductID"

// [Data] USB-formatted HID report.
#define kAirPlayKey_HIDReport			"hidReport"

// [Number] USB-style HID vendor ID.
#define kAirPlayKey_HIDVendorID			"hidVendorID"

// [Boolean] True if audio input should be started.
#define kAirPlayKey_Input				"input"

// [String] BCP-47 language for HID input.
// See <http://www.iana.org/assignments/language-subtag-registry> for a full list.
#define kAirPlayKey_Language			"language"

// [Number] Minimum latency in samples.
#define kAirPlayKey_LatencyMin			"latencyMin"

// [Number] Maximum latency in samples.
#define kAirPlayKey_LatencyMax			"latencyMax"

// [Number] Input latency in microseconds.
#define kAirPlayKey_InputLatencyMicros	"inputLatencyMicros"

// [Number] Output latency in microseconds.
#define kAirPlayKey_OutputLatencyMicros	"outputLatencyMicros"

// [Boolean] Whether or not limited UI is enabled.
#define kAirPlayKey_LimitedUI			"limitedUI"

// [Array] List of UI elements that are affected in limited UI mode.
#define kAirPlayKey_LimitedUIElements	"limitedUIElements"
	#define kAirPlayLimitedUIElement_SoftKeyboard		"softKeyboard"
	#define kAirPlayLimitedUIElement_SoftPhoneKeypad	"softPhoneKeypad"
	#define kAirPlayLimitedUIElement_NonMusicLists		"nonMusicLists"
	#define kAirPlayLimitedUIElement_MusicLists			"musicLists"
	#define kAirPlayLimitedUIElement_JapanMaps			"japanMaps"

// [String] MAC address of the interface for the connection.
// This is used so each side can know the MAC address of the peer. Mainly for tracking P2P peers.
#define kAirPlayKey_MACAddress			"macAddress"

// [String] Manufacturer of the accessory.
#define kAirPlayKey_Manufacturer		"manufacturer"

// [Number] Media timestamp in media units (e.g. audio samples or 90 kHz units for video). 
// This is often used together with synchronized "wallTime" to establish timing relationships.
#define kAirPlayKey_MediaTime			"mediaTime"

// [Array] Array of reverse-DNS strings describing supported MFi accessory protocols (e.g. "com.acme.gadget").
#define kAirPlayKey_MFiProtocols		"mfiProtocols"

// [String] Minimum supported client operating system build version string (e.g. "11A200"). 
#define kAirPlayKey_ClientOSBuildVersionMin		"clientOSBuildVersionMin"

// [String] Model name of the device (e.g. Device1,1).
#define kAirPlayKey_Model				"model"

// [String] Model code of the device.
#define kAirPlayKey_ModelCode			"modelCode"

// [String] Name of an item.
#define kAirPlayKey_Name				"name"

// [Boolean] Indicates if it's dark outside and appearance should change to suit night time use.
#define kAirPlayKey_NightMode			"nightMode"

// [String] Operating system build version string (e.g. "11A200").
#define kAirPlayKey_OSBuildVersion		"osBuildVersion"

// [String] OSInfo string: name, version and architecture (e.g. "Darwin 13.0.0 x86_64").
#define kAirPlayKey_OSInfo				"OSInfo"

// [Dictionary] Parameters for a request.
#define kAirPlayKey_Params				"params"

// [String] path to file.
#define kAirPlayKey_Path				"path"

// [String] Property name.
#define kAirPlayKey_Property			"property"

// [Number] TCP/UDP port number for control (e.g. RTCP).
#define kAirPlayKey_Port_Control		"controlPort"

// [Number] TCP/UDP port number for audio or screen data.
#define kAirPlayKey_Port_Data			"dataPort"

// [Number] TCP/UDP port number for events.
#define kAirPlayKey_Port_Event			"eventPort"

// [Number] UDP port number for keep alive.
#define kAirPlayKey_Port_KeepAlive		"keepAlivePort"

// [Number] TCP/UDP port number for time sync.
#define kAirPlayKey_Port_Timing			"timingPort"

// [String] AirPlay protocol version string (e.g. "1.0").
// The client must check this before displaying a device to the user. If the major version is greater than the major 
// version the client software was built to support, it should hide that device from the user. A change in the minor 
// version indicates the protocol is compatible, but has only minor changes. This mechanism allows future versions of 
// the protocol to hide itself from older and possibly broken clients.
#define kAirPlayKey_ProtocolVersion		"protocolVersion"

// [String] Public HomeKit peer identifier.
#define kAirPlayKey_PublicHKID			"pi"

// [Data] Public key. If Ed25519, it's the full, 32-byte public key. If RSA, it's the certificate fingerprint.
#define kAirPlayKey_PublicKey			"pk"

// [Any] Qualifier object. Used when getting/setting properties, etc.
#define kAirPlayKey_Qualifier			"qualifier"

// [Number] Reason for an operation. The value depends on the context.
#define kAirPlayKey_ReasonCode			"reasonCode"

// [String] Human readable reason.
#define kAirPlayKey_ReasonStr			"reasonStr"

// [Number] Number of redundant packets used for audio loss recovery. 0 or missing means retransmits are used instead.
#define kAirPlayKey_RedundantAudio		"redundantAudio"

// [Array] Array of resources for mode changes, etc.
#define kAirPlayKey_Resources			"resources"

// [Number:AirPlayResourceID] ID of a resource (e.g. screen). See kAirPlayResourceID_*.
#define kAirPlayKey_ResourceID			"resourceID"

// [Boolean] Whether or not to use right-hand drive mode.
#define kAirPlayKey_RightHandDrive		"rightHandDrive"

// [Number] Relative Signal Strength Indication (RSSI).
#define kAirPlayKey_RSSI				"rssi"

// [Number:uint32_t] Sample time.
#define kAirPlayKey_SampleTime			"sampleTime"

// [Dictionary] Vendor specific screen stream configuration data. Keys are vendor specific.
#define kAirPlayKey_ScreenStreamOptions	"screenStreamOptions"

// [Number] Number of audio frames in a packet. Note: if there are 352 stereo samples per packet, this would be 352, not 704.
#define kAirPlayKey_FramesPerPacket		"spf"

// [Number] Number of samples per second (e.g. 44100).
#define kAirPlayKey_SampleRate			"sr"

// [Number] Bit size of each audio sample (e.g. "16").
#define	kAirPlayKey_SampleSize			"ss"

// [String] Serial number of the accessory.
#define kAirPlayKey_SerialNumber		"serialNumber"

// [String] UUID of the session.
#define kAirPlayKey_SessionUUID			"sessionUUID"

// [Number:AirPlaySiriAction] State to put Siri into (e.g. prewarm, buttondown, etc.). See kAirPlaySiriAction_*.
#define kAirPlayKey_SiriAction			"siriAction"

// [Number:AirPlaySpeechMode] Speaking, recognizing speech, none, etc. See kAirPlaySpeechMode_*.
#define kAirPlayKey_SpeechMode			"speechMode"

// [String] AirPlay source version string (e.g. "101.7").
#define kAirPlayKey_SourceVersion		"sourceVersion"

// [Boolean] State of an app, resource, etc.
#define kAirPlayKey_State				"state"

// [Number:AirPlayStatusFlags] Status of an operation.
#define kAirPlayKey_StatusFlags			"statusFlags"

// [Number:OSStatus] Status of an operation.
#define kAirPlayKey_Status				"status"

// [Array of dictionaries] Media stream descriptions to set up or tear down.
#define kAirPlayKey_Streams				"streams"

// [Boolean] True if media control port is supported.
#define kAirPlayKey_SupportsMediaControlPort "supportsMediaControlPort"

// [Boolean] Activate the virtual display in a suspended state (no data connection to receiver).
#define kAirPlayKey_SuspendOnActivate	"SuspendOnActivate"

// [Number] Timestamp in 64-bit NTP format.
#define kAirPlayKey_Timestamp			"timestamp"

// [Number:AirPlayTransferPriority] Priority of a resource transfer (e.g. nice-to-have). See kAirPlayTransferPriority_*.
#define kAirPlayKey_TransferPriority	"transferPriority"

// [Number:AirPlayTransferType] Type of resource transfer to perform (e.g. take, borrow). See kAirPlayTransferType_*.
#define kAirPlayKey_TransferType		"transferType"

// [Data] TXT record for _airplay._tcp.
#define kAirPlayKey_TXTAirPlay			"txtAirPlay"

// [Data] TXT record for _raop._tcp.
#define kAirPlayKey_TXTRAOP				"txtRAOP"

// [Number or String] Type of command, event, stream, etc.
// See kAirPlayCommand_* constants for commands.
// See kAirPlayEvent_* constants for events.
// See kAirPlayStreamType_* constants for streams.
#define kAirPlayKey_Type				"type"

// [String] Unique identifier of the device.
#define kAirPlayKey_UDID				"udid"

// [String] URL for requesting a service (e.g. "http://maps.apple.com/?q=cupertino").
#define kAirPlayKey_URL					"url"

// [Boolean] Internal: Indicate if the screen is being used (mainly for audio).
#define kAirPlayKey_UsingScreen			"usingScreen"

// [String] UUID of an item.
#define kAirPlayKey_UUID				"uuid"

// [Number] ID of a stream connection.
// It gets randomly generated for each stream setup.
#define kAirPlayKey_StreamConnectionID	"streamConnectionID"

// [Any] Value object. Used for setting properties.
#define kAirPlayKey_Value				"value"

// [Dictionary] Vehicle information.
// See kAirPlayVehicleInformation_* constants for entries.
#define kAirPlayKey_VehicleInformation	"vehicleInformation"

// [Number] Wall clock timestamp in synchronized NTP units.
// This is often used together with "mediaTime" to establish timing relationships.
#define kAirPlayKey_WallTime			"wallTime"

// [Data] WiFi PSK for joining a WPA-protected WiFi network.
// This contains between 8 and 63 bytes each being 32-126 decimal, inclusive for the pre-hashed password.
#define kAirPlayKey_WiFiPSK				"wifiPSK"

// [String] WiFi SSID (network name) for the accessory to join.
#define kAirPlayKey_WiFiSSID			"wifiSSID"

// [Number] Width in pixels.
#define kAirPlayKey_WidthPixels			"widthPixels"

// [Number] Width in millimeters.
#define kAirPlayKey_WidthPhysical		"widthPhysical"

// [Boolean] Support sending UDP beacon as keep alive.
#define kAirPlayKey_KeepAliveLowPower	"keepAliveLowPower"

// [Boolean] Whether the receiver supports statistics as part of the keep alive body. 
#define kAirPlayKey_KeepAliveSendStatsAsBody	"keepAliveSendStatsAsBody"

// [Number] FPS for frames queued for sending via wireless.
#define kAirPlayKey_QueuedFramesAvg			"queuedFramesAvg"

// [Number] FPS for frames that are sent.
#define kAirPlayKey_SentFramesAvg			"sentFramesAvg"

// [Number] Average megabits/sec actually being sent.
#define kAirPlayKey_TxUsageAvg				"txUsageAvg"

// [Number] Average percentage bytes that are lost for video connection.
#define kAirPlayKey_LossAvg					"lossAvg"

// [Number] Average round-trip-time for the network.
#define kAirPlayKey_RttAvg					"rttAvg"

// [Number] Primary display FPS (reported from the LCD on the sending device).
#define kAirPlayKey_PrimaryFPSAvg			"primaryFPSAvg"

// [Number] CPU usage.
#define kAirPlayKey_CPUAvg					"cpuAvg"

// [Number] Max megabits/sec (i.e. estimated bandwidth available).
#define kAirPlayKey_TxCapacityAvg			"txCapacityAvg"

// [Number] RSSI of the WiFi interface actively being used.
#define kAirPlayKey_WifiRSSI				"wifiRSSI"

// [Number] FPS of Submit Surface.
#define kAirPlayKey_SubmitSurfaceFPS		"submitSurfaceFPS"

// [Number] FPS of frames given to the encoder.
#define kAirPlayKey_BeforeEncoderFPS		"beforeEncoderFPS"

// [Number] Encoder drops per second.
#define kAirPlayKey_EncoderDropFPS			"encoderDropFPS"

// [Number] Idle encodes per second.
#define kAirPlayKey_IdleEncodeFPS			"idleEncoderFPS"

// [Number] Number of times per second the encoder dropped an idle frame.
#define kAirPlayKey_IdleDropFPS				"idleDropFPS"

// [Number] 
#define kAirPlayKey_EncoderQueueDropFPS		"encoderQueueDropFPS"

// [Number]
#define kAirPlayKey_SinkOverflowDropFPS		"sinkOverflowDropFPS"

// [Number] Current target FPS (30 or 60). Based on available bandwidth.
#define kAirPlayKey_EncoderCurrentFPS		"encoderCurrentFPS"

// [Number] How many times we've backed up enough frames that we had to drop frames and reset.
#define kAirPlayKey_FrameResets				"frameResets"

// [Number] WiFi channel for the current connection.
#define kAirPlayKey_WifiChannel				"wifiChannel"

	// [Data] PNG data.
	#define kAirPlayOEMIconKey_ImageData		"imageData"

	// [Number] Icon height in pixels.
	#define kAirPlayOEMIconKey_HeightPixels		"heightPixels"

	// [Boolean] Whether or not the icon is pre-rendered (default is true if not present)
	#define kAirPlayOEMIconKey_Prerendered		"prerendered"

	// [Number] Icon width in pixels.
	#define kAirPlayOEMIconKey_WidthPixels		"widthPixels"

	// [Dictionary]
	#define kAirPlayKey_VocoderInfo						"vocoderInfo"
		#define kAirPlayVocoderInfoKey_SampleRate		"sampleRate"

#if 0
#pragma mark -
#pragma mark == Logging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlay Logging
	@abstract	Notifications posted by AirPlay.
*/

	#define kAirPlayPrimaryLogPath		"/tmp/AirPlay.log"

#if( !defined( AIRPLAY_LOG_TO_FILE_PRIMARY ) )
	#if( DEBUG || 0 )
		#define AIRPLAY_LOG_TO_FILE_PRIMARY			0
	#else
		#define AIRPLAY_LOG_TO_FILE_PRIMARY			0
	#endif
#endif
	#define kAirPlayPrimaryLogConfig				"?.*:output=file;path=/tmp/AirPlay.log;roll=128K#2"

#if( !defined( AIRPLAY_LOG_TO_FILE_SECONDARY ) )
		#define AIRPLAY_LOG_TO_FILE_SECONDARY		0
#endif
	#define kAirPlaySecondaryLogConfig				"?.*:output2=file;path=/tmp/AirPlay.log;roll=128K#2"

#if( DEBUG || 0 ) 
	#define kAirPlayPhaseLogLevel		kLogLevelNotice
#else
	#define kAirPlayPhaseLogLevel		kLogLevelInfo
#endif

#if 0
#pragma mark -
#pragma mark == Modes ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlay Modes
	@abstract	AirPlay app state and mode management.
*/

// AirPlayAppStateID

typedef int32_t		AirPlayAppStateID;
#define kAirPlayAppStateID_NotApplicable			0
#define kAirPlayAppStateID_Speech					1 // A device is recording audio for the purpose of speech.
#define kAirPlayAppStateID_PhoneCall				2 // A device is on a phone call.
#define kAirPlayAppStateID_TurnByTurn				3 // A device is performing turn-by-turn navigation.

#define kAirPlayAppStateIDString_NotApplicable		"n/a"
#define kAirPlayAppStateIDString_Speech				"speech"
#define kAirPlayAppStateIDString_PhoneCall			"phoneCall"
#define kAirPlayAppStateIDString_TurnByTurn			"turnByTurn"

#define AirPlayAppStateIDFromCFString( X )	MapCFStringToValue( (X), kAirPlayAppStateID_NotApplicable, \
	CFSTR( kAirPlayAppStateIDString_Speech ),		kAirPlayAppStateID_Speech, \
	CFSTR( kAirPlayAppStateIDString_PhoneCall ),	kAirPlayAppStateID_PhoneCall, \
	CFSTR( kAirPlayAppStateIDString_TurnByTurn ),	kAirPlayAppStateID_TurnByTurn, \
	NULL )

#define AirPlayAppStateIDFromString( X )	MapStringToValue( (X), kAirPlayAppStateID_NotApplicable, \
	kAirPlayAppStateIDString_Speech,		kAirPlayAppStateID_Speech, \
	kAirPlayAppStateIDString_PhoneCall,		kAirPlayAppStateID_PhoneCall, \
	kAirPlayAppStateIDString_TurnByTurn,	kAirPlayAppStateID_TurnByTurn, \
	NULL )

#define AirPlayAppStateIDToString( X ) ( \
	( (X) == kAirPlayAppStateID_NotApplicable ) ? kAirPlayAppStateIDString_NotApplicable	: \
	( (X) == kAirPlayAppStateID_Speech )		? kAirPlayAppStateIDString_Speech			: \
	( (X) == kAirPlayAppStateID_PhoneCall )		? kAirPlayAppStateIDString_PhoneCall		: \
	( (X) == kAirPlayAppStateID_TurnByTurn )	? kAirPlayAppStateIDString_TurnByTurn		: \
												  "?" )

#define AirPlayAppStateIDToCFString( X ) ( \
	( (X) == kAirPlayAppStateID_NotApplicable ) ? CFSTR( kAirPlayAppStateIDString_NotApplicable )	: \
	( (X) == kAirPlayAppStateID_Speech )		? CFSTR( kAirPlayAppStateIDString_Speech )			: \
	( (X) == kAirPlayAppStateID_PhoneCall )		? CFSTR( kAirPlayAppStateIDString_PhoneCall )		: \
	( (X) == kAirPlayAppStateID_TurnByTurn )	? CFSTR( kAirPlayAppStateIDString_TurnByTurn )		: \
												  CFSTR( "?" ) )

// AirPlayConstraint

typedef int32_t		AirPlayConstraint;
#define kAirPlayConstraint_NotApplicable			0
#define kAirPlayConstraint_Anytime					100  // Resource may be taken/borrowed at any time.
#define kAirPlayConstraint_UserInitiated			500  // Resource may be taken/borrowed if user initiated.
#define kAirPlayConstraint_Never					1000 // Resource may never be taken/borrowed.

#define kAirPlayConstraintString_NotApplicable		"n/a"
#define kAirPlayConstraintString_Anytime			"anytime"
#define kAirPlayConstraintString_UserInitiated		"userInitiated"
#define kAirPlayConstraintString_Never				"never"

#define AirPlayConstraintFromCFString( X )	MapCFStringToValue( (X), kAirPlayConstraint_NotApplicable, \
	CFSTR( kAirPlayConstraintString_Anytime ),			kAirPlayConstraint_Anytime, \
	CFSTR( kAirPlayConstraintString_UserInitiated ),	kAirPlayConstraint_UserInitiated, \
	CFSTR( kAirPlayConstraintString_Never ),			kAirPlayConstraint_Never, \
	NULL )

#define AirPlayConstraintFromString( X )	MapStringToValue( (X), kAirPlayConstraint_NotApplicable, \
	kAirPlayConstraintString_Anytime,		kAirPlayConstraint_Anytime, \
	kAirPlayConstraintString_UserInitiated,	kAirPlayConstraint_UserInitiated, \
	kAirPlayConstraintString_Never,			kAirPlayConstraint_Never, \
	NULL )

#define AirPlayConstraintToString( X ) ( \
	( (X) == kAirPlayConstraint_NotApplicable ) ? kAirPlayConstraintString_NotApplicable	: \
	( (X) == kAirPlayConstraint_Anytime )		? kAirPlayConstraintString_Anytime			: \
	( (X) == kAirPlayConstraint_UserInitiated )	? kAirPlayConstraintString_UserInitiated	: \
	( (X) == kAirPlayConstraint_Never )			? kAirPlayConstraintString_Never			: \
												  "?" )

#define AirPlayConstraintToCFString( X ) ( \
	( (X) == kAirPlayConstraint_NotApplicable ) ? CFSTR( kAirPlayConstraintString_NotApplicable )	: \
	( (X) == kAirPlayConstraint_Anytime )		? CFSTR( kAirPlayConstraintString_Anytime )			: \
	( (X) == kAirPlayConstraint_UserInitiated )	? CFSTR( kAirPlayConstraintString_UserInitiated )	: \
	( (X) == kAirPlayConstraint_Never )			? CFSTR( kAirPlayConstraintString_Never )			: \
												  CFSTR( "?" ) )

// AirPlayEntity

typedef int32_t		AirPlayEntity;
#define kAirPlayEntity_NotApplicable			0
#define kAirPlayEntity_Controller				1
#define kAirPlayEntity_Accessory				2

#define kAirPlayEntityString_NotApplicable		"n/a"
#define kAirPlayEntityString_Controller			"controller"
#define kAirPlayEntityString_Accessory			"accessory"

#define AirPlayEntityFromCFString( X )	MapCFStringToValue( (X), kAirPlayEntity_NotApplicable, \
	CFSTR( kAirPlayEntityString_Controller ),	kAirPlayEntity_Controller, \
	CFSTR( kAirPlayEntityString_Accessory ),	kAirPlayEntity_Accessory, \
	NULL )

#define AirPlayEntityFromString( X )	MapStringToValue( (X), kAirPlayEntity_NotApplicable, \
	kAirPlayEntityString_Controller,	kAirPlayEntity_Controller, \
	kAirPlayEntityString_Accessory,		kAirPlayEntity_Accessory, \
	NULL )

#define AirPlayEntityToString( X ) ( \
	( (X) == kAirPlayEntity_NotApplicable ) ? kAirPlayEntityString_NotApplicable	: \
	( (X) == kAirPlayEntity_Controller )	? kAirPlayEntityString_Controller		: \
	( (X) == kAirPlayEntity_Accessory )		? kAirPlayEntityString_Accessory		: \
											  "?" )

#define AirPlayEntityToCFString( X ) ( \
	( (X) == kAirPlayEntity_NotApplicable ) ? CFSTR( kAirPlayEntityString_NotApplicable )	: \
	( (X) == kAirPlayEntity_Controller )	? CFSTR( kAirPlayEntityString_Controller )		: \
	( (X) == kAirPlayEntity_Accessory )		? CFSTR( kAirPlayEntityString_Accessory )		: \
											  CFSTR( "?" ) )

// AirPlayResourceID

typedef int32_t		AirPlayResourceID;
#define kAirPlayResourceID_NotApplicable			0
#define kAirPlayResourceID_MainScreen				1 // Main screen.
#define kAirPlayResourceID_MainAudio				2 // Input and output audio stream for Siri, telephony, media.

#define kAirPlayResourceIDString_NotApplicable		"n/a"
#define kAirPlayResourceIDString_MainScreen			"mainScreen"
#define kAirPlayResourceIDString_MainAudio			"mainAudio"

#define AirPlayResourceIDFromCFString( X )	MapCFStringToValue( (X), kAirPlayResourceID_NotApplicable, \
	CFSTR( kAirPlayResourceIDString_MainScreen ),	kAirPlayResourceID_MainScreen, \
	CFSTR( kAirPlayResourceIDString_MainAudio ),	kAirPlayResourceID_MainAudio, \
	NULL )

#define AirPlayResourceIDFromString( X )	MapStringToValue( (X), kAirPlayResourceID_NotApplicable, \
	kAirPlayResourceIDString_MainScreen,	kAirPlayResourceID_MainScreen, \
	kAirPlayResourceIDString_MainAudio,		kAirPlayResourceID_MainAudio, \
	NULL )

#define AirPlayResourceIDToString( X ) ( \
	( (X) == kAirPlayResourceID_NotApplicable ) ? kAirPlayResourceIDString_NotApplicable	: \
	( (X) == kAirPlayResourceID_MainScreen )	? kAirPlayResourceIDString_MainScreen		: \
	( (X) == kAirPlayResourceID_MainAudio )		? kAirPlayResourceIDString_MainAudio		: \
												  "?" )

#define AirPlayResourceIDToCFString( X ) ( \
	( (X) == kAirPlayResourceID_NotApplicable ) ? CFSTR( kAirPlayResourceIDString_NotApplicable )	: \
	( (X) == kAirPlayResourceID_MainScreen )	? CFSTR( kAirPlayResourceIDString_MainScreen )		: \
	( (X) == kAirPlayResourceID_MainAudio )		? CFSTR( kAirPlayResourceIDString_MainAudio )		: \
												  CFSTR( "?" ) )

// AirPlaySiriAction

typedef int32_t		AirPlaySiriAction;
#define kAirPlaySiriAction_NotApplicable				0
#define kAirPlaySiriAction_Prewarm						1 // Pre-warm Siri.
#define kAirPlaySiriAction_ButtonDown					2 // Button down.
#define kAirPlaySiriAction_ButtonUp						3 // Button up.

#define kAirPlaySiriActionString_NotApplicable			"n/a"
#define kAirPlaySiriActionString_Prewarm				"prewarm"
#define kAirPlaySiriActionString_ButtonDown				"buttondown"
#define kAirPlaySiriActionString_ButtonUp				"buttonup"

#define AirPlaySiriActionFromCFString( X )	MapCFStringToValue( (X), kAirPlaySiriAction_NotApplicable, \
	CFSTR( kAirPlaySiriActionString_Prewarm ),		kAirPlaySiriAction_Prewarm, \
	CFSTR( kAirPlaySiriActionString_ButtonDown ),	kAirPlaySiriAction_ButtonDown, \
	CFSTR( kAirPlaySiriActionString_ButtonUp ),		kAirPlaySiriAction_ButtonUp, \
	NULL )

#define AirPlaySiriActionFromString( X )	MapStringToValue( (X), kAirPlaySiriAction_NotApplicable, \
	kAirPlaySiriActionString_Prewarm,		kAirPlaySiriAction_Prewarm, \
	kAirPlaySiriActionString_ButtonDown,	kAirPlaySiriAction_ButtonDown, \
	kAirPlaySiriActionString_ButtonUp,		kAirPlaySiriAction_ButtonUp, \
	NULL )

#define AirPlaySiriActionToString( X ) ( \
	( (X) == kAirPlaySiriAction_NotApplicable ) ? kAirPlaySiriActionString_NotApplicable	: \
	( (X) == kAirPlaySiriAction_Prewarm )		? kAirPlaySiriActionString_Prewarm			: \
	( (X) == kAirPlaySiriAction_ButtonDown )	? kAirPlaySiriActionString_ButtonDown		: \
	( (X) == kAirPlaySiriAction_ButtonUp )		? kAirPlaySiriActionString_ButtonUp			: \
												  "?" )

#define AirPlaySiriActionToCFString( X ) ( \
	( (X) == kAirPlaySiriAction_NotApplicable ) ? CFSTR( kAirPlaySiriActionString_NotApplicable )	: \
	( (X) == kAirPlaySiriAction_Prewarm )		? CFSTR( kAirPlaySiriActionString_Prewarm )			: \
	( (X) == kAirPlaySiriAction_ButtonDown )	? CFSTR( kAirPlaySiriActionString_ButtonDown )		: \
	( (X) == kAirPlaySiriAction_ButtonUp )		? CFSTR( kAirPlaySiriActionString_ButtonUp )		: \
												  CFSTR( "?" ) )

// AirPlaySpeechMode

typedef int32_t		AirPlaySpeechMode;
#define kAirPlaySpeechMode_NotApplicable			0
#define kAirPlaySpeechMode_None						-1 // No speech-related states are active.
#define kAirPlaySpeechMode_Speaking					1  // Device is speaking to the user.
#define kAirPlaySpeechMode_Recognizing				2  // Device is recording audio to recognize speech from the user.

#define kAirPlaySpeechModeString_NotApplicable		"n/a"
#define kAirPlaySpeechModeString_None				"none"
#define kAirPlaySpeechModeString_Speaking			"speaking"
#define kAirPlaySpeechModeString_Recognizing		"recognizing"

#define AirPlaySpeechModeFromCFString( X )	MapCFStringToValue( (X), kAirPlaySpeechMode_NotApplicable, \
	CFSTR( kAirPlaySpeechModeString_None ),			kAirPlaySpeechMode_None, \
	CFSTR( kAirPlaySpeechModeString_Speaking ),		kAirPlaySpeechMode_Speaking, \
	CFSTR( kAirPlaySpeechModeString_Recognizing ),	kAirPlaySpeechMode_Recognizing, \
	NULL )

#define AirPlaySpeechModeFromString( X )	MapStringToValue( (X), kAirPlaySpeechMode_NotApplicable, \
	kAirPlaySpeechModeString_None,			kAirPlaySpeechMode_None, \
	kAirPlaySpeechModeString_Speaking,		kAirPlaySpeechMode_Speaking, \
	kAirPlaySpeechModeString_Recognizing,	kAirPlaySpeechMode_Recognizing, \
	NULL )

#define AirPlaySpeechModeToString( X ) ( \
	( (X) == kAirPlaySpeechMode_NotApplicable ) ? kAirPlaySpeechModeString_NotApplicable	: \
	( (X) == kAirPlaySpeechMode_None )			? kAirPlaySpeechModeString_None				: \
	( (X) == kAirPlaySpeechMode_Speaking )		? kAirPlaySpeechModeString_Speaking			: \
	( (X) == kAirPlaySpeechMode_Recognizing )	? kAirPlaySpeechModeString_Recognizing		: \
												  "?" )

#define AirPlaySpeechModeToCFString( X ) ( \
	( (X) == kAirPlaySpeechMode_NotApplicable ) ? CFSTR( kAirPlaySpeechModeString_NotApplicable )	: \
	( (X) == kAirPlaySpeechMode_None )			? CFSTR( kAirPlaySpeechModeString_None )			: \
	( (X) == kAirPlaySpeechMode_Speaking )		? CFSTR( kAirPlaySpeechModeString_Speaking )		: \
	( (X) == kAirPlaySpeechMode_Recognizing )	? CFSTR( kAirPlaySpeechModeString_Recognizing )		: \
												  CFSTR( "?" ) )

// AirPlayTransferPriority

typedef int32_t		AirPlayTransferPriority;
#define kAirPlayTransferPriority_NotApplicable				0
#define kAirPlayTransferPriority_NiceToHave					100 // Transfer succeeds only if constraint is <= Anytime.
#define kAirPlayTransferPriority_UserInitiated				500 // Transfer succeeds only if constraint is <= UserInitiated.

#define kAirPlayTransferPriorityString_NotApplicable		"n/a"
#define kAirPlayTransferPriorityString_NiceToHave			"niceToHave"
#define kAirPlayTransferPriorityString_UserInitiated		"userInitiated"

#define AirPlayTransferPriorityFromCFString( X )	MapCFStringToValue( (X), kAirPlayTransferPriority_NotApplicable, \
	CFSTR( kAirPlayTransferPriorityString_NiceToHave ),		kAirPlayTransferPriority_NiceToHave, \
	CFSTR( kAirPlayTransferPriorityString_UserInitiated ),	kAirPlayTransferPriority_UserInitiated, \
	NULL )

#define AirPlayTransferPriorityFromString( X )	MapStringToValue( (X), kAirPlayTransferPriority_NotApplicable, \
	kAirPlayTransferPriorityString_NiceToHave,		kAirPlayTransferPriority_NiceToHave, \
	kAirPlayTransferPriorityString_UserInitiated,	kAirPlayTransferPriority_UserInitiated, \
	NULL )

#define AirPlayTransferPriorityToString( X ) ( \
	( (X) == kAirPlayTransferPriority_NotApplicable )	? kAirPlayTransferPriorityString_NotApplicable	: \
	( (X) == kAirPlayTransferPriority_NiceToHave )		? kAirPlayTransferPriorityString_NiceToHave		: \
	( (X) == kAirPlayTransferPriority_UserInitiated )	? kAirPlayTransferPriorityString_UserInitiated	: \
														  "?" )

#define AirPlayTransferPriorityToCFString( X ) ( \
	( (X) == kAirPlayTransferPriority_NotApplicable )	? CFSTR( kAirPlayTransferPriorityString_NotApplicable )	: \
	( (X) == kAirPlayTransferPriority_NiceToHave )		? CFSTR( kAirPlayTransferPriorityString_NiceToHave )	: \
	( (X) == kAirPlayTransferPriority_UserInitiated )	? CFSTR( kAirPlayTransferPriorityString_UserInitiated )	: \
														  CFSTR( "?" ) )

// AirPlayTransferType

typedef int32_t		AirPlayTransferType;
#define kAirPlayTransferType_NotApplicable				0
#define kAirPlayTransferType_Take						1 // Transfer ownership permanently.
#define kAirPlayTransferType_Untake						2 // Release permanent ownership. 
#define kAirPlayTransferType_Borrow						3 // Transfer ownership temporarily.
#define kAirPlayTransferType_Unborrow					4 // Release temporary ownership.

#define kAirPlayTransferTypeString_NotApplicable		"n/a"
#define kAirPlayTransferTypeString_Take					"take"
#define kAirPlayTransferTypeString_Untake				"untake"
#define kAirPlayTransferTypeString_Borrow				"borrow"
#define kAirPlayTransferTypeString_Unborrow				"unborrow"

#define AirPlayTransferTypeFromCFString( X )	MapCFStringToValue( (X), kAirPlayTransferType_NotApplicable, \
	CFSTR( kAirPlayTransferTypeString_Take ),		kAirPlayTransferType_Take, \
	CFSTR( kAirPlayTransferTypeString_Untake ),		kAirPlayTransferType_Untake, \
	CFSTR( kAirPlayTransferTypeString_Borrow ),		kAirPlayTransferType_Borrow, \
	CFSTR( kAirPlayTransferTypeString_Unborrow ),	kAirPlayTransferType_Unborrow, \
	NULL )

#define AirPlayTransferTypeFromString( X )	MapStringToValue( (X), kAirPlayTransferType_NotApplicable, \
	kAirPlayTransferTypeString_Take,		kAirPlayTransferType_Take, \
	kAirPlayTransferTypeString_Untake,		kAirPlayTransferType_Untake, \
	kAirPlayTransferTypeString_Borrow,		kAirPlayTransferType_Borrow, \
	kAirPlayTransferTypeString_Unborrow,	kAirPlayTransferType_Unborrow, \
	NULL )

#define AirPlayTransferTypeToString( X ) ( \
	( (X) == kAirPlayTransferType_NotApplicable ) 	? kAirPlayTransferTypeString_NotApplicable	: \
	( (X) == kAirPlayTransferType_Take )			? kAirPlayTransferTypeString_Take			: \
	( (X) == kAirPlayTransferType_Untake )			? kAirPlayTransferTypeString_Untake			: \
	( (X) == kAirPlayTransferType_Borrow )			? kAirPlayTransferTypeString_Borrow			: \
	( (X) == kAirPlayTransferType_Unborrow )		? kAirPlayTransferTypeString_Unborrow		: \
													  "?" )

#define AirPlayTransferTypeToCFString( X ) ( \
	( (X) == kAirPlayTransferType_NotApplicable ) 	? CFSTR( kAirPlayTransferTypeString_NotApplicable )	: \
	( (X) == kAirPlayTransferType_Take )			? CFSTR( kAirPlayTransferTypeString_Take )			: \
	( (X) == kAirPlayTransferType_Untake )			? CFSTR( kAirPlayTransferTypeString_Untake )		: \
	( (X) == kAirPlayTransferType_Borrow )			? CFSTR( kAirPlayTransferTypeString_Borrow )		: \
	( (X) == kAirPlayTransferType_Unborrow )		? CFSTR( kAirPlayTransferTypeString_Unborrow )		: \
													  CFSTR( "?" ) )

// AirPlayTriState

typedef int32_t		AirPlayTriState;
#define kAirPlayTriState_NotApplicable		0
#define kAirPlayTriState_False				-1
#define kAirPlayTriState_True				1

#define AirPlayTriStateToString( X ) ( \
	( (X) == kAirPlayTriState_NotApplicable ) 	? "n/a"		: \
	( (X) == kAirPlayTriState_False )			? "false"	: \
	( (X) == kAirPlayTriState_True )			? "true"	: \
												  "?" )

// Changes

typedef struct
{
	AirPlayTransferType			type;
	AirPlayTransferPriority		priority;
	AirPlayConstraint			takeConstraint;				// If "take", constraint for peer to take. Missing otherwise.
	AirPlayConstraint			borrowOrUnborrowConstraint;	// If "take", constraint for peer to borrow. Missing otherwise.
															// If "borrow", constraint for peer to unborrow. Missing otherwise.
}	AirPlayResourceChange;

typedef struct
{
	AirPlayResourceChange		screen;
	AirPlayResourceChange		mainAudio;
	AirPlaySpeechMode			speech;
	AirPlayTriState				phoneCall;
	AirPlayTriState				turnByTurn;
	
}	AirPlayModeChanges;

#define AirPlayModeChangesInit( PTR )	memset( (PTR), 0, sizeof( *(PTR) ) )

// State

typedef struct
{
	AirPlayEntity			entity;
	AirPlaySpeechMode		mode;
	
}	AirPlaySpeechState;

typedef struct
{
	AirPlayEntity			screen;		// Owner of the screen.
	AirPlayEntity			mainAudio;	// Owner of main audio.
	AirPlayEntity			phoneCall;	// Owner of phone call.
	AirPlaySpeechState		speech;		// Owner of speech and its mode.
	AirPlayEntity			turnByTurn;	// Owner of navigation.
	
}	AirPlayModeState;

#define AirPlayModeStateInit( PTR )		memset( (PTR), 0, sizeof( *(PTR) ) )

#if 0
#pragma mark -
#pragma mark == Packets - RTP ==
#endif

//===========================================================================================================================
//	RTP Packets
//===========================================================================================================================

// RFC 3550 Section 5.1: RTP Packet
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    +0/0x00
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    +4/0x04
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    +8/0x08
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+   +12/0x0C
// |                                                               |
// |                            payload                            |
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +1440/0x5A0

#define	kRTPHeaderSize								12

#define	RTPHeaderInsertVersion( FIELDS, X )			( ( ( FIELDS ) & ~0xC0 ) | ( ( ( X ) << 6 ) & 0xC0 ) )
#define	RTPHeaderExtractVersion( FIELDS )			( ( ( FIELDS ) >> 6 ) & 0x03 )
#define	kRTPVersion									2

#define	RTPHeaderInsertMarker( FIELDS, X )			( ( ( FIELDS ) & ~0x80 ) | ( ( ( X ) << 7 ) & 0x80 ) )
#define	RTPHeaderExtractMarker( FIELDS )			( ( ( FIELDS ) >> 7 ) & 0x01 )

#define	RTPHeaderInsertPayloadType( FIELDS, X )		( ( ( FIELDS ) & ~0x7F ) | ( ( X ) & 0x7F ) )
#define	RTPHeaderExtractPayloadType( FIELDS )		( ( FIELDS ) & 0x7F )

typedef struct
{
	uint8_t			v_p_x_cc;	// Version (V), Padding (P), Extension (X), and Contributing Sources Count (CC) fields.
	uint8_t			m_pt;		// Marker (M) and Payload Type (PT) fields.
	uint16_t		seq;		// Sequence Number
	uint32_t		ts;			// Timestamp
	uint32_t		ssrc;		// Synchronization Source (SSRC) Identifier
	
}	RTPHeader;

check_compile_time( offsetof( RTPHeader, m_pt )	==  1 );
check_compile_time( offsetof( RTPHeader, seq )	==  2 );
check_compile_time( offsetof( RTPHeader, ts )	==  4 );
check_compile_time( offsetof( RTPHeader, ssrc )	==  8 );
check_compile_time( sizeof( RTPHeader )			== 12 );

typedef struct
{
	RTPHeader		header;			// [0x00] 12-byte RTP header.
	uint8_t			payload[ 1 ];	// [0x0C] Variable length payload.
	
}	RTPPacket;

check_compile_time( offsetof( RTPPacket, header )	==  0 );
check_compile_time( offsetof( RTPPacket, payload )	== 12 );

typedef struct
{
	union
	{
		uint32_t		align;
		RTPPacket		rtp;
		uint8_t			bytes[ kAirTunesMaxPacketSizeUDP ];
		
	}	pkt;
	
	size_t				len;
	uint32_t			sampleCount;
	
}	RTPSavedPacket;

#if 0
#pragma mark -
#pragma mark == Packets - RTCP ==
#endif

//===========================================================================================================================
//	RTCP Packets
//===========================================================================================================================

// RTCPType

typedef enum
{
	kRTCPTypeAny				= 0,
	
	kRTCPTypeSR					= 200,	// Sender Report
	kRTCPTypeRR					= 201,	// Receiver Report
	kRTCPTypeSDES				= 202,	// Source Description
	kRTCPTypeBye				= 203,
	kRTCPTypeApp				= 204,
	
	// Extensions
	
	kRTCPTypeTimeSyncRequest	= 210,	// RTCPTimeSyncPacket
	kRTCPTypeTimeSyncResponse	= 211,	// RTCPTimeSyncPacket
	kRTCPTypeTimeAnnounce		= 212,	// RTCPTimeAnnouncePacket
	kRTCPTypeRetransmitRequest	= 213,	// RTCPRetransmitRequestPacket
	kRTCPTypeRetransmitResponse	= 214	// RTCPRetransmitResponsePacket
	
}	RTCPType;

// RFC 3550 Section 6.4: RTCP Common Header
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +0/0x00
// header |V=2|P|  count  |      PT       |           length              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +4/0x04

#define	RTCPHeaderInsertVersion( FIELDS, X )		( ( ( FIELDS ) & ~0xC0 ) | ( ( ( X ) << 6 ) & 0xC0 ) )
#define	RTCPHeaderExtractVersion( FIELDS )			( ( ( FIELDS ) >> 6 ) & 0x03 )
#define kRTCPVersion								2

#define	RTCPHeaderInsertCount( FIELDS, X )			( ( ( FIELDS ) & ~0x1F ) | ( ( X ) & 0x1F ) )
#define	RTCPHeaderExtractCount( FIELDS )			( ( FIELDS ) & 0x1F )

typedef struct
{
	uint8_t			v_p_c;	// Version (V), Padding (P), and Count (C) fields.
	uint8_t			pt;		// RTCP packet type
	uint16_t		length;	// Packet length in 32-bit words - 1.
	
}	RTCPCommonHeader;

check_compile_time( offsetof( RTCPCommonHeader, pt )	 == 1 );
check_compile_time( offsetof( RTCPCommonHeader, length ) == 2 );
check_compile_time( sizeof( RTCPCommonHeader )			 == 4 );

// AirTunes Receiver Report Packet
// 
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
// RTSP   |      '$'      |  Channel ID   |       Length in bytes         |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+  +4/0x04
// RTCP   |V=2|P|    RC   |   PT=RR=201   |             length            |
// header +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
//        |                     SSRC of packet sender                     |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ +12/0x0C
// report |                 SSRC_1 (SSRC of first source)                 |
// block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10
//   1    | fraction lost |       cumulative number of packets lost       |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +20/0x14
//        |           extended highest sequence number received           |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +24/0x18
//        |                      interarrival jitter                      |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +28/0x1C
//        |                         last SR (LSR)                         |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +32/0x20
//        |                   delay since last SR (DLSR)                  |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ +36/0x24
// ROAP   |       extension length        | ext. version  |      pad      |
// Ext.   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +40/0x28
//        |                     last timestamp received                   |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +44/0x2C
//        |                       output buffer frames                    |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +48/0x30
//        |                        output buffer size                     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +52/0x34
//        |                         hardware latency                      |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ +56/0x38

typedef struct
{
	// RTSP Header
	
	uint8_t			signature;	// '$'
	uint8_t			channelID;	// 0 for data, 1 for control
	uint16_t		rtspLength;	// Number of bytes in payload section.
	
	// RTCP Header
	
	uint8_t			v_p_c;		// Version (V), Padding (P), and Count (C) fields.
	uint8_t			pt;			// RTCP packet type
	uint16_t		length;		// Packet length in 32-bit words - 1.
	uint32_t		ssrc;		// SSRC of packet sender.
	
	// RTCP Receiver Report
	
	uint32_t		ssrc1;		// SSRC of first source.
	uint32_t		lost;		// 8 bit fraction list since last report; 24 bit cumulative packets (signed).
	uint32_t		lastSeq;	// Extended sequence number last received.
	uint32_t		jitter;		// Inter-arrival jitter.
	uint32_t		lsr;		// Last SR packet from this source.
	uint32_t		dlsr;		// Delay since last SR packet.
	
	// ROAP Extension
	
	uint16_t		extensionLength; 			// Length of extension in bytes.
	uint8_t			extensionVersion;			// Version of the extension (currently 1).
	uint8_t			extensionPad;				// Pad to 32-bit boundary.
	uint32_t		lastRTPTimeStampReceived;	// Last RTP time stamp received.
	uint32_t		framesInOutputBuffer;		// Number of audio frames currently in the output buffer.
	uint32_t		maxFramesInOutputBuffer;	// Number of audio frames the output buffer can hold.
	uint32_t		hardwareLatency;			// Latency of the output hardware in frames.
	
}	AirTunesReceiverReportFields;

check_compile_time( offsetof( AirTunesReceiverReportFields, signature )					==  0 );
check_compile_time( offsetof( AirTunesReceiverReportFields, channelID )					==  1 );
check_compile_time( offsetof( AirTunesReceiverReportFields, rtspLength )				==  2 );
check_compile_time( offsetof( AirTunesReceiverReportFields, v_p_c )						==  4 );
check_compile_time( offsetof( AirTunesReceiverReportFields, pt )						==  5 );
check_compile_time( offsetof( AirTunesReceiverReportFields, length )					==  6 );
check_compile_time( offsetof( AirTunesReceiverReportFields, ssrc )						==  8 );
check_compile_time( offsetof( AirTunesReceiverReportFields, ssrc1 )						== 12 );
check_compile_time( offsetof( AirTunesReceiverReportFields, lost )						== 16 );
check_compile_time( offsetof( AirTunesReceiverReportFields, lastSeq )					== 20 );
check_compile_time( offsetof( AirTunesReceiverReportFields, jitter )					== 24 );
check_compile_time( offsetof( AirTunesReceiverReportFields, lsr )						== 28 );
check_compile_time( offsetof( AirTunesReceiverReportFields, dlsr )						== 32 );
check_compile_time( offsetof( AirTunesReceiverReportFields, extensionLength )			== 36 );
check_compile_time( offsetof( AirTunesReceiverReportFields, extensionVersion )			== 38 );
check_compile_time( offsetof( AirTunesReceiverReportFields, extensionPad )				== 39 );
check_compile_time( offsetof( AirTunesReceiverReportFields, lastRTPTimeStampReceived )	== 40 );
check_compile_time( offsetof( AirTunesReceiverReportFields, framesInOutputBuffer )		== 44 );
check_compile_time( offsetof( AirTunesReceiverReportFields, maxFramesInOutputBuffer )	== 48 );
check_compile_time( offsetof( AirTunesReceiverReportFields, hardwareLatency )			== 52 );
check_compile_time( sizeof( AirTunesReceiverReportFields ) 								== 56 );

typedef union
{
	uint32_t							align;			// Force 32-bit alignment.
	uint8_t								bytes[ 1 ];		// Raw bytes.
	AirTunesReceiverReportFields		fields;
	
}	AirTunesReceiverReport;

check_compile_time( offsetof( AirTunesReceiverReport, bytes )	==  0 );
check_compile_time( offsetof( AirTunesReceiverReport, fields )	==  0 );
check_compile_time( sizeof( AirTunesReceiverReport ) 			== 56 );

// RTCPTimeSync Packet
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
// header |V=2|P|M|   0   |      PT       |           length              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
// timing |           RTP timestamp at NTP Transit (T3) time              |
// info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
//        |      NTP Originate (T1) timestamp, most  significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +12/0x0C
//        |      NTP Originate (T1) timestamp, least significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10
//        |      NTP Receive   (T2) timestamp, most  significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +20/0x14
//        |      NTP Receive   (T2) timestamp, least significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +24/0x18
//        |      NTP Transmit  (T3) timestamp, most  significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +28/0x1C
//        |      NTP Transmit  (T3) timestamp, least significant word     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +32/0x20

#define	RTCPTimeSyncInsertMarker( FIELDS, X )	( ( ( FIELDS ) & ~0x10 ) | ( ( ( X ) << 4 ) & 0x10 ) )
#define	RTCPTimeSyncExtractMarker( FIELDS )		( ( ( FIELDS ) >> 4 ) & 0x01 )

typedef struct
{
	uint8_t			v_p_m;			// Version (V), Padding (P), and Marker (M) fields.	
	uint8_t			pt;				// RTCP packet type.
	uint16_t		length;			// Packet length in 32-bit words - 1.
	
	uint32_t		rtpTimestamp;	// RTP timestamp at the same instant as ntpTransmitHi/Lo (clients should use 0).
	uint32_t		ntpOriginateHi;	// Transmit time from the original client request (clients should use 0).
	uint32_t		ntpOriginateLo;
	uint32_t		ntpReceiveHi;	// Time request received by the server (clients should use 0).
	uint32_t		ntpReceiveLo;
	uint32_t		ntpTransmitHi;	// Time client request or server response sent.
	uint32_t		ntpTransmitLo;
	
}	RTCPTimeSyncPacket;

check_compile_time( offsetof( RTCPTimeSyncPacket, pt )				==  1 );
check_compile_time( offsetof( RTCPTimeSyncPacket, length ) 			==  2 );
check_compile_time( offsetof( RTCPTimeSyncPacket, rtpTimestamp )	==  4 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpOriginateHi )	==  8 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpOriginateLo )	== 12 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpReceiveHi )	== 16 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpReceiveLo )	== 20 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpTransmitHi )	== 24 );
check_compile_time( offsetof( RTCPTimeSyncPacket, ntpTransmitLo )	== 28 );
check_compile_time( sizeof( RTCPTimeSyncPacket ) 					== 32 );

// RTCPTimeAnnounce Packet
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
// header |V=2|P|M|   0   |      PT       |           length              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
// timing |                         RTP timestamp                         |
// info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
//        |                   NTP timestamp, high 32 bits                 |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +12/0x0C
//        |                   NTP timestamp, low 32 bits                  |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10
//        |     RTP timestamp when the new timeline should be applied     |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +20/0x14

#define	RTCPTimeAnnounceInsertMarker( FIELDS, X )	( ( ( FIELDS ) & ~0x10 ) | ( ( ( X ) << 4 ) & 0x10 ) )
#define	RTCPTimeAnnounceExtractMarker( FIELDS )		( ( ( FIELDS ) >> 4 ) & 0x01 )

typedef struct
{
	uint8_t			v_p_m;		// Version (V), Padding (P), and Marker (M) fields.	
	uint8_t			pt;			// RTCP packet type.
	uint16_t		length;		// Packet length in 32-bit words - 1.
	
	uint32_t		rtpTime;	// RTP timestamp at the NTP timestamp.
	uint32_t		ntpTimeHi;	// NTP timestamp at the RTP timestamp (high 32 bits).
	uint32_t		ntpTimeLo;	// NTP timestamp at the RTP timestamp (low 32 bits).
	uint32_t		rtpApply;	// RTP timestamp when the new timeline should be applied.
	
}	RTCPTimeAnnouncePacket;

check_compile_time( offsetof( RTCPTimeAnnouncePacket, pt )			==  1 );
check_compile_time( offsetof( RTCPTimeAnnouncePacket, length ) 		==  2 );
check_compile_time( offsetof( RTCPTimeAnnouncePacket, rtpTime )		==  4 );
check_compile_time( offsetof( RTCPTimeAnnouncePacket, ntpTimeHi )	==  8 );
check_compile_time( offsetof( RTCPTimeAnnouncePacket, ntpTimeLo )	== 12 );
check_compile_time( offsetof( RTCPTimeAnnouncePacket, rtpApply )	== 16 );
check_compile_time( sizeof( RTCPTimeAnnouncePacket ) 				== 20 );

// RTCP Retransmit Request Packet
//
//             0                   1                   2                   3
//             0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
// header     |V=2|P|    0    |    PT=213     |           length              |
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
// retransmit |     Sequence Number Start     |         Packet Count          |
// info       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
//            |            Optional NTP Nanoseconds High 32 bits              |
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +12/0x0C
//            |            Optional NTP Nanoseconds Low  32 bits              |
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10

typedef struct
{
	uint8_t			v_p;			// Version (V) and Padding (P) fields.
	uint8_t			pt;				// RTCP packet type (kRTCPTypeRetransmitRequest)
	uint16_t		length;			// Packet length in 32-bit words - 1.
	
	uint16_t		seqStart;		// First packet to retransmit.
	uint16_t		seqCount;		// Number of packets to retransmit.
	
	uint32_t		ntpHi;			// OPTIONAL: High 32 bits of a 64-bit NTP-sync'd nanoseconds at time of request.
	uint32_t		ntpLo;			// OPTIONAL: Low  32 bits of a 64-bit NTP-sync'd nanoseconds at time of request.
	
}	RTCPRetransmitRequestPacket;

#define	kRTCPRetransmitRequestPacketMinSize		offsetof( RTCPRetransmitRequestPacket, ntpHi )
#define	kRTCPRetransmitRequestPacketNTPSize		sizeof( RTCPRetransmitRequestPacket )

check_compile_time( offsetof( RTCPRetransmitRequestPacket, pt )			==  1 );
check_compile_time( offsetof( RTCPRetransmitRequestPacket, length )		==  2 );
check_compile_time( offsetof( RTCPRetransmitRequestPacket, seqStart )	==  4 );
check_compile_time( offsetof( RTCPRetransmitRequestPacket, seqCount )	==  6 );
check_compile_time( offsetof( RTCPRetransmitRequestPacket, ntpHi )		==  8 );
check_compile_time( offsetof( RTCPRetransmitRequestPacket, ntpLo )		== 12 );
check_compile_time( sizeof( RTCPRetransmitRequestPacket )				== 16 );

// RTCP Retransmit Response Packet
//
//             0                   1                   2                   3
//             0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     +0/0x00
// header     |V=2|P|    0    |    PT=214     |           length              |
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     +4/0x0C
// retransmit |                                                               |
// info       |                      original RTP packet                      |
//            |                                                               |
//            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +1444/0x5A4

typedef struct
{
	uint8_t				v_p;			// Version (V) and Padding (P) fields.
	uint8_t				pt;				// RTCP packet type (kRTCPTypeRetransmitResponse)
	uint16_t			length;			// Packet length in 32-bit words - 1.
	
}	RTCPRetransmitResponseHeader;

check_compile_time( offsetof( RTCPRetransmitResponseHeader, v_p )		== 0 );
check_compile_time( offsetof( RTCPRetransmitResponseHeader, pt )		== 1 );
check_compile_time( offsetof( RTCPRetransmitResponseHeader, length )	== 2 );
check_compile_time( sizeof( RTCPRetransmitResponseHeader )				== 4 );

typedef struct
{
	uint16_t	seq; // Sequence number of packet that couldn't be delivered.
	uint16_t	pad; // Pad to 32-bit boundary. Must be 0.
	
}	RTCPRetransmitResponseFail;

check_compile_time( offsetof( RTCPRetransmitResponseFail, seq ) == 0 );
check_compile_time( offsetof( RTCPRetransmitResponseFail, pad ) == 2 );
check_compile_time( sizeof( RTCPRetransmitResponseFail )		== 4 );

typedef union
{
	uint8_t							bytes[ 1440 ];
	RTPPacket						rtp;
	RTCPRetransmitResponseFail		fail;
	
}	RTCPRetransmitResponsePayload;

check_compile_time( offsetof( RTCPRetransmitResponsePayload, bytes )	== 0 );
check_compile_time( offsetof( RTCPRetransmitResponsePayload, rtp )		== 0 );
check_compile_time( offsetof( RTCPRetransmitResponsePayload, fail )		== 0 );
check_compile_time( sizeof( RTCPRetransmitResponsePayload )				== 1440 );

typedef struct
{
	uint8_t								v_p;			// Version (V) and Padding (P) fields.
	uint8_t								pt;				// RTCP packet type (kRTCPTypeRetransmitResponse)
	uint16_t							length;			// Packet length in 32-bit words - 1.
	RTCPRetransmitResponsePayload		payload;
	
}	RTCPRetransmitResponsePacket;

check_compile_time( offsetof( RTCPRetransmitResponsePacket, pt )		==    1 );
check_compile_time( offsetof( RTCPRetransmitResponsePacket, length )	==    2 );
check_compile_time( offsetof( RTCPRetransmitResponsePacket, payload )	==    4 );
check_compile_time( sizeof( RTCPRetransmitResponsePacket )			 	== 1444 );

//	RTCP Futile Retransmit Response Packet:
//	
//				0                   1                   2                   3
//				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
//	header     |V=2|P|    0    |    PT=214     |          length=1             |
//			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
//	retransmit |       sequence number         |             pad               |
//	info       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08

typedef struct
{
	uint8_t			v_p;	// [0x00] Version (V) and Padding (P) fields.
	uint8_t			pt;		// [0x01] RTCP packet type (kRTCPTypeRetransmitResponse)
	uint16_t		length;	// [0x02] Packet length in 32-bit words - 1 = 1.
	uint16_t		seq;	// [0x04] Sequence number of packet that couldn't be delivered.
	uint16_t		pad;	// [0x06] Pad to 32-bit boundary. Must be 0.
							// [0x08] Total
	
}	RTCPFutileRetransmitResponsePacket;

check_compile_time( offsetof( RTCPFutileRetransmitResponsePacket, v_p )		== 0 );
check_compile_time( offsetof( RTCPFutileRetransmitResponsePacket, pt )		== 1 );
check_compile_time( offsetof( RTCPFutileRetransmitResponsePacket, length )	== 2 );
check_compile_time( offsetof( RTCPFutileRetransmitResponsePacket, seq )		== 4 );
check_compile_time( offsetof( RTCPFutileRetransmitResponsePacket, pad )		== 6 );
check_compile_time( sizeof( RTCPFutileRetransmitResponsePacket )			== 8 );

// RTCP Packet Union (for easier access to packet-specific data).

typedef union
{
	uint32_t								align;	// Force 4-byte alignment.
	RTCPCommonHeader						header;
	RTCPTimeSyncPacket						timeSync;
	RTCPTimeAnnouncePacket					timeAnnounce;
	RTCPRetransmitRequestPacket				retransmitReq;
	RTCPRetransmitResponsePacket			retransmitAck;
	RTCPFutileRetransmitResponsePacket		futileAck;
	
}	RTCPPacket;

#if 0
#pragma mark -
#pragma mark == Packets - Keep Alive ==
#endif

//===========================================================================================================================
//	Keep Alive Packet
//===========================================================================================================================

// Idle keep alive packet
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +0/0x00
//        |V=0|S|    0    |Length in bytes|         session ID            |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +4/0x04

typedef struct
{
	uint8_t				v_s;			// Version (V) and Sleep (S) fields.
	uint8_t				length;			// Packet length in bytes.
	uint16_t			sessionID;		// SessionID.
}	AirPlayIdleKeepAlivePacket;

check_compile_time( offsetof( AirPlayIdleKeepAlivePacket, v_s )			==  0 );
check_compile_time( offsetof( AirPlayIdleKeepAlivePacket, length )		==  1 );
check_compile_time( offsetof( AirPlayIdleKeepAlivePacket, sessionID ) 	==  2 );
check_compile_time( sizeof( AirPlayIdleKeepAlivePacket ) 				< 32 );	//UDP_KEEPALIVE_OFFLOAD_DATA_SIZE

#define	AirPlayIdleKeepAliveInsertVersion( FIELDS, X )		( ( ( FIELDS ) & ~0xC0 ) | ( ( ( X ) << 6 ) & 0xC0 ) )
#define	AirPlayIdleKeepAliveExtractVersion( FIELDS )		( ( ( FIELDS ) >> 6 ) & 0x03 )
#define kAirPlayIdleKeepAliveVerion							0

#define	AirPlayIdleKeepAliveInsertSleep( FIELDS, X )		( ( ( FIELDS ) & ~0x20 ) | ( ( ( X ) << 5 ) & 0x20 ) )
#define	AirPlayIdleKeepAliveExtractSleep( FIELDS )			( ( ( FIELDS ) >> 5 ) & 0x01 )


#if 0
#pragma mark -
#pragma mark == Packets - Screen ==
#endif

//===========================================================================================================================
//	Screen Packets
//===========================================================================================================================

#define kAirPlayScreenOpCode_VideoFrame				0
#define kAirPlayScreenOpCode_VideoConfig			1
#define kAirPlayScreenOpCode_KeepAlive				2
#define kAirPlayScreenOpCode_ForceKeyFrame			3
#define kAirPlayScreenOpCode_Ignore					4
#define kAirPlayScreenOpCode_KeepAliveWithBody		5

#define kAirPlayScreenFlag_ShowHUD				( 1 << 0 ) // 1=Show diagnostic HUD on receiver.
#define kAirPlayScreenFlag_RespectTimestamps	( 1 << 1 ) // 1=Display frames at the specified time. 0=Display immediately.
#define kAirPlayScreenFlag_Encrypted			( 1 << 2 ) // 1=Frame is encrypted.
#define kAirPlayScreenFlag_Unused3				( 1 << 3 ) // Never used in production software.
#define kAirPlayScreenFlag_Unused4				( 1 << 4 ) // Unused as of 2011-07-25.
#define kAirPlayScreenFlag_Suspended			( 1 << 5 ) // 1=Suspend display. 0=Resume display.
#define kAirPlayScreenFlag_ClearScreen			( 1 << 6 ) // 1=Clear screen to black. 0=Restore screen.
#define kAirPlayScreenFlag_InterestingFrame		( 1 << 7 ) // 1=Frame is interesting for logging.
#define kAirPlayScreenFlag2_NoDisplaySleep		( 1 << 0 ) // 1=Prevent the screen saver from coming up on the receiver.

#define kAirPlayRemoteLog_Off						0x00
#define kAirPlayRemoteLogFlag_EveryFrame			0x01
#define kAirPlayRemoteLogFlag_Periodic				0x02
#define kAirPlayRemoteLogFlag_InterestingFrames		0x04

typedef struct
{
	uint32_t		bodySize;
	uint8_t			opcode;
	uint8_t			smallParam[ 3 ];
	Value64			params[ 15 ];
	
}	AirPlayScreenHeader;

check_compile_time( offsetof( AirPlayScreenHeader, bodySize )	==   0 );
check_compile_time( offsetof( AirPlayScreenHeader, opcode )		==   4 );
check_compile_time( offsetof( AirPlayScreenHeader, smallParam )	==   5 );
check_compile_time( offsetof( AirPlayScreenHeader, params )		==   8 );
check_compile_time( sizeof( AirPlayScreenHeader )				== 128 );

#if 0
#pragma mark -
#pragma mark == Properties ==
#endif

//===========================================================================================================================
//	Properties
//===========================================================================================================================

// [Array of dictionaries] Supported formats.
// Each dictionary contains the following keys:
//		kAirPlayKey_Type
//		[kAirPlayKey_AudioInputFormats]
//		[kAirPlayKey_AudioOutputFormats]
#define kAirPlayProperty_AudioFormats				"audioFormats"

// [String] Status of the audio jack (connected/disconnect) and optionally a type of connection (analog/digital).
// Return via the Audio-Jack-Status RTSP header. See kAirPlayAudioJackStatus* constants for specific strings.
#define kAirPlayProperty_AudioJackStatus			"audioJackStatus"

// [Ordered array of dictionaries] Audio latencies.
// Each dictionary contains the following keys:
//		[kAirPlayKey_Type] - if not specified, then latencies are good for all stream types
//		[kAirPlayKey_AudioType] - if not specified, then latencies are good for all audio types
//		[kAirPlayKey_SampleRate] - if not specified, then latencies are good for all sample rates
//		[kAirPlayKey_SampleSize] - if not specified, then latencies are good for all sample sizes
//		[kAirPlayKey_Channels] - if not specified, then latencies are good for all channel counts
//		[kAirPlayKey_CompressionType] - if not specified, then latencies are good for all compression types
//		kAirPlayKey_InputLatencyMicros
//		kAirPlayKey_OutputLatencyMicros
#define kAirPlayProperty_AudioLatencies				"audioLatencies"

// [Array of strings] MAC address strings for the Bluetooth devices the accessory has.
#define kAirPlayProperty_BluetoothIDs				"bluetoothIDs"

// [Data or String:"AA:BB:CC:00:11:22"] For sets, overrides the default device ID.
// Note: not normally used, but useful for testing to allow multiple concurrent servers from a single device.
#define kAirPlayProperty_DeviceID					"deviceID"

// [Array of dictionaries] Available displays.
// Each dictionary contains following keys:
//		kAirPlayKey_DisplayFeatures
//		kAirPlayKey_HeightPixels
//		kAirPlayKey_WidthPixels
//		kAirPlayKey_UUID
//		[kAirPlayKey_EDID]
//		[kAirPlayKey_WidthPhysical]
//		[kAirPlayKey_HeightPhysical]
#define kAirPlayProperty_Displays					"displays"

// [Array of strings] Returns any platform-specific extended features. See kAirPlayExtendedFeature_* constants.
#define kAirPlayProperty_ExtendedFeatures			"extendedFeatures"

// [Number:AirPlayFeatures] Returns any platform-specific features.
// Most features are reported by the core, but some features, such as audio meta data types, are reported by the platform.
#define kAirPlayProperty_Features					"features"

// [Array of dictionaries] Returns an array of HID device descriptions.
// Each dictionary contains following keys:
//		kAirPlayKey_HIDDescriptor
//		kAirPlayKey_UUID
//		[kAirPlayKey_HIDCountryCode]
//		[kAirPlayKey_HIDProductID]
//		[kAirPlayKey_HIDVendorID]
//		[kAirPlayKey_Name]
#define kAirPlayProperty_HIDDevices					"hidDevices"

// [String] BCP-47 language for HID input.
// See <http://www.iana.org/assignments/language-subtag-registry> for a full list.
#define kAirPlayProperty_HIDInputLanguage			"hidInputLanguage"

// [Number] Hint about the type of input that's expected (e.g. characters, numbers, etc.).
#define kAirPlayProperty_HIDInputMode				"hidInputMode"

#define kAirPlayHIDInputMode_Default				 0 // Default mode for non-character input (e.g. panning).
#define kAirPlayHIDInputMode_Character				 1 // Optimized for entering characters.
#define kAirPlayHIDInputMode_Scrolling				 2 // Hint to interpret scroll gestures and send wheel HID events.
#define kAirPlayHIDInputMode_ScrollingWithCharacters 3 // Send characters and scrolls as wheel HID events.
#define kAirPlayHIDInputMode_DialPad				 4 // Send 0-9, #, * + character events only.

// [String] Interface name.
#define kAirPlayProperty_InterfaceName				"interfaceName"

// [Dictionary] Initial modes of the accessory. Contains the same keys as the "changeModes" command parameters.
#define kAirPlayProperty_Modes						"modes"

// [Data] PNG data for 104x104 pixel icon representing the accessory's logo.
#define kAirPlayProperty_OEMIcon					"oemIcon"

// [Dictionary] Array dictionaries representing OEM icons (see kAirPlayOEMIconKey_* for keys).
#define kAirPlayProperty_OEMIcons					"oemIcons"

// [String] Path to a file containing PNG data for 104x104 pixel icon representing the accessory's logo.
#define kAirPlayProperty_OEMIconPath				"oemIconPath"

// [String] Label text to accompony the accessory's logo.
#define kAirPlayProperty_OEMIconLabel				"oemIconLabel"

// [Boolean] Whether or not to display the OEM icon.
#define kAirPlayProperty_OEMIconVisible				"oemIconVisible"

// [Boolean] For asking the platform if it's okay to use AirPlay.
// If the platform doesn't implement this, it's assumed to be okay to play.
#define kAirPlayProperty_PlaybackAllowed			"playbackAllowed"

// [Boolean] Indicates if we're currently playing.
#define kAirPlayProperty_Playing					"playing"

// [Number:Integer] Sets the current amount of skew, in samples, detected to allow the platform to compensate for it.
#define kAirPlayProperty_Skew						"skew"

// [Boolean] Read-only.  Returns true if the platform supports and has enabled its own fine-grained skew compensation.
// If the platform reports false here, a potentially lower quality form of skew compensation may be used.
#define kAirPlayProperty_SkewCompensation			"skewCompensation"

// [String] AirPlay source version string (e.g. "101.7").
#define kAirPlayProperty_SourceVersion				"sourceVersion"

// [Integer:AirPlayStatusFlags] Returns any platform-specific system flags.
#define kAirPlayProperty_StatusFlags				"statusFlags"

// [Integer] Offset in milliseconds to adjust the timeline (e.g. to compensate for delays after the device).
#define kAirPlayProperty_TimelineOffset				"timelineOffset"

// [Array] Array of timestamp string labels describing each stage of the screen frame processing pipeline.
#define kAirPlayProperty_TimestampInfo				"timestampInfo"

// [Number:NetTransportType] transportType over which the object is connected.
#define kAirPlayProperty_TransportType				"transportType"

// [Number:Float32] dB attenuation level for audio.
// kAirTunesSilenceVolumeDB (-144 dB) should be treated as silence with no sound coming out.
// kAirTunesMinVolumeDB      (-30 dB) is the minimum volume level.
// kAirTunesMaxVolumeDB        (0 dB) is the maximum volume level.
// kAirTunesDisabledVolumeDB   (1 dB) is a special flag meaning "volume control is disabled" (line level).
//
// The following converts between linear and dB volumes:
//		dB		= 20 * log10( linear )
//		linear	= pow( 10, dB / 20 )
#define kAirPlayProperty_Volume						"volume"

#if 0
#pragma mark -
#pragma mark == Threads ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlay Threads
	@abstract	Constants for managing threads.
*/

// Receiver

	#ifndef kAirPlayThreadPriority_ScreenReceiver
	#define kAirPlayThreadPriority_ScreenReceiver	60 // airtunesd: Receives screen frames.
	#endif
#ifndef kAirPlayThreadPriority_AudioDecoder
#define kAirPlayThreadPriority_AudioDecoder			61 // airtunesd: Decodes audio
#endif
#ifndef kAirPlayThreadPriority_AudioReceiver
#define kAirPlayThreadPriority_AudioReceiver		61 // airtunesd: Receives audio frames.
#endif
#ifndef kAirPlayThreadPriority_AudioSender
#define kAirPlayThreadPriority_AudioSender			62 // airtunesd: Sends audio frames.
#endif
#ifndef kAirPlayThreadPriority_TimeSyncClient
#define kAirPlayThreadPriority_TimeSyncClient		62 // airtunesd: Sends NTP requests and receives NTP responses.
#endif
#ifndef kAirPlayThreadPriority_KeepAliveReceiver
#define kAirPlayThreadPriority_KeepAliveReceiver	63 // airtunesd: Receives keep alive beacons.
#endif

#endif	// __AirPlayCommon_h__
