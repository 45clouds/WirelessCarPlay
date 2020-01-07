/*
	File:    	IPCUtils.h
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
	
	Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__IPCUtils_h__
#define	__IPCUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.

typedef struct IPCClientPrivate *				IPCClientRef;
typedef struct IPCMessagePrivate *				IPCMessageRef;
typedef struct IPCServerConnectionPrivate *		IPCServerConnectionRef;
typedef struct IPCServerPrivate *				IPCServerRef;

#if 0
#pragma mark -
#pragma mark == Client Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		IPCClientDelegate
	@abstract	Delegates certain functionality to external code.
*/
typedef void		( *IPCClientHandleStopped_f )( IPCClientRef inClient, OSStatus inReason );
typedef OSStatus	( *IPCClientHandleMessage_f )( IPCClientRef inClient, IPCMessageRef inMsg );

typedef struct
{
	void *							delegatePtr;
	IPCClientHandleStopped_f		handleStopped_f;
	IPCClientHandleMessage_f		handleMessage_f;
	
}	IPCClientDelegate;

#define IPCClientDelegateInit( PTR )	memset( (PTR), 0, sizeof( IPCClientDelegate ) )

#if 0
#pragma mark -
#pragma mark == Client API ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCClientCreate
	@abstract	Creates an IPC client.
*/
CFTypeID	IPCClientGetTypeID( void );
OSStatus	IPCClientCreate( IPCClientRef *outClient, const IPCClientDelegate *inDelegate );
#define 	IPCClientForget( X ) do { if( *(X) ) { IPCClientStop( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCClientStart
	@abstract	Starts the client.
*/
OSStatus	IPCClientStart( IPCClientRef inClient );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCClientStop
	@abstract	Stops the client.
*/
void	IPCClientStop( IPCClientRef inClient );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCClientSendMessage
	@abstract	Sends a message to the server.
*/
OSStatus	IPCClientSendMessage( IPCClientRef inClient, IPCMessageRef inMsg );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCClientSendMessageWithReplySync
	@abstract	Sends a message to the server and waits for a reply before returning.
	@discussion	Note: must not be called from the delegate callback.
*/
OSStatus	IPCClientSendMessageWithReplySync( IPCClientRef inClient, IPCMessageRef inMsg, IPCMessageRef *outReply );

#if 0
#pragma mark == Client Internals ==
#endif

// IPCClient

struct IPCClientPrivate
{
	CFRuntimeBase			base;			// CF type info. Must be first.
	dispatch_queue_t		queue;			// Queue to perform all operations on.
	Boolean					started;		// // True if we're started and listening for messages.
	SocketRef				sock;			// Socket used for I/O to the server.
	int						sockRefCount;	// Number of references to the socket.
	dispatch_source_t		readSource;		// GCD source for readability notification.
	dispatch_source_t		writeSource;	// GCD source for writability notification.
	Boolean					writeSuspended;	// True if GCD write source has been suspended.
	IPCMessageRef			readMsg;		// Pre-allocated message for reading.
	IPCMessageRef			waitList;		// List of messages waiting for a reply.
	IPCMessageRef			writeList;		// List of messages to send.
	IPCMessageRef *			writeNext;		// Ptr to append next message to send.
	uint32_t				lastXID;		// Last transaction ID used.
	IPCClientDelegate		delegate;		// Delegate used to configure and implement customizations.
};

#if 0
#pragma mark -
#pragma mark == Message ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		IPCMessage
	@abstract	Encapsulates a message.
*/
#define kIPCFlags_None		0			// No flags.
#define kIPCFlag_Reply		( 1 << 0 )	// Reply to a message.

typedef struct
{
	uint32_t		opcode;		// Operation to perform.
	uint32_t		flags;		// Flags for the message.
	uint32_t		xid;		// Transaction ID.
	int32_t			status;		// Status of the message.
	uint32_t		length;		// Number of bytes in the payload following the header.
	
}	IPCHeader;

check_compile_time( offsetof( IPCHeader, opcode )	== 0 );
check_compile_time( offsetof( IPCHeader, flags )	== 4 );
check_compile_time( offsetof( IPCHeader, xid )		== 8 );
check_compile_time( offsetof( IPCHeader, status )	== 12 );
check_compile_time( offsetof( IPCHeader, length )	== 16 );
check_compile_time( sizeof(   IPCHeader )			== 20 );

typedef void ( *IPCMessageCompletion_f )( IPCMessageRef inMsg );

