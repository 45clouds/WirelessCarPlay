/*
	File:    	ScreenUtils.h
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	320.17
	
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
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

#if( defined( LEGACY_REGISTER_SCREEN_HID ) )
	
#if 0
#pragma mark == Screen =
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

#endif
	
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

// Internals

typedef void ( *ScreenStreamSetDelegateContext_f )(
	CFTypeRef		inObject, // Must be a ScreenStreamRef
	void* inContext );

void ScreenStreamSetDelegateContext( ScreenStreamRef inStream, void* inContext );

typedef void ( *ScreenStreamSetWidthHeight_f )(
	CFTypeRef		inObject, // Must be a ScreenStreamRef
	uint32_t width,
	uint32_t height );
	
void ScreenStreamSetWidthHeight( ScreenStreamRef inStream, uint32_t width, uint32_t height );

typedef OSStatus ( *ScreenStreamSetAVCC_f )(
	CFTypeRef		inObject, // Must be a ScreenStreamRef
	const uint8_t* avccPtr,
	size_t avccLen );
	
OSStatus ScreenStreamSetAVCC( ScreenStreamRef inStream, const uint8_t* avccPtr, size_t avccLen );

	
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
