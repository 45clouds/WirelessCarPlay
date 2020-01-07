
/*
	File:    	HIDTelephony.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a
	Version: 	n/a
	
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
	
	Copyright (C) 2007-2016 Apple Inc. All Rights Reserved.
 */

#include "HIDTelephony.h"

OSStatus HIDTelephonyCreateDescriptor( uint8_t **outDescriptor, size_t *outLen )
{
	const uint8_t kDescriptorTemplate[] =
	{
		0x05, 0x0B, // Usage Page (Telephony)
		0x09, 0x07, // Usage (Telephony Keypad)
		0xA1, 0x01, // Collection (Application)
			0x15, 0x00, // Logical min
			0x25, 0x11, // logical max
			0x05, 0x0B, // Usage Page (Telephony)
			0x09, 0x00, // Usage (Unassigned)
			0x09, 0x20, // Usage (Hook Switch)
			0x09, 0x21, // Usage (Flash)
			0x09, 0x26, // Usage (Drop)
			0x09, 0x2F, // Usage (Mute)
			0x09, 0xB0, // Usage (PhoneKey 0)
			0x09, 0xB1, // Usage (PhoneKey 1)
			0x09, 0xB2, // Usage (PhoneKey 2)
			0x09, 0xB3, // Usage (PhoneKey 3)
			0x09, 0xB4, // Usage (PhoneKey 4)
			0x09, 0xB5, // Usage (PhoneKey 5)
			0x09, 0xB6, // Usage (PhoneKey 6)
			0x09, 0xB7, // Usage (PhoneKey 7)
			0x09, 0xB8, // Usage (PhoneKey 8)
			0x09, 0xB9, // Usage (PhoneKey 9)
			0x09, 0xBA, // Usage (PhoneKey Star)
			0x09, 0xBB, // Usage (PhoneKey Pound)
			0x05, 0x07, // Usage Page (Keyboard/Keypad)
			0x09, 0x2A, // Usage (Keyboard DELETE)
			
			0x75, 0x08, // Report Size (8)
			0x95, 0x01, // Report Count (1)
			0x81, 0x00, // Input (Data, Array, Absolute)
		0xC0 // End collection
	};
	
	OSStatus		err;
	size_t			len;
	uint8_t *		desc;
	
	len = sizeof( kDescriptorTemplate );
	desc = (uint8_t *) malloc( len );
	require_action( desc, exit, err = kNoMemoryErr );
	memcpy( desc, kDescriptorTemplate, len );
	
	*outDescriptor = desc;
	*outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

void HIDTelephonyFillReport( uint8_t inReport[ 1 ], unsigned buttonIndex )
{
	inReport[0] = buttonIndex;
}
