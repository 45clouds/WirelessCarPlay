/*
	File:    	APAdvertiser.h
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
	
	Copyright (C) 2015 Apple Inc. All Rights Reserved.
*/

#ifndef APADVERTISER_H
#define APADVERTISER_H

/*!
	@header		APAdvertiser.h
	
	@abstract	API for advertising AirPlay service.
	@discussion	AirPlay devices should use this API to advertise their presence to other nearby AirPlay devices. 
				The advertiser object manages all wireless and wired technologies that AirPlay uses for advertising 
				internally.
				
				Two modes are supported: none and discoverable. Advertising is started by setting the advertising mode
				to discoverable and advertising is stopped by setting it to none.

				The advertiser needs to know about various AirPlay settings before it can set the mode to discoverable. 
				These settings are communicated to the advertiser by setting the advertiser info object.

				Listening port must also be set before the mode can be set to discoverable.

				The advertiser object should be invalidated before the advertiser is deallocated.
*/

#include <CoreUtils/DataBufferUtils.h>

#include "APAdvertiserInfo.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
	@typedef APAdvertiserRef
	A reference to the advertiser, a CF object that adheres to retain/release semantics. When CFRelease() is performed
	on the last reference to the adveriser, the advetiser deallocated.
*/
typedef struct OpaqueAPAdvertiser *		APAdvertiserRef;
	
/*!
	@enum		APAdvertiserMode
	@discussion Advertising modes supported by the AirPlay advertiser.
	
	@constant	kAPAdvertiserMode_None			Advertiser stops advertising.
	@constant	kAPAdvertiserMode_Discoverable	Advertiser starts advertising AirPlay service.
*/
typedef CF_ENUM( uint16_t, APAdvertiserMode ) {
	kAPAdvertiserMode_None				= 0, //! No advertising.
	kAPAdvertiserMode_Discoverable		= 1, //! Device is discoverable.
};
	
/*!
	@constant	kAPAdvertiserProperty_AdvertiserInfo
	@abstract	Required property of type APAdveriserInfoRef: this object contains various AirPlay settings and state
				that must be made available to browsers.
	@discussion The AirPlay advertiser needs to know about various settings and states of the server before it can
				be moved to discovarable mode, where it starts advertising AirPlay service. These settings and states
				are passed to the advertiser with an advertiser info object. This method should be used to set the
				advertiser info object on the advertiser.

				Adveriser info object must contain all required properties otherwise setting this property
				will fail.

				The advertiser will copy the advertiser info object passed here; therefore, if any changes are made
				to the advertiser info object, the user will have to use this method again, to set the new advertiser
				info on the advertiser.

				If the advertiser is in discoverable mode when this method is called, the advertiser will internally
				update the advertised AirPlay service with the new information. There is no need to set the mode
				to none and back to discoverable when setting new advertiser info.

*/
extern const CFStringRef kAPAdvertiserProperty_AdvertiserInfo;

/*!
	@constant	kAPAdvertiserProperty_InterfaceIndex
	@abstract	Optional property of type CFNumberRef (32-bit): specifies the interface that should be used for advertising.
	@discussion Tells the advertiser to restrict advertising to a specific interface. If no interface index is set 
				explicitly, the advertiser will advertise on all available interfaces.

*/
extern const CFStringRef kAPAdvertiserProperty_InterfaceIndex;
	
/*!
	@constant	kAPAdvertiserProperty_ListeningPort
	@abstract	Required property of type CFNumberRef (32-bit): specifies the port where connections are being accepted.
	@discussion The advertiser does not listen for incoming connections, but it needs to know on which port the system
				accepts incoming connections. This property is used tell the advertiser what the listening port is.

				The listening port must be set before the discoverable mode is requested. If the advertiser is already in
				discoverable mode, the user needs to restart advertising by first setting the mode to none and then back
				to discoverable after the new listening port is set.

*/
extern const CFStringRef kAPAdvertiserProperty_ListeningPort;

/*!
	@function	APAdvertiserGetTypeID
	
	@abstract	Returns type ID of the advertiser object.
	
	@result		Type ID of the advertiser object.
*/
CFTypeID APAdvertiserGetTypeID( void );
	
