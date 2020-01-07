/*
	File:    	ScreenUtils.h
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__ScreenUtils_h__
#define	__ScreenUtils_h__

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Screen =
#endif

// IPC mappings

#if( defined( SCREENUTILS_IPC ) && SCREENUTILS_IPC )
	#define ScreenCreate			ScreenCreate_ipc
	#define _ScreenCopyProperty		_ScreenCopyProperty_ipc
	#define _ScreenSetProperty		_ScreenSetProperty_ipc
	#define ScreenRegister			ScreenRegister_ipc
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenCreate
	@abstract	Creates a new Screen object.
*/
typedef struct ScreenPrivate *		ScreenRef;

CFTypeID	ScreenGetTypeID( void );
OSStatus	ScreenCreate( ScreenRef *outScreen, CFDictionaryRef inProperties );
#define 	ScreenForget( X ) do { if( *(X) ) { CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		ScreenProperties
	@abstract	Properties affecting an screen stream.
*/

// [Data] EDID of the screen.
#define kScreenProperty_EDID				CFSTR( "edid" )
	
// [Number] Features of the screen.
#define kScreenProperty_Features			CFSTR( "features" )
	#define kScreenFeature_Reserved					( 1 << 0 ) // 0x01 Reserved for future use.
	#define kScreenFeature_Knobs					( 1 << 1 ) // 0x02 Supports interacting via knobs.
	#define kScreenFeature_LowFidelityTouch			( 1 << 2 ) // 0x04 Supports interacting via low fidelity touch.
	#define kScreenFeature_HighFidelityTouch		( 1 << 3 ) // 0x08 Supports interacting via high fidelity touch.
	#define kScreenFeature_Touchpad					( 1 << 4 ) // 0x10 Supports interacting via touchpad.

// [Number] Maximum frames per second the screen/decoder can support.
#define kScreenProperty_MaxFPS				CFSTR( "maxFPS" )

// [Platform-specific] Platform-specific layer to render screen frames into.
#define kScreenProperty_PlatformLayer		CFSTR( "platformLayer" )

// [Number] Physical height of the screen in millimeters.
#define kScreenProperty_HeightPhysical		CFSTR( "heightPhysical" )

// [Number] Physical width of the screen in millimeters.
#define kScreenProperty_WidthPhysical		CFSTR( "widthPhysical" )

// [Number] Height of the screen in pixels.
#define kScreenProperty_HeightPixels		CFSTR( "heightPixels" )

// [Number] Width of the screen in pixels.
#define kScreenProperty_WidthPixels			CFSTR( "widthPixels" )

// [Number] Primary input device of the screen.
#define kScreenProperty_PrimaryInputDevice	CFSTR( "primaryInputDevice" )
	#define kScreenPrimaryInputDevice_Undeclared		0
	#define kScreenPrimaryInputDevice_TouchScreen		1
	#define kScreenPrimaryInputDevice_TouchPad			2
	#define kScreenPrimaryInputDevice_Knob				3

// [String] UUID of the screen.
#define kScreenProperty_UUID				CFSTR( "uuid" )

// Convenience getters.

#define ScreenCopyProperty( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectCopyProperty( (OBJECT), NULL, _ScreenCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (OUT_ERROR) )

#define ScreenGetPropertyBoolean( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectGetPropertyBooleanSync( (OBJECT), NULL, _ScreenCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERROR) )

#define ScreenGetPropertyCString( OBJECT, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERROR ) \
	CFObjectGetPropertyCStringSync( (OBJECT), NULL, _ScreenCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), \
		(BUF), (MAX_LEN), (OUT_ERROR) )

#define ScreenGetPropertyInt64( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectGetPropertyInt64Sync( (OBJECT), NULL, _ScreenCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (OUT_ERROR) )

// Convenience setters.

#define ScreenSetProperty( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	_ScreenSetProperty( (STREAM), kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenSetPropertyBoolean( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (STREAM), NULL, _ScreenSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenSetPropertyCString( STREAM, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (STREAM), NULL, _ScreenSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (STR), (LEN) )

#define ScreenSetPropertyData( STREAM, PROPERTY, QUALIFIER, PTR, LEN ) \
	CFObjectSetPropertyData( (STREAM), NULL, _ScreenSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (PTR), (LEN) )

#define ScreenSetPropertyDouble( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (STREAM), NULL, _ScreenSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenSetPropertyInt64( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (STREAM), NULL, _ScreenSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

// Internals

