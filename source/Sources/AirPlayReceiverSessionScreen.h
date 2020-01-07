/*
	File:    	AirPlayReceiverSessionScreen.h
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
	
	Copyright (C) 2011-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayReceiverSessionScreen_h_
#define	__AirPlayReceiverSessionScreen_h_

#include "AirPlayCommon.h"
#include <CoreUtils/DataBufferUtils.h>
#include <CoreUtils/NetUtils.h>

#if( TARGET_OS_POSIX )
	#include <net/if.h>
#endif

#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionScreen
//===========================================================================================================================

typedef struct AirPlayReceiverSessionScreenPrivate *		AirPlayReceiverSessionScreenRef;

OSStatus	AirPlayReceiverSessionScreen_Create( AirPlayReceiverSessionScreenRef *outRef );
void		AirPlayReceiverSessionScreen_Delete( AirPlayReceiverSessionScreenRef inSession );
#define 	AirPlayReceiverSessionScreen_Forget( X )		do { if( *(X) ) { AirPlayReceiverSessionScreen_Delete( *(X) ); *(X) = NULL; } } while( 0 )

#define kAirPlayReceiverSessionScreenEvent_ForceKeyFrameNeeded	"forceKeyFrameNeeded"
#define kAirPlayReceiverSessionScreenEvent_TimestampsUpdated	"timestampsUpdated"

typedef void
	( *AirPlayReceiverSessionScreenEventHandlerFunc )(
		AirPlayReceiverSessionScreenRef		inSession,
		CFStringRef							inEventName,
		CFDictionaryRef						inEventData,
		void *								inUserData );

void
	AirPlayReceiverSessionScreen_SetEventHandler(
		AirPlayReceiverSessionScreenRef					inSession,
		AirPlayReceiverSessionScreenEventHandlerFunc	inHandler,
		void *											inUserData,
		dispatch_queue_t								inQueue );

// Synchronously replaces the event handler on the designated event handler queue. This ensures that the swap is safe, but will deadlock
// if called from the event handler queue.
void
	AirPlayReceiverSessionScreen_ReplaceEventHandlerSynchronously(
		AirPlayReceiverSessionScreenRef					inSession,
		AirPlayReceiverSessionScreenEventHandlerFunc	inHandler,
		void *											inUserData,
		dispatch_queue_t								inQueue );

typedef uint64_t ( *AirPlayReceiverSessionScreen_GetSynchronizedNTPTimeFunc )( void *inContext );
typedef uint64_t ( *AirPlayReceiverSessionScreen_GetUpTicksNearSynchronizedNTPTimeFunc )( void *inContext, uint64_t inNTPTime );
typedef struct
{
	void *																context;
	AirPlayReceiverSessionScreen_GetSynchronizedNTPTimeFunc				getSynchronizedNTPTimeFunc;
	AirPlayReceiverSessionScreen_GetUpTicksNearSynchronizedNTPTimeFunc	getUpTicksNearSynchronizedNTPTimeFunc;
}	AirPlayReceiverSessionScreenTimeSynchronizer;

void
	AirPlayReceiverSessionScreen_SetTimeSynchronizer(
		AirPlayReceiverSessionScreenRef								inSession,
		const AirPlayReceiverSessionScreenTimeSynchronizer *		inTimeSynchronizer );

void		AirPlayReceiverSessionScreen_SetUserVersion( AirPlayReceiverSessionScreenRef inSession, uint32_t inUserVersion );
void		AirPlayReceiverSessionScreen_SetOverscanOverride( AirPlayReceiverSessionScreenRef inSession, int inOverscanOverride );
void		AirPlayReceiverSessionScreen_SetSessionUUID( AirPlayReceiverSessionScreenRef inSession, uint8_t inUUID[ 16 ] );
void		AirPlayReceiverSessionScreen_SetClientDeviceID( AirPlayReceiverSessionScreenRef inSession, uint64_t inDeviceID );
void		AirPlayReceiverSessionScreen_SetClientDeviceUDID( AirPlayReceiverSessionScreenRef inSession, CFStringRef inDeviceUDID );
void		AirPlayReceiverSessionScreen_SetClientIfMACAddr( AirPlayReceiverSessionScreenRef inSession, uint8_t * inIfMACAddr, size_t inIfMACAddrLen );
void		AirPlayReceiverSessionScreen_SetClientModelCode( AirPlayReceiverSessionScreenRef inSession, CFStringRef inModelCode );
void		AirPlayReceiverSessionScreen_SetClientOSBuildVersion( AirPlayReceiverSessionScreenRef inSession, CFStringRef inOSBuildVersion );
void		AirPlayReceiverSessionScreen_SetIFName( AirPlayReceiverSessionScreenRef inSession, char inIfName[ IF_NAMESIZE + 1 ] );
void		AirPlayReceiverSessionScreen_SetTransportType( AirPlayReceiverSessionScreenRef inSession, NetTransportType inTransportType );
CFMutableDictionaryRef
	AirPlayReceiverSessionScreen_CopyDisplaysInfo(
		AirPlayReceiverSessionScreenRef		inSession,
		OSStatus *							outErr );
CFStringRef	AirPlayReceiverSessionScreen_CopyDisplayUUID( AirPlayReceiverSessionScreenRef inSession, OSStatus *outErr );
CFArrayRef AirPlayReceiverSessionScreen_CopyTimestampInfo( AirPlayReceiverSessionScreenRef inSession, OSStatus *outErr );
OSStatus
	AirPlayReceiverSessionScreen_SetChaChaSecurityInfo(
		AirPlayReceiverSessionScreenRef		inSession,
		const uint8_t *						inKey,
		const size_t						inKeyLen );
OSStatus
	AirPlayReceiverSessionScreen_SetSecurityInfo(
		AirPlayReceiverSessionScreenRef		inSession,
		const uint8_t						inKey[ 16 ],
		const uint8_t						inIV[ 16 ] );
OSStatus
	AirPlayReceiverSessionScreen_Setup(
		AirPlayReceiverSessionScreenRef		inSession,
		CFDictionaryRef						inStreamDesc,
		uint32_t							inSessionID );
OSStatus
	AirPlayReceiverSessionScreen_ProcessFrames(
		AirPlayReceiverSessionScreenRef		inSession,
		NetSocketRef						inNetSock,
		int									inTimeoutDataSecs );
OSStatus	AirPlayReceiverSessionScreen_StartSession( AirPlayReceiverSessionScreenRef inSession, CFDictionaryRef inScreenStreamOptions );
void		AirPlayReceiverSessionScreen_StopSession( AirPlayReceiverSessionScreenRef inSession );

#define kAirPlayReceiverSessionScreenCommand_ForceKeyFrame	'f'
#define kAirPlayReceiverSessionScreenCommand_Quit			'q'
#define kAirPlayReceiverSessionScreenCommand_ServerDied		'd'
#define kAirPlayReceiverSessionScreenCommand_Stop			's' // 32-bit session ID to stop follows command byte.

OSStatus
	AirPlayReceiverSessionScreen_SendCommand(
		AirPlayReceiverSessionScreenRef		inSession,
		char								inCommand,
		const void *						inExtraPtr,
		size_t								inExtraLen );

void		AirPlayReceiverSessionScreen_SetReceiveEndTime( AirPlayReceiverSessionScreenRef inSession, CFAbsoluteTime inTime );
void		AirPlayReceiverSessionScreen_SetAuthEndTime( AirPlayReceiverSessionScreenRef inSession, CFAbsoluteTime inTime );
void		AirPlayReceiverSessionScreen_SetNTPEndTime( AirPlayReceiverSessionScreenRef inSession, CFAbsoluteTime inTime );
void
	AirPlayReceiverSessionScreen_LogStarted(
		AirPlayReceiverSessionScreenRef		inSession,
		CFDictionaryRef						inParams,
		NetTransportType					inTransportType );
void		AirPlayReceiverSessionScreen_LogEnded( AirPlayReceiverSessionScreenRef inSession, OSStatus inReason );
void		AirPlayReceiverSessionScreen_ClearStats( void );
void		AirPlayReceiverSessionScreen_MarkStats( const char *inCommentPtr, size_t inCommentLen );
OSStatus	AirPlayReceiverSessionScreen_PrintStats( DataBuffer *inDB );

#ifdef __cplusplus
}
#endif

#endif	// __AirPlayReceiverSessionScreen_h_
