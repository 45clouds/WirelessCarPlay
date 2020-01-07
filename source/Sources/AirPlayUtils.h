/*
	File:    	AirPlayUtils.h
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayUtils_h_
#define	__AirPlayUtils_h_

#include "AirPlayCommon.h"

#include <CoreUtils/ChaCha20Poly1305.h>
#include <sys/queue.h>

	#include "APSAudioConverter.h"

#include CF_HEADER
#include COREAUDIO_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
// Constants
//===========================================================================================================================

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

OSStatus
	ASBDToAirPlayAudioFormat(
		const AudioStreamBasicDescription *	inASBD,
		AirPlayAudioFormat *				outFormat );
	
OSStatus
	AirPlayAudioFormatToASBD( 
		AirPlayAudioFormat				inFormat, 
		AudioStreamBasicDescription *	outASBD, 
		uint32_t *						outBitsPerChannel );

OSStatus	AirPlayAudioFormatToPCM( AirPlayAudioFormat inFormat, AudioStreamBasicDescription *outASBD );
	
AudioFormatID	AirPlayCompressionTypeToAudioFormatID( AirPlayCompressionType inCompressionType );
AirPlayCompressionType	AudioFormatIDToAirPlayCompressionType( AudioFormatID inFormatID );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayCreateModesDictionary
	@abstract	Creates dictionary for the initial modes or the params to a changeModes request.
	
	@param		inChanges	Changes being request. Initialize with AirPlayModeChangesInit() then set fields.
	@param		inReason	Optional reason for the change. Mainly for diagnostics. May be NULL.
	@param		outErr		Optional error code to indicate a reason for failure. May be NULL.
*/
CFDictionaryRef	AirPlayCreateModesDictionary( const AirPlayModeChanges *inChanges, CFStringRef inReason, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlay_DeriveAESKeySHA512
	@abstract	Derives a new pair of aesKey and aesIV for given master key using SHA-512.
	
	@param		inMasterKeyPtr	Master key.
	@param		inMasterKeyLen	Master key length.
	@param		inKeySaltPtr	Key salt.
	@param		inKeySaltLen	Key salt length.
	@param		inIVSaltPtr		IV salt.
	@param		inIVSaltLen		IV salt length.
	@param		outKey			16-byte place for derived key.
	@param		outIV			16-byte place for derived IV.
*/
void	AirPlay_DeriveAESKeySHA512(
		const void *		inMasterKeyPtr,
		size_t				inMasterKeyLen,
		const void *		inKeySaltPtr,
		size_t				inKeySaltLen,
		const void *		inIVSaltPtr,
		size_t				inIVSaltLen,
		uint8_t				outKey[ 16 ],
		uint8_t				outIV[ 16 ] );
		
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlay_DeriveAESKeySHA512ForScreen
	@abstract	Derives a new pair of aesKey and aesIV for given master key and screen stream connection ID using SHA-512.
	
	@param		inMasterKeyPtr				Master key.
	@param		inMasterKeyLen				Master key length.
	@param		inScreenStreamConnectionID	Should be unique within session.
	@param		outKey						16-byte place for derived key.
	@param		outIV						16-byte place for derived IV.
*/
void	AirPlay_DeriveAESKeySHA512ForScreen(
		const void *		inMasterKeyPtr,
		size_t				inMasterKeyLen,
		uint64_t			inScreenStreamConnectionID,
		uint8_t				outKey[ 16 ],
		uint8_t				outIV[ 16 ] );


//===========================================================================================================================
//	RTPJitterBuffer
//===========================================================================================================================

typedef struct RTPJitterBufferContext		RTPJitterBufferContext;
typedef struct RTPPacketNode				RTPPacketNode;

TAILQ_HEAD( RTPPacketNodeList, RTPPacketNode );
typedef struct RTPPacketNodeList RTPPacketNodeList;

struct RTPPacketNode
{
	TAILQ_ENTRY( RTPPacketNode )	list;
	RTPSavedPacket					pkt;			// Full RTP packet with header and payload.
	uint8_t *						ptr;			// Ptr to RTP payload. Note: this may not point to the beginning of the payload.
	dispatch_semaphore_t			decodeLock;		// Lock to protect node while decoding (no reading from / writing to node while decoding). 
	uint8_t *						decodeBuffer;	// Intermediate decode buffer.
	RTPJitterBufferContext *		jitterBuffer;	// Owning jitter buffer context.
};

struct RTPJitterBufferContext
{
	dispatch_semaphore_t			nodeLock;			// Lock to protect node lists.
	pthread_t						decodeThread;		// Thread to offload decoding work.
	pthread_t *						decodeThreadPtr;	// Ptr to decodeThread when valid.
	pthread_cond_t					decodeCondition;	// Condition to signal when decode work is ready.
	pthread_cond_t *				decodeConditionPtr;	// Ptr to decodeCondition when valid.
	pthread_mutex_t					decodeMutex;		// Mutex for signaling decodeCondition.
	pthread_mutex_t *				decodeMutexPtr;		// Ptr to decodeMutex when valid.
	Boolean							decodeDone;			// Sentinal for terminating decodeThread.
	RTPPacketNode *					packets;			// Backing store for all the packets.
	RTPPacketNodeList				freeList;			// List of free nodes.
	RTPPacketNodeList				preparedList;		// List of nodes prepared for reading, sorted by timestamp.
	RTPPacketNodeList				receivedList;		// List of nodes received (but not prepared), sorted by timestamp.
	uint32_t						nodesAllocated;		// Allocated size of the Jitter Buffer in nodes.
	uint32_t						nodesUsed;			// Depth of the Jitter Buffer in nodes.
	AudioStreamBasicDescription		inputFormat;		// Format of sample data in enqueued nodes.
	AudioStreamBasicDescription		outputFormat;		// Format of sample data read from buffer (must be PCM).
	AudioConverterRef				decoder;			// AudioConverter instance for decoding.
	AudioStreamPacketDescription	packetDescription;	// Description of encoded packet.
	uint8_t *						decodeBuffers;		// Backing store for all packet decode buffers.
	uint32_t						bufferMs;			// Milliseconds of audio to buffer before starting.
	uint32_t						nextTS;				// Next timestamp to read from.
	uint64_t						startTicks;			// Ticks when we should start playing audio.
	Boolean							disabled;			// True if all input data should be dropped.
	Boolean							buffering;			// True if we're buffering until the high watermark is reached.
	uint32_t						nLate;				// Number of times samples were dropped because of being late.
	uint32_t						nGaps;				// Number of times samples that were missing (e.g. lost packet).
	uint32_t						nSkipped;			// Number of times we had to skip samples (before timing window).
	uint32_t						nRebuffer;			// Number of times we had to re-buffer because we ran dry.
	const char *					label;				// Optional label for logging.
	dispatch_queue_t				logQueue;			// Queue to keep logging off of time critical threads / locks.
};

OSStatus
	RTPJitterBufferInit(
		RTPJitterBufferContext *				ctx,
		const AudioStreamBasicDescription *		inInputFormat,
		const AudioStreamBasicDescription *		inOutputFormat,
		uint32_t								inBufferMs );
void		RTPJitterBufferFree( RTPJitterBufferContext *ctx );
void		RTPJitterBufferReset( RTPJitterBufferContext *ctx, Float64 inDelta );
OSStatus	RTPJitterBufferGetFreeNode( RTPJitterBufferContext *ctx, RTPPacketNode **outNode );
void		RTPJitterBufferPutFreeNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode );
OSStatus	RTPJitterBufferPutBusyNode( RTPJitterBufferContext *ctx, RTPPacketNode *inNode );
OSStatus	RTPJitterBufferRead( RTPJitterBufferContext *ctx, void *inBuffer, size_t inLen );

//===========================================================================================================================
// ChaChaPoly encryption/decryption
//===========================================================================================================================

typedef struct
{
	Boolean							isValid;
	chacha20_poly1305_state			state;						// Encryption/Decryption context
	uint8_t							key[ 32 ];					// Key used to initialize encryption/decryption context.
	uint8_t							nonce[ 8 ];					// Packet counter.
	
}	ChaChaPolyCryptor;

#ifdef __cplusplus
}
#endif

#endif	// __AirPlayUtils_h_
