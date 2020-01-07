/*
	File:    	APSAudioConverterStub.c
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

//=================================================================================================================================
// https://developer.apple.com/library/prerelease/ios/documentation/MusicAudio/Reference/AudioConverterServicesReference/index.html
//=================================================================================================================================

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>

#include "APSAudioConverter.h"
#include <glib.h>

#if WIRELESS_CARPLAY
#include <fdk-aac/FDK_audio.h>
#include <fdk-aac/aacdecoder_lib.h>
#include <opus/opus.h>
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

typedef struct AudioConverterPrivate * AudioConverterPrivateRef;
struct AudioConverterPrivate
{
	uint32_t sourceFormatID;
	uint32_t destFormatID;
	uint32_t sampleRate;
	uint32_t channels;
	uint32_t framesPerPacket;
	void *nativeCodecRef;

#if WIRELESS_CARPLAY
	uint8_t *inData;
	uint32_t inLen;
	uint8_t *outData;
	uint32_t outLen;
#if 0
	FILE *opusfile;
	FILE *pcmfile2;
	FILE *pcmfile;
	OpusDecoder *decoder_pcm;
#endif
	/* * FDK **/
	CStreamInfo *mStreamInfo;
#endif
};

#if WIRELESS_CARPLAY
//===========================================================================================================================
//	AACDecBuf
//===========================================================================================================================

static int AACDecBuf(AudioConverterPrivateRef me)
{
	HANDLE_AACDECODER mAACDecoder = (HANDLE_AACDECODER)me->nativeCodecRef;
	UCHAR* inBuffer[1];
	UINT inBufferLength[1] = {0};
	UINT bytesValid[1] = {0};

	inBuffer[0] = me->inData;
	inBufferLength[0] = me->inLen;
	bytesValid[0] = inBufferLength[0];

	aacDecoder_Fill( mAACDecoder, inBuffer, inBufferLength, bytesValid );

	AAC_DECODER_ERROR decoderErr = AAC_DEC_NOT_ENOUGH_BITS;
	decoderErr = aacDecoder_DecodeFrame( mAACDecoder,
			(INT_PCM *)me->outData,
			me->outLen,
			0 /*  flags */);
	if(decoderErr != 0){
		g_message("aacDecoder_DecodeFrame: ret:%X\n", decoderErr);              
	}
	else{
#if 0
		g_message("aacDecoder_DecodeFrame: OK[%d]\n",me->mStreamInfo->sampleRate);
		g_message("numTotalBytes:%d numBadBytes:%d numTotalAccessUnits:%d numBadAccessUnits %d\n", 
				me->mStreamInfo->numTotalBytes,
				me->mStreamInfo->numBadBytes,
				me->mStreamInfo->numTotalAccessUnits,
				me->mStreamInfo->numBadAccessUnits
			);
#endif
	}
	return 0;
}

//===========================================================================================================================
//	AACDecInit
//===========================================================================================================================

static int AACDecInit(AudioConverterPrivateRef me)
{
	HANDLE_AACDECODER mAACDecoder;

	mAACDecoder = aacDecoder_Open(TT_MP4_RAW, /*  num layers */ 1);
	if( mAACDecoder != NULL){

		me->mStreamInfo = aacDecoder_GetStreamInfo(mAACDecoder);
		if (me->mStreamInfo != NULL) {
			me->nativeCodecRef = (void*)mAACDecoder;
			UCHAR* inBuffer[1];
			UINT inBufferLength[1] = {0};

			UCHAR AAC_conf[] = { 0x00, 0x00 };

			static const UINT rateTable[] = { 
				96000, 88200, 64000, 48000, 44100, 32000, 24000, 
				22050, 16000, 12000, 11025, 8000,  7350, 
				0, 0, 0 };
			
			int i;
			for( i = 0; i< 16; i++ )
			{
				if( rateTable[i] == me->sampleRate ) break;
			}

			int audioObjectType = 2; 		//AAC_LC = 2  5bit 00010;
			int samplingFrequencyIndex = i; //4bit

			AAC_conf[0] = ( audioObjectType << 3 ) | (( samplingFrequencyIndex & 0xe ) >> 1 );
			AAC_conf[1] = (( samplingFrequencyIndex & 0x1 ) << 7 ) | ( me->channels << 3 );

			inBuffer[0] = AAC_conf;
			inBufferLength[0] = sizeof( AAC_conf );
				
			AAC_DECODER_ERROR decoderErr = aacDecoder_ConfigRaw(mAACDecoder,
					inBuffer,
					inBufferLength);

			if(decoderErr == 0){
				g_message(" aacDecoder_DecodeFrame: ret:%X\n", decoderErr);
				g_message("aacSampleRate: %d\n", me->mStreamInfo->aacSampleRate);
				g_message("profile: %d\n", me->mStreamInfo->profile);
				g_message("channelConfig: %d\n", me->mStreamInfo->channelConfig);
				g_message("frameSize: %d\n", me->mStreamInfo->frameSize);
				g_message("aacSamplesPerFrame: %d\n", me->mStreamInfo->aacSamplesPerFrame);
			}
			else {
				g_message(" aacDecoder_ConfigRaw NG %X\n",decoderErr);
			}
			return 0;
		}
		else{
			g_message("aacDecoder_GetStreamInfo NG[%X]\n",me->mStreamInfo);
		}
	}
	else{
		g_message("aacDecoder_Open NG[%X]\n",me->nativeCodecRef);

		if (IS_LITTLE_ENDIAN()) {
			g_message("aacDecoder_Open NG[%X]\n",me->nativeCodecRef);
		}

		return -1;
	}

	return 0;
}

