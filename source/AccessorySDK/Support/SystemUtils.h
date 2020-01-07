/*
	File:    	SystemUtils.h
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
	
	Copyright (C) 2010-2013 Apple Inc. All Rights Reserved.
*/
/*!
	@header		System information API
	@discussion	APIs for getting information about the system, such as device name, model, etc.
*/

#ifndef	__SystemUtils_h__
#define	__SystemUtils_h__

#include "CommonServices.h"

#include CF_HEADER


#ifdef __cplusplus
extern "C" {
#endif

#if( TARGET_OS_DARWIN )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CopyDeviceSettingsPlist
	@abstract	Copies a settings plist for a device.
*/
CF_RETURNS_RETAINED
CFDictionaryRef	CopyDeviceSettingsPlist( CFStringRef inBundleID, CFStringRef inFilename, CFStringRef inModel );
#endif

#if( TARGET_OS_DARWIN )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CopySystemVersionPlist
	@abstract	Copies the system version plist.
*/
CF_RETURNS_RETAINED
CFDictionaryRef	CopySystemVersionPlist( void );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Gestalt Helpers
	@abstract	Simplifies getting values from Gestalt.
*/
	#define kGestaltBuildVersion				CFSTR( "BuildVersion" )
	#define kGestaltDeviceClass					CFSTR( "DeviceClass" )
	#define kGestaltDeviceName					CFSTR( "DeviceName" )
	#define kGestaltDeviceSupports1080p			CFSTR( "DeviceSupports1080p" )
	#define kGestaltEthernetMacAddress			CFSTR( "EthernetMacAddress" )
	#define kGestaltInternalBuild				CFSTR( "InternalBuild" )
	#define kGestaltUniqueDeviceID				CFSTR( "UniqueDeviceID" )
	#define kGestaltUserAssignedDeviceName		CFSTR( "UserAssignedDeviceName" )
	#define kGestaltWifiAddress					CFSTR( "WifiAddress" )
	#define kGestaltWifiAddressData				CFSTR( "WifiAddressData" )

// [Data:16] Gets a UUID to represent the whole system. May change if the system is reset to factory defaults.
#define kGestaltSystemUUID		CFSTR( "systemUUID" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GestaltSetDomain
	@abstract	Sets the domain to use for all Gestalt calls within the process. Defaults to the application domain.
*/
void	GestaltSetDomain( CFStringRef inDomain );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GestaltSetHook
	@abstract	Sets a hook function to intercept all Gestalt requests and optionally override them.
	@discussion
	
	A hook function must either return NULL with outErr (if non-NULL) filled in with a non-zero error code.
	Or it must return a non-NULL object that the caller is required to release and fill in outErr (if non-NULL) with kNoErr.
	kNotHandledErr should be returned if hook does not want to provide an override (the default handler will handle it).
*/
typedef CFTypeRef ( *GestaltHook_f )( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr, void *inContext );
void	GestaltSetHook( GestaltHook_f inHook, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GestaltCopyAnswer
	@abstract	Returns information about the system. Caller must release result if non-NULL.
*/
CF_RETURNS_RETAINED
CFTypeRef	GestaltCopyAnswer( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr );
Boolean		GestaltGetBoolean( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr );
char *		GestaltGetCString( CFStringRef inQuestion, CFDictionaryRef inOptions, char *inBuf, size_t inMaxLen, OSStatus *outErr );
uint8_t *
	GestaltGetData( 
		CFStringRef		inQuestion, 
		CFDictionaryRef	inOptions, 
		void *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen, 
		OSStatus *		outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetDeviceModelString
	@abstract	Gets the model string for the device (e.g. "Device1,1").
	
	@param		inBuf		Buffer to receive device model string. Must be at least inMaxLen bytes (includes null terminator).
	@param		inMaxLen	Number of bytes "inBuf" can hold. Includes the null terminator.
	
	@result		Pointer to "inBuf".
*/
char *	GetDeviceModelString( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetDeviceInternalModelString
	@internal
	@abstract	Gets the internal model string for the device (e.g. "K48").
*/
char *	GetDeviceInternalModelString( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetDeviceName
	@abstract	Gets the most user-friendly name available for the device.
	
	@param		inBuf		Buffer to receive device name. Must be at least inMaxLen bytes (includes null terminator).
	@param		inMaxLen	Number of bytes "inBuf" can hold. Includes the null terminator.
	
	@result		Pointer to "inBuf" or a constant, empty string if "inMaxLen" is 0.
*/
char *	GetDeviceName( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetDeviceUniqueID
	@internal
	@abstract	Gets the UDID (or something similar). It's a 40-hex digit string inBuf needs to be 41+ bytes.
*/
char *	GetDeviceUniqueID( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetSystemBuildVersionString
	@abstract	Gets the build version of the OS (e.g. "12A85").
	
	@param		inBuf		Buffer to receive device name. Must be at least inMaxLen bytes (includes null terminator).
	@param		inMaxLen	Number of bytes "inBuf" can hold. Includes the null terminator.
	
	@result		Pointer to "inBuf".
*/
char *	GetSystemBuildVersionString( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetSystemUUID
	@abstract	Gets a UUID to present the system as a whole.
	
	@param		inMaxAgeSecs	Max age of the UUID. If it becomes older than this, a new UUID is generated. -1 means forever.
	@param		outUUID			Receives UUID.
	
	@discussion
	
	This is intended to get a UUID that's persistent up to the specified max age. The first time this is called on a system, 
	it generates a random UUID and then saves it persistently. After that, until it hits its max age, the same UUID will be
	returned. After the max age, a new, random UUID will be generated and saved persistently. Restoring a system to factory 
	defaults will erase the generated UUID and cause the next call to this function to generate a new one.
	
	This function protects itself from multiple threads within the same process, but if used from multiple processes, it's
	the callers responsibility to serialize across multiple processes.
*/
void	GetSystemUUID( int inMaxAgeSecs, uint8_t outUUID[ 16 ] );

#if( TARGET_OS_MACOSX )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HasIvyBridge
	@abstract	Returns true if running on a system with the IvyBridge chipset. 
*/
Boolean	HasIvyBridge( void );
#endif



#ifdef __cplusplus
}
#endif

#endif // __SystemUtils_h__
