/*
	File:    	DebugIPCUtils.c
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
	
	Copyright (C) 2011-2015 Apple Inc. All Rights Reserved.
*/

#include "DebugIPCUtils.h"

#include <net/if.h>

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "NetUtils.h"
#include "PrintFUtils.h"
#include "TickUtils.h"
#include "UUIDUtils.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kIPCSignature			"ipc1"
#define kIPCAddr				0xE8050F49	// 232.7.31.12 (in the Source-Specific Multicast range).
#define kIPCPort				3721		// Note: registered for Xsync.
#define kIPCPacketHeaderSize	offsetof( IPCPacket, payload )
#define kIPCMaxPacketSize		1472		// Enet MTU (1500) - IPv4 header (20) - UDP header (8) = 1472.
#define kIPCMaxPayloadSize		1434		// Max IPC packet size (1472) - IPC header (38) = 1434.
#define kIPCMaxFragments		254
#define kIPCMaxMessages			20

typedef struct
{
	char		signature[ 4 ];					// [0x04]
	uint8_t		messageUUID[ 16 ];				// [0x04]
	uint8_t		originatorUUID[ 16 ];			// [0x14]
	uint8_t		fragmentIndex;					// [0x24]
	uint8_t		fragmentCount;					// [0x25]
	uint8_t		payload[ kIPCMaxPayloadSize ];	// [0x26]
	
}	IPCPacket;

check_compile_time( offsetof( IPCPacket, signature )		== 0x00 );
check_compile_time( offsetof( IPCPacket, messageUUID )		== 0x04 );
check_compile_time( offsetof( IPCPacket, originatorUUID )	== 0x14 );
check_compile_time( offsetof( IPCPacket, fragmentIndex )	== 0x24 );
check_compile_time( offsetof( IPCPacket, fragmentCount )	== 0x25 );
check_compile_time( offsetof( IPCPacket, payload )			== 0x26 );
check_compile_time( sizeof(   IPCPacket )					== kIPCMaxPacketSize );

typedef struct IPCPacketEntry *		IPCPacketEntryRef;
struct IPCPacketEntry
{
	IPCPacketEntryRef		next;
	IPCPacket				pkt;
	size_t					len;
};

typedef struct IPCMessage *		IPCMessageRef;
struct IPCMessage
{
	IPCMessageRef		next;
	uint64_t			receiveTicks;
	IPCPacketEntryRef	packetList;
	int					packetCount;
};

struct IPCAgentPrivate
{
	dispatch_queue_t			internalQueue;
	dispatch_semaphore_t		quitSem;
	uint8_t						selfUUID[ 16 ];
	SocketRef					sock;
	dispatch_source_t			source;
	uint32_t					loopbackIfIndex;
	IPCMessageRef				messageList;
	int							messageCount;
	sockaddr_ip					groupAddr;
	
	IPCMessageHandlerFunc		handlerFunc;
	void *						handlerContext;
};

DEBUG_STATIC void		_IPCAgent_Delete( void *inContext );
DEBUG_STATIC void		_IPCAgent_Finalize( void *inContext );
DEBUG_STATIC void		_IPCAgent_ReadHandler( void *inArg );
DEBUG_STATIC void		_IPCAgent_CancelHandler( void *inArg );
DEBUG_STATIC OSStatus	_IPCAgent_ProcessMessageBytes( IPCAgentRef inAgent, const void *inMsg, size_t inLen );
DEBUG_STATIC OSStatus	_IPCAgent_ProcessMessagePackets( IPCAgentRef inAgent, IPCMessageRef inMsg );
DEBUG_STATIC void		_IPCAgent_CleanupStalePackets( IPCAgentRef inAgent );
DEBUG_STATIC void		_IPCAgent_FreeMessage( IPCMessageRef inMsg );
DEBUG_STATIC void		_IPCAgent_SendMessage( void *inContext );
DEBUG_STATIC void		_IPCAgent_PerformHandler( CFDictionaryRef inMsg, void *inContext );

//===========================================================================================================================
//	IPCAgent_Create
//===========================================================================================================================

