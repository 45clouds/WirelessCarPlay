/*
	File:    	HTTPMessage.h
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

#ifndef	__HTTPMessage_h__
#define	__HTTPMessage_h__

#include "CommonServices.h"
#include "DebugServices.h"
#include "HTTPUtils.h"
#include "NetUtils.h"

#include CF_RUNTIME_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		HTTPMessage
	@abstract	Encapsulates an HTTP message.
*/
typedef struct HTTPMessagePrivate *		HTTPMessageRef;

CFTypeID	HTTPMessageGetTypeID( void );
OSStatus	HTTPMessageCreate( HTTPMessageRef *outMessage );
void		HTTPMessageReset( HTTPMessageRef inMsg );

OSStatus	HTTPMessageInitRequest( HTTPMessageRef inMsg, const char *inProtocol, const char *inMethod, const char *inFormat, ... );
OSStatus	HTTPMessageInitResponse( HTTPMessageRef inMsg, const char *inProtocol, int inStatusCode, OSStatus inError );

OSStatus
	HTTPMessageGetHeaderField( 
		HTTPMessageRef	inMsg, 
		const char *	inName, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen );
OSStatus	HTTPMessageSetHeaderField( HTTPMessageRef inMsg, const char *inName, const char *inFormat, ... );

#define		HTTPMessageReadMessage( SOCK, MSG ) HTTPMessageReadMessageEx( (MSG), _SocketHTTPReader, (void *)(intptr_t)(SOCK) )
OSStatus	HTTPMessageReadMessageEx( HTTPMessageRef inMsg, NetTransportRead_f inRead_f, void *inRead_ctx );
OSStatus	HTTPMessageWriteMessage( HTTPMessageRef inMsg, NetTransportWriteV_f inWriteV_f, void *inWriteV_ctx );
OSStatus	HTTPMessageSetBody( HTTPMessageRef inMsg, const char *inContentType, const void *inData, size_t inLen );
OSStatus	HTTPMessageSetBodyLength( HTTPMessageRef inMsg, size_t inLen );

#if( COMPILER_HAS_BLOCKS )
	typedef void ( ^HTTPMessageCompletionBlock )( HTTPMessageRef inMsg );
	void	HTTPMessageSetCompletionBlock( HTTPMessageRef inMsg, HTTPMessageCompletionBlock inBlock );
#endif

OSStatus
	HTTPMessageGetOrCopyFormVariable( 
		HTTPMessageRef	inMsg, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char **			outValueStorage );

#define HTTPMessageIsInterleavedBinary( MSG )		( ( (MSG)->header.len == 4 ) && ( (MSG)->header.buf[ 0 ] == '$' ) )
#define HTTPMessageIsLastURLSegment( MSG )			( (MSG)->header.url.segmentPtr == (MSG)->header.url.segmentEnd )

OSStatus
	HTTPMessageScanFFormVariable( 
		HTTPMessageRef	inConnection, 
		const char *	inName, 
		int *			outMatchCount, 
		const char *	inFormat, 
		... );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			HTTPMessageSetBodyFileDescriptor
	@brief		Sets up to send the body of a message from a file descriptor.
	
	@param		inMsg				Message to set up.
	@param		inFD				File descriptor to read from.
	@param		inByteOffset		Byte offset in the file to start reading from. May be negative to treat as an offset from the end.
	@param		inByteCount			Byte offset in the file to start reading from. May be -1 to read to the end of the file.
	@param		inCloseWhenDone		true to close the file descriptor when done.
*/
OSStatus
	HTTPMessageSetBodyFileDescriptor( 
		HTTPMessageRef	inMsg, 
		FDRef			inFD, 
		int64_t			inByteOffset, 
		int64_t			inByteCount, 
		Boolean			inCloseWhenDone );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			HTTPMessageSetBodyFilePath
	@brief		Sets up to send the body of a message from a file path.
	
	@param		inMsg				Message to set up.
	@param		inPath				File path to read from.
	@param		inByteOffset		Byte offset in the file to start reading from. May be negative to treat as an offset from the end.
	@param		inByteCount			Byte offset in the file to start reading from. May be -1 to read to the end of the file.