//===========================================================================================================================
//	AACDecFree
//===========================================================================================================================

static int AACDecFree( AudioConverterPrivateRef me )
{
	if( me->nativeCodecRef ) aacDecoder_Close(( HANDLE_AACDECODER )me->nativeCodecRef );
	return 0;
}

//===========================================================================================================================
//	OPUS_DecInit
//===========================================================================================================================

static int OPUSDecInit( AudioConverterPrivateRef  me)
{
	OpusDecoder *decoder;

	int err = 0;

	decoder = opus_decoder_create( me->sampleRate, me->channels, &err );

	if (err < 0){
		g_message("opus_decoder_create error\n");
	}
	else{
		me->nativeCodecRef = (void*)decoder;
	}

	return err;
}

#define MAX_FRAME_SIZE 6 * 960
#define CHANNELS 2
#define FRAME_SIZE 960

static opus_int16 opus_out[MAX_FRAME_SIZE*CHANNELS];
static opus_int16 opus_in[FRAME_SIZE*CHANNELS*10];

#if 0
static opus_int16 opus_out2[MAX_FRAME_SIZE*CHANNELS];
static char opus_out3[MAX_FRAME_SIZE*CHANNELS*2];
#endif

//===========================================================================================================================
//	OPUSDecBuf
//===========================================================================================================================

static int OPUSDecBuf(AudioConverterPrivateRef        me)
{
	OpusDecoder *decoder = ( OpusDecoder * )me->nativeCodecRef;
	int _size;
	uint32_t i;

	if( decoder == NULL )
	{
		g_message("OPUSDecBuf Error decoder == NULL\n");
		return -1;
	}

	_size = opus_decode(decoder, me->inData, me->inLen, opus_out, MAX_FRAME_SIZE, 0);

	if( _size< 0 )
	{
		g_message("opus_decode Error _size[%d]\n", _size );
		_size = -1;
	}
	else 
	{
		if(( me->channels * _size ) * 2 <= me->outLen )
		{
			for( i = 0; i < me->channels * _size; i++ ) 
			{         
				me->outData[2*i]   = opus_out[i] & 0xFF;         
				me->outData[2*i+1] = ( opus_out[i] >> 8 ) & 0xFF;     
			}
		}
		else{
			g_message("opus_decode NG,buffer low _size[%d] out len[%d]\n",CHANNELS * _size * 2, me->outLen);
			_size = 0;
		}
	}

	return _size;
}

//===========================================================================================================================
//	OPUSDecFree
//===========================================================================================================================

static int OPUSDecFree( AudioConverterPrivateRef me )
{
	int err = 0;

	if(me->nativeCodecRef)opus_decoder_destroy(( OpusDecoder * )me->nativeCodecRef );
	me->nativeCodecRef = NULL;

	return err;
}

//===========================================================================================================================
//	OPUSEncInit
//===========================================================================================================================

static int OPUSEncInit( AudioConverterPrivateRef me )
{
	OpusEncoder *encoder;
	int err = 0;

	// OPUS_APPLICATION_AUDIO / OPUS_APPLICATION_VOIP
	encoder = opus_encoder_create( me->sampleRate, me->channels, OPUS_APPLICATION_VOIP, &err );
	g_message("sampleRate = %d channels = %d",me->sampleRate,me->channels);
	if (err < 0){
		g_message("opus_encoder_create NG\n");
	}
	else{
		opus_encoder_ctl(encoder, OPUS_SET_BITRATE( OPUS_AUTO ));
		opus_encoder_ctl(encoder, OPUS_SET_BANDWIDTH( OPUS_AUTO ));
		opus_encoder_ctl(encoder, OPUS_SET_VBR( 1 ));
		opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT( 1 ));
		opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY( 10 ));
		opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC( 0 ));
		opus_encoder_ctl(encoder, OPUS_SET_FORCE_CHANNELS( OPUS_AUTO ));
		opus_encoder_ctl(encoder, OPUS_SET_DTX( 0 ));
		opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC( 0 ));
		opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH( 16 ));

		me->nativeCodecRef = ( void * )encoder;
