/*
	File:    	AudioConverterStub.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	320.17
	
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

//=================================================================================================================================
// https://developer.apple.com/library/prerelease/ios/documentation/MusicAudio/Reference/AudioConverterServicesReference/index.html
//=================================================================================================================================

#include "AudioConverter.h"
#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================
typedef struct AudioConverterPrivate * AudioConverterPrivateRef;
struct AudioConverterPrivate
{
	uint32_t	sourceFormatID;
	uint32_t	destFormatID;
	uint32_t	sampleRate;
	uint32_t	channels;
	uint32_t	framesPerPacket;
	void		*nativeCodecRef;
};

//===========================================================================================================================
//	AudioConverterNew
//===========================================================================================================================

OSStatus AudioConverterNew( const AudioStreamBasicDescription *	inSourceFormat, const AudioStreamBasicDescription *	inDestinationFormat, AudioConverterRef * outAudioConverter )
{
	OSStatus						err;
	AudioConverterPrivateRef		me;

	// Sample rate conversion and mixing are not supported
	if( inDestinationFormat->mSampleRate != inSourceFormat->mSampleRate )
		return kUnsupportedErr;
	if( inDestinationFormat->mChannelsPerFrame != inSourceFormat->mChannelsPerFrame )
		return kUnsupportedErr;
	
	me = (AudioConverterPrivateRef) calloc( 1, sizeof( *me ) );
	require_action( me, exit, err = kNoMemoryErr );

	// $$$ TODO: The accessory will need to implement support for:
	// AAC LC decode
	// Opus encode/decode or AAC ELD encode/decode
	// Parameters are provided here to initialize the codec

	me->sourceFormatID = inSourceFormat->mFormatID;
	me->destFormatID = inDestinationFormat->mFormatID;
	me->sampleRate = inDestinationFormat->mSampleRate;
	me->channels = inDestinationFormat->mChannelsPerFrame;
	me->framesPerPacket = inDestinationFormat->mFramesPerPacket;

	switch( me->sourceFormatID )
	{
		case kAudioFormatMPEG4AAC:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			// $$$ TODO: Initialize codec for AAC LC -> PCM decompression
			err = kNoErr;
			break;

		case kAudioFormatMPEG4AAC_ELD:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			// $$$ TODO: Initialize codec for AAC ELD -> PCM decompression
			err = kNoErr;
			break;

		case kAudioFormatOpus:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			// $$$ TODO: Initialize codec for Opus -> PCM decompression
			err = kNoErr;
			break;

		case kAudioFormatLinearPCM:
			if( inDestinationFormat->mFormatID == kAudioFormatMPEG4AAC_ELD )
			{
				// $$$ TODO: Initialize codec for PCM -> AAC ELD compression
				err = kNoErr;
				goto exit;
			}
			else if( inDestinationFormat->mFormatID == kAudioFormatOpus )
			{
				// $$$ TODO: Initialize codec for PCM -> Opus compression
				err = kNoErr;
				break;
			}
		default:
			err = kUnsupportedErr;
			goto exit;
	}
	*outAudioConverter = me;

exit:
	if( me ) AudioConverterDispose( (AudioConverterRef) me );
	return( err );
}

//===========================================================================================================================
//	AudioConverterDispose
//===========================================================================================================================

OSStatus AudioConverterDispose( AudioConverterRef inConverter )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;

	// $$$ TODO: Last chance to free any codec resources for this stream.

	if( me->nativeCodecRef )
	{
		switch( me->sourceFormatID )
		{
			case kAudioFormatMPEG4AAC:
				// $$$ TODO: Free any resources for the AAC LC decoder
				break;

			case kAudioFormatMPEG4AAC_ELD:
				// $$$ TODO: Free any resources for the AAC ELD decoder
				break;

			case kAudioFormatOpus:
				// $$$ TODO: Free any resources for the Opus decoder
				break;

			case kAudioFormatLinearPCM:
				if( me->destFormatID == kAudioFormatMPEG4AAC_ELD )
				{
				// $$$ TODO: Free any resources for the AAC ELD encoder
				}
				else if( me->destFormatID == kAudioFormatOpus )
				{
				// $$$ TODO: Free any resources for the Opus encoder
				}
				break;
		}
	}
	free( me );
	return( kNoErr );
}

//===========================================================================================================================
//	AudioConverterReset
//===========================================================================================================================

OSStatus AudioConverterReset( AudioConverterRef inConverter )
{
	(void) inConverter;
	
	// $$$ TODO: Discard any data buffered by the codec
	return( kNoErr );
}

static OSStatus _AudioConverterSetPropertyAACDecode( AudioConverterPrivateRef const me, AudioConverterPropertyID inPropertyID, uint32_t inSize, const void * inData )
{
	(void)inSize;
	(void)inData;
	(void)me;

	switch ( inPropertyID )
	{
		default:
			return kUnsupportedErr;
	}
	return kNoErr;
}

static OSStatus _AudioConverterSetPropertyAACELDDecode( AudioConverterPrivateRef const me, AudioConverterPropertyID	inPropertyID, uint32_t inSize, const void * inData )
{
	(void)me;
	(void)inSize;
	(void)inData;
	
	switch ( inPropertyID )
	{
		default:
			return kUnsupportedErr;
	}
}

static OSStatus _AudioConverterSetPropertyOpusDecode( AudioConverterPrivateRef const me, AudioConverterPropertyID	inPropertyID, uint32_t inSize, const void * inData )
{
	(void)me;
	(void)inSize;
	(void)inData;
	
	switch ( inPropertyID )
	{
		default:
			return kUnsupportedErr;
	}
}


static OSStatus _AudioConverterSetPropertyAACELDEncode( AudioConverterPrivateRef const me, AudioConverterPropertyID	inPropertyID, uint32_t inSize, const void * inData )
{
	(void)me;
	(void)inSize;
	(void)inData;
	
	switch ( inPropertyID )
	{
		case kAudioCodecPropertyPacketSizeLimitForVBR:
		{
			if( inSize != sizeof( uint32_t ) )
				return kSizeErr;

			// $$$ TODO: Set up encoder properties
			return kNoErr;
		}
		default:
			return kUnsupportedErr;
	}
}

static OSStatus _AudioConverterSetPropertyOpusEncode( AudioConverterPrivateRef const me, AudioConverterPropertyID inPropertyID, uint32_t inSize, const void * inData )
{
	(void)me;
	(void)inSize;
	(void)inData;
	
	switch ( inPropertyID )
	{
		case kAudioCodecPropertyPacketSizeLimitForVBR:
		{
			if( inSize != sizeof( uint32_t ) )
				return kSizeErr;

			// $$$ TODO: Set up encoder properties
			return kNoErr;
		}
		default:
			return kUnsupportedErr;
	}
}

//===========================================================================================================================
//	AudioConverterSetProperty
//===========================================================================================================================

OSStatus AudioConverterSetProperty( AudioConverterRef inConverter, AudioConverterPropertyID	inPropertyID, uint32_t inSize, const void * inData )
{
	AudioConverterPrivateRef const		me = (AudioConverterPrivateRef) inConverter;
	
	if( !me->nativeCodecRef )
		return kStateErr;
	switch( me->sourceFormatID )
	{
		case kAudioFormatMPEG4AAC:
			return _AudioConverterSetPropertyAACDecode( me, inPropertyID, inSize, inData );

		case kAudioFormatMPEG4AAC_ELD:
			return _AudioConverterSetPropertyAACELDDecode( me, inPropertyID, inSize, inData );

		case kAudioFormatOpus:
			return _AudioConverterSetPropertyOpusDecode( me, inPropertyID, inSize, inData );

		case kAudioFormatLinearPCM:
			if( me->destFormatID == kAudioFormatMPEG4AAC_ELD )
				return _AudioConverterSetPropertyAACELDEncode( me, inPropertyID, inSize, inData );
			else if( me->destFormatID == kAudioFormatOpus )
				return _AudioConverterSetPropertyOpusEncode( me, inPropertyID, inSize, inData );

		default:
			return kUnsupportedErr;
	}
}

static OSStatus _AudioConverterFillComplexBufferAACDecode( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void * inInputDataProcUserData, uint32_t * ioOutputDataPacketSize, AudioBufferList * outOutputData, AudioStreamPacketDescription * outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;
	OSStatus							err;
	
	(void)outPacketDescription;
	(void)me;

	AudioBufferList						bufferList;
	uint32_t							packetCount;
	AudioStreamPacketDescription *		packetDesc;
	
	// $$$ TODO: Callback will provide number of frames per packet in ioOutputDataPacketSize
	if( *ioOutputDataPacketSize < kAudioSamplesPerPacket_AAC_LC )
	{
		return kSizeErr;
	}

	packetCount = 1;
	packetDesc  = NULL;
	// $$$ TODO: Request 1 packet of AAC LC through callback to decode kAudioSamplesPerPacket_AAC_LC of output.
	// The codec is responsible for consuming all bytes provided.
	err = inInputDataProc( inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData );
	require_noerr_quiet( err, exit );

	// $$$ TODO: Push AAC LC input into decoder and return PCM data into supplied buffer
	// input parameters: bufferList.mBuffers[ 0 ].mData, bufferList.mBuffers[ 0 ].mDataByteSize
	// output parameters: outOutputData->mBuffers[ 0 ].mData, outOutputData->mBuffers[ 0 ].mDataByteSize

	if( err == kNoErr && outPacketDescription )
	{
		outPacketDescription[ 0 ].mStartOffset = 0;
		outPacketDescription[ 0 ].mVariableFramesInPacket = 0;
		outPacketDescription[ 0 ].mDataByteSize = outOutputData->mBuffers[ 0 ].mDataByteSize;
		err = kNoErr;
	}

	// $$$ TODO: Set the number of samples produced
	*ioOutputDataPacketSize = kAudioSamplesPerPacket_AAC_LC;
exit:
	return err;
}

static OSStatus _AudioConverterFillComplexBufferAACELDDecode( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void * inInputDataProcUserData, uint32_t * ioOutputDataPacketSize, AudioBufferList * outOutputData, AudioStreamPacketDescription * outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;
	OSStatus							err;
	
	(void)outPacketDescription;
	(void)me;

	AudioBufferList						bufferList;
	uint32_t							packetCount;
	AudioStreamPacketDescription *		packetDesc;

	// $$$ TODO: Callback will provide number of frames per packet in ioOutputDataPacketSize
	if( *ioOutputDataPacketSize < kAudioSamplesPerPacket_AAC_ELD )
		return kSizeErr;

	packetCount = 1;
	packetDesc  = NULL;
	// $$$ TODO: Request 1 packet of AAC ELD through callback to decode kAudioSamplesPerPacket_AAC_ELD of output.
	// The codec is responsible for consuming all bytes provided.
	err = inInputDataProc( inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData );
	require_noerr_quiet( err, exit );

	// $$$ TODO: Push AAC ELD input into decoder and return PCM data into supplied buffer
	// input parameters: bufferList.mBuffers[ 0 ].mData, bufferList.mBuffers[ 0 ].mDataByteSize
	// output parameters: outOutputData->mBuffers[ 0 ].mData, outOutputData->mBuffers[ 0 ].mDataByteSize

	if( err == kNoErr && outPacketDescription )
	{
		outPacketDescription[ 0 ].mStartOffset = 0;
		outPacketDescription[ 0 ].mVariableFramesInPacket = 0;
		outPacketDescription[ 0 ].mDataByteSize = outOutputData->mBuffers[ 0 ].mDataByteSize;
		err = kNoErr;
	}

	// $$$ TODO: Set the number of samples produced
	*ioOutputDataPacketSize = kAudioSamplesPerPacket_AAC_ELD;
exit:
	return err;
}

static OSStatus _AudioConverterFillComplexBufferOpusDecode( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void * inInputDataProcUserData, uint32_t * ioOutputDataPacketSize, AudioBufferList * outOutputData, AudioStreamPacketDescription * outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;
	OSStatus							err;
	
	AudioBufferList						bufferList;
	uint32_t							packetCount;
	AudioStreamPacketDescription *		packetDesc;

	(void)outPacketDescription;
	(void)ioOutputDataPacketSize;
	(void)outOutputData;
	(void)me;

	// $$$ TODO: Callback will provide number of frames per packet in ioOutputDataPacketSize
	bufferList.mNumberBuffers = 1;
	packetCount = 1;
	packetDesc  = NULL;

	// $$$ TODO: Request 1 packet of OPUS through callback to decode 20 ms of output.
	// The codec is responsible for consuming all bytes provided.
	err = inInputDataProc( inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData );
	require_noerr_quiet( err, exit );

	// $$$ TODO: Push OPUS input into decoder and return PCM data into supplied buffer
	// input parameters: bufferList.mBuffers[ 0 ].mData, bufferList.mBuffers[ 0 ].mDataByteSize
	// output parameters: outOutputData->mBuffers[ 0 ].mData, outOutputData->mBuffers[ 0 ].mDataByteSize

	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		// outPacketDescription[ 0 ].mStartOffset = ;
		// outPacketDescription[ 0 ].mVariableFramesInPacket = 0;
		// outPacketDescription[ 0 ].mDataByteSize = ;
	}

	// $$$ TODO: Set the number of samples produced
	// *ioOutputDataPacketSize = ;
exit:
	return err;
}

static OSStatus _AudioConverterFillComplexBufferOpusEncode( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void * inInputDataProcUserData, uint32_t * ioOutputDataPacketSize, AudioBufferList * outOutputData, AudioStreamPacketDescription * outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;
	OSStatus							err = kNoErr;
	uint32_t							numPacket = 0;

	AudioBufferList						bufferList;
	uint32_t							packetCount;
	AudioStreamPacketDescription *		packetDesc;

	(void)inInputDataProc;
	(void)inInputDataProcUserData;
	(void)outPacketDescription;
	(void)ioOutputDataPacketSize;
	(void)outOutputData;
	(void)me;

	// $$$ TODO: Encode 20ms worth of samples per output packet
	bufferList.mNumberBuffers = 1;
	packetCount = me->framesPerPacket;
	packetDesc  = NULL;

	// $$$ TODO: Request number of frames (packetCount) to produce 1 packet representing 20 ms. The number
	// of frames provided will be returned in packetCount.  If the number of frames received is less  than
	// the requested amount, save the data internally and return no packets encoded in ioOutputDataPacketSize.
	err = inInputDataProc( inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData );
	require_noerr_quiet( err, exit );

	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		// outPacketDescription[ numPacket ].mStartOffset = ;
		// outPacketDescription[ numPacket ].mVariableFramesInPacket = 0;
		// outPacketDescription[ numPacket ].mDataByteSize = ;
	}

	// $$$ TODO: Set the number of packets encoded
	*ioOutputDataPacketSize = numPacket;
	
exit:
	return err;
}

static OSStatus _AudioConverterFillComplexBufferAACELDEncode( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void * inInputDataProcUserData, uint32_t * ioOutputDataPacketSize, AudioBufferList * outOutputData, AudioStreamPacketDescription * outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;
	OSStatus							err = kNoErr;
	uint32_t							numPacket = 0;

	AudioBufferList						bufferList;
	uint32_t							packetCount;
	AudioStreamPacketDescription *		packetDesc;

	(void)inInputDataProc;
	(void)inInputDataProcUserData;
	(void)outPacketDescription;
	(void)ioOutputDataPacketSize;
	(void)outOutputData;
	(void)me;

	bufferList.mNumberBuffers = 1;
	packetCount = me->framesPerPacket;
	packetDesc  = NULL;

	// $$$ TODO: Request number of frames (packetCount) to produce 1 AAC ELD packet.  The number
	// of frames provided will be returned in packetCount.  If the number of frames received is less than
	// the requested amount, save the data internally and return no packets encoded in ioOutputDataPacketSize.
	err = inInputDataProc( inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData );
	require_noerr_quiet( err, exit );

	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		// outPacketDescription[ numPacket ].mStartOffset = ;
		// outPacketDescription[ numPacket ].mVariableFramesInPacket = 0;
		// outPacketDescription[ numPacket ].mDataByteSize = ;
	}

	// $$$ TODO: Set the number of packets encoded
	*ioOutputDataPacketSize = numPacket;
	
exit:
	return err;
}

//===========================================================================================================================
//	AudioConverterFillComplexBuffer
//===========================================================================================================================

OSStatus AudioConverterFillComplexBuffer( AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc,
		void *								inInputDataProcUserData,
		uint32_t *							ioOutputDataPacketSize,
		AudioBufferList *					outOutputData,
		AudioStreamPacketDescription *		outPacketDescription )
{
	AudioConverterPrivateRef const me = (AudioConverterPrivateRef) inConverter;

	if( !me->nativeCodecRef )
		return kStateErr;
	
	switch ( me->sourceFormatID )
	{
		case kAudioFormatMPEG4AAC:
			// AAC LC to PCM
			return _AudioConverterFillComplexBufferAACDecode(  inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription );

		case kAudioFormatMPEG4AAC_ELD:
			// AAC ELD to PCM
			return _AudioConverterFillComplexBufferAACELDDecode( inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription );

		case kAudioFormatOpus:
			// Opus to PCM
			return _AudioConverterFillComplexBufferOpusDecode( inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription );

		case kAudioFormatLinearPCM:
			if( me->destFormatID == kAudioFormatMPEG4AAC_ELD )
			{
				// PCM to AAC ELD
				return _AudioConverterFillComplexBufferAACELDEncode( inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription );
			}
			else if( me->destFormatID == kAudioFormatOpus )
			{
				// PCM to AAC Opus
				return _AudioConverterFillComplexBufferOpusEncode( inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription );
			}
		default:
			return kUnsupportedErr;
	}
}
