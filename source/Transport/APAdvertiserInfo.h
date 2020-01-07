/*
	File:    	APAdvertiserInfo.h
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

#ifndef APADVERTISERINFO_H
#define APADVERTISERINFO_H

/*!
	@header		APAdvertiserInfo.h
	
	@abstract	Storage object for a number of AirPlay properties required by advertisers and browsers.
	@discussion	AirPlay devices that advertise their presence to other nearby AirPlay devices must make a number
				of AirPlay properties available to the devices that interested in conneting to them. These properties
				are stored in the advertiser info object.

				Properties are set and copied via set property and copy property methods.
	
				Some properties are required while others are optional. All required properties must be set before the 
				advertiser info object can be passed to an advertiser object.
*/


#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DataBufferUtils.h>
#include CF_HEADER

#ifdef __cplusplus
extern "C" {
#endif
	
/*!
	@constant	kAPAdvertiserInfoProperty_DeviceID
	@abstract	Required property of type CFString: deviceID is a globally unique ID of the device which must be
				persistent.
*/

extern const CFStringRef kAPAdvertiserInfoProperty_DeviceID;
	
/*!
	@constant	kAPAdvertiserInfoProperty_DeviceName
	@abstract	Required property of type CFString: Device name as presented to the user (such as "John's Apple TV").
*/
extern const CFStringRef kAPAdvertiserInfoProperty_DeviceName;

/*!
	@constant	kAPAdvertiserInfoProperty_AirTunesProtocolVersion
	@abstract	Required property of type CFString: AirTunes protocol version supported by the device (e.g. "65536" for 1.0).
*/
extern const CFStringRef kAPAdvertiserInfoProperty_AirTunesProtocolVersion;
	
/*!
	@constant	kAPAdvertiserInfoProperty_CompressionTypes
	@abstract	Required property of type CFNumber (32-bit): Supported compression types 
				(e.g. "0,1" for none and Apple Lossless).
*/
extern const CFStringRef kAPAdvertiserInfoProperty_CompressionTypes;
	
/*!
	@constant	kAPAdvertiserInfoProperty_EncryptionKeyIndex
	@abstract	Required property of type CFNumber (32-bit): Index of the encryption key to use. Currently "1".
*/
extern const CFStringRef kAPAdvertiserInfoProperty_EncryptionKeyIndex;
	
/*!
	@constant	kAPAdvertiserInfoProperty_EncryptionTypes
	@abstract	Required property of type CFNumber (32-bit): Supported encryption types (e.g. "0,1" for none and AES).
*/
extern const CFStringRef kAPAdvertiserInfoProperty_EncryptionTypes;
	
/*!
	@constant	kAPAdvertiserInfoProperty_TransportTypes
	@abstract	Required property of type CFString: Comma-separated list of supported audio transports (e.g. "TCP,UDP").
*/
extern const CFStringRef kAPAdvertiserInfoProperty_TransportTypes;
	
/*!
	@constant	kAPAdvertiserInfoProperty_AirPlayVersion
	@abstract	Optional property of type CFString: Source version string (e.g. "101.7").
*/
extern const CFStringRef kAPAdvertiserInfoProperty_AirPlayVersion;

/*!
	@constant	kAPAdvertiserInfoProperty_Features
	@abstract	Optional property of type CFNumber (64-bit): Supported features.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_Features;
	
/*!
	@constant	kAPAdvertiserInfoProperty_FirmwareVersion
	@abstract	Optional property of type CFString: Firmware Source Version (e.g. 1.2.3).
*/
extern const CFStringRef kAPAdvertiserInfoProperty_FirmwareVersion;
	
/*!
	@constant	kAPAdvertiserInfoProperty_DeviceModel
	@abstract	Optional property of type CFString: Model of device (e.g. Device1,1). MUST be globally unique.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_DeviceModel;
	
/*!
	@constant	kAPAdvertiserInfoProperty_SystemFlags
	@abstract	Optional property of type CFNumber (32-bit): System flags.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_SystemFlags;
	
/*!
	@constant	kAPAdvertiserInfoProperty_PasswordRequired
	@abstract	Optional property of type CFBoolean: true=Password required. missing/0=Password not required.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_PasswordRequired;
	
/*!
	@constant	kAPAdvertiserInfoProperty_PINRequired
	@abstract	Optional property of type CFBoolean: "true" if PIN required.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_PINRequired;
	
/*!
	@constant	kAPAdvertiserInfoProperty_ProtocolVersion
	@abstract	Optional property of type CFString: Protocol version string (e.g. "1.0"). Missing means 1.0.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_ProtocolVersion;

/*!
	@constant	kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity
	@abstract	Optional property of type CFString: Hex 32-byte HomeKit pairing identity.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity;	
	
/*!
	@constant	kAPAdvertiserInfoProperty_PublicKey
	@abstract	Optional property of type CFString: Hex 32-byte Ed25519 public key.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_PublicKey;
	
/*!
	@constant	kAPAdvertiserInfoProperty_RFC2617DigestAuthSupported
	@abstract	Optional property of type CFBoolean: "true" if device supports RFC-2617-style digest authentication.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_RFC2617DigestAuthSupported;
	
/*!
	@constant	kAPAdvertiserInfoProperty_MetadataTypes
	@abstract	Optional property of type CFNumber (32-bit): Supported meta data types (e.g. "0,1" for text and graphics). 
				AirTunes 2.1 and later.
*/
extern const CFStringRef kAPAdvertiserInfoProperty_MetadataTypes;

/*!
	@typedef APAdvertiserInfoRef
	A reference to advertiser info, a CF object that adheres to retain/release semantics. When CFRelease() is performed
	on the last reference to advertiser info, advertiser info deallocated.
*/
typedef struct OpaqueAPAdvertiserInfo *	APAdvertiserInfoRef;

/*!
	@function	APAdvertiserInfoGetTypeID
	
	@abstract	Returns type ID of the advertiser info object.
	
	@result		Type ID of the advertiser info object.
*/
CFTypeID APAdvertiserInfoGetTypeID( void );
	
/*!
	@function	APAdvertiserInfoCreate
	
	@abstract	Creates an advertiser info object.
	@discussion	

	@param		inAllocator			The allocator to use for allocating advertiser info object. Pass
									kCFAllocatorDefault to use the default allocator.
	@param		outAdvertiserInfo	Receives a newly created advertiser info object with retain count of 1.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoCreate( CFAllocatorRef inAllocator, APAdvertiserInfoRef *outAdvertiserInfo );
	
/*!
	@function	APAdvertiserInfoCopyProperty

	@abstract	Delievers a copy of the specified property that is currently set on advertiser info.
	@discussion	This method can be used to get a copy of the property currently set on the advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to copy the property from.
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
	APAdvertiserInfoCopyProperty(
		APAdvertiserInfoRef inAdvertiserInfo,
		CFStringRef inProperty,
		CFAllocatorRef inAllocator,
		void *outValue );
	
/*!
	@function	APAdvertiserInfoSetProperty

	@abstract	Sets the specified property on advertiser info.
	@discussion	This method can be used to set a property on the advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to set the property on.
	
	@param		inProperty				Name of the property to be copied. This paramater must be one of the predefined
										property keys above.
	@param		inValue					Value of the parameter that needs to be set.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise. If an 
				unrecognized property name is passed the method will fail with error code kParamErr.
*/
OSStatus APAdvertiserInfoSetProperty( APAdvertiserInfoRef inAdvertiserInfo, CFStringRef inProperty, CFTypeRef inValue );

/*!
	@function	APAdvertiserInfoCopyAirPlayData

	@abstract	Copies _airplay TXT record data.
	@discussion	This method can be used to obtain a copy of the _airplay TXT record data corresponding to the data in the
				advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to copy _airplay TXT record data from.
	@param		outData					Receives _airplay TXT record data corresponding to the data in advertiser info.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoCopyAirPlayData( APAdvertiserInfoRef inAdvertiserInfo, CFDataRef *outData );

/*!
	@function	APAdvertiserInfoCopyRAOPData

	@abstract	Copies _raop TXT record data.
	@discussion	This method can be used to obtain a copy of the _raop TXT record data data corresponding to the data in the
				advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to copy _raop TXT record data from.
	@param		outData					Receives _raop TXT record data corresponding to the data in advertiser info.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoCopyRAOPData( APAdvertiserInfoRef inAdvertiserInfo, CFDataRef *outData );

/*!
	@function	APAdvertiserInfoCreateAirPlayServiceName

	@abstract	Creates _airplay service name.
	@discussion	Creates _airplay service name corresponding to the advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to copy _airplay TXT record data from.
	@param		outServiceName			Receives _airplay service name.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoCreateAirPlayServiceName( APAdvertiserInfoRef inAdvertiserInfo, CFStringRef *outServiceName );

/*!
	@function	APAdvertiserInfoCopy

	@abstract	Copies the advertiser info object.
	@discussion

	@param		inAllocator				The allocator to use for allocating the copy of the property. Pass
										kCFAllocatorDefault to use the default allocator.
	@param		inAdvertiserInfo		Advertiser info object to be copied.
	@param		outAdvertiserInfoCopy	Receives a copy of the advertiser info object.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus
	APAdvertiserInfoCopy(
		CFAllocatorRef inAllocator,
		APAdvertiserInfoRef inAdvertiserInfo,
		APAdvertiserInfoRef *outAdvertiserInfoCopy );

/*!
	@function	APAdvertiserInfoCreateRAOPServiceName

	@abstract	Creates _raop service name.
	@discussion	Creates _raop service name corresponding to the advertiser info object.

	@param		inAdvertiserInfo		Advertiser info object to copy _raop TXT record data from.
	@param		outServiceName			Receives _raop service name.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoCreateRAOPServiceName( APAdvertiserInfoRef inAdvertiserInfo, CFStringRef *outServiceName );

/*!
	@function	APAdvertiserInfoDebugShow

	@abstract	Prints advertiser info state information in the data buffer for debug purposes.
	@discussion	Use this method to print advertiser info state information in the data buffer for debug purposes.

	@param		inAdvertiserInfo		Advertiser info object we need debug state information on.
	@param		inVerbose				If non-zero, more verbose state information will be printed.
	@param		inDataBuf				DataBuffer object where state information will be printed.

	@result		Returns kNoErr if successful and an error code that specifies the reason for failure otherwise.
*/
OSStatus APAdvertiserInfoDebugShow( APAdvertiserInfoRef inAdvertiserInfo, int inVerbose, DataBuffer *inDataBuf );

#ifdef __cplusplus
}
#endif

#endif /* defined(APADVERTISERINFO_H) */