OSStatus	IPCAgent_Create( IPCAgentRef *outAgent )
{
	OSStatus		err;
	IPCAgentRef		obj;
	
	obj = (IPCAgentRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->sock = kInvalidSocketRef;
	UUIDGet( obj->selfUUID );
	
	obj->internalQueue = dispatch_queue_create( "IPCAgent", NULL );
	require_action( obj->internalQueue, exit, err = kNoMemoryErr );
	dispatch_set_context( obj->internalQueue, obj );
	dispatch_set_finalizer_f( obj->internalQueue, _IPCAgent_Finalize );
	
	*outAgent = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	if( obj ) _IPCAgent_Finalize( obj );
	return( err );
}

//===========================================================================================================================
//	IPCAgent_DeleteAsync
//===========================================================================================================================

void	IPCAgent_DeleteAsync( IPCAgentRef inAgent )
{
	dispatch_async_f( inAgent->internalQueue, inAgent, _IPCAgent_Delete );
}

void	IPCAgent_DeleteSync( IPCAgentRef inAgent )
{
	dispatch_semaphore_t		quitSem;
	
	quitSem = dispatch_semaphore_create( 0 );
	check( quitSem );
	
	inAgent->quitSem = quitSem;
	dispatch_async_f( inAgent->internalQueue, inAgent, _IPCAgent_Delete );
	if( quitSem )
	{
		dispatch_semaphore_wait( quitSem, DISPATCH_TIME_FOREVER );
		arc_safe_dispatch_release( quitSem );
	}
}

DEBUG_STATIC void	_IPCAgent_Delete( void *inContext )
{
	IPCAgentRef const		agent = (IPCAgentRef) inContext;
	IPCMessageRef			msg;
	
	agent->handlerFunc = NULL;
	dispatch_socket_forget( agent->source, &agent->sock, false );
	
	while( ( msg = agent->messageList ) != NULL )
	{
		agent->messageList = msg->next;
		_IPCAgent_FreeMessage( msg );
	}
	
	arc_safe_dispatch_release( agent->internalQueue );
}

DEBUG_STATIC void	_IPCAgent_Finalize( void *inContext )
{
	IPCAgentRef const		agent = (IPCAgentRef) inContext;
	
	check( !IsValidSocket( agent->sock ) );
	check( agent->source == NULL );
	check( agent->messageList == NULL );
	check( agent->handlerFunc == NULL );
	agent->internalQueue = NULL;
	
	if( agent->quitSem ) dispatch_semaphore_signal( agent->quitSem );
	
	free( agent );
}

//===========================================================================================================================
//	IPCAgent_SetMessageHandler
//===========================================================================================================================

void	IPCAgent_SetMessageHandler( IPCAgentRef inAgent, IPCMessageHandlerFunc inHandler, void *inContext )
{
	inAgent->handlerFunc    = inHandler;
	inAgent->handlerContext = inContext;
}

//===========================================================================================================================
//	IPCAgent_Start
//===========================================================================================================================

DEBUG_STATIC void	_IPCAgent_Start( void *inContext );

void	IPCAgent_Start( IPCAgentRef inAgent )
{
	dispatch_async_f( inAgent->internalQueue, inAgent, _IPCAgent_Start );
}

DEBUG_STATIC void	_IPCAgent_Start( void *inContext )
{
	IPCAgentRef const		agent = (IPCAgentRef) inContext;
	OSStatus				err;
	SocketRef				sock;
	dispatch_source_t		source;
	char					ifname[ IF_NAMESIZE + 1 ];
	
	sock = kInvalidSocketRef;
	
	// Set up multicast UDP socket to send and receive multicast packets.
	
	err = ServerSocketOpen( AF_INET, SOCK_DGRAM, IPPROTO_UDP, kIPCPort, NULL, kSocketBufferSize_DontSet, &sock );
	require_noerr( err, exit );
	
	err = SocketSetPacketReceiveInterface( sock, true );
	require_noerr( err, exit );
	
	err = GetLoopbackInterfaceInfo( ifname, sizeof( ifname ), &agent->loopbackIfIndex );
	require_noerr( err, exit );
	
	err = SocketSetMulticastInterface( sock, ifname, agent->loopbackIfIndex );
	require_noerr( err, exit );
	
	err = SocketSetMulticastLoop( sock, true );
	require_noerr( err, exit );
	
	memset( &agent->groupAddr.v4, 0, sizeof( agent->groupAddr.v4 ) );
	SIN_LEN_SET( &agent->groupAddr.v4 );
	agent->groupAddr.v4.sin_family		= AF_INET;
	agent->groupAddr.v4.sin_port		= htons( kIPCPort );
	agent->groupAddr.v4.sin_addr.s_addr	= htonl( kIPCAddr );
	
	err = SocketJoinMulticast( sock, &agent->groupAddr, ifname, agent->loopbackIfIndex );
	require_noerr( err, exit );
	
	// Set up a GCD source to get notified when a packet has been received.
	
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) sock, 0, agent->internalQueue );
	require_action( source, exit, err = kUnknownErr );
	dispatch_set_context( source, agent );
	dispatch_source_set_event_handler_f(  source, _IPCAgent_ReadHandler );
	dispatch_source_set_cancel_handler_f( source, _IPCAgent_CancelHandler );
	dispatch_resume( source );
	
	agent->sock   = sock;
	agent->source = source;
	sock = kInvalidSocketRef;
	
