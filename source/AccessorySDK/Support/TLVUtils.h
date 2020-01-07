/*
	File:    	TLVUtils.h
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
	
	Copyright (C) 2008-2015 Apple Inc. All Rights Reserved.
*/
/*!
	@header			TLV API
	@discussion		APIs for building and parsing Type-Length-Value (TLV) data.
*/

#ifndef	__TLVUtils_h__
#define	__TLVUtils_h__

#include "CommonServices.h"
#include "DataBufferUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == TLV8 ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8Get
	@abstract	Parses TLVs to find the data and length of specific TLV and optionally prepare for finding subsequent items.
*/
OSStatus
	TLV8Get( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		const uint8_t **	outPtr,		// May be NULL.
		size_t *			outLen, 	// May be NULL.
		const uint8_t **	outNext );	// May be NULL.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8GetBytes
	@abstract	Parses TLVs and copies the bytes of the TLV value into the specified buffer.
*/
OSStatus
	TLV8GetBytes( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		size_t				inMinLen, 
		size_t				inMaxLen, 
		void *				inBuffer, 
		size_t *			outLen, 
		const uint8_t **	outNext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8GetSInt64 / TLV8GetUInt64
	@abstract	Parses a TLV8 to find an item and convert it to an integer.
	@discussion	This expects integers to little endian.
*/
int64_t
	TLV8GetSInt64( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		OSStatus *			outErr, 
		const uint8_t **	outNext );

uint64_t
	TLV8GetUInt64( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		OSStatus *			outErr, 
		const uint8_t **	outNext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8GetNext
	@abstract	Parses a TLV8 to get the next item.
*/
OSStatus
	TLV8GetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t *			outType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext ); // May be NULL.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8CopyCoalesced
	@abstract	Parses a TLV8 to return a malloc'd copy of one or more coalesced items of the same type.
*/
uint8_t *
	TLV8CopyCoalesced( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		size_t *			outLen, 
		const uint8_t **	outNext, 
		OSStatus *			outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8GetOrCopyCoalesced
	@abstract	Parses a TLV8 to return one or more coalesced items of the same type.
	@discussion
	
	If only one item was needed then *outPtr will point to it directly and *outStorage will be NULL.
	If more than one item was needed then *outStorage will be non-NULL and *outPtr will point to it.
	If *outStorage is non-NULL then the caller must call free on it when they are done using it.
*/
OSStatus
	TLV8GetOrCopyCoalesced( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		uint8_t **			outStorage, 
		const uint8_t **	outNext ); // May be NULL.

//===========================================================================================================================
/*!	@group		TLV8Buffer
	@abstract	APIs for building TLV8 buffers.
*/
typedef struct
{
	uint8_t *		ptr;					// Pointer to the beginning of the buffer (either mallocedPtr or inlineBuffer).
	size_t			len;					// Current length of the data in the buffer.
	size_t			maxLen;					// Max length allowed to be stored in the buffer.
	uint8_t *		mallocedPtr;			// Pointer to malloc'd buffer if it has more data than inlineBuffer can hold or NULL.
	uint8_t			inlineBuffer[ 300 ];	// Buffer to avoid malloc if the data will fit.
	
}	TLV8Buffer;

void		TLV8BufferInit( TLV8Buffer *inBuffer, size_t inMaxLen );
void		TLV8BufferFree( TLV8Buffer *inBuffer );
#define		TLV8BufferGetPtr( BUFFER )		( (BUFFER)->ptr )
#define		TLV8BufferGetLen( BUFFER )		( (BUFFER)->len )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8BufferAppend
	@abstract	Appends a TLV item to the buffer, resizing or reallocating as needed.
*/
OSStatus	TLV8BufferAppend( TLV8Buffer *inBuffer, uint8_t inType, const void *inPtr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8BufferAppendSInt64 / TLV8BufferAppendUInt64
	@abstract	Appends an integer TLV item to the buffer, resizing or reallocating as needed.
	@discussion	Only the minimum number of bytes needed to hold the integer are used. Multi-byte items are little endian.
*/
OSStatus	TLV8BufferAppendSInt64( TLV8Buffer *inBuffer, uint8_t inType, int64_t x );
OSStatus	TLV8BufferAppendUInt64( TLV8Buffer *inBuffer, uint8_t inType, uint64_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8BufferDetach
	@abstract	Returns a malloc'd ptr for the contents and resets the TLVBuffer to an init state.
	@discussion	Caller must free returned ptr on success.
*/
OSStatus	TLV8BufferDetach( TLV8Buffer *inBuffer, uint8_t **outPtr, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV8MaxPayloadBytesForTotalBytes
	@abstract	Calculates the max number of payload bytes for a given total bytes.
*/
size_t	TLV8MaxPayloadBytesForTotalBytes( size_t inN );

#if 0
#pragma mark -
#pragma mark == TLV16 ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV16Get
	@abstract	Parses TLV16's to find the pointer and length of specific TLV.
*/
OSStatus
	TLV16Get( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		Boolean				inBigEndian, 
		uint16_t			inType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext );
#define TLV16BEGet( SRC, END, TYPE, OUT_PTR, OUT_LEN, OUT_NEXT ) \
	TLV16Get( (SRC), (END), true, (TYPE), (OUT_PTR), (OUT_LEN), (OUT_NEXT) )
#define TLV16LEGet( SRC, END, TYPE, OUT_PTR, OUT_LEN, OUT_NEXT ) \
	TLV16Get( (SRC), (END), false, (TYPE), (OUT_PTR), (OUT_LEN), (OUT_NEXT) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLV16GetNext
	@abstract	Parses a TLV16 to get the next item.
*/
OSStatus
	TLV16GetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		Boolean				inBigEndian, 
		uint16_t *			outType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext );
#define TLV16BEGetNext( SRC, END, OUT_TYPE, OUT_PTR, OUT_LEN, OUT_NEXT ) \
	TLV16GetNext( (SRC), (END), true, (OUT_TYPE), (OUT_PTR), (OUT_LEN), (OUT_NEXT) )
#define TLV16LEGetNext( SRC, END, OUT_TYPE, OUT_PTR, OUT_LEN, OUT_NEXT ) \
	TLV16GetNext( (SRC), (END), false, (OUT_TYPE), (OUT_PTR), (OUT_LEN), (OUT_NEXT) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DataBuffer_AppendTLV
	@abstract	Appends a value to a TLV16.
*/
OSStatus	DataBuffer_AppendTLV16( DataBuffer *inDB, Boolean inBigEndian, uint16_t inType, const void *inData, size_t inLen );
#define		DataBuffer_AppendTLV16BE( DB, TYPE, PTR, LEN )	DataBuffer_AppendTLV16( (DB), true, (TYPE), (PTR), (LEN) )
#define		DataBuffer_AppendTLV16LE( DB, TYPE, PTR, LEN )	DataBuffer_AppendTLV16( (DB), false, (TYPE), (PTR), (LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TLVUtilsTest
	@abstract	Unit test.
*/
void	TLVUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __TLVUtils_h__