struct IPCMessagePrivate
{
	CFRuntimeBase				base;					// CF type info. Must be first.
	IPCMessageRef				next;					// Next message in the list.
	IPCMessageRef				waitNext;				// Next message on the wait list.
	Boolean						headerRead;				// True if the header has been read.
	IPCHeader					header;					// Header of the message read or written.
	size_t						readOffset;				// Offset into the data that we've read so far.
	uint8_t *					bodyPtr;				// Pointer to the body buffer.
	size_t						bodyLen;				// Total body length.
	uint8_t						smallBodyBuf[ 32000 ];	// Fixed buffer used for small messages to avoid allocations.
	uint8_t *					bigBodyBuf;				// malloc'd buffer for large bodies.
	iovec_t						iov[ 2 ];				// Used for gathered I/O to avoid non-MTU packets when possible.
	iovec_t *					iop;					// Ptr to the current iovec being sent.
	int							ion;					// Number of iovecs remaining to be sent.
	OSStatus					status;					// Status of the message.
	IPCMessageCompletion_f		completion;				// Function to call when a message completes.
	IPCMessageRef				replyMsg;				// Receives reply message when it arrives.
	dispatch_semaphore_t		replySem;				// Optional semaphore to signal when the reply arrives.
	void *						context;				// Context for caller use.
};

OSStatus	IPCMessageCreate( IPCMessageRef *outMsg );
OSStatus	IPCMessageCreateCopy( IPCMessageRef *outMsg, IPCMessageRef inMsg );
OSStatus	IPCMessageSetBodyLength( IPCMessageRef inMsg, size_t inLen );
OSStatus
	IPCMessageSetContent( 
		IPCMessageRef	inMsg, 
		uint32_t		inOpcode, 
		uint32_t		inFlags, 
		OSStatus		inStatus, 
		const void *	inBody, 
		size_t			inLen );

#if 0
#pragma mark -
#pragma mark == Server Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		IPCServerDelegate
	@abstract	Delegates certain functionality to external code.
*/
typedef OSStatus	( *IPCServerConnectionInit_f )( IPCServerConnectionRef inCnx );
typedef void		( *IPCServerConnectionFree_f )( IPCServerConnectionRef inCnx );
typedef OSStatus	( *IPCServerConnectionHandleMessage_f )( IPCServerConnectionRef inCnx, IPCMessageRef inMsg );

typedef struct
{
	void *									delegatePtr;
	IPCServerConnectionInit_f				initConnection_f;
	IPCServerConnectionFree_f				freeConnection_f;
	IPCServerConnectionHandleMessage_f		handleMessage_f;
	
}	IPCServerDelegate;

#define IPCServerDelegateInit( PTR )	memset( (PTR), 0, sizeof( IPCServerDelegate ) )

#if 0
#pragma mark -
#pragma mark == Server API ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCServerCreate
	@abstract	Creates an IPC server.
*/
CFTypeID	IPCServerGetTypeID( void );
OSStatus	IPCServerCreate( IPCServerRef *outServer, const IPCServerDelegate *inDelegate );
#define 	IPCServerForget( X ) do { if( *(X) ) { IPCServerStop( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCServerStart
	@abstract	Starts the IPC server.
*/
OSStatus	IPCServerStart( IPCServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCServerStop
	@abstract	Stops the IPC server.
*/
OSStatus	IPCServerStop( IPCServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCServerConnectionSendMessage
	@abstract	Sends a message on a connection.
*/
OSStatus	IPCServerConnectionSendMessage( IPCServerConnectionRef inCnx, IPCMessageRef inMsg );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IPCServerConnectionSendMessageWithReplySync
	@abstract	Sends a message to the client and waits for a reply before returning.
	@discussion	Note: must not be called from a message handler.
*/
OSStatus	IPCServerConnectionSendMessageWithReplySync( IPCServerConnectionRef inCnx, IPCMessageRef inMsg, IPCMessageRef *outReply );

#if 0
#pragma mark == Server Internals ==
#endif

// IPCServerConnection

struct IPCServerConnectionPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	IPCServerConnectionRef			next;			// Next connection in the server's list.
	IPCServerRef					server;			// Server this connection is a part of.
	SocketRef						sock;			// Socket for this connection.
	sockaddr_ip						peerAddr;		// Address of the peer side of the connection.
	dispatch_queue_t				queue;			// Queue to perform all operations on.
	dispatch_source_t				readSource;		// GCD source for readability notification.
	dispatch_source_t				writeSource;	// GCD source for writability notification.
	Boolean							writeSuspended;	// True if GCD write source has been suspended.
	IPCMessageRef					readMsg;		// Pre-allocated message for reading.
	IPCMessageRef					waitList;		// List of messages waiting for a reply.
	IPCMessageRef					writeList;		// List of messages to send.
	IPCMessageRef *					writeNext;		// Ptr to append next message to send.
	uint32_t						lastXID;		// Last transaction ID used.
	void *							delegateCtx;	// Delegate-specific data.
};

// IPCServer

typedef struct
{
	dispatch_source_t		source;
	SocketRef				sock;
	IPCServerRef			server;
	
}	IPCServerListenerContext;

struct IPCServerPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	dispatch_queue_t				queue;			// Queue to perform all operations on.
	IPCServerListenerContext *		listener;		// Listener context for IPv4 connections.
	IPCServerConnectionRef			connections;	// Linked list of connections.
	Boolean							started;		// True if we're started and listening for connections.
	IPCServerDelegate				delegate;		// Delegate used to configure and implement customizations.
};

#ifdef __cplusplus
}
#endif

#endif // __IPCUtils_h__