exit:
	ForgetSocket( &sock );
	if( err ) dlog( kLogLevelWarning, "### [DebugIPC] Start failed: %#m\n", err );
}

//===========================================================================================================================
//	_IPCAgent_ReadHandler
//===========================================================================================================================

DEBUG_STATIC void	_IPCAgent_ReadHandler( void *inArg )
{
	IPCAgentRef const		agent = (IPCAgentRef) inArg;
	OSStatus				err;
	IPCPacketEntryRef		peMem;
	IPCPacketEntryRef		pe;
	struct iovec			iov;
	struct msghdr			msg;
	uint8_t					controlData[ 256 ];
	sockaddr_ip				sip;
	ssize_t					n;
	int						cmp;
	IPCMessageRef *			msgNext;
	IPCMessageRef			msgCurr;
	IPCPacketEntryRef *		pktNext;
	IPCPacketEntryRef		pktCurr;
	
	_IPCAgent_CleanupStalePackets( agent );
	
	// Read the packet into a newly allocate buffer.
	
	peMem = (IPCPacketEntryRef) malloc( sizeof( *pe ) );
	require_action( peMem, exit, err = kNoMemoryErr );
	pe = peMem;
	
	iov.iov_base		= &pe->pkt;
	iov.iov_len			= sizeof( pe->pkt );
	msg.msg_name		= &sip;
	msg.msg_namelen		= (socklen_t) sizeof( sip );
	msg.msg_iov			= &iov;
	msg.msg_iovlen		= 1;
	msg.msg_control		= controlData;
	msg.msg_controllen	= (socklen_t) sizeof( controlData );
	msg.msg_flags		= 0;
	for( ;; )
	{
		n = recvmsg( agent->sock, &msg, 0 );
		err = map_socket_value_errno( agent->sock, n >= 0, n );
		if( err == EINTR ) continue;
		if( err && SocketIsDefunct( agent->sock ) )
		{
			dlog( kLogLevelNotice, "[DebugIPC] Socket became defunct\n" );
			dispatch_socket_forget( agent->source, &agent->sock, false );
			err = kNoErr;
			goto exit;
		}
		require_noerr( err, exit );
		break;
	}
	require_action( n >= ( (ssize_t) kIPCPacketHeaderSize ), exit, err = kUnderrunErr );
	require_action( msg.msg_controllen >= sizeof( struct cmsghdr ), exit, err = kSizeErr );
	require_action( SocketGetPacketReceiveInterface( &msg, NULL ) == agent->loopbackIfIndex, exit, err = kInternalErr );
	require_action( memcmp( pe->pkt.signature, kIPCSignature, 4 ) == 0, exit, err = kSignatureErr );
	require_action( pe->pkt.fragmentIndex < pe->pkt.fragmentCount, exit, err = kRangeErr );
	require_action( pe->pkt.fragmentCount > 0, exit, err = kCountErr );
	require_action( pe->pkt.fragmentCount <= kIPCMaxFragments, exit, err = kCountErr );
	pe->len = ( (size_t) n ) - kIPCPacketHeaderSize;
	
	// Ignore the packet if we sent it.
	
	if( UUIDCompare( pe->pkt.originatorUUID, agent->selfUUID ) == 0 )
	{
		err = kNoErr;
		goto exit;
	}
	
	// If the packet is not fragmented, process it immediately.
	
	if( pe->pkt.fragmentCount == 1 )
	{
		err = _IPCAgent_ProcessMessageBytes( agent, pe->pkt.payload, pe->len );
		require_noerr( err, exit );
		goto exit;
	}
	
	// Search for a message to associate this packet. If none is found, start a new message.
	
	cmp = -1;
	for( msgNext = &agent->messageList; ( msgCurr = *msgNext ) != NULL; msgNext = &msgCurr->next )
	{
		cmp = memcmp( msgCurr->packetList->pkt.messageUUID, pe->pkt.messageUUID, sizeof( pe->pkt.messageUUID ) );
		if( cmp >= 0 ) break;
	}
	if( cmp != 0 )
	{
		require_action( agent->messageCount < kIPCMaxMessages, exit, err = kNoResourcesErr );
		
		msgCurr = (IPCMessageRef) malloc( sizeof( *msgCurr ) );
		require_action( msgCurr, exit, err = kNoMemoryErr );
		
		pe->next				= NULL;
		msgCurr->receiveTicks	= UpTicks();
		msgCurr->packetList		= pe;
		msgCurr->packetCount	= 1;
		
		msgCurr->next	= *msgNext;
		*msgNext		= msgCurr;
		peMem			= NULL;
		++agent->messageCount;
		goto exit;
	}
	require_action( pe->pkt.fragmentCount == msgCurr->packetList->pkt.fragmentCount, exit, err = kCountErr );
	
	// Add the packet to the message in fragment order.
	
	cmp = -1;
	for( pktNext = &msgCurr->packetList; ( pktCurr = *pktNext ) != NULL; pktNext = &pktCurr->next )
	{
		cmp = ( (int) pktCurr->pkt.fragmentIndex ) - ( (int) pe->pkt.fragmentIndex );
		if( cmp >= 0 ) break;
	}
	require_action( cmp != 0, exit, err = kDuplicateErr );
	
	pe->next = pktCurr;
	*pktNext = pe;
	peMem    = NULL;
	
	// Process the message if we've received all its packets.
	
	if( ++msgCurr->packetCount == pe->pkt.fragmentCount )
	{
		--agent->messageCount;
		*msgNext = msgCurr->next;
		err = _IPCAgent_ProcessMessagePackets( agent, msgCurr );
		_IPCAgent_FreeMessage( msgCurr );
		require_noerr( err, exit );
	}
	
exit:
	if( peMem )	free( peMem );
	if( err )	dlog( kLogLevelWarning, "[DebugIPC] ### Packet receive error: %#m\n", err );
}