CFTypeRef
	_ScreenCopyProperty( 
		CFTypeRef		inObject, // Must be a ScreenRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr );
OSStatus
	_ScreenSetProperty( 
		CFTypeRef		inObject, // Must be a ScreenRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenCopyMain
	@abstract	Returns the main screen.
*/
ScreenRef	ScreenCopyMain( OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenRegister
	@abstract	Registers a screen with the system.
*/
OSStatus	ScreenRegister( ScreenRef inScreen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenDeregister
	@abstract	Deregisters a screen from the system.
*/
OSStatus	ScreenDeregister( ScreenRef inScreen );

#if 0
#pragma mark -
#pragma mark == ScreenStream =
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamCreate
	@abstract	Creates a new ScreenStream.
*/
typedef struct ScreenStreamPrivate *		ScreenStreamRef;

CFTypeID	ScreenStreamGetTypeID( void );
OSStatus	ScreenStreamCreate( ScreenStreamRef *outStream );
#define 	ScreenStreamForget( X ) do { if( *(X) ) { ScreenStreamStop( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

OSStatus	ScreenStreamInitialize( ScreenStreamRef inStream );
typedef OSStatus	( *ScreenStreamInitialize_f )( ScreenStreamRef inStream );

void	ScreenStreamFinalize( ScreenStreamRef inStream );
typedef void		( *ScreenStreamFinalize_f )( ScreenStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamGetContext / ScreenStreamSetContext
	@abstract	Gets/sets a context pointer. Useful for DLL implementors to access their internal state.
*/
void *	ScreenStreamGetContext( ScreenStreamRef inStream );
void	ScreenStreamSetContext( ScreenStreamRef inStream, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamSetFrameHook
	@abstract	Sets a function to be called with the decoded data before it's displayed.
*/
typedef void ( *ScreenStreamFrameHook_f )( CFTypeRef inFrame, uint64_t inDisplayTicks, void *inContext );
void	ScreenStreamSetFrameHook( ScreenStreamRef inStream, ScreenStreamFrameHook_f inHook, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		ScreenStreamProperties
	@abstract	Properties affecting an screen stream.
*/

// [Data] H.264 AVCC data. This must only affect data queued after this call is (previously queued data used the previous AVCC).
#define kScreenStreamProperty_AVCC				CFSTR( "avcc" )

// [Platform-specific] Platform-specific layer to render screen frames into.
#define kScreenStreamProperty_PlatformLayer		CFSTR( "platformLayer" )

// [Number] Physical height of the screen in millimeters.
#define kScreenStreamProperty_HeightPhysical	CFSTR( "heightPhysical" )

// [Number] Physical width of the screen in millimeters.
#define kScreenStreamProperty_WidthPhysical		CFSTR( "widthPhysical" )

// [Number] Height of the screen in pixels.
#define kScreenStreamProperty_HeightPixels		CFSTR( "heightPixels" )

// [Number] Width of the screen in pixels.
#define kScreenStreamProperty_WidthPixels		CFSTR( "widthPixels" )

// [Dictionary] Rectangle to render the final frame into, scaling if needed.
#define kScreenStreamProperty_DestinationRect	CFSTR( "destinationRect" )

// [Dictionary] Rectangle within the screen image to act as a clipping rect (only use pixels within this rect).
#define kScreenStreamProperty_SourceRect		CFSTR( "sourceRect" )

// [Boolean] Require a hardware decoder.
#define kScreenStreamProperty_RequireHardwareDecoder		CFSTR( "requireHardwareDecoder" )

// [String] Source data format. Defaults to H.264 AVCC if not specified.
#define kScreenStreamProperty_SourceFormat		CFSTR( "sourceFormat" )
	#define kScreenStreamFormat_H264AnnexB		CFSTR( "h264AnnexB" )
	#define kScreenStreamFormat_H264AVCC		CFSTR( "h264AVCC" )

// Convenience setters.

#define ScreenStreamSetProperty( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	_ScreenStreamSetProperty( (STREAM), kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenStreamSetPropertyBoolean( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (STREAM), NULL, _ScreenStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenStreamSetPropertyCString( STREAM, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (STREAM), NULL, _ScreenStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (STR), (LEN) )

#define ScreenStreamSetPropertyData( STREAM, PROPERTY, QUALIFIER, PTR, LEN ) \
	CFObjectSetPropertyData( (STREAM), NULL, _ScreenStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (PTR), (LEN) )

#define ScreenStreamSetPropertyDouble( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (STREAM), NULL, _ScreenStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define ScreenStreamSetPropertyInt64( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (STREAM), NULL, _ScreenStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

// Internals

OSStatus
	_ScreenStreamSetProperty( 
		CFTypeRef		inObject, // Must be a ScreenStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );
typedef OSStatus
	( *_ScreenStreamSetProperty_f )( 
		CFTypeRef		inObject, // Must be a ScreenStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamStart
	@abstract	Starts a stream to prepare it for processing screen data.
*/
OSStatus	ScreenStreamStart( ScreenStreamRef inStream );
typedef OSStatus ( *ScreenStreamStart_f )( ScreenStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamStop
	@abstract	Stops a stream.
*/
void	ScreenStreamStop( ScreenStreamRef inStream );
typedef void ( *ScreenStreamStop_f )( ScreenStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ScreenStreamProcessData
	@abstract	Processes screen data in H.264 AVCC-format.
	
	@param		inStream		Stream to operate on.
	@param		inData			Ptr to H.264 data in AVCC format (NAL units with length prefix).
	@param		inLen			Number of bytes of H.264 data.
	@param		inDisplayTicks	UpTicks when the data should be displayed.
	@param		inOptions		Optional data associated with the frame. May be NULL.
	@param		inCompletion	Function to call after the data has been used or is no longer needed. May be NULL.
	@param		inContext		Context to pass to completion function. May be NULL.
*/
typedef void ( *ScreenStreamCompletion_f )( void *inContext );

OSStatus
	ScreenStreamProcessData( 
		ScreenStreamRef				inStream, 
		const uint8_t *				inData, 
		size_t						inLen, 
		uint64_t					inDisplayTicks, 
		CFDictionaryRef				inOptions, 
		ScreenStreamCompletion_f	inCompletion, 
		void *						inContext );
typedef OSStatus
	( *ScreenStreamProcessData_f )( 
		ScreenStreamRef				inStream, 
		const uint8_t *				inData, 
		size_t						inLen, 
		uint64_t					inDisplayTicks, 
		CFDictionaryRef				inOptions, 
		ScreenStreamCompletion_f	inCompletion, 
		void *						inContext );

#ifdef __cplusplus
}
#endif

#endif // __ScreenUtils_h__
