/*
	File:    	EasyConfigUtils.h
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
	
	Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__EasyConfigUtils_h__
#define	__EasyConfigUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		EasyConfigConstants
	@abstract	Constants for element IDs, keys, etc.
*/

// [String] Password used to change settings on the accessory (e.g. configure the network, etc.).
#define kEasyConfigKey_AdminPassword		"adminPassword"
#define kEasyConfigTLV_AdminPassword		0x00

// [String] Unique 10 character string assigned by Apple to an app via the Provisioning Portal (e.g. "24D4XFAF43").
#define kEasyConfigKey_BundleSeedID			"bundleSeedID"
#define kEasyConfigTLV_BundleSeedID			0x01

// [String] Firmware revision of the accessory.
#define kEasyConfigKey_FirmwareRevision		"firmwareRevision"
#define kEasyConfigTLV_FirmwareRevision		0x02

// [String] Hardware revision of the accessory.
#define kEasyConfigKey_HardwareRevision		"hardwareRevision"
#define kEasyConfigTLV_HardwareRevision		0x03

// [String] BCP-47 language to configure the device for. 
// See <http://www.iana.org/assignments/language-subtag-registry> for a full list.
#define kEasyConfigKey_Language				"language"
#define kEasyConfigTLV_Language				0x04

// [String] Manufacturer of the accessory (e.g. "Apple").
#define kEasyConfigKey_Manufacturer			"manufacturer"
#define kEasyConfigTLV_Manufacturer			0x05

// [Array] Array of reverse-DNS strings describing supported MFi accessory protocols (e.g. "com.acme.gadget").
#define kEasyConfigKey_MFiProtocols			"mfiProtocols"
#define kEasyConfigTLV_MFiProtocol			0x06 // Single MFi protocol string per element.

// [String] Model name of the device (e.g. Device1,1).
#define kEasyConfigKey_Model				"model"
#define kEasyConfigTLV_Model				0x07

// [String] Name that accessory should use to advertise itself (e.g. what shows up in iTunes for an AirPlay accessory).
#define kEasyConfigKey_Name					"name"
#define kEasyConfigTLV_Name					0x08

// [String] Password used to AirPlay to the accessory.
#define kEasyConfigKey_PlayPassword			"playPassword"
#define kEasyConfigTLV_PlayPassword			0x09

// [String] Serial number of the accessory.
#define kEasyConfigKey_SerialNumber			"serialNumber"
#define kEasyConfigTLV_SerialNumber			0x0A

// [Data] WiFi PSK for joining a WPA-protected WiFi network.
// If it's between 8 and 63 bytes each being 32-126 decimal, inclusive then it's a pre-hashed password.
// Otherwise, it's expected to be a pre-hashed, 256-bit pre-shared key.
#define kEasyConfigKey_WiFiPSK				"wifiPSK"
#define kEasyConfigTLV_WiFiPSK				0x0B

// [String] WiFi SSID (network name) for the accessory to join.
#define kEasyConfigKey_WiFiSSID				"wifiSSID"
#define kEasyConfigTLV_WiFiSSID				0x0C

// Descriptors string for use with PrintF %{tlv8} format specifier.

#define kEasyConfigTLVDescriptors \
	"\x00" "Admin Password\0" \
	"\x01" "Bundle Seed ID\0" \
	"\x02" "Firmware Revision\0" \
	"\x03" "Hardware Revision\0" \
	"\x04" "Language\0" \
	"\x05" "Manufacturer\0" \
	"\x06" "MFI Protocols\0" \
	"\x07" "Model\0" \
	"\x08" "Name\0" \
	"\x09" "Play Password\0" \
	"\x0A" "Serial Number\0" \
	"\x0B" "WiFi PSK\0" \
	"\x0C" "WiFi SSID\0" \
	"\x00"

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	EasyConfigCreateDictionaryFromTLV
	@abstract	Converts EasyConfig TLV8 entries into a dictionary. Caller must CFRelease if non-NULL.
*/
CF_RETURNS_RETAINED
CFDictionaryRef	EasyConfigCreateDictionaryFromTLV( const void *inTLVPtr, size_t inTLVLen, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	EasyConfigCreateTLVfromDictionary
	@abstract	Converts an EasyConfig dictionary into a packed array of TLV8 entries. Caller must free if non-NULL.
*/
uint8_t *	EasyConfigCreateTLVfromDictionary( CFDictionaryRef inDict, size_t *outLen, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	EasyConfigUtilsTest
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	EasyConfigUtilsTest( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __EasyConfigUtils_h__
