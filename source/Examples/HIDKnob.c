/*
	File:    	HIDKnob.c
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

#include "HIDKnob.h"

//===========================================================================================================================
//	HIDKnobCreateDescriptor
//===========================================================================================================================

OSStatus	HIDKnobCreateDescriptor( uint8_t **outDescriptor, size_t *outLen )
{
	static const uint8_t		kDescriptorTemplate[] =
	{
		0x05, 0x01, // Usage Page (Generic Desktop)
		0x09, 0x08, // Usage (MuultiAxisController)
		0xA1, 0x01, // Collection (Application)
			0x05, 0x09, // Usage Page (Button)
			0x09, 0x01, // Usage (Button 1 primary/trigger)
			0x15, 0x00, // Logical Minimum (0)
			0x25, 0x01, // Logical Maximum (1)
			0x75, 0x01, // Report Size (1)
			0x95, 0x01, // Report Count (1)
			0x81, 0x02, // Input (Data, Variable, Absolute)
			0x05, 0x0c, // Usage Page (Consumer)
			0x0a, 0x23, 0x02,// Usage (AC Home)
			0x0a, 0x24, 0x02,// Usage (AC Back)
			0x95, 0x02, // Report Count (2)
			0x81, 0x02, // Input (Data, Variable, Absolute)
			0x95, 0x05, // Report Size (5)
			0x81, 0x01, // Input (Constant)
			0x05, 0x01, // Usage Page (Generic Desktop)
			0x09, 0x01, // Usage (Pointer)
			0xA1, 0x00, // Collection (Physical)
				0x09, 0x30, // Usage (X)
				0x09, 0x31, // Usage (Y)
				0x15, 0x81, // Logical Minimum (-127)
				0x25, 0x7f, // Logical Maximum (127)
				0x75, 0x08, // Report Size (8)
				0x95, 0x02, // Report Count (2)
				0x81, 0x02, // Input (Data, Variable, Absolute)
			0xC0, // End Collection
			0x09, 0x38, // Usage (Wheel)
			0x15, 0x81, // Logical Minimum (-127)
			0x25, 0x7f, // Logical Maximum (127)
			0x75, 0x08, // Report Size (8)
			0x95, 0x01, // Report Count (1)
			0x81, 0x06, // Input (Data, Variable, Relative)
		0xC0    //End Collection
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

//===========================================================================================================================
//	HIDKnobFillReport
//===========================================================================================================================

void
HIDKnobFillReport(
				  uint8_t	inReport[ 4 ],
				  Boolean	inButton,
				  Boolean	inHome,
				  Boolean	inBack,
				  int8_t	inX,
				  int8_t	inY,
				  int8_t	inWheel )
{
	inReport[ 0 ] = (uint8_t)( inButton | ( inHome << 1 ) | ( inBack << 2 ) );
	inReport[ 1 ] = (uint8_t) inX;
	inReport[ 2 ] = (uint8_t) inY;
	inReport[ 3 ] = (uint8_t) inWheel;
}