/*!
	@function	APAdvertiserCreate
	
	@abstract	Creates an AirPlay advertiser object.
	@discussion	

	@param		outAdvertiser		Receives a newly created advertiser object with retain count of 1.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserCreate( APAdvertiserRef *outAdvertiser );
	
/*!
	@function	APAdvertiserInvalidate

	@abstract	Invalidates the advertiser object.
	@discussion	This method should be called when the advertiser not needed anymore, before it the advertiser object is 
				destroyed. It will prepare the object for destruction.

	@param		inAdvertiser		Advertiser object that should be invalidated.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInvalidate( APAdvertiserRef inAdvertiser );
	
/*!
	@function	APAdvertiserGetMode

	@abstract	Gets the advertising mode currently set on the advertiser.
	@discussion	This method can be used to get the advertising mode currently set on the advertiser.

	@param		inAdvertiser		Advertiser object to get the advertising mode from.
	@param		outAdvertisingMode	Receives the advertising mode currently set on the advertiser.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise. 
*/
OSStatus APAdvertiserGetMode( APAdvertiserRef inAdvertiser, APAdvertiserMode *outAdvertisingMode );	
	
/*!
	@function	APAdvertiserSetMode

	@abstract	Sets the specified advertising mode on the advertiser.
	@discussion	This method can be used to set a specified advertising mode on the advertiser. There are two supported
				advertising modes: discoverable and none.

				In discoverable mode, the advertiser will start advertising AirPlay service and in none mode, the advertiser
				will stop advertising AirPlay service.

				Listening port and advertiser info must be set before the mode can be set to discoverable. Otherwise, this
				method faill and return an error.

	@param		inAdvertiser		Advertiser object to set the advertising mode on.
	@param		inAdvertisingMode	Advertising mode to be set on the advertier.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise. If 
				discoverable mode is requested and no advertising info was set on the advertiser, this method will fail 
				with error code kNotPrepairedErr.
*/
OSStatus APAdvertiserSetMode( APAdvertiserRef inAdvertiser, APAdvertiserMode inAdvertisingMode );
	
/*!
	@function	APAdvertiserCopyProperty

	@abstract	Delievers a copy of the specified property that is currently set on the advertiser.
	@discussion	This method can be used to get a copy of the property currently set on the advertiser object.

	@param		inAdvertiser			Advertiser object to copy the property from.
	@param		inProperty				Name of the property to be copied. This paramater must be one of the predefined
										property keys above.
	@param		inAllocator				The allocator to use for allocating the copy of the property. Pass
										kCFAllocatorDefault to use the default allocator.
	@param		outValue				Receives a copy of the requested property. This parameter must not be NULL.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise. If an 
				unrecognized property is requested or if the passed outValue is NULL, the method will fail with error code 
				kParamErr.
*/
OSStatus
	APAdvertiserCopyProperty(
		APAdvertiserRef inAdvertiser,
		CFStringRef inProperty,
		CFAllocatorRef inAllocator,
		void *outValue );
	
/*!
	@function	APAdvertiserSetProperty

	@abstract	Sets the specified property on the advertiser.
	@discussion	This method can be used to set a property on the advertiser object.

	@param		inAdvertiser			Advertiserh object to set the property on.
	
	@param		inProperty				Name of the property to be copied. This paramater must be one of the predefined
										property keys above.
	@param		inValue					Value of the parameter that needs to be set.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise. If an 
				unrecognized property name is passed the method will fail with error code kParamErr.
*/
OSStatus APAdvertiserSetProperty( APAdvertiserRef inAdvertiser, CFStringRef inProperty, CFTypeRef inValue );

/*!
	@function	APAdvertiserDebugShow

	@abstract	Prints advertiser state information in the data buffer for debug purposes.
	@discussion	Use this method to print advertiser state information in the data buffer for debug purposes.

	@param		inAdvertiser			Advertiser object we need debug state information on.
	@param		inVerbose				If non-zero, more verbose state information will be printed.
	@param		inDataBuf				DataBuffer object where state information will be printed.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserDebugShow( APAdvertiserRef inAdvertiser, int inVerbose, DataBuffer *inDataBuf );

/*!
	@function	APAdvertiserUpdatePreferences

	@abstract	Updates advertiser preferences.
	@discussion	Use this method to tell the advertiser to re-read all preferences from defaults and update advertising
				accordingly. There is no need to restart advertising if the advertiser is already in discoverable mode.

	@param		inAdvertiser			Advertiser object we want to update preferences on.
	@param		inShouldSynchronize		True if defaults should be read from permanent storage. Set this to false if another
										object has already requested a read from permanent storage.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserUpdatePreferences( APAdvertiserRef inAdvertiser, Boolean inShouldSynchronize );

#ifdef __cplusplus
}
#endif

#endif /* defined(APADVERTISER_H) */