#if 0
		//only for debug
		//
		me->pcmfile = fopen("/tmp/pcmin", "wb");
		me->opusfile = fopen("/tmp/opusout", "wb");
		me->pcmfile2 = fopen("/tmp/pcmin2", "wb");
		me->decoder_pcm = opus_decoder_create(me->sampleRate, me->channels, &err);
#endif
	}

	return err;
}

//===========================================================================================================================
//	OPUS_EncBuf
//===========================================================================================================================

static int OPUS_EncBuf(AudioConverterPrivateRef         me)
{
	OpusEncoder *encoder = (OpusEncoder *)me->nativeCodecRef;
	int _size;
	int inFrameNumer;
	int i;

	if( encoder == NULL ){
		g_message("OPUS_EncBuf NG : encoder == NULL\n");
		return -1;
	}

	if( me->channels == 0 ){
		g_message("OPUS_EncBuf NG : channels == 0\n");
		return -1;
	}

	inFrameNumer = me->inLen/2;

	//dlog(kLogLevelError,"wanglin OPUS_EncBuf inFrameNumer[%d] channels[%d]\n",inFrameNumer,me->channels);
#if 0
	fwrite(me->inData, me->inLen, 1, me->pcmfile);
#endif

	/* Convert from little-endian ordering. */      
	for ( i = 0;i < inFrameNumer; i++ )         
		opus_in[i] = me->inData[ 2*i + 1 ] << 8| me->inData[ 2*i ];

	_size = opus_encode( encoder, opus_in, inFrameNumer/me->channels, me->outData, me->outLen );

	if( _size < 0 ){
		g_message("opus_encode NG : _size[%d]\n",_size);
		_size = -1;
	}
	else {
#if 0
		fwrite(me->outData, _size, 1, me->opusfile);

		int frame_size = opus_decode(me->decoder_pcm, me->outData, _size, opus_out2, MAX_FRAME_SIZE, 0);
		g_message("_size = %d\n",_size);
		if( frame_size < 0 ){
			dlog(kLogLevelError,"wanglin opus_decode NG,frame_size[%d]\n",frame_size);
			return -1;
		}
		else {
			//dlog(kLogLevelError,"wanglin opus_decode OK,frame_size[%d]\n",frame_size);
			for(i=0; i < (int)me->channels*frame_size; i++) {         
				opus_out3[2*i]=opus_out2[i]&0xFF;         
				opus_out3[2*i+1]=(opus_out2[i]>>8)&0xFF;     
			}
			fwrite(opus_out3, frame_size*2, 1, me->pcmfile2);
		}
#endif
	}

	return _size;
}

//===========================================================================================================================
//	OPUS_EncBuf
//===========================================================================================================================

static int OPUSEncFree(AudioConverterPrivateRef       me)
{
	int err = 0;

	if(me->nativeCodecRef)opus_encoder_destroy( (OpusEncoder *)me->nativeCodecRef);
	me->nativeCodecRef = NULL;
#if 0
	if(me->pcmfile)fclose(me->pcmfile);
	if(me->opusfile)fclose(me->opusfile);
	if(me->pcmfile2)fclose(me->pcmfile2);
	if(me->decoder_pcm)opus_decoder_destroy( me->decoder_pcm);

	me->pcmfile = NULL;
	me->opusfile = NULL;
	me->pcmfile2 = NULL;
#endif

	return err;
}
#endif

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
	
	g_message("AudioConverterNew mFormatID[%d]mSampleRate[%d]channels[%d]\n", me->sourceFormatID,me->sampleRate,me->channels);	

	switch( me->sourceFormatID )
	{
		case kAudioFormatMPEG4AAC:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			// $$$ TODO: Initialize codec for AAC LC -> PCM decompression
			g_message("Initialize codec for AAC LC -> PCM decompression\n" );	
#if WIRELESS_CARPLAY
			AACDecInit( me );
#endif
			err = kNoErr;
			break;

		case kAudioFormatMPEG4AAC_ELD:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			// $$$ TODO: Initialize codec for AAC ELD -> PCM decompression
			g_message("Initialize codec for AAC ELD -> PCM decompression\n" );
			err = kNoErr;
			break;

		case kAudioFormatOpus:
			require_action_quiet( inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr );
			g_message("Initialize codec for Opus -> PCM decompression\n" );
			// $$$ TODO: Initialize codec for Opus -> PCM decompression
#if WIRELESS_CARPLAY
			OPUSDecInit( me );
#endif
			err = kNoErr;
			break;

		case kAudioFormatLinearPCM:
			if( inDestinationFormat->mFormatID == kAudioFormatMPEG4AAC_ELD )
			{
				// $$$ TODO: Initialize codec for PCM -> AAC ELD compression
				g_message("Initialize codec for PCM -> AAC ELD compression\n" );
				err = kNoErr;
				goto exit;
			}
			else if( inDestinationFormat->mFormatID == kAudioFormatOpus )
			{
				// $$$ TODO: Initialize codec for PCM -> Opus compression
				g_message("Initialize codec for PCM -> Opus compression\n" );
#if WIRELESS_CARPLAY
				OPUSEncInit( me );
#endif
				err = kNoErr;
				break;
			}
		default:
			err = kUnsupportedErr;
			goto exit;
	}
	
	*outAudioConverter = (AudioConverterRef) me;
	me = NULL;
	
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
#if WIRELESS_CARPLAY
				AACDecFree( me );