*/
OSStatus
	HTTPMessageSetBodyFilePath( 
		HTTPMessageRef	inMsg, 
		const char *	inPath, 
		int64_t			inByteOffset, 
		int64_t			inByteCount );

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kHTTPDefaultMaxBodyLen		16000000
#define kHTTPNoTimeout				UINT64_C( 0xFFFFFFFFFFFFFFFF )

typedef int		HTTPChunkState;
#define kHTTPChunkState_Invalid				0
#define kHTTPChunkState_ReadingHeader		1
#define kHTTPChunkState_ReadingBody			2
#define kHTTPChunkState_ReadingBodyEnd		3
#define kHTTPChunkState_ReadingTrailer		4
#define kHTTPChunkState_Done				5

typedef uint32_t	HTTPMessageFlags;
#define kHTTPMessageFlags_None		0			// No flags.
#define kHTTPMessageFlag_NoCopy		( 1 << 0 )	// Don't copy data. Caller must ensure data remains valid.

typedef void ( *HTTPMessageUser_f )( void *inArg );
typedef void ( *HTTPMessageBinaryCompletion_f )( OSStatus inStatus, void *inContext );
typedef void ( *HTTPMessageCompletionFunc )( HTTPMessageRef inMsg );

struct HTTPMessagePrivate
{
	CFRuntimeBase					base;					// CF type info. Must be first.
	HTTPMessageRef					next;					// Next message in the list.
	HTTPHeader						header;					// Header of the message read or written.
	Boolean							headerRead;				// True if the header has been read.
	HTTPChunkState					chunkState;				// State of chunk reading or invalid if not chunked.
	Boolean							closeAfterRequest;		// True if connection should be shutdown after sending request.
	uint8_t *						bodyPtr;				// Pointer to the body buffer.
	size_t							bodyLen;				// Total body length.
	size_t							maxBodyLen;				// Max allowed body length.
	size_t							bodyOffset;				// Offset into the body that we've read so far.
	uint8_t							smallBodyBuf[ 32000 ];	// Fixed buffer used for small messages to avoid allocations.
	uint8_t *						bigBodyBuf;				// malloc'd buffer for large bodies.
	HTTPHeader *					requestHeader;			// Copy of request header when using HTTP auth.
	uint8_t *						requestBodyPtr;			// Copy of request body when using HTTP auth.
	size_t							requestBodyLen;			// Number of bytes in requestBodyPtr.
	iovec_t							iov[ 2 ];				// Used for gathered I/O to avoid non-MTU packets when possible.
	iovec_t *						iop;					// Ptr to the current iovec being sent.
	int								ion;					// Number of iovecs remaining to be sent.
	uint64_t						timeoutNanos;			// DEPRECATED: Nanoseconds until timing out on connects.
	int								connectTimeoutSecs;		// Seconds until timing out on connects if > 0.
	int								dataTimeoutSecs;		// Seconds until timing out on reads/writes if > 0.
	OSStatus						status;					// Status of the message.
	void *							httpContext1;			// Context pointer for use by HTTP library code. Don't touch this.
	void *							httpContext2;			// Context pointer for use by HTTP library code. Don't touch this.
	void *							userContext1;			// Context pointer for use by user code.
	void *							userContext2;			// Context pointer for use by user code.
	void *							userContext3;			// Context pointer for use by user code.
	void *							userContext4;			// Context pointer for use by user code.
	HTTPMessageBinaryCompletion_f	binaryCompletion_f;		// Function pointer for binary messages.
	HTTPMessageCompletionFunc		completion;				// Function to call when a message completes.
#if( COMPILER_HAS_BLOCKS )
	__unsafe_unretained
	HTTPMessageCompletionBlock		completionBlock;		// Block to invoke when a message completes.
#endif
	FDRef							fileFD;					// File descriptor for file-based messages.
	Boolean							closeFD;				// True if we should close the file descriptor when we're done with it.
	int64_t							fileRemain;				// Remaining bytes to send from the file.
};

#ifdef __cplusplus
}
#endif

#endif // __HTTPMessage_h__
