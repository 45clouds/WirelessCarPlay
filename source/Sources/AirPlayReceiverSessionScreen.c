/*
	File:    	AirPlayReceiverSessionScreen.c
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
	
	Copyright (C) 2011-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "AirPlayReceiverSessionScreen.h"

#include "AESUtils.h"
#include "CFUtils.h"
#include "DebugServices.h"
#include "ScreenUtils.h"
#include "TickUtils.h"
#include <errno.h>
#include "AirPlayCommon.h"
#include "AirPlayUtils.h"
#include "ScreenUtils.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kAirPlayReceiverSessionScreenCommandMaxSize		64

// AirPlayReceiverSessionScreenPrivate

struct AirPlayReceiverSessionScreenPrivate
{
	int64_t											videoLatencyMs;
	
	AirPlayReceiverSessionScreenTimeSynchronizer	timeSynchronizer;
	uint32_t										frameErrors;
	uint32_t										negativeAheadFrames;
	
	Boolean											respectTimestamps;
	int64_t											displayDeltaMs;
	uint32_t										lateFrames;
	
	double											ticksPerSecF;
	
	AirPlayScreenHeader								screenHeader;
	
	AES_CTR_Context									aesContext;
	Boolean											aesValid;
	
	ChaChaPolyCryptor								chachaCryptor;
	
	SocketRef										commandSock;
	ScreenStreamRef									screenStream;
};

// Prototypes

static void			_AirPlayReceiverSessionScreen_Cleanup( AirPlayReceiverSessionScreenRef me );
static OSStatus
	_AirPlayReceiverSessionScreen_ProcessFrame(
		AirPlayReceiverSessionScreenRef		me,
		AirPlayScreenHeader *				inHeader, 
		uint8_t *							inFramePtr );
static OSStatus		_AirPlayReceiverSessionScreen_ProcessCommand( AirPlayReceiverSessionScreenRef me );

// Logging

ulog_define( AirPlayReceiverSessionScreenCore, kLogLevelNotice, kLogFlags_Default, "AirPlayReceiverSessionScreen", NULL );
#define apvs_ucat()						&log_category_from_name( AirPlayReceiverSessionScreenCore )
#define apvs_ulog( LEVEL, ... )			ulog( apvs_ucat(), (LEVEL), __VA_ARGS__ )

ulog_define( AirPlayReceiverSessionScreenFrames, kLogLevelNotice, kLogFlags_Default, "AirPlayReceiverSessionScreen", "AirPlayReceiverSessionScreenFrames:rate=5;3000" );
#define apvs_frames_ulog( LEVEL, ... )	ulog( &log_category_from_name( AirPlayReceiverSessionScreenFrames ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_Create
//===========================================================================================================================

OSStatus AirPlayReceiverSessionScreen_Create( AirPlayReceiverSessionScreenRef *outRef )
{
	OSStatus							err;
	AirPlayReceiverSessionScreenRef		obj;

	obj = (AirPlayReceiverSessionScreenRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->commandSock = kInvalidSocketRef;
	
	*outRef = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	if( obj ) AirPlayReceiverSessionScreen_Delete( obj );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_Delete
//===========================================================================================================================

void AirPlayReceiverSessionScreen_Delete( AirPlayReceiverSessionScreenRef inSession )
{
	check( inSession->commandSock == kInvalidSocketRef );

	if( inSession->aesValid )
	{
		inSession->aesValid = false;
		AES_CTR_Final( &inSession->aesContext );
	}
	MemZeroSecure( &inSession->chachaCryptor, sizeof( inSession->chachaCryptor ) );

	free( inSession );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_SetTimeSynchronizer
//===========================================================================================================================

void
	AirPlayReceiverSessionScreen_SetTimeSynchronizer(
		AirPlayReceiverSessionScreenRef								inSession,
		const AirPlayReceiverSessionScreenTimeSynchronizer *		inTimeSynchronizer )
{
	inSession->timeSynchronizer = *inTimeSynchronizer;
}

#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
//===========================================================================================================================
//	AirPlayReceiverSessionScreen_CopyDisplaysInfo
//===========================================================================================================================

CFMutableDictionaryRef AirPlayReceiverSessionScreen_CopyDisplaysInfo( AirPlayReceiverSessionScreenRef inSession, OSStatus *outErr )
{
	OSStatus					err;
	CFMutableDictionaryRef		info;
	ScreenRef					mainScreen = NULL;
	int64_t						a, b;
	CFTypeRef					obj;

	(void) inSession;
	
	info = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( info, exit, err = kNoMemoryErr );

	mainScreen = ScreenCopyMain( &err );
	require_noerr( err, exit );
	
	// EDID
	
	obj = ScreenCopyProperty( mainScreen, kScreenProperty_EDID, NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_EDID ), obj );
		CFRelease( obj );
	}

	// Features
	
	a = 0;
	b = ScreenGetPropertyInt64( mainScreen, kScreenProperty_Features, NULL, NULL );
	if( b & kScreenFeature_Knobs )				a |= kAirPlayDisplayFeatures_Knobs;
	if( b & kScreenFeature_LowFidelityTouch )	a |= kAirPlayDisplayFeatures_LowFidelityTouch;
	if( b & kScreenFeature_HighFidelityTouch )	a |= kAirPlayDisplayFeatures_HighFidelityTouch;
	if( b & kScreenFeature_Touchpad )			a |= kAirPlayDisplayFeatures_Touchpad;
	CFDictionarySetInt64( info, CFSTR( kAirPlayKey_DisplayFeatures ), a );
	
    // Primary Input Device
    
    a = ScreenGetPropertyInt64( mainScreen, CFSTR( "primaryInputDevice" ), NULL, &err );
    if ( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_PrimaryInputDevice ), a);
    
	// MaxFPS
	
	a = ScreenGetPropertyInt64( mainScreen, kScreenProperty_MaxFPS, NULL, &err );
	if( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_MaxFPS ), a );
	
	// Physical Width/Height.
	
	a = ScreenGetPropertyInt64( mainScreen, kScreenProperty_WidthPhysical, NULL, &err );
	if( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_WidthPhysical ), a );
	
	a = ScreenGetPropertyInt64( mainScreen, kScreenProperty_HeightPhysical, NULL, &err );
	if( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_HeightPhysical ), a );
	
	// Pixel Width/Height
	
	a = ScreenGetPropertyInt64( mainScreen, kScreenProperty_WidthPixels, NULL, &err );
	if( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_WidthPixels ), a );
	
	a = ScreenGetPropertyInt64( mainScreen, kScreenProperty_HeightPixels, NULL, &err );
	if( !err ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_HeightPixels ), a );
	
	// UUID
	
	obj = ScreenCopyProperty( mainScreen, kScreenProperty_UUID, NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_UUID ), obj );
		CFRelease( obj );
	}
	
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( mainScreen );
	if( err ) ForgetCF( &info );
	if( outErr ) *outErr = err;
	return( info );
}
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_CopyTimestampInfo
//===========================================================================================================================

CFArrayRef AirPlayReceiverSessionScreen_CopyTimestampInfo( AirPlayReceiverSessionScreenRef inSession, OSStatus *outErr )
{
	(void) inSession;
	(void) outErr;
	
	return( NULL );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_SetChaChaSecurityInfo
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionScreen_SetChaChaSecurityInfo(
		AirPlayReceiverSessionScreenRef		inSession,
		const uint8_t *						inKey,
		const size_t						inKeyLen )
{
	OSStatus	err;
	
	MemZeroSecure( &inSession->chachaCryptor, sizeof( inSession->chachaCryptor ) );
	require_action_quiet( inKey && ( inKeyLen == 32 ), exit, err = kNoErr );
	
	memcpy( inSession->chachaCryptor.key, inKey, inKeyLen );
	inSession->chachaCryptor.isValid = true;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_SetSecurityInfo
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionScreen_SetSecurityInfo(
		AirPlayReceiverSessionScreenRef		inSession,
		const uint8_t						inKey[ 16 ],
		const uint8_t						inIV[ 16 ] )
{
	OSStatus err;

	AES_CTR_Forget( &inSession->aesContext, &inSession->aesValid );
	err = AES_CTR_Init( &inSession->aesContext, inKey, inIV );
	require_noerr( err, exit );
	inSession->aesValid = true;
	
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_Setup
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionScreen_Setup(
		AirPlayReceiverSessionScreenRef		inSession,
		CFDictionaryRef						inStreamDesc,
		uint32_t							inSessionID )
{
	OSStatus	err;

	(void) inSessionID;

	inSession->videoLatencyMs = CFDictionaryGetInt64( inStreamDesc, CFSTR( "latencyMs" ), &err );
	if( err ) inSession->videoLatencyMs = 70;

	return( kNoErr );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_ProcessFrames
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionScreen_ProcessFrames(
		AirPlayReceiverSessionScreenRef		inSession,
		NetSocketRef						inNetSock,
		int									inTimeoutDataSecs )
{
	SocketRef									tcpSock;
	OSStatus									err;
	size_t										len;
	fd_set										readSet;
	int											maxFD, n;
	struct timeval								timeout;
	uint64_t									deadSenderTicks, lastAliveTicks;
	
	tcpSock = NetSocket_GetNative( inNetSock );
	maxFD = -1;
	if( (int) tcpSock					> maxFD ) maxFD = (int) tcpSock;
	if( (int) inSession->commandSock	> maxFD ) maxFD = (int) inSession->commandSock;
	maxFD += 1;
	
	deadSenderTicks = inTimeoutDataSecs * UpTicksPerSecond();
	lastAliveTicks  = UpTicks();
	FD_ZERO( &readSet );
	for( ;; )
	{
		FD_SET( tcpSock, &readSet );
		FD_SET( inSession->commandSock, &readSet );
		timeout.tv_sec  = 0;
		timeout.tv_usec = 100000; // 100 ms
		n = select( maxFD, &readSet, NULL, NULL, &timeout );
		err = select_errno( n );
		if( err == kTimeoutErr )
		{
			if( ( UpTicks() - lastAliveTicks ) > deadSenderTicks )
			{
				apvs_ulog( kLogLevelNotice, "### Sender went dead. Stopping.\n" );
				goto exit;
			}
			continue;
		}
		require_noerr( err, exit );
		
		if( FD_ISSET( tcpSock, &readSet ) )
		{
			uint8_t *		frameBuffer;
			
			err = NetSocket_Read( inNetSock, sizeof( inSession->screenHeader ),
				sizeof( inSession->screenHeader ), &inSession->screenHeader, NULL, inTimeoutDataSecs );
			if( err == kConnectionErr ) { err = kNoErr; goto exit; }
			require_noerr_quiet( err, exit );
			len = inSession->screenHeader.bodySize;
			
			if( len > 0 )
			{
				frameBuffer = (uint8_t *) malloc( len );
				if( frameBuffer == NULL ) { dlogassert( "malloc( %zu ) failed\n", len ); continue; }
				
				err = NetSocket_Read( inNetSock, len, len, frameBuffer, NULL, inTimeoutDataSecs );
				if( err ) free( frameBuffer );
				if( err == kConnectionErr ) goto exit;
				require_noerr( err, exit );
			}
			else
			{
				frameBuffer = NULL;
			}
			
			_AirPlayReceiverSessionScreen_ProcessFrame( inSession, &inSession->screenHeader, frameBuffer );
			// Note: frameBuffer is no longer our responsibility to munmap.
			lastAliveTicks = UpTicks();
		}
		if( FD_ISSET( inSession->commandSock, &readSet ) )
		{
			err = _AirPlayReceiverSessionScreen_ProcessCommand( inSession );
			if( err ) goto exit;
		}
	}

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_StartSession
//===========================================================================================================================

OSStatus AirPlayReceiverSessionScreen_StartSession( AirPlayReceiverSessionScreenRef inSession, void* context )
{
	OSStatus		err;

	inSession->ticksPerSecF			= (double) UpTicksPerSecond();
	inSession->lateFrames			= 0;
	inSession->negativeAheadFrames	= 0;
	
	err = OpenSelfConnectedLoopbackSocket( &inSession->commandSock );
	require_noerr( err, exit );
	
	// Set up the display.
	
	err = ScreenStreamCreate( &inSession->screenStream );
	require_noerr( err, exit );
	ScreenStreamSetDelegateContext( inSession->screenStream, context);
	
	err = ScreenStreamStart( inSession->screenStream );
	require_noerr( err, exit );
	
exit:
	if( err ) AirPlayReceiverSessionScreen_StopSession( inSession );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_StopSession
//===========================================================================================================================

void AirPlayReceiverSessionScreen_StopSession( AirPlayReceiverSessionScreenRef inSession )
{
	_AirPlayReceiverSessionScreen_Cleanup( inSession );
}

//===========================================================================================================================
//	_AirPlayReceiverSessionScreen_Cleanup
//===========================================================================================================================

static void	_AirPlayReceiverSessionScreen_Cleanup( AirPlayReceiverSessionScreenRef me )
{
	ScreenStreamForget( &me->screenStream );
	ForgetSocket( &me->commandSock );
	AES_CTR_Forget( &me->aesContext, &me->aesValid );
}

//===========================================================================================================================
//	_AirPlayReceiverSessionScreen_ProcessFrame
//
//	Warning: Unconventionally, ensuring that inFramePtr is munmap'd is the responsibility of this function.
//===========================================================================================================================

static OSStatus
	_AirPlayReceiverSessionScreen_ProcessFrame(
		AirPlayReceiverSessionScreenRef		me,
		AirPlayScreenHeader *				inHeader, 
		uint8_t *							inFramePtr )
{
	OSStatus		err;
	uint64_t		displayTicks;
	uint64_t		nowTicks;
	size_t			tempSize;
	
	if( inHeader->opcode == kAirPlayScreenOpCode_VideoFrame )
	{
		// Processing timestamps.
		
		if( me->respectTimestamps )
		{
			displayTicks = me->timeSynchronizer.getUpTicksNearSynchronizedNTPTimeFunc(
				me->timeSynchronizer.context, inHeader->params[ 0 ].u64 );
		}
		else
		{
			displayTicks = UpTicks();
		}
		nowTicks = UpTicks();
		
		if( displayTicks >= nowTicks )
		{
			me->displayDeltaMs = UpTicksToMilliseconds( displayTicks - nowTicks );
		}
		else
		{
			me->displayDeltaMs = -(int64_t) UpTicksToMilliseconds( nowTicks - displayTicks );
			++me->negativeAheadFrames;
		}
		me->displayDeltaMs = me->videoLatencyMs - me->displayDeltaMs;
		if( me->displayDeltaMs >= ( 2 * me->videoLatencyMs ) )
		{
			++me->lateFrames;
			tempSize = ( inHeader->bodySize >= 16 ) ? 16 : inHeader->bodySize;
			apvs_frames_ulog( kLogLevelNotice, "Late frame (%lld ms, %u total late frames): %.3H ... %.3H\n", 
				me->displayDeltaMs, me->lateFrames, inHeader, 16, 16, 
				inFramePtr + ( inHeader->bodySize - tempSize ), (int) tempSize, (int) tempSize );
		}

		if( me->chachaCryptor.isValid )
		{
			if( inHeader->bodySize >= 16 )
			{
				size_t len;
				chacha20_poly1305_init_64x64( &me->chachaCryptor.state, me->chachaCryptor.key, me->chachaCryptor.nonce );
				chacha20_poly1305_add_aad( &me->chachaCryptor.state, inHeader, sizeof( AirPlayScreenHeader ) );
				len = chacha20_poly1305_decrypt( &me->chachaCryptor.state, inFramePtr, inHeader->bodySize - 16, inFramePtr );
				len += chacha20_poly1305_verify( &me->chachaCryptor.state, &inFramePtr[ len ], &inFramePtr[ inHeader->bodySize - 16 ], &err );
				require_noerr( err, exit );
				require_action( len == inHeader->bodySize - 16, exit, err = kInternalErr );
				inHeader->bodySize -= 16;
				LittleEndianIntegerIncrement( me->chachaCryptor.nonce, sizeof( me->chachaCryptor.nonce ) );
			}
		}
		else
		if( me->aesValid )
		{
			err = AES_CTR_Update( &me->aesContext, inFramePtr, inHeader->bodySize, inFramePtr );
			require_noerr( err, exit );
		}
		
		err = ScreenStreamProcessData( me->screenStream, inFramePtr, inHeader->bodySize, displayTicks, NULL, NULL, NULL );
		require_noerr( err, exit );
	}
	else if( inHeader->opcode == kAirPlayScreenOpCode_VideoConfig )
	{
		me->respectTimestamps = ( inHeader->smallParam[ 1 ] & kAirPlayScreenFlag_RespectTimestamps ) != 0;
		
		ScreenStreamSetWidthHeight( me->screenStream, inHeader->params[ 1 ].f32[ 0 ], inHeader->params[ 1 ].f32[ 1 ] );
		
		if( inHeader->bodySize > 0 )
		{
			err = ScreenStreamSetAVCC( me->screenStream, inFramePtr, inHeader->bodySize );
			require_noerr( err, exit );
		}
	}
	else if( inHeader->opcode == kAirPlayScreenOpCode_KeepAlive )
	{
		// This opcode is intended to be ignored.
	}
	else if( inHeader->opcode == kAirPlayScreenOpCode_KeepAliveWithBody )
	{
		// This opcode is intended to be ignored.
	}
	else if( inHeader->opcode == kAirPlayScreenOpCode_Ignore )
	{
		// This opcode is intended to be ignored. It's only for measuring bandwidth.
	}
	else
	{
		apvs_ulog( kLogLevelError, "### Unknown screen opcode: %u\n", inHeader->opcode );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( inFramePtr ) free( inFramePtr );
	if( err )
	{
		++me->frameErrors;
		apvs_frames_ulog( kLogLevelError, "### Process frame error (%u): %#m\n", me->frameErrors, err );
	}
	return( err );
}

//===========================================================================================================================
//	_AirPlayReceiverSessionScreen_ProcessCommand
//===========================================================================================================================

static OSStatus	_AirPlayReceiverSessionScreen_ProcessCommand( AirPlayReceiverSessionScreenRef me )
{
	OSStatus		err;
	char			buf[ kAirPlayReceiverSessionScreenCommandMaxSize ];
	ssize_t			n;
	
	n = recv( me->commandSock, buf, sizeof( buf ), 0 );
	err = map_global_value_errno( n > 0, n );
	require_noerr( err, exit );
	
	switch( buf[ 0 ] )
	{
		case kAirPlayReceiverSessionScreenCommand_Quit:
			apvs_ulog( kLogLevelNotice, "User quit event received\n" );
			err = kEndingErr;
			break;
		
		default:
			apvs_ulog( kLogLevelWarning, "### Unknown pipe action: '%c'\n", buf[ 0 ] );
			break;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionScreen_SendCommand
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionScreen_SendCommand(
		AirPlayReceiverSessionScreenRef		inSession,
		char								inCommand,
		const void *						inExtraPtr,
		size_t								inExtraLen )
{
	OSStatus		err = kNoErr;
	ssize_t			n;
	iovec_t			iov[ 2 ];
	int				ion;

	require_action( ( 1 + inExtraLen ) <= kAirPlayReceiverSessionScreenCommandMaxSize, exit, err = kSizeErr );
	
	if( IsValidSocket( inSession->commandSock ) )
	{
		ion = 1;
		iov[ 0 ].iov_base = (void *) &inCommand;
		iov[ 0 ].iov_len  = 1;
		if( inExtraLen > 0 )
		{
			ion = 2;
			iov[ 1 ].iov_base = (void *) inExtraPtr;
			iov[ 1 ].iov_len  = inExtraLen;
		}
		n = writev( inSession->commandSock, iov, ion );
		err = map_global_value_errno( n == (ssize_t)( 1 + inExtraLen ), n );
	}
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif
