/*
	File:    	AirPlayReceiverServer.h
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayReceiverServer_h__
#define	__AirPlayReceiverServer_h__

#include "AirPlayCommon.h"
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/CFUtils.h>

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Creation ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCreate
	@abstract	Creates the server and initializes the server. Caller must CFRelease it when done.
*/
typedef struct AirPlayReceiverServerPrivate *		AirPlayReceiverServerRef;
typedef struct AirPlayReceiverSessionPrivate *		AirPlayReceiverSessionRef;
typedef struct AirPlayReceiverConnectionPrivate *	AirPlayReceiverConnectionRef;

CFTypeID			AirPlayReceiverServerGetTypeID( void );
OSStatus			AirPlayReceiverServerCreate( AirPlayReceiverServerRef *outServer );
OSStatus			AirPlayReceiverServerCreateWithConfigFilePath( CFStringRef inConfigFilePath, AirPlayReceiverServerRef *outServer );
dispatch_queue_t	AirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer );

#if 0
#pragma mark -
#pragma mark == Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlayReceiverServerDelegate
	@abstract	Allows functionality to be delegated to external code.
*/
typedef OSStatus
	( *AirPlayReceiverServerControl_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inCommand, 
		CFTypeRef					inQualifier, 
		CFDictionaryRef				inParams, 
		CFDictionaryRef *			outParams, 
		void *						inContext );

typedef CFTypeRef
	( *AirPlayReceiverServerCopyProperty_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext );

typedef OSStatus
	( *AirPlayReceiverServerSetProperty_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		CFTypeRef					inValue, 
		void *						inContext );

typedef void
	( *AirPlayReceiverSessionCreated_f )( 
		AirPlayReceiverServerRef	inServer, 
		AirPlayReceiverSessionRef	inSession, 
		void *						inContext );
	
typedef void
	( *AirPlayReceiverSessionFailed_f )(
		AirPlayReceiverServerRef	inServer,
		OSStatus					inReason,
		void *						inContext );

typedef struct
{
	void *									context;			// Context pointer for the delegate to use.
	void *									context2;			// Extra context pointer for the delegate to use.
	AirPlayReceiverServerControl_f			control_f;			// Function to call for control requests.
	AirPlayReceiverServerCopyProperty_f		copyProperty_f;		// Function to call for copyProperty requests.
	AirPlayReceiverServerSetProperty_f		setProperty_f;		// Function to call for setProperty requests.
	AirPlayReceiverSessionCreated_f			sessionCreated_f;	// Function to call when a session is created.
	AirPlayReceiverSessionFailed_f			sessionFailed_f;	// Function to call when a session creation fails.
	
}	AirPlayReceiverServerDelegate;

#define AirPlayReceiverServerDelegateInit( PTR )	memset( (PTR), 0, sizeof( AirPlayReceiverServerDelegate ) );

void	AirPlayReceiverServerSetDelegate( AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate *inDelegate );

#if 0
#pragma mark -
#pragma mark == Control ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerControl
	@abstract	Controls the server.
*/
OSStatus
	AirPlayReceiverServerControl( 
		CFTypeRef			inServer, // Must be AirPlayReceiverServerRef.
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams );

// Convenience accessors.

#define AirPlayReceiverServerStart( SERVER )	\
	AirPlayReceiverServerControl( (SERVER), kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StartServer ), NULL, NULL, NULL )

#define AirPlayReceiverServerStop( SERVER )	\
	AirPlayReceiverServerControl( (SERVER), kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StopServer ), NULL, NULL, NULL )

#define AirPlayReceiverServerControlF( SERVER, COMMAND, QUALIFIER, OUT_RESPONSE, FORMAT, ... ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_RESPONSE), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerControlV( SERVER, COMMAND, QUALIFIER, OUT_RESPONSE, FORMAT, ARGS ) \
	CFObjectControlSyncV( (SERVER), NULL, AirPlayReceiverServerControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_RESPONSE), (FORMAT), (ARGS) )

#define AirPlayReceiverServerControlAsync( SERVER, COMMAND, QUALIFIER, PARAMS, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT ) \
	CFObjectControlAsync( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (PARAMS), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT) )

#define AirPlayReceiverServerControlAsyncF( SERVER, COMMAND, QUALIFIER, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT, FORMAT, ... ) \
	CFObjectControlAsyncF( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerControlAsyncV( SERVER, COMMAND, QUALIFIER, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT, FORMAT, ARGS ) \
	CFObjectControlAsyncV( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT) (FORMAT), (ARGS) )

#define AirPlayReceiverServerPostEvent( SERVER, EVENT, QUALIFIER, PARAMS ) \
	CFObjectControlAsync( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (EVENT), (QUALIFIER), (PARAMS), NULL, NULL, NULL )

#if 0
#pragma mark -
#pragma mark == Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCopyProperty
	@abstract	Copies a property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
	AirPlayReceiverServerCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr );

// Convenience accessors.

#define AirPlayReceiverServerGetBoolean( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyBooleanSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerGetCString( SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR ) \
	CFObjectGetPropertyCStringSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR) )

#define AirPlayReceiverServerGetDouble( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyDoubleSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerGetInt64( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyInt64Sync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerSetProperty
	@abstract	Sets a property on the server.
*/
OSStatus
	AirPlayReceiverServerSetProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue );

// Convenience accessors.

#define AirPlayReceiverServerSetBoolean( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )
	
#define AirPlayReceiverServerSetBooleanAsync( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), \
		AirPlayReceiverServerSetProperty, kCFObjectFlagAsync, (PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetCString( SERVER, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (STR), (LEN) )

#define AirPlayReceiverServerSetData( SERVER, PROPERTY, QUALIFIER, PTR, LEN ) \
	CFObjectSetPropertyData( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (PTR), (LEN) )

#define AirPlayReceiverServerSetDouble( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetInt64( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetPropertyF( SERVER, PROPERTY, QUALIFIER, FORMAT, ... ) \
	CFObjectSetPropertyF( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerSetPropertyV( SERVER, PROPERTY, QUALIFIER, FORMAT, ARGS ) \
	CFObjectSetPropertyV( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (FORMAT), (ARGS) )

#if 0
#pragma mark -
#pragma mark == Platform ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformInitialize
	@abstract	Initializes the platform-specific aspects of the server. Called once when the server is created.
*/
OSStatus	AirPlayReceiverServerPlatformInitialize( AirPlayReceiverServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformFinalize
	@abstract	Finalizes the platform-specific aspects of the server. Called once when the server is released.
*/
void	AirPlayReceiverServerPlatformFinalize( AirPlayReceiverServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformControl
	@abstract	Controls the platform-specific aspects of the server.
*/
OSStatus
	AirPlayReceiverServerPlatformControl( 
		CFTypeRef			inServer, // Must be AirPlayReceiverServerRef.
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams );

// Convenience accessors.

#define AirPlayReceiverServerPlatformControlF( SERVER, COMMAND, QUALIFIER, OUT_PARAMS, FORMAT, ... ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerPlatformControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_PARAMS), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerPlatformControlV( SERVER, COMMAND, QUALIFIER, OUT_PARAMS, FORMAT, ARGS ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerPlatformControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_PARAMS), (FORMAT), (ARGS) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformCopyProperty
	@abstract	Copies/gets a platform-specific property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
	AirPlayReceiverServerPlatformCopyProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr );

// Convenience accessors.

#define AirPlayReceiverServerPlatformGetBoolean( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyBooleanSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetCString( SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR ) \
	CFObjectGetPropertyCStringSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetDouble( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyDoubleSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetInt64( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyInt64Sync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformSetProperty
	@abstract	Sets a platform-specific property on the server.
*/
OSStatus
	AirPlayReceiverServerPlatformSetProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue );

// Convenience accessors.

#define AirPlayReceiverServerPlatformSetBoolean( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerPlatformSetCString( SERVER, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (STR), (LEN) )

#define AirPlayReceiverServerPlatformSetDouble( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerPlatformSetInt64( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerStopAllAudioConnections
	@abstract	Stops all Audio-capable HTTP connections.
*/
void	AirPlayReceiverServerStopAllAudioConnections( AirPlayReceiverServerRef inServer );



#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverServer_h__
