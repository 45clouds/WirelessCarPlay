/*
	File:    	HIDTouchpad.c
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

#include "HIDTouchpad.h"

OSStatus HIDTouchpadCreateDescriptor( uint8_t **outDescriptor, size_t *outLen, uint16_t width, uint16_t height, uint16_t widthMM, uint16_t heightMM )
{
    static uint8_t gTouchpadPlusMultiCharacter[] = {
        0x05, 0x0D,                               // Usage Page (Digitizer)
        0x09, 0x05,                               // Usage (Touch Pad)
        0xA1, 0x01,                               // Collection (Application)
        0x05, 0x0D,                               //   Usage Page (Digitizer)
        0x09, 0x22,                               //   Usage (Finger)
        0xA1, 0x02,                               //   Collection (Logical)
        0x05, 0x0D,                               //     Usage Page (Digitizer)
        0x09, 0x33,                               //     Usage (Touch)
        0x15, 0x00,                               //     Logical Minimum......... (0)
        0x25, 0x01,                               //     Logical Maximum......... (1)
        0x75, 0x01,                               //     Report Size............. (1)
        0x95, 0x01,                               //     Report Count............ (1)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0x75, 0x07,                               //     Report Size............. (7)
        0x95, 0x01,                               //     Report Count............ (1)
        0x81, 0x01,                               //     Input...................(Constant)
        0x05, 0x01,                               //     Usage Page (Generic Desktop)
        0x09, 0x30,                               //     Usage (X)
        0x15, 0x00,                               //     Logical Minimum......... (0)
        0x26, 0xFF, 0xFF,                         //     Logical Maximum......... (width)
		0x35, 0x00,                               //     Physical Minimum........ (0)
		0x46, 0xFF, 0xFF,                         //     Physical Maximum........ (widthMM)
		0x55, 0x0F,                               //     Unit Exponent (-1)
		0x65, 0x11,                               //     Unit (cm)
        0x75, 0x10,                               //     Report Size............. (16)
        0x95, 0x01,                               //     Report Count............ (1)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0x09, 0x31,                               //     Usage (Y)
        0x15, 0x00,                               //     Logical Minimum......... (0)
        0x26, 0xFF, 0xFF,                         //     Logical Maximum......... (height)
		0x35, 0x00,                               //     Physical Minimum........ (0)
		0x46, 0xFF, 0xFF,                         //     Physical Maximum........ (heightMM)
		0x55, 0x0F,                               //     Unit Exponent (-1)
		0x65, 0x11,                               //     Unit (cm)
        0x75, 0x10,                               //     Report Size............. (16)
        0x95, 0x01,                               //     Report Count............ (1)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0xC0,                                     //   End Collection
        0x05, 0x0D,                               //   Usage Page (Digitizer)
        0x09, 0x24,                               //   Usage (Gesture Character)
        0xA1, 0x02,                               //   Collection (Logical)
        0x05, 0x0D,                               //     Usage Page (Digitizer)
        0x09, 0x63,                               //     Usage (Gesture Character Data)
        0x75, 0x20,                               //     Report Size............. (32)
        0x95, 0x01,                               //     Report Count............ (1)
        0x82, 0x02, 0x01,                         //     Input...................(Data, Variable, Absolute, Buffered bytes)
        0x09, 0x65,                               //     Usage (Gesture Character Encoding UTF8)
        0x09, 0x62,                               //     Usage (Gesture Character Data Length)
        0x75, 0x08,                               //     Report Size............. (8)
        0x95, 0x02,                               //     Report Count............ (2)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0x09, 0x61,                               //     Usage (Gesture Character Quality)
        0x15, 0x00,                               //     Logical Minimum......... (0)
        0x25, 0x64,                               //     Logical Maximum......... (100)
        0x75, 0x08,                               //     Report Size............. (8)
        0x95, 0x01,                               //     Report Count............ (1)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0xC0,                                     //   End Collection
        0x05, 0x0D,                               //   Usage Page (Digitizer)
        0x09, 0x24,                               //   Usage (Gesture Character)
        0xA1, 0x02,                               //   Collection (Logical)
        0x05, 0x0D,                               //     Usage Page (Digitizer)
        0x09, 0x63,                               //     Usage (Gesture Character Data)
        0x75, 0x20,                               //     Report Size............. (32)
        0x95, 0x01,                               //     Report Count............ (1)
        0x82, 0x02, 0x01,                         //     Input...................(Data, Variable, Absolute, Buffered bytes)
        0x09, 0x65,                               //     Usage (Gesture Character Encoding UTF8)
        0x09, 0x62,                               //     Usage (Gesture Character Data Length)
        0x75, 0x08,                               //     Report Size............. (8)
        0x95, 0x02,                               //     Report Count............ (2)
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
        0x09, 0x61,                               //     Usage (Gesture Character Quality)
        0x15, 0x00,                               //     Logical Minimum......... (0)
        0x25, 0x64,                               //     Logical Maximum......... (100)  
        0x75, 0x08,                               //     Report Size............. (8)  
        0x95, 0x01,                               //     Report Count............ (1)  
        0x81, 0x02,                               //     Input...................(Data, Variable, Absolute) 
        0xC0,                                     //   End Collection  
        0xC0                                      // End Collection
    };
    
    OSStatus                err;
    size_t                        len;
    uint8_t *                desc;
    
    len = sizeof( gTouchpadPlusMultiCharacter );
    desc = (uint8_t *) malloc( len );
    require_action( desc, exit, err = kNoMemoryErr );
    memcpy( desc, gTouchpadPlusMultiCharacter, len );
	desc[39] = width;
	desc[40] = width >> 8;
	desc[44] = widthMM;
	desc[45] = widthMM >> 8;
	desc[61] = height;
	desc[62] = height >> 8;
	desc[66] = heightMM;
	desc[67] = heightMM >> 8;
    
    *outDescriptor = desc;
    *outLen = len;
    err = kNoErr;
    
exit:
    return( err );
}

void	HIDTouchpadFillReport(
                              uint8_t inReport[ 19 ],
                              uint8_t character1Length,
                              uint8_t character1Data[4],
                              uint8_t character1Quality,
                              uint8_t character2Length,
                              uint8_t character2Data[4],
                              uint8_t character2Quality,
                              uint8_t tranducerState,
                              uint16_t tranducerX,
                              uint16_t tranducerY)
{
	inReport[0] = tranducerState;
	WriteLittle16( &inReport[1], tranducerX );
	WriteLittle16( &inReport[3], tranducerY );
	
	// first candidate
	memcpy( &inReport[5], character1Data, character1Length );
	memset( &inReport[5 + character1Length], 0, 4 - character1Length );
	inReport[9] = 1; // utf8 encoding
	inReport[10] = character1Length;
	inReport[11] = character1Quality;
	
	// second candidate
	memcpy( &inReport[12], character2Data, character2Length );
	memset( &inReport[12 + character2Length], 0, 4 - character2Length );
	inReport[16] = 1; // utf8 encoding
	inReport[17] = character2Length;
	inReport[18] = character2Quality;
}

