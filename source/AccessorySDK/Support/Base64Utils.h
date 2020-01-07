/*
	File:    	Base64Utils.h
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
	
	Copyright (C) 2002-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__Base64Utils_h__
#define	__Base64Utils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodedSize
	@abstract	Calculates the base-64 encoded size.

	@param		SIZE	Size of the data to be base-64 encoded.
	
	@result		Number of bytes when base-64 encoded. Every 3 bytes are turned into 4 bytes with up to 2 bytes of padding.
*/

#define Base64EncodedSize( SIZE )			( ( ( (SIZE) + 2 ) / 3 ) * 4 )
#define Base64EncodedMaxSize( SIZE )		Base64EncodedSize( SIZE )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodeEx
	@abstract	Base-64 encodes data.

	@param		inSourceData				Pointer to data to base-64 encode.
	@param		inSourceSize				Size of data to base-64 encode.
	@param		inFlags						Flags to control the encoding.
	@param		inEncodedDataBuffer			Buffer to receive the base-64 encoded data.
	@param		inEncodedDataBufferSize		Maximum number of bytes the encoded data buffer can hold.
	@param		outEncodedSize				Receives the size of the resulting encoded data. May be NULL.
	
	@result		Error code if the input data was malformed and kNoErr otherwise.
				Note: even in the case of an error, the input decode buffer and the optional decoded size pointer 
				will be written to with the actual number of decoded bytes so this can decode partial buffers.
*/

typedef uint32_t		Base64Flags;

#define kBase64Flags_None			0			//! No flags.
#define kBase64Flag_NoPadding		( 1 << 0 )	//! When encoding, don't pad with '='.
#define kBase64Flag_URLCharSet		( 1 << 1 )	//! Encode using the "base64url" variant. See RFC 4648 for details.
#define kBase64Flag_URL				( kBase64Flag_NoPadding | kBase64Flag_URLCharSet )

OSStatus
	Base64EncodeEx( 
		const void *inSourceData, 
		size_t		inSourceSize, 
		Base64Flags	inFlags, 
		void *		inEncodedDataBuffer, 
		size_t		inEncodedDataBufferSize, 
		size_t *	outEncodedSize );

