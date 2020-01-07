/*
	File:    	KeychainUtils.h
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

#ifndef	__KeychainUtils_h__
#define	__KeychainUtils_h__

#include "CommonServices.h"

#include CF_HEADER
#include KEYCHAIN_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	KeychainAddFormatted
	@abstract	Adds an item to the Keychain.
*/
OSStatus	KeychainAddFormatted( CFTypeRef *outAddedItems, const char *inAttributesFormat, ... );
OSStatus	KeychainAddFormattedVAList( CFTypeRef *outResult, const char *inAttributesFormat, va_list inArgs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	KeychainCopyMatching
	@abstract	Searches for and returns keys and optionally the password.
	@discussion
	
	Note: errSecItemNotFound if the item is not found.
	Note: errSecAuthFailed is returned if the wrong password is entered or the user clicks "Deny".
*/
CF_RETURNS_RETAINED
CFTypeRef	KeychainCopyMatchingFormatted( OSStatus *outErr, const char *inFormat, ... );

CF_RETURNS_RETAINED
CFTypeRef	KeychainCopyMatchingFormattedVAList( OSStatus *outErr, const char *inFormat, va_list inArgs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	KeychainDeleteFormatted
	@abstract	Searches for and deletes all the Keychain items matching the query.
*/
OSStatus	KeychainDeleteFormatted( const char *inFormat, ... );
OSStatus	KeychainDeleteFormattedVAList( const char *inFormat, va_list inArgs );
#if( !KEYCHAIN_LITE_ENABLED )
OSStatus	KeychainDeleteItemByPersistentRef( CFDataRef inRef, CFDictionaryRef inAttrs );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	KeychainUpdate
	@abstract	Updates an existing Keychain item.
*/
OSStatus	KeychainUpdateFormatted( CFTypeRef inRefOrQuery, const char *inAttributesToUpdateFormat, ... );
OSStatus	KeychainUpdateFormattedVAList( CFTypeRef inRefOrQuery, const char *inAttributesToUpdateFormat, va_list inArgs );

#if( !EXCLUDE_UNIT_TESTS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	KeychainUtils_Test
	@abstract	Unit test.
*/
OSStatus	KeychainUtils_Test( int inPrint );
#endif

#ifdef __cplusplus
}
#endif

#endif // __KeychainUtils_h__
