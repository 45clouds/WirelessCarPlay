/*
	File:    	SerialPortUtils.h
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__SerialPortUtils_h__
#define	__SerialPortUtils_h__

#include "CommonServices.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		SerialStreamFlags
	@abstract	Flags controlling how serial data is read or written.
*/
typedef uint32_t		SerialStreamFlags;
#define kSerialStreamFlags_None			0
#define kSerialStreamFlags_Copy			( 1 << 0 ) // Copy data before returning so callers buffer can be free'd immediately.
#define kSerialStreamFlags_CR			( 1 << 1 ) // Treat CR as line ending instead of newline (for ReadLine).
#define kSerialStreamFlags_CRorBell		( 1 << 2 ) // Treat CR or BELL as line ending instead of newline (for ReadLine).

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamCreate
	@abstract	Creates a new SerialStream.
*/
typedef struct SerialStreamPrivate *	SerialStreamRef;

CFTypeID	SerialStreamGetTypeID( void );
OSStatus	SerialStreamCreate( SerialStreamRef *outStream );
#define 	SerialStreamForget( X ) do { if( *(X) ) { SerialStreamInvalidate( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamSetConfig
	@abstract	Sets the configuration (e.g. baud rate).
*/
#define kSerialFlowControl_None		0
#define kSerialFlowControl_XOnOff	1
#define kSerialFlowControl_HW		2

typedef struct
{
	const char *		devicePath;		// Path to the serial port to use (e.g. /dev/xyz).
	int					baudRate;		// Bits per second to run the serial port at.
	int					flowControl;	// Flow control. See kSerialFlowControl_*.
	
}	SerialStreamConfig;

#define SerialStreamConfigInit( PTR )		memset( (PTR), 0, sizeof( SerialStreamConfig ) )

OSStatus	SerialStreamSetConfig( SerialStreamRef inStream, const SerialStreamConfig *inConfig );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamSetDispatchQueue
	@abstract	Sets the GCD queue to perform all operations on.
	@discussion	Note: this cannot be changed once operations have started.
*/
void	SerialStreamSetDispatchQueue( SerialStreamRef inStream, dispatch_queue_t inQueue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamInvalidate
	@abstract	Cancels all outstanding operations. A new stream must be recreated to start serial communication again.
*/
void	SerialStreamInvalidate( SerialStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamRead
	@abstract	Reads data from the serial port asynchronously.
	
	@param		inStream		Stream to read from.
	@param		inMinLen		Minimum number of bytes to read. If you're reading a string, this would usually be 1.
	@param		inMaxLen		Maximum number of bytes to read. This is often the size of the buffer.
	@param		inBuffer		Buffer to read into.
								If this is non-NULL then it must remain valid until the completion is called.
								If this is NULL then data will be written to a private buffer passed to the completion.
	@param		inCompletion	Function to call when the read completes or fails.
	@param		inContext		Caller-specified context to pass to completion function.
*/
typedef void ( *SerialStreamReadCompletion_f )( OSStatus inStatus, void *inBuffer, size_t inLen, void *inContext );

OSStatus
	SerialStreamRead( 
		SerialStreamRef					inStream, 
		size_t							inMinLen, 
		size_t							inMaxLen, 
		void *							inBuffer, 
		SerialStreamReadCompletion_f	inCompletion, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamReadLine
	@abstract	Reads a line of data from the serial port asynchronously.
	@discussion	The completion function is passed a ptr to the start of the line and its length (excluding line endings).
	
	@param		inStream		Stream to read from.
	@param		inFlags			Flags controlling how the line is read.
	@param		inCompletion	Function to call when the read completes or fails.
	@param		inContext		Caller-specified context to pass to completion function.
*/
OSStatus
	SerialStreamReadLine( 
		SerialStreamRef					inStream, 
		SerialStreamFlags				inFlags, 
		SerialStreamReadCompletion_f	inCompletion, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamReadLineSync
	@abstract	Synchronous wrapper around the async API.
*/
OSStatus	SerialStreamReadLineSync( SerialStreamRef inStream, SerialStreamFlags inFlags, char **outLine, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamReadSync
	@abstract	Synchronous wrapper around the async API.
*/
OSStatus
	SerialStreamReadSync( 
		SerialStreamRef	inStream, 
		size_t			inMinLen, 
		size_t			inMaxLen, 
		void *			inBuffer, 
		size_t *		outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamWrite
	@abstract	Writes data to the serial port asynchronously.
	
	@param		inStream		Stream to write to.
	@param		inFlags			Flags to control the write.
								If kSerialStreamFlags_Copy is set, data is copied into internal buffers to allow the caller
								to free or reuse the buffers passed in immediately after the call returns.
	@param		inArray			Array of ptr/len pairs for the data to write.
								If kSerialStreamFlags_Copy is not set, this array and the buffers it points to must remain
								valid until the completion is called.
	@param		inCount			Number of entries in the iovec array.
	@param		inCompletion	Function to call when the write completes or fails.
	@param		inContext		Caller-specified context to pass to completion function.
*/
typedef void ( *SerialStreamWriteCompletion_f )( OSStatus inStatus, void *inContext );

OSStatus
	SerialStreamWrite( 
		SerialStreamRef					inStream, 
		SerialStreamFlags				inFlags, 
		iovec_t *						inArray, 
		int								inCount, 
		SerialStreamWriteCompletion_f	inCompletion, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SerialStreamWriteSync
	@abstract	Synchronous wrapper around the async API.
*/
OSStatus	SerialStreamWriteSync( SerialStreamRef inStream, iovec_t *inArray, int inCount );

#ifdef __cplusplus
}
#endif

#endif // __SerialPortUtils_h__
