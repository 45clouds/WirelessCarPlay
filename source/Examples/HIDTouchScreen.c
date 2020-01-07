/*
	File:    	HIDTouchScreen.c
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

#include "HIDTouchScreen.h"

//===========================================================================================================================
//	HIDTouchScreenSingleCreateDescriptor
//===========================================================================================================================

OSStatus	HIDTouchScreenSingleCreateDescriptor( uint8_t **outDescriptor, size_t *outLen, uint16_t width, uint16_t height )
{
	const uint8_t kDescriptorTemplate[] = 
	{
		0x05, 0x0D,			// Usage Page (Digitizer)
		0x09, 0x04, 		// Usage (Touch Screen)
		0xA1, 0x01, 		// Collection (Application)
		
		0x05, 0x0D, 			// Usage Page (Digitizer)
		0x09, 0x22, 			// Usage (Finger)
		0xA1, 0x02, 			// Collection (Logical)
		// Finger
		0x05, 0x0D, 				// Usage Page (Digitizer)
		0x09, 0x33, 				// Usage (Touch)
		0x15, 0x00,					// Logical Minimum (0)
		0x25, 0x01,					// Logical Maximum (1)
		0x75, 0x01,					// Report Size (1)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		
		// Constant
		0x75, 0x07,					// Report Size (7)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x01,					// Input (Constant)
		
		// X Y
		0x05, 0x01,					// Usage Page (Generic Desktop)
		0x09, 0x30,					// Usage (X)
		0x15, 0x00,					// Logical Minimum (0) 
		0x26, 0xff, 0x7f,			// Logical Maximum (width)
		0x75, 0x10,					// Report Size (16) 
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0x09, 0x31,					// Usage (Y)
		0x15, 0x00,					// Logical Minimum (0) 
		0x26, 0xff, 0x7f,			// Logical Maximum (height)
		0x75, 0x10,					// Report Size (16) 
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0xC0, 					// End Collection
		0xC0 				// End Collection
	};
	
	OSStatus		err;
	size_t			len;
	uint8_t *		desc;
	
	len = sizeof( kDescriptorTemplate );
	desc = (uint8_t *) malloc( len );
	require_action( desc, exit, err = kNoMemoryErr );
	memcpy( desc, kDescriptorTemplate, len );
	desc[0x27] = width;
	desc[0x28] = width >> 8;
	desc[0x34] = height;
	desc[0x35] = height >> 8;
	*outDescriptor = desc;
	*outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HIDTouchScreenFillReport
//===========================================================================================================================

void	HIDTouchScreenFillReport( uint8_t inReport[ 5 ], uint8_t inButtons, uint16_t inX, uint16_t inY )
{
	inReport[ 0 ] = inButtons;
	inReport[ 1 ] = inX;
	inReport[ 2 ] = inX >> 8;
	inReport[ 3 ] = inY;
	inReport[ 4 ] = inY >> 8;
}

//===========================================================================================================================
//	HIDTouchScreenMultiCreateDescriptor
//===========================================================================================================================

OSStatus	HIDTouchScreenMultiCreateDescriptor( uint8_t **outDescriptor, size_t *outLen, uint16_t width, uint16_t height )
{
	const uint8_t kDescriptorTemplate[] =
	{
		0x05, 0x0D,			// Usage Page (Digitizer)
		0x09, 0x04, 		// Usage (Touch Screen)
		0xA1, 0x01, 		// Collection (Application)
		
		0x05, 0x0D, 			// Usage Page (Digitizer)
		0x09, 0x22, 			// Usage (Finger)
		0xA1, 0x02, 			// Collection (Logical)
		// Finger
		0x05, 0x0D, 				// Usage Page (Digitizer)
		0x09, 0x38, 				// Usage (Transducer Index)
		0x75, 0x08, 				// Report Size (8)
		0x95, 0x01, 				// Report Count (1)
		0x81, 0x02, 				// Input (Data, Variable, Absolute)
		0x09, 0x33, 				// Usage (Touch)
		0x15, 0x00,					// Logical Minimum (0)
		0x25, 0x01,					// Logical Maximum (1)
		0x75, 0x01,					// Report Size (1)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		
		// Constant
		0x75, 0x07,					// Report Size (7)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x01,					// Input (Constant)
		
		// X Y
		0x05, 0x01,					// Usage Page (Generic Desktop)
		0x09, 0x30,					// Usage (X)
		0x15, 0x00,					// Logical Minimum (0)
		0x26, 0xff, 0x7f,			// Logical Maximum (width)
		0x75, 0x10,					// Report Size (16)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0x09, 0x31,					// Usage (Y)
		0x15, 0x00,					// Logical Minimum (0)
		0x26, 0xff, 0x7f,			// Logical Maximum (height)
		0x75, 0x10,					// Report Size (16)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0xC0, 					// End Collection

		0x05, 0x0D, 			// Usage Page (Digitizer)
		0x09, 0x22, 			// Usage (Finger)
		0xA1, 0x02, 			// Collection (Logical)
		// Finger
		0x05, 0x0D, 				// Usage Page (Digitizer)
		0x09, 0x38, 				// Usage (Transducer Index)
		0x75, 0x08, 				// Report Size (8)
		0x95, 0x01, 				// Report Count (1)
		0x81, 0x02, 				// Input (Data, Variable, Absolute)
		0x09, 0x33, 				// Usage (Touch)
		0x15, 0x00,					// Logical Minimum (0)
		0x25, 0x01,					// Logical Maximum (1)
		0x75, 0x01,					// Report Size (1)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		
		// Constant
		0x75, 0x07,					// Report Size (7)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x01,					// Input (Constant)
		
		// X Y
		0x05, 0x01,					// Usage Page (Generic Desktop)
		0x09, 0x30,					// Usage (X)
		0x15, 0x00,					// Logical Minimum (0)
		0x26, 0xff, 0x7f,			// Logical Maximum (width)
		0x75, 0x10,					// Report Size (16)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0x09, 0x31,					// Usage (Y)
		0x15, 0x00,					// Logical Minimum (0)
		0x26, 0xff, 0x7f,			// Logical Maximum (height)
		0x75, 0x10,					// Report Size (16)
		0x95, 0x01,					// Report Count (1)
		0x81, 0x02,					// Input (Data, Variable, Absolute)
		0xC0, 					// End Collection
		0xC0 				// End Collection
	};
	
	OSStatus		err;
	size_t			len;
	uint8_t *		desc;
	
	len = sizeof( kDescriptorTemplate );
	desc = (uint8_t *) malloc( len );
	require_action( desc, exit, err = kNoMemoryErr );
	memcpy( desc, kDescriptorTemplate, len );
	desc[0x2F] = width;
	desc[0x30] = width >> 8;
	desc[0x3C] = height;
	desc[0x3D] = height >> 8;
	desc[0x6E] = width;
	desc[0x6F] = width >> 8;
	desc[0x7B] = height;
	desc[0x7C] = height >> 8;
	*outDescriptor = desc;
	*outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HIDTouchScreenMultiFillReport
//===========================================================================================================================

void	HIDTouchScreenMultiFillReport( uint8_t inReport[ 12 ], uint8_t inTransducer1, uint8_t inButtons1, uint16_t inX1, uint16_t inY1, uint8_t inTransducer2, uint8_t inButtons2, uint16_t inX2, uint16_t inY2 )
{
	inReport[ 0 ] = inTransducer1;
	inReport[ 1 ] = inButtons1;
	inReport[ 2 ] = inX1;
	inReport[ 3 ] = inX1 >> 8;
	inReport[ 4 ] = inY1;
	inReport[ 5 ] = inY1 >> 8;
	inReport[ 6 ] = inTransducer2;
	inReport[ 7 ] = inButtons2;
	inReport[ 8 ] = inX2;
	inReport[ 9 ] = inX2 >> 8;
	inReport[ 10 ] = inY2;
	inReport[ 11 ] = inY2 >> 8;
}