#define Base64Encode( SRC_PTR, SRC_LEN, ENCODED_BUF_PTR, ENCODED_BUF_LEN, OUT_ENCODED_LEN )	\
	Base64EncodeEx( (SRC_PTR), (SRC_LEN), kBase64Flags_None, (ENCODED_BUF_PTR), (ENCODED_BUF_LEN), (OUT_ENCODED_LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodeCopyEx
	@abstract	Base-64 encodes data into a malloc'd, null-terminated string.
	
	@param		inSourceData	Pointer to data to base-64 encode.
	@param		inSourceSize	Size of data to base-64 encode.
	@param		inFlags			Flags to control the encoding.
	@param		outEncodedData	Receives a malloc'd, null-terminated string containing the base-64 encoded data.
	@param		outEncodedSize	Receives the size of the resulting base-64 encoded data (excluding the null terminator). May be NULL.
*/

OSStatus
	Base64EncodeCopyEx( 
		const void *	inSourceData, 
		size_t			inSourceSize, 
		Base64Flags		inFlags, 
		void *			outEncodedData, 
		size_t *		outEncodedSize );

#define Base64EncodeCopy( SRC_PTR, SRC_LEN, OUT_ENCODED_PTR, OUT_ENCODED_LEN ) \
	Base64EncodeCopyEx( (SRC_PTR), (SRC_LEN), kBase64Flags_None, (OUT_ENCODED_PTR), (OUT_ENCODED_LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodedLinesSize
	@abstract	Calculates the base-64 encoded size when including new-lines and indentation.

	@param		inSize			Size of the data to be base-64 encoded.
	@param		inIndentCount	Number of tabs to indent per line.
	@param		inLineEnding	Null-terminated string containing the line ending to use (e.g. "\n" for Unix line endings).
	
	@result		Size of the base-64 encoded data when including newlines and indentation.
*/

size_t	Base64EncodedLinesMaxSize( size_t inSize, size_t inIndentCount, const char *inLineEnding );
#define Base64EncodedLinesSize( LEN, INDENT_COUNT, LINE_ENDING ) \
	Base64EncodedLinesMaxSize( (LEN), (INDENT_COUNT), (LINE_ENDING) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodeLinesEx
	@abstract	Base-64 encodes data into lines with custom indentation.

	@param		inSourceData				Pointer to data to base-64 encode.
	@param		inSourceSize				Size of data to base-64 encode.
	@param		inIndentCount				Number of tabs to use to indent each line.
	@param		inLineEnding				String containing the line ending to use (e.g. "\n" for Unix line endings).
	@param		inFlags						Flags to control the encoding.
	@param		inEncodedDataBuffer			Buffer to receive the base-64 encoded data.
	@param		inEncodedDataBufferSize		Maximum number of bytes the encoded data buffer can hold.
	@param		outEncodedSize				Receives the size of the resulting encoded data. May be NULL.

	@result		Error code if the input data was malformed and kNoErr otherwise.
				Note: even in the case of an error, the input decode buffer and the optional decoded size pointer 
				will be written to with the actual number of decoded bytes so this can decode partial buffers.
*/

OSStatus
	Base64EncodeLinesEx( 
		const void *	inSourceData, 
		size_t 			inSourceSize, 
		size_t 			inIndentCount, 
		const char *	inLineEnding, 
		Base64Flags		inFlags, 
		void *			inEncodedDataBuffer, 
		size_t			inEncodedDataBufferSize, 
		size_t *		outEncodedSize );

#define Base64EncodeLines( SRC_PTR, SRC_LEN, INDENT_COUNT, LINE_ENDING, ENCODED_BUF_PTR, ENCODED_BUF_LEN, OUT_ENCODED_LEN ) \
	Base64EncodeLinesEx( (SRC_PTR), (SRC_LEN), (INDENT_COUNT), (LINE_ENDING), kBase64Flags_None, \
		(ENCODED_BUF_PTR), (ENCODED_BUF_LEN), (OUT_ENCODED_LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64EncodeLinesCopyEx
	@abstract	Base-64 encodes data into a malloc'd, null-terminated string of lines with custom indentation.
	
	@param		inSourceData	Pointer to data to base-64 encode.
	@param		inSourceSize	Size of data to base-64 encode.
	@param		inIndentCount	Number of tabs to use to indent each line.
	@param		inLineEnding	Null-terminated string containing the line ending to use (e.g. "\n" for Unix line endings).
	@param		inFlags			Flags to control the encoding.
	@param		outEncodedData	Receives a malloc'd, null-terminated string containing the base-64 encoded data.
	@param		outEncodedSize	Receives the size of the resulting base-64 encoded data (excluding the null terminator). May be NULL.
*/

OSStatus
	Base64EncodeLinesCopyEx( 
		const void *	inSourceData, 
		size_t 			inSourceSize, 
		size_t 			inIndentCount, 
		const char *	inLineEnding, 
		Base64Flags		inFlags, 
		void *			outEncodedData, 
		size_t *		outEncodedSize );

#define Base64EncodeLinesCopy( SRC_PTR, SRC_LEN, INDENT_COUNT, LINE_ENDING, OUT_ENCODED_PTR, OUT_ENCODED_LEN ) \
	Base64EncodeLinesCopyEx( (SRC_PTR), (SRC_LEN), (INDENT_COUNT), (LINE_ENDING), kBase64Flags_None, \
		(OUT_ENCODED_PTR), (OUT_ENCODED_LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64DecodedMaxSize
	@abstract	Calculates the maximum decoded size given the base-64 encoded size.

	@param		SIZE	Size of the base-64 encoded data.
	
	@result		Maximum size of the data after decoding. Every 4 bytes are turned into 3 bytes with up to 2 bytes of padding.
*/

#define Base64DecodedMaxSize( SIZE )	( ( ( (SIZE) + 3 ) / 4 ) * 3 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64DecodedSize
	@abstract	Calculates the size of the data after decoding the base-64 data (no decode is actually performed).

	@param		inEncodedData	Pointer to base-64 encoded data.
	@param		inEncodedSize	Size of the base-64 encoded data.
	
	@result		Size of the data after decoding (no decode is actually performed).
*/

size_t	Base64DecodedSize( const void *inEncodedData, size_t inEncodedSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64Decode
	@abstract	Decodes base-64 encoded data. Skips whitespace.
	
	@param		inEncodedData				Pointer to base-64 encoded data.
	@param		inEncodedSize				Size of the base-64 encoded data.
	@param		inDecodedDataBuffer			Buffer to receive the decoded data.
	@param		inDecodedDataBufferSize		Maximum number of bytes the decoded data buffer can hold.
	@param		outDecodedSize				Receives the size of the resulting decoded data. May be NULL.
	
	@result		Error code if the input data was malformed and kNoErr otherwise.
				Note: even in the case of an error, the input decode buffer and the optional decoded size pointer 
				will be written to with the actual number of decoded bytes so this can decode partial buffers.
*/

OSStatus
	Base64Decode( 
		const void *	inEncodedData, 
		size_t 			inEncodedSize, 
		void *			inDecodedDataBuffer, 
		size_t			inDecodedDataBufferSize, 
		size_t *		outDecodedSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64DecodeCopy
	@abstract	Decodes data into a malloc'd, null-terminated buffer.
	
	@param		inEncodedData	Base-64 data to decode.
	@param		inEncodedSize	Number of bytes to base-64 data to decode.
	@param		outDecodedData	Receives a malloc'd, null-terminated buffer containing the decoded data.
	@param		outDecodedSize	Receives the size of the resulting decoded data (excluding the null terminator). May be NULL.
	
	@discussion
	
	The output buffer is null terminated for convenience when dealing with string data (e.g. UTF-8 strings, which 
	do not contain embedded nulls), but normal binary may contain zeros, which would look like a null terminator.
*/

OSStatus	Base64DecodeCopy( const void *inEncodedData, size_t inEncodedSize, void *outDecodedData, size_t *outDecodedSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Base64UtilsTest
	@abstract	Unit test.
*/

OSStatus	Base64UtilsTest( Boolean inPrint );

#ifdef __cplusplus
}
#endif

#endif // __Base64Utils_h__