#endif
				break;

			case kAudioFormatMPEG4AAC_ELD:
				// $$$ TODO: Free any resources for the AAC ELD decoder
				break;

			case kAudioFormatOpus:
				// $$$ TODO: Free any resources for the Opus decoder
#if WIRELESS_CARPLAY
				OPUSDecFree( me );
#endif
				break;

			case kAudioFormatLinearPCM:
				if( me->destFormatID == kAudioFormatMPEG4AAC_ELD )
				{
				// $$$ TODO: Free any resources for the AAC ELD encoder
				}
				else if( me->destFormatID == kAudioFormatOpus )
				{
				// $$$ TODO: Free any resources for the Opus encoder
#if WIRELESS_CARPLAY
					OPUSEncFree( me );
#endif
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

#if WIRELESS_CARPLAY
	me->inData = bufferList.mBuffers[ 0 ].mData;
	me->inLen = bufferList.mBuffers[ 0 ].mDataByteSize;
	me->outData = outOutputData->mBuffers[ 0 ].mData;
	me->outLen = outOutputData->mBuffers[ 0 ].mDataByteSize; //ioOutputDataPacketSize;

//	g_message("inLen = %d outLen = %d\n",me->inLen, me->outLen);

	memset(me->outData,0,me->outLen);

	AACDecBuf(me);
#endif

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
#if WIRELESS_CARPLAY
	me->inData = bufferList.mBuffers[ 0 ].mData;
	me->inLen = bufferList.mBuffers[ 0 ].mDataByteSize;
	me->outData = outOutputData->mBuffers[ 0 ].mData;
	me->outLen = outOutputData->mBuffers[ 0 ].mDataByteSize; //ioOutputDataPacketSize;
//	g_message("inLen = %d outLen = %d\n",me->inLen, me->outLen);

	int _size = OPUSDecBuf(me);

	if( _size < 0 ){
		g_message( "OPUSDecBuf NG,_size[%d]\n", _size );
		_size = 0;
	}

	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		outPacketDescription[ 0 ].mStartOffset = 0;
		outPacketDescription[ 0 ].mVariableFramesInPacket = 0;
		outPacketDescription[ 0 ].mDataByteSize = _size * me->channels * 2;
	}

	// $$$ TODO: Set the number of samples produced
	*ioOutputDataPacketSize = _size;
#endif	
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
#ifdef WIRELESS_CARPLAY
	me->inData = bufferList.mBuffers[ 0 ].mData;
	me->inLen = bufferList.mBuffers[ 0 ].mDataByteSize;
	me->outData = outOutputData->mBuffers[ 0 ].mData;
	me->outLen = outOutputData->mBuffers[ 0 ].mDataByteSize;

	int _ret = OPUS_EncBuf(me);

//	g_message("inLen = %d outLen = %d  time = %lld \n",me->inLen, me->outLen, UpTicks());
	if( _ret < 0 ){
		numPacket = 0;
		*ioOutputDataPacketSize = 0;
		g_message(" OPUS_EncBuf Err[%d]\n", _ret );
	}
	else{
		if(( uint32_t )_ret > me->outLen ){
			g_message("OPUS_EncBuf Err too long[%d]\n", _ret );
		}
		numPacket = _ret;
		*ioOutputDataPacketSize = 1;
	}

	outOutputData->mBuffers[ 0 ].mDataByteSize = numPacket;

	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		outPacketDescription[ numPacket ].mStartOffset = 0;
		outPacketDescription[ numPacket ].mVariableFramesInPacket = 0;
		outPacketDescription[ numPacket ].mDataByteSize = numPacket;
	}
#else
	if( outPacketDescription )
	{
		// $$$ TODO: Fill out outputPacketDescription with the output results
		// outPacketDescription[ numPacket ].mStartOffset = ;
		// outPacketDescription[ numPacket ].mVariableFramesInPacket = 0;
		// outPacketDescription[ numPacket ].mDataByteSize = ;
	}

	// $$$ TODO: Set the number of packets encoded
	*ioOutputDataPacketSize = numPacket;
#endif	
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