//===========================================================================================================================
//	_IPCAgent_CancelHandler
//===========================================================================================================================

DEBUG_STATIC void	_IPCAgent_CancelHandler( void *inArg )
{
	IPCAgentRef const		agent = (IPCAgentRef) inArg;
	
	ForgetSocket( &agent->sock );
	agent->source = NULL;
}

//===========================================================================================================================
//	_IPCAgent_ProcessMessageBytes
//===========================================================================================================================

DEBUG_STATIC OSStatus	_IPCAgent_ProcessMessageBytes( IPCAgentRef inAgent, const void *inMsg, size_t inLen )
{
	OSStatus			err;
	CFDataRef			plistData;
	CFDictionaryRef		plistObj;
	
	plistObj = NULL;
	plistData = CFDataCreate( NULL, inMsg, (CFIndex) inLen );
	require_action( plistData, exit, err = kNoMemoryErr );
	
	plistObj = (CFDictionaryRef) CFPropertyListCreateWithData( NULL, plistData, 0, NULL, NULL );
	CFRelease( plistData );
	require_action( plistObj, exit, err = kMalformedErr );
	require_action( CFGetTypeID( plistObj ) == CFDictionaryGetTypeID(), exit, err = kTypeErr );
	
	if( inAgent->handlerFunc ) inAgent->handlerFunc( plistObj, inAgent->handlerContext );
	err = kNoErr;
	
exit:
	if( plistObj ) CFRelease( plistObj );
	return( err );
}

//===========================================================================================================================
//	_IPCAgent_ProcessMessagePackets
//===========================================================================================================================

DEBUG_STATIC OSStatus	_IPCAgent_ProcessMessagePackets( IPCAgentRef inAgent, IPCMessageRef inMsg )
{
	OSStatus				err;
	IPCPacketEntryRef		pe;
	CFMutableDataRef		plistData;
	CFDictionaryRef			plistObj;
	
	plistObj = NULL;
	plistData = CFDataCreateMutable( NULL, 0 );
	require_action( plistData, exit, err = kUnknownErr );
	
	for( pe = inMsg->packetList; pe; pe = pe->next )
	{
		CFDataAppendBytes( plistData, pe->pkt.payload, (CFIndex) pe->len );
	}
	
	plistObj = (CFDictionaryRef) CFPropertyListCreateWithData( NULL, plistData, 0, NULL, NULL );
	CFRelease( plistData );
	require_action( plistObj, exit, err = kMalformedErr );
	require_action( CFGetTypeID( plistObj ) == CFDictionaryGetTypeID(), exit, err = kTypeErr );
	
	if( inAgent->handlerFunc ) inAgent->handlerFunc( plistObj, inAgent->handlerContext );
	err = kNoErr;
	
exit:
	if( plistObj ) CFRelease( plistObj );
	return( err );
}

