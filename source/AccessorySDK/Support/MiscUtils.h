/*
	File:    	MiscUtils.h
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
	
	Copyright (C) 2001-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef	__MiscUtils_h__
#define	__MiscUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <stdarg.h>
	#include <stddef.h>
	#include <stdlib.h>
#endif

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#include <IOKit/IOKitLib.h>
#endif
#if( TARGET_OS_POSIX )
	#include CF_HEADER
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MemReverse
	@abstract	Copies data from one block to another and reverses the order of bytes in the process.
	@discussion	"inSrc" may be the same as "inDst", but may not point to an arbitrary location inside it.
*/
void	MemReverse( const void *inSrc, size_t inLen, void *inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Swap16Mem
	@abstract	Endian swaps each 16-bit chunk of memory.
	@discussion	"inSrc" may be the same as "inDst", but may not point to an arbitrary location inside it.
*/
void	Swap16Mem( const void *inSrc, size_t inLen, void *inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetHomePath
	@abstract	Gets a path to the home directory for the current process.
*/
char *	GetHomePath( char *inBuffer, size_t inMaxLen );

#if( TARGET_OS_POSIX )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	mkpath
	@abstract	Creates a directory and all intermediate directories if they do not exist.
*/
int	mkpath( const char *path, mode_t mode, mode_t dir_mode );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NormalizePath
	@abstract	Normalizes a path to expand tidle's at the beginning and resolve ".", "..", and sym links.
	
	@param		inSrc		Source path to normalize.
	@param		inLen		Number of bytes in "inSrc" or kSizeCString if it's a NUL-terminated string.
	@param		inDst		Buffer to received normalized string. May be the same as "inSrc" to normalized in-place.
	@param		inMaxLen	Max number of bytes (including a NUL terminator) to write to "inDst".
	@param		inFlags		Flags to control the normalization process.
	
	@result		Ptr to inDst or "" if inMaxLen is 0.
	
	@discussion
	
	If the path is exactly "~" then expand to the current user's home directory.
	If the path is exactly "~user" then expand to "user"'s home directory.
	If the path begins with "~/" then expand the "~/" to the current user's home directory.
	If the path begins with "~user/" then expand the "~user/" to "user"'s home directory.
*/
#define kNormalizePathDontExpandTilde		( 1 << 0 ) // Don't replace "~" or "~user" with user home directory path.
#define kNormalizePathDontResolve			( 1 << 1 ) // Don't resolve ".", "..", or sym links.

char *	NormalizePath( const char *inSrc, size_t inLen, char *inDst, size_t inMaxLen, uint32_t inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetProcessNameByPID
	@abstract	Gets a process name from a PID.
*/
#if( TARGET_OS_POSIX )
	char *	GetProcessNameByPID( pid_t inPID, char *inNameBuf, size_t inMaxLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RemovePath
	@abstract	Removes a file or directory. If it's a directory, it recursively removes all items inside.
*/
OSStatus	RemovePath( const char *inPath );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RollLogFiles
	@abstract	Optionally closes the current log file, shifts existing files, and caps the log files to a max count.
*/
#if( TARGET_HAS_C_LIB_IO )
	OSStatus	RollLogFiles( FILE **ioLogFile, const char *inEndMessage, const char *inBaseName, int inMaxFiles );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RunProcessAndCaptureOutput
	@abstract	Runs a process specified via a command line and captures everything it writes to stdout.
*/
OSStatus	RunProcessAndCaptureOutput( const char *inCmdLine, char **outResponse );
OSStatus	RunProcessAndCaptureOutputEx( const char *inCmdLine, char **outResponse, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	systemf
	@abstract	Invokes a command line built using a format string.
	
	@param		inLogPrefix		Optional prefix to print before logging the command line. If NULL, no logging is performed.
	@param		inFormat		printf-style format string used to build the command line.
	
	@result		If the command line was executed, the exit status of it is returned. Otherwise, errno is returned.
*/
int	systemf( const char *inLogPrefix, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 2, 3 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	H264ConvertAVCCtoAnnexBHeader
	@abstract	Parses a H.264 AVCC atom and converts it to an Annex-B header and gets the nal_size for subsequent frames.
	
	@param	inHeaderBuf			Buffer to receive an Annex-B header containing the AVCC data. May be NULL to just find the size.
	@param	inHeaderMaxLen		Max length of inHeaderBuf. May be zero to just find the size.
	@param	outHeaderLen		Ptr to receive the length of the Annex-B header. May be NULL.
	@param	outNALSize			Ptr to receive the number of bytes in the nal_size field before each AVCC-style frame. May be NULL.
	@param	outNext				Ptr to receive pointer to byte following the last byte that was parsed. May be NULL.
*/
OSStatus
	H264ConvertAVCCtoAnnexBHeader( 
		const uint8_t *		inAVCCPtr,
		size_t				inAVCCLen,
		uint8_t *			inHeaderBuf,
		size_t				inHeaderMaxLen,
		size_t *			outHeaderLen,
		size_t *			outNALSize,
		const uint8_t **	outNext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	H264GetNextNALUnit
	@abstract	Parses a H.264 AVCC-style stream (NAL size-prefixed NAL units) and returns each piece of NAL unit data.
	
	@param	inSrc			Ptr to start of a single NAL unit. This must begin with a NAL size header.
	@param	inEnd			Ptr to end of the NAL unit.
	@param	outDataPtr		Ptr to data to NAL unit data (minus NAL size header).
	@param	outDataLen		Number of bytes of NAL unit data.
	@param	outSrc			Receives ptr to pass as inSrc for the next call.
*/
OSStatus
	H264GetNextNALUnit( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		size_t				inNALSize, 
		const uint8_t * *	outDataPtr, 
		size_t *			outDataLen,
		const uint8_t **	outSrc );

#if 0
#pragma mark -
#pragma mark == MirroredRingBuffer ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		MirroredRingBuffer
	@abstract	Structure for managing the ring buffer.
*/
typedef struct
{
	uint8_t *			buffer;			// Buffer backing the ring buffer.
	const uint8_t *		end;			// End of the buffer. Useful for check if a pointer is within the ring buffer.
	uint32_t			size;			// Max number of bytes the ring buffer can hold.
	uint32_t			mask;			// Mask for power-of-2 sized buffers to wrap without using a slower mod operator.
	uint32_t			readOffset;		// Current offset for reading. Don't access directly. Use macros.
	uint32_t			writeOffset;	// Current offset for writing. Don't access directly. Use macros.
#if( TARGET_OS_WINDOWS )
	HANDLE				mapFile;		// Handle to file mapping.
#endif
	
}	MirroredRingBuffer;

#define kMirroredRingBufferInit		{ NULL, NULL, 0, 0, 0, 0 }

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		MirroredRingBuffer init/free.
	@abstract	Initializes/frees a ring buffer.
*/
OSStatus	MirroredRingBufferInit( MirroredRingBuffer *inRing, size_t inMinSize, Boolean inPowerOf2 );
void		MirroredRingBufferFree( MirroredRingBuffer *inRing );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		MirroredRingBufferAccessors
	@abstract	Macros for accessing the ring buffer.
*/

// Macros for getting read/write pointers for power-of-2 sized ring buffers.
#define MirroredRingBufferGetReadPtr( RING )			( &(RING)->buffer[ (RING)->readOffset  & (RING)->mask ] )
#define MirroredRingBufferGetWritePtr( RING )			( &(RING)->buffer[ (RING)->writeOffset & (RING)->mask ] )

// Macros for getting read/write pointers for non-power-of-2 sized ring buffers.
#define MirroredRingBufferGetReadPtrSlow( RING )		( &(RING)->buffer[ (RING)->readOffset  % (RING)->size ] )
#define MirroredRingBufferGetWritePtrSlow( RING )		( &(RING)->buffer[ (RING)->writeOffset % (RING)->size ] )

#define MirroredRingBufferReadAdvance( RING, COUNT )	do { (RING)->readOffset  += (COUNT); } while( 0 )
#define MirroredRingBufferWriteAdvance( RING, COUNT )	do { (RING)->writeOffset += (COUNT); } while( 0 )
#define MirroredRingBufferReset( RING )					do { (RING)->readOffset = (RING)->writeOffset; } while( 0 )

#define MirroredRingBufferContainsPtr( RING, PTR )		( ( ( (const uint8_t *)(PTR) ) >= (RING)->buffer ) && \
														  ( ( (const uint8_t *)(PTR) ) <  (RING)->end ) )
#define MirroredRingBufferGetBytesUsed( RING )			( (uint32_t)( (RING)->writeOffset - (RING)->readOffset ) )
#define MirroredRingBufferGetBytesFree( RING )			( (RING)->size - MirroredRingBufferGetBytesUsed( RING ) )

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MiscUtilsTest
	@abstract	Unit test.
*/
OSStatus	MiscUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __MiscUtils_h__
