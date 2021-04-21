/*
	File:    	HIDUtils.h
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
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
	
	Copyright (C) 2011-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef	__HIDUtils_h__
#define	__HIDUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == HID Descriptors ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HID Descriptor Building Macros
	@abstract	Macros to simplify building HID descriptors.
*/

#define kHIDType_Main					0
#define kHIDType_Global					1
#define kHIDType_Local					2
#define kHIDType_Reserved				3

#define kHIDMainTag_Input				0x80 // 0b1000 00 nn
#define kHIDMainTag_Output				0x90 // 0b1001 00 nn
#define kHIDMainTag_Feature				0xB0 // 0b1011 00 nn
#define kHIDMainTag_Collection			0xA0 // 0b1010 00 nn
#define kHIDMainTag_EndCollection		0xC0 // 0b1100 00 nn

#define kHIDGlobalTag_UsagePage			0x04 // 0b0000 01 nn
#define kHIDGlobalTag_LogicalMin		0x14 // 0b0001 01 nn
#define kHIDGlobalTag_LogicalMax		0x24 // 0b0010 01 nn
#define kHIDGlobalTag_PhysicalMin		0x34 // 0b0011 01 nn
#define kHIDGlobalTag_PhysicalMax		0x44 // 0b0100 01 nn
#define kHIDGlobalTag_UnitExponent		0x54 // 0b0101 01 nn
#define kHIDGlobalTag_Unit				0x64 // 0b0110 01 nn
#define kHIDGlobalTag_ReportSize		0x74 // 0b0111 01 nn
#define kHIDGlobalTag_ReportID			0x84 // 0b1000 01 nn
#define kHIDGlobalTag_ReportCount		0x94 // 0b1001 01 nn
#define kHIDGlobalTag_Push				0xA4 // 0b1010 01 nn
#define kHIDGlobalTag_Pop				0xB4 // 0b1011 01 nn

#define kHIDLocalTag_Usage				0x08 // 0b0000 10 nn
#define kHIDLocalTag_UsageMin			0x18 // 0b0001 10 nn
#define kHIDLocalTag_UsageMax			0x28 // 0b0010 10 nn
#define kHIDLocalTag_DesignatorIndex	0x38 // 0b0011 10 nn
#define kHIDLocalTag_DesignatorMin		0x48 // 0b0100 10 nn
#define kHIDLocalTag_DesignatorMax		0x58 // 0b0101 10 nn
#define kHIDLocalTag_StringIndex		0x78 // 0b0111 10 nn
#define kHIDLocalTag_StringMin			0x88 // 0b1000 10 nn
#define kHIDLocalTag_StringMax			0x98 // 0b1001 10 nn
#define kHIDLocalTag_Delimiter			0xA8 // 0b1010 10 nn

#define kHIDCollection_Physical			0x00
#define kHIDCollection_Application		0x01
#define kHIDCollection_Logical			0x02
#define kHIDCollection_Report			0x03
#define kHIDCollection_NamedArray		0x04
#define kHIDCollection_UsageSwitch		0x05
#define kHIDCollection_UsageModifier	0x06

#define HID_ITEM( TYPE, TAG, SIZE )		( (TAG) | (TYPE) | (SIZE) )
#define HID_USAGE_PAGE( X )				0x05, (X)
#define HID_USAGE( X )					0x09, (X)
#define HID_COLLECTION( X )				0xA1, (X)
#define HDI_END_COLLECITON()			0xC0

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================


#if 0
#pragma mark -
#pragma mark == HIDDevice ==
#endif

//===========================================================================================================================
//	HIDDevice
//===========================================================================================================================

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDCopyDevices
 @abstract	Copies a property from the browser. See kHIDBrowserProperty_* constants.
 */
CFArrayRef HIDCopyDevices( OSStatus *outErr );

void HIDSetSession( void *inContext );
	
// [Number] USB-style HID country code.
#define kHIDDeviceProperty_CountryCode			CFSTR( "countryCode" )

// [String] UUID of the display this HID device is associated with (or NULL if it's not associated with a display).
#define kHIDDeviceProperty_DisplayUUID			CFSTR( "displayUUID" )

// [String] Name of the device (e.g. "ShuttleXpress").
#define kHIDDeviceProperty_Name					CFSTR( "name" )

// [Number] USB product ID of the device.
#define kHIDDeviceProperty_ProductID			CFSTR( "productID" )

// [Data] USB-style HID report descriptor.
#define kHIDDeviceProperty_ReportDescriptor		CFSTR( "hidDescriptor" )

// [Number] Sample rate of the device in Hz.
#define kHIDDeviceProperty_SampleRate			CFSTR( "sampleRate" )

// [Number] USB vendor ID of the device.
#define kHIDDeviceProperty_VendorID				CFSTR( "vendorID" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDeviceGetTypeID
	@abstract	Gets the CF type ID of all HIDDevice objects.
*/
typedef struct HIDDevicePrivate *		HIDDeviceRef;

CFTypeID	HIDDeviceGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDeviceCreateVirtual
	@abstract	Creates a virtual HID device.
*/
OSStatus	HIDDeviceCreateVirtual( HIDDeviceRef *outDevice, CFDictionaryRef inProperties );
#define		HIDDeviceForget( X ) do { if( *(X) ) { CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDeviceCopyProperty
	@abstract	Copies a property from the device. See kHIDDeviceProperty_* constants.
*/
CFTypeRef	HIDDeviceCopyProperty( HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus *outErr );

CFStringRef HIDDeviceCopyID( HIDDeviceRef inDevice );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDeviceSetProperty
	@abstract	Set a property of the device. See kHIDDeviceProperty_* constants.
*/
OSStatus	HIDDeviceSetProperty( HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDevicePostReport
	@abstract	Posts a HID report.
*/
OSStatus	HIDDevicePostReport( HIDDeviceRef inDevice, const void *inReportPtr, size_t inReportLen );

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDCopyDevices
	@abstract	Copies a snapshot of the current array of HIDDevice objects.
	@discussion	Use HIDBrowser to monitor for HIDDevices being attached and detached.
*/
CFArrayRef	HIDCopyDevices( OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDRegisterDevice
	@abstract	Register a virtual HID device.
*/
OSStatus	HIDRegisterDevice( HIDDeviceRef inDevice );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDDeregisterDevice
	@abstract	Deregister a virtual HID device.
*/
OSStatus	HIDDeregisterDevice( HIDDeviceRef inDevice );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDPostReport
	@abstract	Posts a report from a HID device specified by UID.
*/
OSStatus	HIDPostReport( uint32_t inUID, const void *inReportPtr, size_t inReportLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HIDUtilsTest	
	@abstract	Unit test.
*/
OSStatus	HIDUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __HIDUtils_h__
