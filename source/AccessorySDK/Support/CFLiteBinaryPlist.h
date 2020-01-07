/*
	File:    	CFLiteBinaryPlist.h
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__CFLiteBinaryPlist_h__
#define	__CFLiteBinaryPlist_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <stddef.h>
#endif

#include CF_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFBinaryPlistStreamedFlags
	@abstract	Flags for controlling streamed binary plists.
*/
typedef uint32_t	CFBinaryPlistStreamedFlags;
#define kCFBinaryPlistStreamedFlags_None		0
#define kCFBinaryPlistStreamedFlag_Header		( 1 << 0 ) // Write the file header.
#define kCFBinaryPlistStreamedFlag_Trailer		( 1 << 1 ) // Write the file trailer.
#define kCFBinaryPlistStreamedFlag_Begin		( 1 << 2 ) // Only write up to the begin marker of an array/dict.
#define kCFBinaryPlistStreamedFlag_End			( 1 << 3 ) // Only write the end marker of an array/dict.
#define kCFBinaryPlistStreamedFlag_Body			( 1 << 4 ) // Body portion of object without any begin marker or end.
#define kCFBinaryPlistStreamedFlag_NoCopy		( 1 << 5 ) // Try to point to input data instead of making a copy.
#define kCFBinaryPlistStreamedFlag_Unique		( 1 << 6 ) // Use back references to write only unique objects.

#define kCFBinaryPlistStreamedFlags_ReadDefault		( \
	kCFBinaryPlistStreamedFlag_Header | \
	kCFBinaryPlistStreamedFlag_Trailer | \
	kCFBinaryPlistStreamedFlag_Body )

#define kCFBinaryPlistStreamedFlags_Default	( \
	kCFBinaryPlistStreamedFlag_Header | \
	kCFBinaryPlistStreamedFlag_Trailer | \
	kCFBinaryPlistStreamedFlag_Begin | \
	kCFBinaryPlistStreamedFlag_End | \
	kCFBinaryPlistStreamedFlag_Body )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistStreamedCreateData
	@abstract	Converts an object to a streamed binary plist.
*/
CF_RETURNS_RETAINED
CFDataRef	CFBinaryPlistStreamedCreateData( CFTypeRef inObj, OSStatus *outErr );
CFDataRef	CFBinaryPlistStreamedCreateDataEx( CFTypeRef inObj, CFBinaryPlistStreamedFlags inFlags, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistStreamedWriteBytes
	@abstract	Writes raw bytes as a data element.
	@discussion	Note that this does not produce a complete plist.
				It's intended to be used with the BeginOnly/EndOnly features of CFBinaryPlistStreamedWriteObject.
*/
typedef OSStatus ( *CFBinaryPlistStreamedWrite_f )( const void *inData, size_t inLen, void *inContext );
OSStatus
	CFBinaryPlistStreamedWriteBytes( 
		const void *					inData, 
		size_t							inLen, 
		CFBinaryPlistStreamedWrite_f	inCallback, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistStreamedWriteObject
	@abstract	Converts an object to a streamed binary plist.
*/
OSStatus
	CFBinaryPlistStreamedWriteObject( 
		CFTypeRef						inObj, 
		CFBinaryPlistStreamedFlags		inFlags, 
		CFBinaryPlistStreamedWrite_f	inCallback, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistStreamedCreateWithBytes
	@abstract	Converts a streamed binary plist to an object.
*/
CF_RETURNS_RETAINED
CFTypeRef	CFBinaryPlistStreamedCreateWithBytes( const void *inPtr, size_t inLen, OSStatus *outErr );

CF_RETURNS_RETAINED
CFTypeRef
	CFBinaryPlistStreamedCreateWithBytesEx( 
		const void *				inPtr, 
		size_t						inLen, 
		CFBinaryPlistStreamedFlags	inFlags, 
		OSStatus *					outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistStreamedCreateWithData
	@abstract	Converts a streamed binary plist to an object.
*/
CFTypeRef	CFBinaryPlistStreamedCreateWithData( CFDataRef inData, CFBinaryPlistStreamedFlags inFlags, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistV0CreateData
	@abstract	Converts an object to a version 0 binary plist (i.e. compatible with Mac/iOS binary plists).
*/
#define kCFBinaryPlistFlag_UTF8Strings		( 1 << 0 ) // Write non-ASCII strings as UTF-8 (incompatible with Darwin CF).

CF_RETURNS_RETAINED
CFDataRef	CFBinaryPlistV0CreateData( CFTypeRef inObj, OSStatus *outErr );

CF_RETURNS_RETAINED
CFDataRef	CFBinaryPlistV0CreateDataEx( CFTypeRef inObj, uint32_t inFlags, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFBinaryPlistV0CreateWithData
	@abstract	Converts binary plist data to an object.
*/
CF_RETURNS_RETAINED
CFPropertyListRef	CFBinaryPlistV0CreateWithData( const void *inPtr, size_t inLen, OSStatus *outErr );

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLiteBinaryPlistTest
	@abstract	Unit test.
*/
OSStatus	CFLiteBinaryPlistTest( void );

#ifdef __cplusplus
}
#endif

#endif // __CFLiteBinaryPlist_h__