//===========================================================================================================================
//	_IPCAgent_CleanupStalePackets
//===========================================================================================================================

DEBUG_STATIC void	_IPCAgent_CleanupStalePackets( IPCAgentRef inAgent )
{
	uint64_t				nowTicks, staleTicks;
	IPCMessageRef *			next;
	IPCMessageRef			curr;
	
	nowTicks   = UpTicks();
	staleTicks = 5 * UpTicksPerSecond();
	for( next = &inAgent->messageList; ( curr = *next ) != NULL; )
	{
		if( ( nowTicks - curr->receiveTicks ) > staleTicks )
		{
			--inAgent->messageCount;
			*next = curr->next;
			_IPCAgent_FreeMessage( curr );
			continue;
		}
		next = &curr->next;
	}
}

//===========================================================================================================================
//	_IPCAgent_FreeMessage
//===========================================================================================================================

DEBUG_STATIC void	_IPCAgent_FreeMessage( IPCMessageRef inMsg )
{
	IPCPacketEntryRef		pe;
	
	while( ( pe = inMsg->packetList ) != NULL )
	{
		inMsg->packetList = pe->next;
		free( pe );
	}
	free( inMsg );
}

//===========================================================================================================================
//	IPCAgent_SendMessage
//===========================================================================================================================

typedef struct
{
	IPCAgentRef			agent;
	CFDictionaryRef		msg;
	
}	IPCAgent_SendMessageParams;

OSStatus	IPCAgent_SendMessage( IPCAgentRef inAgent, CFDictionaryRef inMsg )
{
	OSStatus							err;
	IPCAgent_SendMessageParams *		params;
	
	params = (IPCAgent_SendMessageParams *) malloc( sizeof( *params ) );
	require_action( params, exit, err = kNoMemoryErr );
	
	params->agent = inAgent;
	params->msg   = inMsg;
	CFRetain( inMsg );
	
	dispatch_async_f( inAgent->internalQueue, params, _IPCAgent_SendMessage );
	static_analyzer_malloc_freed( params );
	err = kNoErr;
	
exit:
	return( err );
}

