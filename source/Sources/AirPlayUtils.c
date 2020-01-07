/*
	File:    	AirPlayUtils.c
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#include "AirPlayUtils.h"

#include <CoreUtils/CFUtils.h>
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/PrintFUtils.h>
#include <CoreUtils/StringUtils.h>
#include <CoreUtils/ThreadUtils.h>
#include <CoreUtils/TickUtils.h>

#include "AirPlayCommon.h"

#include COREAUDIO_HEADER
#include SHA_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

ulog_define( AirPlayJitterBuffer, kLogLevelNotice, kLogFlags_Default, "AirPlay", "AirPlayJitterBuffer:rate=3;1000" );
#define ap_jitter_ulog( LEVEL, ... )		ulog( &log_category_from_name( AirPlayJitterBuffer ), (LEVEL), __VA_ARGS__ )
#define ap_jitter_label( CTX )				( (CTX)->label ? (CTX)->label : "Default" )

//===========================================================================================================================
//	ASBDToAirPlayAudioFormat
//===========================================================================================================================

OSStatus
	ASBDToAirPlayAudioFormat(
		const AudioStreamBasicDescription *	inASBD,
		AirPlayAudioFormat *				outFormat )
{
	AirPlayAudioFormat format = kAirPlayAudioFormat_Invalid;
	
	switch( inASBD->mFormatID )
	{
		case kAudioFormatLinearPCM:
			if( inASBD->mSampleRate == 8000 )
			{
				if( inASBD->mChannelsPerFrame == 1 )
					format = kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 )
					format = kAirPlayAudioFormat_PCM_8KHz_16Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 16000 )
			{
				if( inASBD->mChannelsPerFrame == 1 )
					format = kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 )
					format = kAirPlayAudioFormat_PCM_16KHz_16Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 24000 )
			{
				if( inASBD->mChannelsPerFrame == 1 )
					format = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 )
					format = kAirPlayAudioFormat_PCM_24KHz_16Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 32000 )
			{
				if( inASBD->mChannelsPerFrame == 1 )
					format = kAirPlayAudioFormat_PCM_32KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 )
					format = kAirPlayAudioFormat_PCM_32KHz_16Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 44100 )
			{
				if( inASBD->mChannelsPerFrame == 1 && inASBD->mBitsPerChannel == 16 )
					format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 && inASBD->mBitsPerChannel == 16)
					format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
				else if( inASBD->mChannelsPerFrame == 1 && inASBD->mBitsPerChannel == 24 )
					format = kAirPlayAudioFormat_PCM_44KHz_24Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 && inASBD->mBitsPerChannel == 24 )
					format = kAirPlayAudioFormat_PCM_44KHz_24Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 48000 )
			{
				if( inASBD->mChannelsPerFrame == 1 && inASBD->mBitsPerChannel == 16 )
					format = kAirPlayAudioFormat_PCM_48KHz_16Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 && inASBD->mBitsPerChannel == 16 )
					format = kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
				else if( inASBD->mChannelsPerFrame == 1 && inASBD->mBitsPerChannel == 24 )
					format = kAirPlayAudioFormat_PCM_48KHz_24Bit_Mono;
				else if( inASBD->mChannelsPerFrame == 2 && inASBD->mBitsPerChannel == 24 )
					format = kAirPlayAudioFormat_PCM_48KHz_24Bit_Stereo;
			}
			break;
		case kAudioFormatAppleLossless:
			if( inASBD->mSampleRate == 44100 && inASBD->mChannelsPerFrame == 2 )
			{
				if( inASBD->mFormatFlags & kAppleLosslessFormatFlag_16BitSourceData )
					format = kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo;
				else if( inASBD->mFormatFlags & kAppleLosslessFormatFlag_24BitSourceData )
					format = kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo;
			}
			else if( inASBD->mSampleRate == 48000 && inASBD->mChannelsPerFrame == 2 )
			{
				if( inASBD->mFormatFlags & kAppleLosslessFormatFlag_16BitSourceData )
					format = kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo;
				else if( inASBD->mFormatFlags & kAppleLosslessFormatFlag_24BitSourceData )
					format = kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo;
			}
			break;
		case kAudioFormatMPEG4AAC:
			if( inASBD->mChannelsPerFrame == 2 )
			{
				if( inASBD->mSampleRate == 44100 )
					format = kAirPlayAudioFormat_AAC_LC_44KHz_Stereo;
				else if( inASBD->mSampleRate == 48000 )
					format = kAirPlayAudioFormat_AAC_LC_48KHz_Stereo;
			}
			break;
		case kAudioFormatMPEG4AAC_ELD:
			if( inASBD->mChannelsPerFrame == 1 )
			{
				if( inASBD->mSampleRate == 16000 )
					format = kAirPlayAudioFormat_AAC_ELD_16KHz_Mono;
				else if( inASBD->mSampleRate == 24000 )
					format = kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
			}
			else if( inASBD->mChannelsPerFrame == 2 )
			{
				if( inASBD->mSampleRate == 44100 )
					format = kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo;
				else if( inASBD->mSampleRate == 48000 )
					format = kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo;
			}
			break;
		case kAudioFormatOpus:
			if( inASBD->mChannelsPerFrame == 1 )
			{
				if( inASBD->mSampleRate == 16000 )
					format = kAirPlayAudioFormat_OPUS_16KHz_Mono;
				else if( inASBD->mSampleRate == 24000 )
					format = kAirPlayAudioFormat_OPUS_24KHz_Mono;
				else if( inASBD->mSampleRate == 48000 )
					format = kAirPlayAudioFormat_OPUS_48KHz_Mono;
			}
			break;
		default:
			format = kAirPlayAudioFormat_Invalid;
	}
	
	if( format != kAirPlayAudioFormat_Invalid )
	{
		*outFormat = format;
		return( kNoErr );
	}
	
	return( kUnsupportedErr );
}

//===========================================================================================================================
//	AirPlayAudioFormatToASBD
//===========================================================================================================================

OSStatus
	AirPlayAudioFormatToASBD( 
		AirPlayAudioFormat				inFormat, 
		AudioStreamBasicDescription *	outASBD, 
		uint32_t *						outBitsPerChannel )
{
	switch( inFormat )
	{
		case kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono:		ASBD_FillPCM( outASBD,  8000, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_8KHz_16Bit_Stereo:		ASBD_FillPCM( outASBD,  8000, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono:		ASBD_FillPCM( outASBD, 16000, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_16KHz_16Bit_Stereo:	ASBD_FillPCM( outASBD, 16000, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono:		ASBD_FillPCM( outASBD, 24000, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_24KHz_16Bit_Stereo:	ASBD_FillPCM( outASBD, 24000, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_32KHz_16Bit_Mono:		ASBD_FillPCM( outASBD, 32000, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_32KHz_16Bit_Stereo:	ASBD_FillPCM( outASBD, 32000, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_44KHz_16Bit_Mono:		ASBD_FillPCM( outASBD, 44100, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo:	ASBD_FillPCM( outASBD, 44100, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_44KHz_24Bit_Mono:		ASBD_FillPCM( outASBD, 44100, 24, 24, 1 ); break;
		case kAirPlayAudioFormat_PCM_44KHz_24Bit_Stereo:	ASBD_FillPCM( outASBD, 44100, 24, 24, 2 ); break;
		case kAirPlayAudioFormat_PCM_48KHz_16Bit_Mono:		ASBD_FillPCM( outASBD, 48000, 16, 16, 1 ); break;
		case kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo:	ASBD_FillPCM( outASBD, 48000, 16, 16, 2 ); break;
		case kAirPlayAudioFormat_PCM_48KHz_24Bit_Mono:		ASBD_FillPCM( outASBD, 48000, 24, 24, 1 ); break;
		case kAirPlayAudioFormat_PCM_48KHz_24Bit_Stereo:	ASBD_FillPCM( outASBD, 48000, 24, 24, 2 ); break;
		case kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo:	ASBD_FillALAC( outASBD, 44100, 16, 2 ); break;
		case kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo:	ASBD_FillALAC( outASBD, 44100, 24, 2 ); break;
		case kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo:	ASBD_FillALAC( outASBD, 48000, 16, 2 ); break;
		case kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo:	ASBD_FillALAC( outASBD, 48000, 24, 2 ); break;
		case kAirPlayAudioFormat_AAC_LC_44KHz_Stereo:		ASBD_FillAAC_LC( outASBD, 44100, 2 ); break;
		case kAirPlayAudioFormat_AAC_LC_48KHz_Stereo:		ASBD_FillAAC_LC( outASBD, 48000, 2 ); break;
		case kAirPlayAudioFormat_AAC_ELD_16KHz_Mono:		ASBD_FillAAC_ELD( outASBD, 16000, 1 ); break;
		case kAirPlayAudioFormat_AAC_ELD_24KHz_Mono:		ASBD_FillAAC_ELD( outASBD, 24000, 1 ); break;
		case kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo:		ASBD_FillAAC_ELD( outASBD, 44100, 2 ); break;
		case kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo:		ASBD_FillAAC_ELD( outASBD, 48000, 2 ); break;
		case kAirPlayAudioFormat_OPUS_16KHz_Mono:			ASBD_FillOpus( outASBD, 16000, 1 ); break;
		case kAirPlayAudioFormat_OPUS_24KHz_Mono:			ASBD_FillOpus( outASBD, 24000, 1 ); break;
		case kAirPlayAudioFormat_OPUS_48KHz_Mono:			ASBD_FillOpus( outASBD, 48000, 1 ); break;
		default: return( kUnsupportedErr );
	}
	if( outBitsPerChannel )
	{
		if( ( inFormat == kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo )	||
			( inFormat == kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo )	||
			( inFormat == kAirPlayAudioFormat_AAC_LC_44KHz_Stereo )		||
			( inFormat == kAirPlayAudioFormat_AAC_LC_48KHz_Stereo )		||
			( inFormat == kAirPlayAudioFormat_AAC_ELD_16KHz_Mono )		||
			( inFormat == kAirPlayAudioFormat_AAC_ELD_24KHz_Mono )		||
			( inFormat == kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo )	||
			( inFormat == kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo )	||
			( inFormat == kAirPlayAudioFormat_OPUS_16KHz_Mono )			||
			( inFormat == kAirPlayAudioFormat_OPUS_24KHz_Mono )			||
			( inFormat == kAirPlayAudioFormat_OPUS_48KHz_Mono ) )
		{
			*outBitsPerChannel = 16;
		}
		else if( ( inFormat == kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo ) ||
				 ( inFormat == kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo ) )
		{
			*outBitsPerChannel = 24;
		}
		else
		{
			check( outASBD->mBitsPerChannel > 0 );
			*outBitsPerChannel = outASBD->mBitsPerChannel;
		}
	}
	return( kNoErr );
}

//===========================================================================================================================
//	AirPlayAudioFormatToPCM
//===========================================================================================================================

OSStatus	AirPlayAudioFormatToPCM( AirPlayAudioFormat inFormat, AudioStreamBasicDescription *outASBD )
{
	OSStatus						err;
	AudioStreamBasicDescription		asbd;
	uint32_t						bitsPerChannel;
	
	err = AirPlayAudioFormatToASBD( inFormat, &asbd, &bitsPerChannel );
	require_noerr_quiet( err, exit );
	
	if( asbd.mFormatID == kAudioFormatLinearPCM )
	{
		memcpy( outASBD, &asbd, sizeof( asbd ) );
	}
	else
	{
		ASBD_FillPCM( outASBD, asbd.mSampleRate, bitsPerChannel, RoundUp( bitsPerChannel, 8 ), asbd.mChannelsPerFrame );
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayAudioFormatToPCM
//===========================================================================================================================

AudioFormatID	AirPlayCompressionTypeToAudioFormatID( AirPlayCompressionType inCompressionType )
{
	AudioFormatID formatID = 0;
	switch( inCompressionType )
	{
		case kAirPlayCompressionType_PCM:		formatID = kAudioFormatLinearPCM; break;
		case kAirPlayCompressionType_ALAC:		formatID = kAudioFormatAppleLossless; break;
		case kAirPlayCompressionType_AAC_LC:	formatID = kAudioFormatMPEG4AAC; break;
		case kAirPlayCompressionType_AAC_ELD:	formatID = kAudioFormatMPEG4AAC_ELD; break;
		case kAirPlayCompressionType_OPUS:		formatID = kAudioFormatOpus; break;
		default:								formatID = 0;
	}
	return( formatID );
}

AirPlayCompressionType	AudioFormatIDToAirPlayCompressionType( AudioFormatID inFormatID )
{
	AirPlayCompressionType compressionType = 0;
	switch( inFormatID )
	{
		case kAudioFormatLinearPCM:			compressionType = kAirPlayCompressionType_PCM; break;
		case kAudioFormatAppleLossless:		compressionType = kAirPlayCompressionType_ALAC; break;
		case kAudioFormatMPEG4AAC:			compressionType = kAirPlayCompressionType_AAC_LC; break;
		case kAudioFormatMPEG4AAC_ELD:		compressionType = kAirPlayCompressionType_AAC_ELD; break;
		case kAudioFormatOpus:				compressionType = kAirPlayCompressionType_OPUS; break;
		default:							compressionType = 0;
	}
	return( compressionType );
}



//===========================================================================================================================
//	AirPlayCreateModesDictionary
//===========================================================================================================================

EXPORT_GLOBAL
CFDictionaryRef	AirPlayCreateModesDictionary( const AirPlayModeChanges *inChanges, CFStringRef inReason, OSStatus *outErr )
{
	CFDictionaryRef				result		= NULL;
	Boolean const				useStrings	= false;
	CFMutableDictionaryRef		params;
	CFMutableArrayRef			appStates	= NULL;
	CFMutableArrayRef			resources	= NULL;
	CFMutableDictionaryRef		tempDict	= NULL;
	CFStringRef					key;
	OSStatus					err;
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	
	// AppState: PhoneCall
	
	if( inChanges->phoneCall != kAirPlayTriState_NotApplicable )
	{
		tempDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( tempDict, exit, err = kNoMemoryErr );
		if( useStrings )
		{
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_AppStateID ), 
				AirPlayAppStateIDToCFString( kAirPlayAppStateID_PhoneCall ) );
		}
		else
		{
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_AppStateID ), kAirPlayAppStateID_PhoneCall );
		}
		CFDictionarySetBoolean( tempDict, CFSTR( kAirPlayKey_State ), inChanges->phoneCall != kAirPlayTriState_False );
		
		err = CFArrayEnsureCreatedAndAppend( &appStates, tempDict );
		CFRelease( tempDict );
		tempDict = NULL;
		require_noerr( err, exit );
	}
	
	// AppState: Speech
	
	if( inChanges->speech != kAirPlayEntity_NotApplicable )
	{
		tempDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( tempDict, exit, err = kNoMemoryErr );
		if( useStrings )
		{
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_AppStateID ), 
				AirPlayAppStateIDToCFString( kAirPlayAppStateID_Speech ) );
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_SpeechMode ), AirPlaySpeechModeToCFString( inChanges->speech ) );
		}
		else
		{
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_AppStateID ), kAirPlayAppStateID_Speech );
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_SpeechMode ), inChanges->speech );
		}
		
		err = CFArrayEnsureCreatedAndAppend( &appStates, tempDict );
		CFRelease( tempDict );
		tempDict = NULL;
		require_noerr( err, exit );
	}
	
	// AppState: TurnByTurn
	
	if( inChanges->turnByTurn != kAirPlayTriState_NotApplicable )
	{
		tempDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( tempDict, exit, err = kNoMemoryErr );
		if( useStrings )
		{
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_AppStateID ), 
				AirPlayAppStateIDToCFString( kAirPlayAppStateID_TurnByTurn ) );
		}
		else
		{
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_AppStateID ), kAirPlayAppStateID_TurnByTurn );
		}
		CFDictionarySetBoolean( tempDict, CFSTR( kAirPlayKey_State ), inChanges->turnByTurn != kAirPlayTriState_False );
		
		err = CFArrayEnsureCreatedAndAppend( &appStates, tempDict );
		CFRelease( tempDict );
		tempDict = NULL;
		require_noerr( err, exit );
	}
	
	// Resource: Screen
	
	if( inChanges->screen.type != kAirPlayTransferType_NotApplicable )
	{
		tempDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( tempDict, exit, err = kNoMemoryErr );
		
		if( useStrings )
		{
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_ResourceID ), 
				AirPlayResourceIDToCFString( kAirPlayResourceID_MainScreen ) );
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TransferType ), 
				AirPlayTransferTypeToCFString( inChanges->screen.type ) );
			if( inChanges->screen.priority != kAirPlayTransferPriority_NotApplicable )
			{
				CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TransferPriority ), 
					AirPlayTransferPriorityToCFString( inChanges->screen.priority ) );
			}
			if( inChanges->screen.takeConstraint != kAirPlayConstraint_NotApplicable )
			{
				CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TakeConstraint ), 
					AirPlayConstraintToCFString( inChanges->screen.takeConstraint ) );
			}
			if( inChanges->screen.borrowOrUnborrowConstraint != kAirPlayConstraint_NotApplicable )
			{
				if(      inChanges->screen.type == kAirPlayTransferType_Take )   key = CFSTR( kAirPlayKey_BorrowConstraint );
				else if( inChanges->screen.type == kAirPlayTransferType_Borrow ) key = CFSTR( kAirPlayKey_UnborrowConstraint );
				else { dlogassert( "Bad borrow/unborrow constraint" ); err = kParamErr; goto exit; }
				CFDictionarySetValue( tempDict, key, AirPlayConstraintToCFString( inChanges->screen.borrowOrUnborrowConstraint ) );
			}
		}
		else
		{
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_ResourceID ), kAirPlayResourceID_MainScreen );
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TransferType ), inChanges->screen.type );
			if( inChanges->screen.priority != kAirPlayTransferPriority_NotApplicable )
			{
				CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TransferPriority ), inChanges->screen.priority );
			}
			if( inChanges->screen.takeConstraint != kAirPlayConstraint_NotApplicable )
			{
				CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TakeConstraint ), inChanges->screen.takeConstraint );
			}
			if( inChanges->screen.borrowOrUnborrowConstraint != kAirPlayConstraint_NotApplicable )
			{
				if(      inChanges->screen.type == kAirPlayTransferType_Take )   key = CFSTR( kAirPlayKey_BorrowConstraint );
				else if( inChanges->screen.type == kAirPlayTransferType_Borrow ) key = CFSTR( kAirPlayKey_UnborrowConstraint );
				else { dlogassert( "Bad borrow/unborrow constraint" ); err = kParamErr; goto exit; }
				CFDictionarySetInt64( tempDict, key, inChanges->screen.borrowOrUnborrowConstraint );
			}
		}
		
		err = CFArrayEnsureCreatedAndAppend( &resources, tempDict );
		CFRelease( tempDict );
		tempDict = NULL;
		require_noerr( err, exit );
	}
	
	// Resource: MainAudio
	
	if( inChanges->mainAudio.type != kAirPlayTransferType_NotApplicable )
	{
		tempDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( tempDict, exit, err = kNoMemoryErr );
		
		if( useStrings )
		{
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_ResourceID ), 
				AirPlayResourceIDToCFString( kAirPlayResourceID_MainAudio ) );
			CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TransferType ), 
				AirPlayTransferTypeToCFString( inChanges->mainAudio.type ) );
			if( inChanges->mainAudio.priority != kAirPlayTransferPriority_NotApplicable )
			{
				CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TransferPriority ), 
					AirPlayTransferPriorityToCFString( inChanges->mainAudio.priority ) );
			}
			if( inChanges->mainAudio.takeConstraint != kAirPlayConstraint_NotApplicable )
			{
				CFDictionarySetValue( tempDict, CFSTR( kAirPlayKey_TakeConstraint ), 
					AirPlayConstraintToCFString( inChanges->mainAudio.takeConstraint ) );
			}
			if( inChanges->mainAudio.borrowOrUnborrowConstraint != kAirPlayConstraint_NotApplicable )
			{
				if(      inChanges->mainAudio.type == kAirPlayTransferType_Take )   key = CFSTR( kAirPlayKey_BorrowConstraint );
				else if( inChanges->mainAudio.type == kAirPlayTransferType_Borrow ) key = CFSTR( kAirPlayKey_UnborrowConstraint );
				else { dlogassert( "Bad borrow/unborrow constraint" ); err = kParamErr; goto exit; }
				CFDictionarySetValue( tempDict, key, AirPlayConstraintToCFString( inChanges->mainAudio.borrowOrUnborrowConstraint ) );
			}
		}
		else
		{
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_ResourceID ), kAirPlayResourceID_MainAudio );
			CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TransferType ), inChanges->mainAudio.type );
			if( inChanges->mainAudio.priority != kAirPlayTransferPriority_NotApplicable )
			{
				CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TransferPriority ), inChanges->mainAudio.priority );
			}
			if( inChanges->mainAudio.takeConstraint != kAirPlayConstraint_NotApplicable )
			{
				CFDictionarySetInt64( tempDict, CFSTR( kAirPlayKey_TakeConstraint ), inChanges->mainAudio.takeConstraint );
			}
			if( inChanges->mainAudio.borrowOrUnborrowConstraint != kAirPlayConstraint_NotApplicable )
			{
				if(      inChanges->mainAudio.type == kAirPlayTransferType_Take )   key = CFSTR( kAirPlayKey_BorrowConstraint );
				else if( inChanges->mainAudio.type == kAirPlayTransferType_Borrow ) key = CFSTR( kAirPlayKey_UnborrowConstraint );
				else { dlogassert( "Bad borrow/unborrow constraint" ); err = kParamErr; goto exit; }
				CFDictionarySetInt64( tempDict, key, inChanges->mainAudio.borrowOrUnborrowConstraint );
			}
		}
		
		err = CFArrayEnsureCreatedAndAppend( &resources, tempDict );
		CFRelease( tempDict );
		tempDict = NULL;
		require_noerr( err, exit );
	}
	
	if( appStates )	CFDictionarySetValue( params, CFSTR( kAirPlayKey_AppStates ), appStates );
	if( resources )	CFDictionarySetValue( params, CFSTR( kAirPlayKey_Resources ), resources );
	if( inReason )	CFDictionarySetValue( params, CFSTR( kAirPlayKey_ReasonStr ), inReason );
	result = params;
	params = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( params );
	CFReleaseNullSafe( appStates );
	CFReleaseNullSafe( resources );
	CFReleaseNullSafe( tempDict );
	if( outErr ) *outErr = err;
	return( result );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlay_DeriveAESKeySHA512
//===========================================================================================================================

void
	AirPlay_DeriveAESKeySHA512(
		const void *		inMasterKeyPtr,
		size_t				inMasterKeyLen,
		const void *		inKeySaltPtr,
		size_t				inKeySaltLen,
		const void *		inIVSaltPtr,
		size_t				inIVSaltLen,
		uint8_t				outKey[ 16 ],
		uint8_t				outIV[ 16 ] )
{
	SHA512_CTX		shaCtx;
	uint8_t			buf[ 64 ];
	
	if( outKey )
	{
		SHA512_Init( &shaCtx );
		SHA512_Update( &shaCtx, inKeySaltPtr, inKeySaltLen );
		SHA512_Update( &shaCtx, inMasterKeyPtr, inMasterKeyLen );
		SHA512_Final( buf, &shaCtx );
		memcpy( outKey, buf, 16 );
	}
	if( outIV )
	{
		SHA512_Init( &shaCtx );
		SHA512_Update( &shaCtx, inIVSaltPtr, inIVSaltLen );
		SHA512_Update( &shaCtx, inMasterKeyPtr, inMasterKeyLen );
		SHA512_Final( buf, &shaCtx );
		memcpy( outIV, buf, 16 );
	}
	MemZeroSecure( buf, sizeof( buf ) );
}

//===========================================================================================================================
//	AirPlay_DeriveAESKeySHA512ForScreen
//===========================================================================================================================

void
	AirPlay_DeriveAESKeySHA512ForScreen(
		const void *		inMasterKeyPtr,
		size_t				inMasterKeyLen,
		uint64_t			inScreenStreamConnectionID,
		uint8_t				outKey[ 16 ],
		uint8_t				outIV[ 16 ] )
{

	char *screenStreamKeySalt = NULL, *screenStreamIVSalt = NULL;
	size_t screenStreamKeySaltLen, screenStreamIVSaltLen;
				
	screenStreamKeySaltLen = ASPrintF( &screenStreamKeySalt, "%s%llu", kAirPlayEncryptionStreamPrefix_Key, inScreenStreamConnectionID );
	screenStreamIVSaltLen = ASPrintF( &screenStreamIVSalt, "%s%llu", kAirPlayEncryptionStreamPrefix_IV, inScreenStreamConnectionID );
				
	AirPlay_DeriveAESKeySHA512( inMasterKeyPtr, inMasterKeyLen, screenStreamKeySalt, screenStreamKeySaltLen, screenStreamIVSalt, screenStreamIVSaltLen, outKey, outIV );

	MemZeroSecure( screenStreamKeySalt, screenStreamKeySaltLen );
	MemZeroSecure( screenStreamIVSalt, screenStreamIVSaltLen );

	free( screenStreamKeySalt );
	free( screenStreamIVSalt );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	RTPJitterBufferInternals
//===========================================================================================================================

	#define RTPJitterBufferLock( CTX, UNUSED )		dispatch_semaphore_wait( (CTX)->nodeLock, DISPATCH_TIME_FOREVER )
	#define RTPJitterBufferUnlock( CTX, UNUSED )	dispatch_semaphore_signal( (CTX)->nodeLock )

#define RTPJitterBufferSamplesToMs( CTX, X )	( ( ( 1000 * (X) ) + (uint32_t)( (CTX)->inputFormat.mSampleRate / 2 ) ) / (uint32_t)(CTX)->inputFormat.mSampleRate )
#define RTPJitterBufferMsToSamples( CTX, X )	( ( (X) * (uint32_t)(CTX)->inputFormat.mSampleRate ) / 1000 )
#define RTPJitterBufferBufferedMs( CTX )		RTPJitterBufferSamplesToMs( (CTX), _RTPJitterBufferBufferedSamples( (CTX), false ) )
#define RTPJitterBufferPreparedMs( CTX )		RTPJitterBufferSamplesToMs( (CTX), _RTPJitterBufferBufferedSamples( (CTX), true ) )

static uint32_t _RTPJitterBufferBufferedSamples( RTPJitterBufferContext *ctx, Boolean preparedOnly );
static OSStatus _RTPJitterBufferDecodeNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode );
static void * _RTPJitterBufferDecodeThread( void *inCtx );

//===========================================================================================================================
//	_RTPJitterBufferLog
//===========================================================================================================================

typedef struct
{
	LogLevel	level;
	char *		msg;
}
RTPJitterBufferLogContext;

static void _RTPJitterBufferLog( void *inCtx )
{
	RTPJitterBufferLogContext *		ctx = inCtx;

	if( ctx )
	{
		ap_jitter_ulog( ctx->level, "%s", ctx->msg );
		free( ctx->msg );
		free( ctx );
	}
}

static void RTPJitterBufferLog( RTPJitterBufferContext *ctx, LogLevel inLevel, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 3, 4 )
{
	RTPJitterBufferLogContext *		logCtx;
	va_list							args;

	if( log_category_enabled( &log_category_from_name( AirPlayJitterBuffer ), inLevel ) )
	{
		logCtx = malloc( sizeof( *logCtx ) );
		logCtx->level = inLevel;
		va_start( args, inFormat );
		VASPrintF( &logCtx->msg, inFormat, args );
		va_end( args );
		dispatch_async_f( ctx->logQueue, logCtx, _RTPJitterBufferLog);
	}
}

//===========================================================================================================================
//	RTPJitterBufferInit
//===========================================================================================================================

OSStatus
	RTPJitterBufferInit(
		RTPJitterBufferContext *				ctx,
		const AudioStreamBasicDescription *		inInputFormat,
		const AudioStreamBasicDescription *		inOutputFormat,
		uint32_t								inBufferMs )
{
	OSStatus		err;
	size_t			i;
	uint32_t		framesPerPacket;
	
	memset( ctx, 0, sizeof( *ctx ) );
	
	ctx->nodeLock = dispatch_semaphore_create( 1 );
	require_action( ctx->nodeLock, exit, err = kNoResourcesErr );
	require_action( inInputFormat, exit, err = kParamErr );
	require_action( inInputFormat->mFramesPerPacket > 0 || inInputFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kParamErr );
	require_action( !inOutputFormat || inOutputFormat->mSampleRate == inInputFormat->mSampleRate, exit, err = kParamErr );
	require_action( !inOutputFormat || inOutputFormat->mChannelsPerFrame == inInputFormat->mChannelsPerFrame, exit, err = kParamErr );

	framesPerPacket = inInputFormat->mFramesPerPacket > 0 ? inInputFormat->mFramesPerPacket : kAirPlaySamplesPerPacket_PCM;
	
	//   Min # of packets to hold inBufferMs audio
	//   = inBufferMs * inSampleRate / ( 1000 * framesPerPacket ); (Assumes packets are fully filled when PCM or contain one compressed packet otherwise)
	//   = inBufferMs * inSampleRate / ( 1000 * framesPerPacket ) + 0.5; ( round up )
	//   = ( inBufferMs * inSampleRate + 500 * framesPerPacket ) / ( 1000 * framesPerPacket );
	//   The allocated JB is 2 times the minium to handle jitter.

	ctx->nodesAllocated = 2 * ( inBufferMs * ( (uint32_t) inInputFormat->mSampleRate ) + 500 * framesPerPacket ) / ( 1000 * framesPerPacket );
	if( 50 > ctx->nodesAllocated )
		ctx->nodesAllocated = 50; // ~400 ms at 352 samples per packet and 44100 Hz.
	ctx->packets = (RTPPacketNode *) calloc( ctx->nodesAllocated, sizeof( *ctx->packets ) );
	require_action( ctx->packets, exit, err = kNoMemoryErr );
	
	TAILQ_INIT( &ctx->freeList );
	TAILQ_INIT( &ctx->preparedList );
	TAILQ_INIT( &ctx->receivedList );
	
	ctx->inputFormat			= *inInputFormat;
	ctx->inputFormat.mReserved	= 0;
	ctx->outputFormat			= inOutputFormat ? *inOutputFormat : *inInputFormat;
	ctx->outputFormat.mReserved	= 0;
	ctx->bufferMs				= inBufferMs;
	ctx->buffering				= true;
	
	// Set up a decoder if the input and output formats don't match
	
	if( memcmp( &ctx->inputFormat, &ctx->outputFormat, sizeof( AudioStreamBasicDescription ) ) != 0)
	{
		err = AudioConverterNew( &ctx->inputFormat, &ctx->outputFormat, &ctx->decoder );
		require_noerr( err, exit );
		
		// Allocate decode buffer backing
		ctx->decodeBuffers = malloc( framesPerPacket * ctx->outputFormat.mBytesPerFrame * ctx->nodesAllocated );
	}
		
	// Set up the decode thread
	err = pthread_mutex_init( &ctx->decodeMutex, NULL );
	require_noerr( err, exit );
	ctx->decodeMutexPtr = &ctx->decodeMutex;
	
	err = pthread_cond_init( &ctx->decodeCondition, NULL );
	require_noerr( err, exit );
	ctx->decodeConditionPtr = &ctx->decodeCondition;

	err = pthread_create( &ctx->decodeThread, NULL, _RTPJitterBufferDecodeThread, ctx );
	require_noerr( err, exit );
	ctx->decodeThreadPtr = &ctx->decodeThread;

	for( i = 0; i < ctx->nodesAllocated; ++i )
	{
		TAILQ_INSERT_HEAD( &ctx->freeList, &ctx->packets[ i ], list );
		ctx->packets[ i ].jitterBuffer = ctx;
		if( ctx->decodeBuffers )
		{
			ctx->packets[ i ].decodeLock = dispatch_semaphore_create( 1 );
			require_action( ctx->packets[ i ].decodeLock, exit, err = kNoMemoryErr );
			ctx->packets[ i ].decodeBuffer = ctx->decodeBuffers + ( framesPerPacket * ctx->outputFormat.mBytesPerFrame * i );
		}
	}
	
	ctx->logQueue = dispatch_queue_create( "com.apple.airplay.jitterbufferlog", NULL );
	
	err = kNoErr;
	
exit:
	if( err ) RTPJitterBufferFree( ctx );
	return( err );
}

//===========================================================================================================================
//	RTPJitterBufferFree
//===========================================================================================================================

void	RTPJitterBufferFree( RTPJitterBufferContext *ctx )
{
	size_t		i;
	
	if( ( ctx->nLate > 0 ) || ( ctx->nGaps > 0 ) || ( ctx->nSkipped > 0 ) || ( ctx->nRebuffer > 0 ) )
	{
		RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit,
			"### %s: Buffering issues during session: Late=%u Missing=%u Gaps=%u Rebuffers=%u\n", 
			ap_jitter_label( ctx ), ctx->nLate, ctx->nGaps, ctx->nSkipped, ctx->nRebuffer );
	}
	ctx->nLate		= 0;
	ctx->nGaps		= 0;
	ctx->nSkipped	= 0;
	ctx->nRebuffer	= 0;
	
	if( ctx->logQueue )
	{
		dispatch_sync_f( ctx->logQueue, NULL, _RTPJitterBufferLog );
		dispatch_forget( &ctx->logQueue );
	}
	
	if( ctx->decodeThreadPtr )
	{
		ctx->decodeDone = true;
		pthread_mutex_lock( ctx->decodeMutexPtr );
		pthread_cond_signal( ctx->decodeConditionPtr );
		pthread_mutex_unlock( ctx->decodeMutexPtr );
		pthread_join( ctx->decodeThread, NULL );
	}
	
	pthread_mutex_forget( &ctx->decodeMutexPtr );
	pthread_cond_forget( &ctx->decodeConditionPtr );
	ctx->decodeThreadPtr = NULL;
	
	TAILQ_INIT( &ctx->freeList );
	TAILQ_INIT( &ctx->preparedList );
	TAILQ_INIT( &ctx->receivedList );

	dispatch_forget( &ctx->nodeLock );
	if( ctx->packets )
	{
		for( i = 0; i < ctx->nodesAllocated; ++i )
			dispatch_forget( &ctx->packets[i].decodeLock );
	}
	ForgetMem( &ctx->packets );
	AudioConverterForget( &ctx->decoder );
	ForgetMem( &ctx->decodeBuffers );
}

//===========================================================================================================================
//	RTPJitterBufferReset
//===========================================================================================================================

void	RTPJitterBufferReset( RTPJitterBufferContext *ctx, Float64 inDelta )
{
	RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_RESET_LOCK_ENTER );

	ctx->nextTS += (uint32_t) inDelta;

	RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_RESET_LOCK_EXIT );
}

//===========================================================================================================================
//	_RTPJitterBufferDiscardExcess
//===========================================================================================================================

// TAILQ_FOREACH_SAFE is not available on all platforms, so define it here for those platforms that don't have it.

#ifndef TAILQ_FOREACH_SAFE
	#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
		for ((var) = TAILQ_FIRST((head));						\
			(var) && ((tvar) = TAILQ_NEXT((var), field), 1);	\
			(var) = (tvar))
#endif

// Internal function that is accessed after taking the jitter buffer lock
static void	_RTPJitterBufferDiscardExcess( RTPJitterBufferContext *ctx)
{
	uint32_t				desired;
	uint32_t				queueSize;
	uint32_t				prevSampleSize;
	uint32_t				highWatermarkMs;
	size_t					listNdx, listCount;
	RTPPacketNodeList *		listsToDiscardFrom[] = { &ctx->preparedList, &ctx->receivedList };
	RTPPacketNode *			nodeToDiscard;
	RTPPacketNode *			safeNode;
	
	require( ( ctx->bufferMs > 0 && ctx->inputFormat.mSampleRate > 0 ), exit );

	highWatermarkMs = ctx->bufferMs + 20;
	// 200ms is derived from the 50 nodes PCM samples @44.1KHz
	if( highWatermarkMs < 200 ) highWatermarkMs = 200;

	desired =  highWatermarkMs * ( (uint32_t) ctx->inputFormat.mSampleRate ) / 1000;
	
	queueSize = prevSampleSize = _RTPJitterBufferBufferedSamples( ctx, false );

	listCount = sizeof( listsToDiscardFrom ) / sizeof( listsToDiscardFrom[0] );
	for( listNdx = 0; listNdx < listCount; ++listNdx )
	{
		TAILQ_FOREACH_SAFE( nodeToDiscard, listsToDiscardFrom[ listNdx ], list, safeNode )
		{
			// Stop discarding when we've reached our desired size
			
			if ( ( queueSize = _RTPJitterBufferBufferedSamples( ctx, false ) ) <= desired )
				break;
			
			// Make sure the node isn't in use by the decode thread
			
			if( !nodeToDiscard->decodeLock || dispatch_semaphore_wait( nodeToDiscard->decodeLock, DISPATCH_TIME_NOW ) == 0 )
			{
				TAILQ_REMOVE( listsToDiscardFrom[ listNdx ], nodeToDiscard, list );
				TAILQ_INSERT_HEAD( &ctx->freeList, nodeToDiscard, list );
				ctx->nodesUsed -= 1;
				
				if( nodeToDiscard->decodeLock )
					dispatch_semaphore_signal( nodeToDiscard->decodeLock );
			}
		}
	}
	
	if( queueSize != prevSampleSize )
	{
		RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit,
			"Jitter Buffer Discard Excess, prevSize=%d(%dms) newSize=%d(%dms)\n",
			prevSampleSize, RTPJitterBufferSamplesToMs( ctx, prevSampleSize ),
			queueSize, RTPJitterBufferSamplesToMs( ctx, queueSize ) );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	RTPJitterBufferGetFreeNode
//===========================================================================================================================

OSStatus	RTPJitterBufferGetFreeNode( RTPJitterBufferContext *ctx, RTPPacketNode **outNode )
{
	OSStatus				err;
	RTPPacketNode *			node;
	
	RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_GETFREENODE_LOCK_ENTER );
	node = TAILQ_FIRST( &ctx->freeList );
	if( node )
	{
		TAILQ_REMOVE( &ctx->freeList, node, list );
		err = kNoErr;
	}
	else
	{
		node = NULL;
		err = kNoSpaceErr;
	}

	*outNode = node;
	RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_GETFREENODE_LOCK_EXIT );
	return( err );
}

//===========================================================================================================================
//	RTPJitterBufferPutFreeNode
//===========================================================================================================================

void	RTPJitterBufferPutFreeNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode )
{
	RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_PUTFREENODE_LOCK_ENTER );
	TAILQ_INSERT_HEAD( &ctx->freeList, inNode, list );
	RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_PUTFREENODE_LOCK_EXIT );
}

//===========================================================================================================================
//	RTPJitterBufferPutBusyNode
//===========================================================================================================================

OSStatus	RTPJitterBufferPutBusyNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode )
{
	uint32_t const		ts = inNode->pkt.pkt.rtp.header.ts;
	OSStatus			err;
	RTPPacketNode *		node;
	
	RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_PUTBUSYNODE_LOCK_ENTER );
	
	// If we are hitting the allocation limit, discard excess samples from Jitter Buffer.
	// FIXME: This will cause a glitch, and the better solution is to use General Audio model of
	// maintaining optimal jitter buffer size at all times with clock adjustment.
	if( ctx->nodesUsed >= ctx->nodesAllocated - 3 )
	{
		_RTPJitterBufferDiscardExcess( ctx );
		// Update the playhead
		if( !TAILQ_EMPTY( &ctx->preparedList ) )
		{
			node = TAILQ_FIRST( &ctx->preparedList );
		}
		else
		{
			check( !TAILQ_EMPTY( &ctx->receivedList ) );
			node = TAILQ_FIRST( &ctx->receivedList );
		}
		ctx->nextTS = node->pkt.pkt.rtp.header.ts;
	}
	
	// Insert the new node in timestamp order. It's most likely to be a later timestamp so start at the end.
	
	TAILQ_FOREACH_REVERSE( node, &ctx->receivedList, RTPPacketNodeList, list)
	{
		if( Mod32_LE( node->pkt.pkt.rtp.header.ts, ts ) ) break;
	}
	
	if( !node )
	{
		// If the received list is empty (or the node would go at the head), make sure we didn't miss our chance
		node = TAILQ_LAST( &ctx->preparedList, RTPPacketNodeList );
		require_action( !node || Mod32_LT( node->pkt.pkt.rtp.header.ts, ts ), exit,
			err = Mod32_EQ( node->pkt.pkt.rtp.header.ts, ts ) ? kDuplicateErr : kOrderErr );
		TAILQ_INSERT_HEAD( &ctx->receivedList, inNode, list );
	}
	else
	{
		require_action( !Mod32_EQ( node->pkt.pkt.rtp.header.ts, ts ), exit, err = kDuplicateErr );
		TAILQ_INSERT_AFTER( &ctx->receivedList, node, inNode, list );
	}
	ctx->nodesUsed += 1;
	
	// Signal the decode thread that a new node is available
	pthread_mutex_lock( ctx->decodeMutexPtr );
	pthread_cond_signal( ctx->decodeConditionPtr );
	pthread_mutex_unlock( ctx->decodeMutexPtr );
	
	// If this is the first packet after we started buffering then schedule audio to start after the buffer window.
	
	if( ctx->buffering && ( ctx->startTicks == 0 ) )
	{
		ctx->startTicks = UpTicks() + MillisecondsToUpTicks( ctx->bufferMs );
		RTPJitterBufferLog( ctx, kLogLevelInfo | kLogLevelFlagDontRateLimit, "%s: Starting audio in %u ms\n",
			ap_jitter_label( ctx ), ctx->bufferMs );
	}
	err = kNoErr;
	
exit:
	RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_PUTBUSYNODE_LOCK_EXIT );
	return( err );
}

//===========================================================================================================================
//	_RTPJitterBufferBufferedSamples
//===========================================================================================================================

static uint32_t _RTPJitterBufferBufferedSamples( RTPJitterBufferContext *ctx, Boolean preparedOnly )
{
	RTPPacketNode	*first, *last;
	uint32_t		preparedSamples, receivedSamples;
	
	// Calculate size of prepared list first (which is always fully decoded)
	if( !TAILQ_EMPTY( &ctx->preparedList ) )
	{
		first = TAILQ_FIRST( &ctx->preparedList );
		last = TAILQ_LAST( &ctx->preparedList, RTPPacketNodeList );
		
		// Calculate sample count of all but the last node based on timestamps
		preparedSamples = (uint32_t)( last->pkt.pkt.rtp.header.ts - first->pkt.pkt.rtp.header.ts );

		// Calculate sample count of last packet based on size
		preparedSamples += last->pkt.len / ctx->outputFormat.mBytesPerFrame;
	}
	else
	{
		preparedSamples = 0;
	}
	
	// Add size of received list next (which is never decoded)
	if( !preparedOnly && !TAILQ_EMPTY( &ctx->receivedList ) )
	{
		first = TAILQ_FIRST( &ctx->receivedList );
		last = TAILQ_LAST( &ctx->receivedList, RTPPacketNodeList );
		
		// Calculate sample count of all but the last node based on timestamps
		receivedSamples = (uint32_t)( last->pkt.pkt.rtp.header.ts - first->pkt.pkt.rtp.header.ts );

		// Calculate sample count of last packet based on format
		receivedSamples += ctx->inputFormat.mFramesPerPacket;
	}
	else
	{
		receivedSamples = 0;
	}

	return( preparedSamples + receivedSamples );
}

//===========================================================================================================================
//	_RTPJitterBufferAudioDecoderDecodeCallback
//
//	See <http://developer.apple.com/library/mac/#qa/qa2001/qa1317.html> for AudioConverterFillComplexBuffer callback details.
//===========================================================================================================================

static OSStatus
	_RTPJitterBufferAudioDecoderDecodeCallback(
		AudioConverterRef				inAudioConverter,
		UInt32 *						ioNumberDataPackets,
		AudioBufferList *				ioData,
		AudioStreamPacketDescription **	outDataPacketDescription,
		void *							inUserData )
{
	RTPPacketNode *		node = (RTPPacketNode *) inUserData;
	
	(void) inAudioConverter;
	
	if( node->pkt.len > 0 )
	{
		check( *ioNumberDataPackets > 0 );
		*ioNumberDataPackets = 1;
		
		ioData->mNumberBuffers					= 1;
		ioData->mBuffers[ 0 ].mNumberChannels	= node->jitterBuffer->inputFormat.mChannelsPerFrame;
		ioData->mBuffers[ 0 ].mDataByteSize		= (UInt32)( node->pkt.len );
		ioData->mBuffers[ 0 ].mData				= (void *) node->ptr;
		node->pkt.len							= 0;
		
		if( outDataPacketDescription )
		{
			node->jitterBuffer->packetDescription.mStartOffset				= 0;
			node->jitterBuffer->packetDescription.mVariableFramesInPacket	= 0;
			node->jitterBuffer->packetDescription.mDataByteSize				= ioData->mBuffers[ 0 ].mDataByteSize;
			*outDataPacketDescription										= &node->jitterBuffer->packetDescription;
		}

		return( kNoErr );
	}
	
	*ioNumberDataPackets = 0;
	return( kUnderrunErr );
}

//===========================================================================================================================
//	_RTPJitterBufferDecodeNode
//===========================================================================================================================

static OSStatus _RTPJitterBufferDecodeNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode )
{
	OSStatus			err;
	UInt32				outputPacketCount;
	AudioBufferList		bufferList;
	
	outputPacketCount							= ctx->inputFormat.mFramesPerPacket;
	bufferList.mNumberBuffers					= 1;
	bufferList.mBuffers[ 0 ].mNumberChannels	= ctx->outputFormat.mChannelsPerFrame;
	bufferList.mBuffers[ 0 ].mDataByteSize		= (uint32_t) ctx->inputFormat.mFramesPerPacket * ctx->outputFormat.mBytesPerPacket;
	bufferList.mBuffers[ 0 ].mData				= inNode->decodeBuffer;
	
	err = AudioConverterFillComplexBuffer( ctx->decoder, _RTPJitterBufferAudioDecoderDecodeCallback, inNode,
										  &outputPacketCount, &bufferList, NULL );
	require( err == kNoErr || err == kUnderrunErr, exit );

	if( outputPacketCount > 0 )
	{
		inNode->ptr = inNode->decodeBuffer;
		inNode->pkt.len = outputPacketCount * ctx->outputFormat.mBytesPerFrame;
	}
	else
	{
		inNode->pkt.len = 0;
	}
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_RTPJitterBufferDecodeThread
//===========================================================================================================================

static void * _RTPJitterBufferDecodeThread( void *inCtx )
{
	OSStatus					err;
	RTPJitterBufferContext *	ctx = inCtx;
	RTPPacketNode *				node;
	uint32_t					nextDecodeTS = 0;
	
	SetThreadName( "AirPlayAudioDecoder" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_AudioDecoder );

	while( !ctx->decodeDone )
	{
		pthread_mutex_lock( ctx->decodeMutexPtr );
		pthread_cond_wait( ctx->decodeConditionPtr, ctx->decodeMutexPtr );
		pthread_mutex_unlock( ctx->decodeMutexPtr );
		
		// Re-check after being signalled
		if( ctx->decodeDone )	break;
		
		RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_DECODEPASS_LOCK_ENTER );
		while( !TAILQ_EMPTY( &ctx->receivedList ) )
		{
			node = TAILQ_FIRST( &ctx->receivedList );

			if( node->pkt.pkt.rtp.header.ts == nextDecodeTS )
			{
				// Next node is in sequence so we can go ahead and decode it
				if( ctx->decoder )
				{
					// Lock the node for decode, but keep it on the received list (keeps time calculations accurate while decoding)
					// NB: This should never block since we're the decode thread and this is an encoded node
					dispatch_semaphore_wait( node->decodeLock, DISPATCH_TIME_FOREVER );
					RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_DECODEITERATION_LOCK_EXIT );

					err = _RTPJitterBufferDecodeNode( ctx, node );

					RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_DECODEITERATION_LOCK_ENTER );
					dispatch_semaphore_signal( node->decodeLock );

					TAILQ_REMOVE( &ctx->receivedList, node, list );
					if( err )
					{
						TAILQ_INSERT_TAIL( &ctx->freeList, node, list );
						ctx->nodesUsed -= 1;
					}
					else
					{
						TAILQ_INSERT_TAIL( &ctx->preparedList, node, list );
						nextDecodeTS += ctx->inputFormat.mFramesPerPacket;
					}
				}
				else
				{
					// No decode needed - put it directly onto the prepared list
					TAILQ_REMOVE( &ctx->receivedList, node, list );
					TAILQ_INSERT_TAIL( &ctx->preparedList, node, list );
					
					err = kNoErr;
					nextDecodeTS += node->pkt.len / ctx->outputFormat.mBytesPerFrame;
				}
			}
			else if( node->pkt.pkt.rtp.header.ts > nextDecodeTS )
			{
				// We have a gap, so decide whether we can afford to wait a bit
				if( RTPJitterBufferPreparedMs( ctx ) >= 0.5 * ctx->bufferMs )
				{
					RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit, "Delaying decode of node at time %u\n",
						node->pkt.pkt.rtp.header.ts, nextDecodeTS );
					break;
				}

				nextDecodeTS = node->pkt.pkt.rtp.header.ts;
				continue;
			}
			else
			{
				TAILQ_REMOVE( &ctx->receivedList, node, list );
				TAILQ_INSERT_TAIL( &ctx->freeList, node, list );
				ctx->nodesUsed -= 1;

				RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit, "Discarding late decode node at time %u\n",
					node->pkt.pkt.rtp.header.ts );
				
				// The node must have arrived late, just after we decoded one of its successors, so just toss it
				err = kOrderErr;
			}
		}
		RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_DECODEPASS_LOCK_EXIT );
	}
	
	return NULL;
}


//===========================================================================================================================
//	RTPJitterBufferRead
//===========================================================================================================================

OSStatus	RTPJitterBufferRead( RTPJitterBufferContext *ctx, void *inBuffer, size_t inLen )
{
	uint32_t const		lenTS = (uint32_t)( inLen / ctx->outputFormat.mBytesPerFrame );
	uint8_t *			dst   = (uint8_t *) inBuffer;
	RTPPacketNode *		node;
	uint32_t			nowTS, limTS, srcTS, endTS, delta;
	size_t				len;
	Boolean				cap;
	uint64_t			ticks;
	
	RTPJitterBufferLock( ctx, AIRPLAY_SIGNPOST_JB_READ_LOCK_ENTER );
	
	if( ctx->buffering )
	{
		ticks = UpTicks();
		if( ( ctx->startTicks == 0 ) || ( ticks < ctx->startTicks ) )
		{
			memset( inBuffer, 0, inLen );
			goto exit;
		}
		
		node = TAILQ_EMPTY( &ctx->preparedList) ? TAILQ_FIRST( &ctx->receivedList ) : TAILQ_FIRST( &ctx->preparedList );
		if( node )
		{
			// At this point a glitch is expected and the jitter buffer would have grown
			// following the spike. Discard excess samples.
			_RTPJitterBufferDiscardExcess( ctx );
			
			nowTS = node->pkt.pkt.rtp.header.ts;
			ctx->buffering = false;
			RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit, "%s: Buffering complete, %d ms (%d), %llu ticks late\n",
				ap_jitter_label( ctx ), RTPJitterBufferBufferedMs( ctx ), _RTPJitterBufferBufferedSamples( ctx, false ),
				ticks - ctx->startTicks );
		}
		else
		{
			// Nothing buffered
			nowTS = 0;
		}
	}
	else
	{
		nowTS = ctx->nextTS;
	}
	limTS = nowTS + lenTS;
	
	while( ( node = TAILQ_FIRST( &ctx->preparedList ) ) != NULL )
	{
		srcTS = node->pkt.pkt.rtp.header.ts;

		// If node is after the timing window, it's too early so fill in silence and advance the play-head.

		if( Mod32_GE( srcTS, limTS ) )
		{
			delta = limTS - nowTS;
			if( delta )
			{
				len	= delta * ctx->outputFormat.mBytesPerFrame;
				memset( dst, 0, len );
			}
			nowTS = limTS;
			break;
		}
		
		// If the node is before the timing window, it's too late so go to the next one.
		
		endTS = (uint32_t)( srcTS + ( node->pkt.len / ctx->outputFormat.mBytesPerFrame ) );
		if( Mod32_LE( endTS, nowTS ) )
		{
			++ctx->nLate;
			RTPJitterBufferLog( ctx, kLogLevelNotice, "%s: Late: %d ms (%u total)\n", ap_jitter_label( ctx ),
				RTPJitterBufferSamplesToMs( ctx, (int)( nowTS - endTS ) ), ctx->nLate );

			goto next;
		}
		
		// If the node has samples before the timing window, they're late so skip them.
		
		if( Mod32_LT( srcTS, nowTS ) )
		{
			++ctx->nSkipped;
			RTPJitterBufferLog( ctx, kLogLevelNotice, "%s: Skip: %d ms (%u total)\n", ap_jitter_label( ctx ),
				RTPJitterBufferSamplesToMs( ctx, (int)( nowTS - srcTS ) ), ctx->nSkipped );
			
			delta = nowTS - srcTS;
			len   = delta * ctx->outputFormat.mBytesPerFrame;
			node->ptr += len;
			node->pkt.len -= len;
			node->pkt.pkt.rtp.header.ts += delta;
			srcTS = nowTS;
		}
		
		// If the node starts after the beginning of the timing window, there's a gap so fill in silence.
		
		else if( Mod32_GT( srcTS, nowTS ) )
		{
			++ctx->nGaps;
			RTPJitterBufferLog( ctx, kLogLevelNotice, "%s: Gap:  %d ms (%u total)\n", ap_jitter_label( ctx ),
				RTPJitterBufferSamplesToMs( ctx, (int)( srcTS - nowTS ) ), ctx->nGaps );
			
			delta = srcTS - nowTS;
			len   = delta * ctx->outputFormat.mBytesPerFrame;
			memset( dst, 0, len );
			dst   += len;
			nowTS += delta;
		}
		
		// Node is completely within the timing window.

		else
		{
		}
		
		// Copy into the playout buffer.
		
		cap = Mod32_GT( endTS, limTS );
		if( cap ) endTS = limTS;
		
		delta = endTS - srcTS;
		len   = delta * ctx->outputFormat.mBytesPerFrame;
		if( len >= node->pkt.len )
		{
			len = node->pkt.len;
			delta = (uint32_t)( len / ctx->outputFormat.mBytesPerFrame );
			cap = false;
		}

		memcpy( dst, node->ptr, len );

		dst   += len;
		nowTS += delta;
		if( cap )
		{
			node->ptr     += len;
			node->pkt.len -= len;
			node->pkt.pkt.rtp.header.ts	+= delta;
			break;
		}
		
	next:
		TAILQ_REMOVE( &ctx->preparedList, node, list );
		TAILQ_INSERT_HEAD( &ctx->freeList, node, list );
		ctx->nodesUsed -= 1;
		
		// Signal decode thread to check if more should be decoded
		if( ctx->decodeThreadPtr )
		{
			pthread_mutex_lock( ctx->decodeMutexPtr );
			pthread_cond_signal( ctx->decodeConditionPtr );
			pthread_mutex_unlock( ctx->decodeMutexPtr );
		}
	}
	
	// If more samples are needed for this timing window then we've run dry. If it's prolonged then re-buffer.
	
	if( Mod32_LT( nowTS, limTS ) )
	{
		++ctx->nRebuffer;
		RTPJitterBufferLog( ctx, kLogLevelNotice | kLogLevelFlagDontRateLimit,
			"%s: Re-buffering: %d ms buffered (%d), %d ms missing (%d), %u total) nowTS (%u) \n", ap_jitter_label( ctx ),
			RTPJitterBufferBufferedMs( ctx ), _RTPJitterBufferBufferedSamples( ctx, false ),
			RTPJitterBufferSamplesToMs( ctx, (int)( limTS - nowTS ) ), (int)( limTS - nowTS ), ctx->nRebuffer, nowTS );
		
		delta = limTS - nowTS;
		len = delta * ctx->outputFormat.mBytesPerFrame;
		memset( dst, 0, len );
		ctx->buffering = true;
		ctx->startTicks = 0;
		nowTS = limTS;
	}
	ctx->nextTS = nowTS;
	
exit:
	RTPJitterBufferUnlock( ctx, AIRPLAY_SIGNPOST_JB_READ_LOCK_EXIT );
	return( kNoErr );
}
