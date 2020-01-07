/*
	File:    	DataBufferUtils.h
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
	
	Copyright (C) 2008-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__DataBufferUtils_h__
#define	__DataBufferUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		DataBuffer
	@abstract	Dynamically sized buffer of data.
	@discussion
	
	You use a DataBuffer by calling DataBuffer_Init, zero or more DataBuffer_Append*, etc. functions to modify the
	DataBuffer, then use DataBuffer_Commit to finalize it for use via DataBuffer_GetPtr() and DataBuffer_GetLen().
*/
#define kDataBufferDefaultMaxSize		0x7FFFFFFF

typedef struct
{
	// WARNING: All fields are private. Use the API instead of touching them directly.
	
	uint8_t *		staticBufferPtr;	//! Optional static buffer to use if the content will fit. May be NULL.
	size_t			staticBufferLen;	//! Number of bytes the static buffer can hold.
	size_t			maxGrowLen;			//! Max number of bytes to allow it to grow to.
	uint8_t *		bufferPtr;			//! May be malloc'd or a static buffer.
	size_t			bufferLen;			//! Number of bytes in use.
	size_t			bufferMaxLen;		//! Number of bytes the current buffer can hold.
	uint8_t			malloced;			//! Non-zero if buffer was malloc'd. 0 if static or NULL.
	OSStatus		firstErr;			//! First error that occurred or kNoErr.
	
}	DataBuffer;

#define	DataBuffer_GetError( DB )		( DB )->firstErr
#define	DataBuffer_GetPtr( DB )			( DB )->bufferPtr
#define	DataBuffer_GetEnd( DB )			( DataBuffer_GetPtr( DB ) + DataBuffer_GetLen( DB ) )
#define	DataBuffer_GetLen( DB )			( DB )->bufferLen

void		DataBuffer_Init( DataBuffer *inDB, void *inStaticBufferPtr, size_t inStaticBufferLen, size_t inMaxGrowLen );
void		DataBuffer_Free( DataBuffer *inDB );
uint8_t *	DataBuffer_Disown( DataBuffer *inDB );
OSStatus	DataBuffer_Commit( DataBuffer *inDB, uint8_t **outPtr, size_t *outLen );
OSStatus	DataBuffer_Detach( DataBuffer *inDB, uint8_t **outPtr, size_t *outLen );
OSStatus	DataBuffer_DetachCString( DataBuffer *inDB, char **outStr );
#define		DataBuffer_Reset( DB )		do { ( DB )->bufferLen = 0; ( DB )->firstErr = 0; } while( 0 )

#define		DataBuffer_Insert( DB, OFFSET, PTR, LEN )	DataBuffer_Replace( DB, OFFSET, 0, PTR, LEN )
#define		DataBuffer_Remove( DB, OFFSET, LEN )		DataBuffer_Replace( DB, OFFSET, LEN, NULL, 0 )
OSStatus	DataBuffer_Replace( DataBuffer *inDB, size_t inOffset, size_t inOldLen, const void *inNewData, size_t inNewLen );

OSStatus	DataBuffer_Resize( DataBuffer *inDB, size_t inNewLen, void *outPtr );
#define		DataBuffer_Grow( DB, AMOUNT, OUT_PTR )	DataBuffer_Resize( DB, DataBuffer_GetLen( DB ) + ( AMOUNT ), OUT_PTR )
OSStatus	DataBuffer_Shrink( DataBuffer *inDB, size_t inAmount );

OSStatus	DataBuffer_Append( DataBuffer *inDB, const void *inData, size_t inLen );
OSStatus	DataBuffer_AppendF( DataBuffer *inDB, const char *inFormat, ... );
OSStatus	DataBuffer_AppendFNested( DataBuffer *inDB, const char *inTemplate, const char *inFormat, ... );
OSStatus	DataBuffer_AppendFVAList( DataBuffer *inDB, const char *inFormat, va_list inArgs );

#if( TARGET_OS_POSIX )
	OSStatus	DataBuffer_RunProcessAndAppendOutput( DataBuffer *inBuffer, const char *inCmdLine );
#endif
#if( TARGET_HAS_STD_C_LIB )
	OSStatus	DataBuffer_AppendANSIFile( DataBuffer *inBuffer, FILE *inFile );
	OSStatus	DataBuffer_AppendFile( DataBuffer *inBuffer, const char *inPath );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DataBufferUtils_Test
	@abstract	Unit test.
*/
OSStatus	DataBufferUtils_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __DataBufferUtils_h__