DEBUG_STATIC void	_IPCAgent_SendMessage( void *inContext )
{
	IPCAgent_SendMessageParams * const		params = (IPCAgent_SendMessageParams *) inContext;
	IPCAgentRef const						agent = params->agent;
	OSStatus								err;
	CFDataRef								plistData;
	const uint8_t *							payloadPtr;
	size_t									payloadLen;
	IPCPacket								pkt;
	size_t									len;
	ssize_t									n;
	
	plistData = CFPropertyListCreateData( NULL, params->msg, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
	require_action( plistData, exit, err = kUnknownErr );
	payloadPtr = CFDataGetBytePtr( plistData );
	payloadLen = (size_t) CFDataGetLength( plistData );
	
	check_compile_time_code( sizeof_string( kIPCSignature ) == sizeof( pkt.signature ) );
	memcpy( pkt.signature, kIPCSignature, 4 );
	UUIDGet( pkt.messageUUID );
	UUIDCopy( pkt.originatorUUID, agent->selfUUID );
	pkt.fragmentIndex = 0;
	pkt.fragmentCount = (uint8_t)( ( payloadLen + ( kIPCMaxPayloadSize - 1 ) ) / kIPCMaxPayloadSize );
	
	do
	{
		len = Min( payloadLen, kIPCMaxPayloadSize );
		memcpy( pkt.payload, payloadPtr, len );
		payloadPtr += len;
		payloadLen -= len;
		
		len = offsetof( IPCPacket, payload ) + len;
		n = sendto( agent->sock, &pkt, len, 0, &agent->groupAddr.sa, (socklen_t) sizeof( agent->groupAddr.v4 ) );
		err = map_socket_value_errno( agent->sock, n == ( (ssize_t) len ), n );
		require_noerr( err, exit );
		
	}	while( ++pkt.fragmentIndex < pkt.fragmentCount );
	
exit:
	if( plistData ) CFRelease( plistData );
	CFRelease( params->msg );
	free( params );
}

//===========================================================================================================================
//	IPCAgent_Perform
//===========================================================================================================================

typedef struct
{
	IPCAgentRef					agent;
	dispatch_semaphore_t		lock;
	dispatch_semaphore_t		msgSem;
	CFMutableArrayRef			msgs;
	
}	IPCAgentContext;

OSStatus	IPCAgent_Perform( CFDictionaryRef inMessage, IPCMessageHandlerFunc inResponseHandler, void *inResponseContext )
{
	OSStatus			err;
	IPCAgentContext		context;
	CFIndex				n;
	CFDictionaryRef		msg;
	
	context.agent	= NULL;
	context.msgSem	= NULL;
	context.msgs	= NULL;
	
	context.lock = dispatch_semaphore_create( 1 );
	require_action( context.lock, exit, err = kUnknownErr );
	
	context.msgSem = dispatch_semaphore_create( 0 );
	require_action( context.msgSem, exit, err = kUnknownErr );
	
	context.msgs = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( context.msgs, exit, err = kNoMemoryErr );
	
	err = IPCAgent_Create( &context.agent );
	require_noerr( err, exit );
	
	IPCAgent_SetMessageHandler( context.agent, _IPCAgent_PerformHandler, &context );
	IPCAgent_Start( context.agent );
	
	err = IPCAgent_SendMessage( context.agent, inMessage );
	require_noerr( err, exit );
	
	for( ;; )
	{
		err = (OSStatus) dispatch_semaphore_wait( context.msgSem, 
			dispatch_time( DISPATCH_TIME_NOW, 300 * kNanosecondsPerMillisecond ) );
		if( err ) break;
		
		for( ;; )
		{
			dispatch_semaphore_wait( context.lock, DISPATCH_TIME_FOREVER );
			n = CFArrayGetCount( context.msgs );
			if( n == 0 )
			{
				dispatch_semaphore_signal( context.lock );
				break;
			}
			
			msg = CFArrayGetValueAtIndex( context.msgs, 0 );
			CFRetain( msg );
			CFArrayRemoveValueAtIndex( context.msgs, 0 );
			dispatch_semaphore_signal( context.lock );
			
			inResponseHandler( msg, inResponseContext );
			CFRelease( msg );
		}
	}
	err = kNoErr;
	
exit:
	if( context.agent )		IPCAgent_DeleteSync( context.agent );
	if( context.lock )		arc_safe_dispatch_release( context.lock );
	if( context.msgSem )	arc_safe_dispatch_release( context.msgSem );
	if( context.msgs )		CFRelease( context.msgs );
	return( err );
}

DEBUG_STATIC void	_IPCAgent_PerformHandler( CFDictionaryRef inMsg, void *inContext )
{
	IPCAgentContext * const		context = (IPCAgentContext *) inContext;
	Boolean						wasEmpty;
	
	dispatch_semaphore_wait( context->lock, DISPATCH_TIME_FOREVER );
		wasEmpty = ( CFArrayGetCount( context->msgs ) == 0 );
		CFArrayAppendValue( context->msgs, inMsg );
	dispatch_semaphore_signal( context->lock );
	if( wasEmpty ) dispatch_semaphore_signal( context->msgSem );
}

#if 0
#pragma mark -
#pragma mark == Debugging Support ==
#endif

//===========================================================================================================================
//	Debugging Support
//===========================================================================================================================

static void	_DebugIPC_MessageHandler( CFDictionaryRef inMsg, void *inContext );
static void	_DebugIPC_ShowHandler( CFDictionaryRef inMsg, void *inContext );

static IPCAgentRef				gDebugIPCAgent			= NULL;
static DebugIPCHandlerFunc		gDebugIPCHandlerFunc	= NULL;
static void *					gDebugIPCHandlerContext	= NULL;

//===========================================================================================================================
//	DebugIPC_EnsureInitialized
//===========================================================================================================================

OSStatus	DebugIPC_EnsureInitialized( DebugIPCHandlerFunc inHandler, void *inContext )
{
	OSStatus		err;
	
	gDebugIPCHandlerFunc    = inHandler;
	gDebugIPCHandlerContext = inContext;
	
	if( !gDebugIPCAgent )
	{
		err = IPCAgent_Create( &gDebugIPCAgent );
		require_noerr( err, exit );
		
		IPCAgent_SetMessageHandler( gDebugIPCAgent, _DebugIPC_MessageHandler, gDebugIPCAgent );
		IPCAgent_Start( gDebugIPCAgent );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	DebugIPC_EnsureFinalized
//===========================================================================================================================

void	DebugIPC_EnsureFinalized( void )
{
	if( gDebugIPCAgent )
	{
		IPCAgent_DeleteSync( gDebugIPCAgent );
		gDebugIPCAgent = NULL;
	}
}

//===========================================================================================================================
//	DebugIPC_LogControl
//===========================================================================================================================

OSStatus	DebugIPC_LogControl( const char *inNewConfig )
{
	return( DebugIPC_PerformF( _DebugIPC_ShowHandler, NULL, 
		"{"
			"%kO=%O"
			"%kO=%s" // Note: value may be NULL, which will exclude the value to mean "don't change, just show".
		"}", 
		kDebugIPCKey_Command, kDebugIPCOpCode_Logging, 
		kDebugIPCKey_Value,   inNewConfig ) );
}

//===========================================================================================================================
//	DebugIPC_Perform
//===========================================================================================================================

OSStatus	DebugIPC_Perform( CFDictionaryRef inRequest, IPCMessageHandlerFunc inHandler, void *inContext )
{
	return( IPCAgent_Perform( inRequest, inHandler, inContext ) );
}

OSStatus	DebugIPC_PerformF( IPCMessageHandlerFunc inHandler, void *inContext, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = DebugIPC_PerformV( inHandler, inContext, inFormat, args );
	va_end( args );
	return( err );
}

OSStatus	DebugIPC_PerformV( IPCMessageHandlerFunc inHandler, void *inContext, const char *inFormat, va_list inArgs )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &request, inFormat, inArgs );
	require_noerr( err, exit );
	
	if( !inHandler ) inHandler = _DebugIPC_ShowHandler;
	err = IPCAgent_Perform( request, inHandler, inContext );
	CFRelease( request );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_DebugIPC_MessageHandler
//===========================================================================================================================

static void	_DebugIPC_MessageHandler( CFDictionaryRef inMsg, void *inContext )
{
	IPCAgentRef const			agent = (IPCAgentRef) inContext;
	OSStatus					err;
	CFStringRef					opcode;
	CFDictionaryRef				response;
	CFMutableDictionaryRef		mutableResponse;
	CFStringRef					value;
	char *						tempCStr;
	
	(void) inContext;
	
	response		= NULL;
	mutableResponse	= NULL;
	
	if( gDebugIPCHandlerFunc )
	{
		err = gDebugIPCHandlerFunc( inMsg, &response, gDebugIPCHandlerContext );
		if( !err ) goto send;
		if( err != kNotHandledErr ) goto exit;
	}
	
	opcode = (CFStringRef) CFDictionaryGetValue( inMsg, kDebugIPCKey_Command );
	require_action_quiet( opcode, exit, err = kNotHandledErr );
	require_action( CFIsType( opcode, CFString ), exit, err = kTypeErr );
	
	// Logging
	
	if( CFStringCompare( opcode, kDebugIPCOpCode_Logging, 0 ) == 0 )
	{
		value = (CFStringRef) CFDictionaryGetValue( inMsg, kDebugIPCKey_Value );
		if( value )
		{
			require_action( CFIsType( value, CFString ), exit, err = kTypeErr );
			
			err = LogControlCF( value );
			require_noerr( err, exit );
		}
		
		tempCStr = NULL;
		err = LogShow( &tempCStr );
		require_noerr( err, exit );
		
		err = CFPropertyListCreateFormatted( NULL, &mutableResponse, "{%kO=%s}", kDebugIPCKey_Value, tempCStr );
		free( tempCStr );
		require_noerr( err, exit );
		response = mutableResponse;
	}
	
	if( mutableResponse ) CFDictionarySetValue( mutableResponse, kDebugIPCKey_ResponseType, opcode );
	
send:
	if( response )
	{
		err = IPCAgent_SendMessage( agent, response );
		require_noerr( err, exit );
	}
	
exit:
	if( response ) CFRelease( response );
}

//===========================================================================================================================
//	_DebugIPC_ShowHandler
//===========================================================================================================================

static void	_DebugIPC_ShowHandler( CFDictionaryRef inMsg, void *inContext )
{
	(void) inContext;
	
	FPrintF( stdout, "%@\n", CFDictionaryGetValue( inMsg, kDebugIPCKey_Value ) );
}

#if 0
#pragma mark -
#pragma mark == Unit Test ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	DebugIPCUtils_Test
//===========================================================================================================================

DEBUG_STATIC void	_DebugIPCUtils_TestRequestHandler( CFDictionaryRef inMsg, void *inContext );
DEBUG_STATIC void	_DebugIPCUtils_TestResponseHandler( CFDictionaryRef inMsg, void *inContext );

static IPCAgentRef		gDebugIPC_Agent1		= NULL;
static IPCAgentRef		gDebugIPC_Agent2		= NULL;
static int				gDebugIPC_RequestState	= 0;
static int				gDebugIPC_Agent1State	= 0;
static int				gDebugIPC_Agent2State	= 0;

OSStatus	DebugIPCUtils_Test( void )
{
	OSStatus					err;
	CFMutableDictionaryRef		msg;
	CFMutableDataRef			data;
	
	msg = NULL;
	
	err = IPCAgent_Create( &gDebugIPC_Agent1 );
	require_noerr( err, exit );
	
	IPCAgent_SetMessageHandler( gDebugIPC_Agent1, _DebugIPCUtils_TestRequestHandler, &gDebugIPC_Agent1State );
	IPCAgent_Start( gDebugIPC_Agent1 );
	
	err = IPCAgent_Create( &gDebugIPC_Agent2 );
	require_noerr( err, exit );
	
	IPCAgent_SetMessageHandler( gDebugIPC_Agent2, _DebugIPCUtils_TestRequestHandler, &gDebugIPC_Agent2State );
	IPCAgent_Start( gDebugIPC_Agent2 );
	
	msg = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( msg, exit, err = kNoMemoryErr );
	
	gDebugIPC_RequestState = 1;
	CFDictionarySetValue( msg, CFSTR( "key" ), CFSTR( "value1" ) );
	IPCAgent_Perform( msg, _DebugIPCUtils_TestResponseHandler, NULL );
	require_action( gDebugIPC_Agent1State == 1, exit, err = -1 );
	require_action( gDebugIPC_Agent2State == 1, exit, err = -1 );
	
	data = CFDataCreateMutable( NULL, 0 );
	require_action( data, exit, err = kNoMemoryErr );
	CFDataSetLength( data, 12345 );
	memset( CFDataGetMutableBytePtr( data ), 'z', 12345 );
	CFDictionarySetValue( msg, CFSTR( "data" ), data );
	CFRelease( data );
	
	gDebugIPC_RequestState = 2;
	CFDictionarySetValue( msg, CFSTR( "key" ), CFSTR( "value2" ) );
	IPCAgent_Perform( msg, _DebugIPCUtils_TestResponseHandler, NULL );
	require_action( gDebugIPC_Agent1State == 2, exit, err = -1 );
	require_action( gDebugIPC_Agent2State == 2, exit, err = -1 );
	
	gDebugIPC_RequestState = 3;
	CFDictionarySetValue( msg, CFSTR( "key" ), CFSTR( "value3" ) );
	IPCAgent_Perform( msg, _DebugIPCUtils_TestResponseHandler, NULL );
	require_action( gDebugIPC_Agent1State == 3, exit, err = -1 );
	require_action( gDebugIPC_Agent2State == 3, exit, err = -1 );
	
exit:
	if( gDebugIPC_Agent1 )
	{
		IPCAgent_DeleteSync( gDebugIPC_Agent1 );
		gDebugIPC_Agent1 = NULL;
	}
	if( gDebugIPC_Agent2 )
	{
		IPCAgent_DeleteSync( gDebugIPC_Agent2 );
		gDebugIPC_Agent2 = NULL;
	}
	if( msg ) CFRelease( msg );
	printf( "DebugIPCUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

DEBUG_STATIC void	_DebugIPCUtils_TestRequestHandler( CFDictionaryRef inMsg, void *inContext )
{
	int * const		agentStatePtr = (int *) inContext;
	
	if( gDebugIPC_RequestState == 1 )
	{
		require( CFEqual( CFDictionaryGetValue( inMsg, CFSTR( "key" ) ), CFSTR( "value1" ) ), exit );
		require( *agentStatePtr == 0, exit );
		*agentStatePtr = 1;
	}
	else if( gDebugIPC_RequestState == 2 )
	{
		require( CFEqual( CFDictionaryGetValue( inMsg, CFSTR( "key" ) ), CFSTR( "value2" ) ), exit );
		require( *agentStatePtr == 1, exit );
		*agentStatePtr = 2;
	}
	else if( gDebugIPC_RequestState == 3 )
	{
		require( CFEqual( CFDictionaryGetValue( inMsg, CFSTR( "key" ) ), CFSTR( "value3" ) ), exit );
		require( *agentStatePtr == 2, exit );
		*agentStatePtr = 3;
	}
	else
	{
		dlogassert( "Bad state" );
	}
	
exit:
	return;
}

DEBUG_STATIC void	_DebugIPCUtils_TestResponseHandler( CFDictionaryRef inMsg, void *inContext )
{
	(void) inMsg;
	(void) inContext;
}
#endif // !EXCLUDE_UNIT_TESTS
