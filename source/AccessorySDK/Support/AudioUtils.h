/*
	File:    	AudioUtils.h
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/
/*!
	@header		AudioStream API
	@discussion	Provides APIs for audio streams.
*/

#ifndef	__AudioUtils_h__
#define	__AudioUtils_h__

#include "CFUtils.h"
#include "CommonServices.h"

#include CF_HEADER
#include COREAUDIO_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsDTSEncodedData
	@abstract	Returns true if the buffer contains data that looks like DTS-encoded data.
*/

Boolean	IsDTSEncodedData( const void *inData, size_t inByteCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsSilencePCM
	@abstract	Returns true if the buffer contains 16-bit PCM samples of only silence (0's).
*/

Boolean	IsSilencePCM( const void *inData, size_t inByteCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		SineTable
	@abstract	API for generating sine waves.
*/

typedef struct SineTable *		SineTableRef;
struct SineTable
{
	int			sampleRate;
	int			frequency;
	int			position;
	int16_t		values[ 1 ]; // Variable length array.
};

OSStatus	SineTable_Create( SineTableRef *outTable, int inSampleRate, int inFrequency );
void		SineTable_Delete( SineTableRef inTable );
void		SineTable_GetSamples( SineTableRef inTable, int inBalance, int inSampleCount, void *inSampleBuffer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		VolumeAdjuster
	@abstract	API for adjusting volume to minimize glitches.
*/

typedef struct
{
	Q16x16		currentVolume;
	Q16x16		targetVolume;
	int32_t		volumeIncrement; // Q2.30 format.
	uint32_t	rampStepsRemaining;
	
}	VolumeAdjusterContext;

void	VolumeAdjusterInit( VolumeAdjusterContext *ctx );
void	VolumeAdjusterSetVolumeDB( VolumeAdjusterContext *ctx, Float32 inDB );
void	VolumeAdjusterApply( VolumeAdjusterContext *ctx, int16_t *inSamples, size_t inCount, size_t inChannels );

#if 0
#pragma mark -
#pragma mark == AudioStream ==
#endif

//===========================================================================================================================
/*!	@group		AudioStream
	@abstract	Plays or records a stream of audio.
	@discussion
	
	The general flow is that the audio stream is created; configured with input or output flags, format of the samples, 
	preferred latency, audio callback, etc.; and then started. When started, the implementation will generally set up 
	2 or 3 buffers of audio samples (often silence at first). The size of the buffer depends on the preferred latency, 
	but is generally less than 50 ms. Smaller and fewer buffers reduces latency since the minimum latency is the number
	of samples in each buffer times the number of buffers plus any latency introduced by the driver and/or hardware.
	Smaller buffers can increase CPU usage because it needs to wake up the CPU to supply data more frequently. It also 
	increases the likelihood of the hardware running dry and dropping audio if there are thread scheduling delays that
	prevent the audio thread from running in time to provide new samples to the hardware.
	
	As each buffer completes in the hardware, it reuses the buffer by calling the AudioStream callback to provide more 
	data. It then re-schedules the buffer with the audio hardware to play when the current buffer is finished. For systems
	that don't provide direct access to the audio hardware, it may use another mechanism, such as a file descriptor. The
	AudioStream implementation waits for the file descriptor to become writable, calls the AudioStream callback to fill 
	audio data into a buffer, and then writes that buffer to the file descriptor. This process repeats as long as the 
	audio stream is started. The audio driver wakes up the audio thread when buffers complete by indicating the file 
	descriptor is writable.
	
	Timing is important for proper synchronization of audio. When the AudioStream callback is invoked, it provides the 
	sample number for the first sample to be filled in and a host time in the future for when the first sample will be
	heard. The host time should be as close as possible to when the sample will really be heard. If the hardware or driver
	supports it, the sample time would come directly from the hardware's playback position within the buffer. This can 
	be correlated with a host time by getting an accurate host time at the beginning of the audio buffer the hardware is
	playing.
*/

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetTypeID
	@abstract	Gets the CF type ID of all AudioStream objects.
*/
CFTypeID	AudioStreamGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamCreate
	@abstract	Creates a new AudioStream.
*/
typedef struct AudioStreamPrivate *		AudioStreamRef;

OSStatus	AudioStreamCreate( AudioStreamRef *outStream );
#define 	AudioStreamForget( X ) do { if( *(X) ) { AudioStreamStop( *(X), false ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamInitialize
	@abstract	Initialize function for DLL-based AudioStreams.
	@discussion	Called when AudioStream is created to give the DLL a chance to initialize itself.
*/
OSStatus	AudioStreamInitialize( AudioStreamRef inStream );
typedef OSStatus ( *AudioStreamInitialize_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamFinalize
	@abstract	Finalize function for DLL-based AudioStreams.
	@discussion	Called when AudioStream is finalized to give the DLL a chance to finalize itself.
*/
void	AudioStreamFinalize( AudioStreamRef inStream );
typedef void ( *AudioStreamFinalize_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetContext / AudioStreamSetContext
	@abstract	Gets/sets a context pointer for DLL implementors to access their internal state.
*/
void *	AudioStreamGetContext( AudioStreamRef inStream );
void	AudioStreamSetContext( AudioStreamRef inStream, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetInputCallback / AudioStreamSetOutputCallback
	@abstract	Sets a function to be called for audio input or output (depending on the direction of the stream).
	
	@param		inSampleTime	Sample number for the first sample in the buffer. This sample number should increment by 
								the number of samples in each buffer. If there is a gap in the sample number sequence, it
								means something caused an audible glitch, such as software not being able to fill buffers
								fast enough (e.g. something held off the audio thread for too long). The callback will 
								need to handle this by skipping samples in the gap and fill from the specified sample time.
	
	@param		inHost			UpTicks()-compatible timestamp for when the first sample the buffer will be heard. This 
								takes into consideration software and hardware buffer and other latencies to provide a 
								host time that's as accurate as possible.
	
	@param		inBuffer		For input, the buffer contains the audio received from the hardware (e.g. microphone).
								For output, the callback is expected to write audio data to be played to this buffer.
	
	@param		inLen			Number of bytes in the buffer.
	
	@param		inContext		Pointer that was supplied when callback was set.
								
*/
typedef void
	( *AudioStreamInputCallback_f )( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext );

void	AudioStreamSetInputCallback( AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void *inContext );
typedef void ( *AudioStreamSetInputCallback_f )( AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void *inContext );

typedef void
	( *AudioStreamOutputCallback_f )( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );

void	AudioStreamSetOutputCallback( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext );
typedef void ( *AudioStreamSetOutputCallback_f )( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AudioStreamProperties
	@abstract	Properties of an audio stream.
*/

// [String] Type of audio content (e.g. telephony, media, etc.).
#define kAudioStreamProperty_AudioType				CFSTR( "audioType" )
	#define kAudioStreamAudioType_Alert					CFSTR( "alert" )
	#define kAudioStreamAudioType_Default				CFSTR( "default" )
	#define kAudioStreamAudioType_Media					CFSTR( "media" )
	#define kAudioStreamAudioType_SpeechRecognition		CFSTR( "speechRecognition" )
	#define kAudioStreamAudioType_Telephony				CFSTR( "telephony" )

// [Data:AudioStreamBasicDescription] Format for the input/output callback(s).
#define kAudioStreamProperty_Format					CFSTR( "format" )

// [Boolean] Use this stream to read audio from a microphone or other input. Default is false.
#define kAudioStreamProperty_Input					CFSTR( "input" )

// [Number] Number of bytes in the I/O buffer.
#define kAudioStreamProperty_IOBufferSize			CFSTR( "ioBufferSize" )

// [Number] Gets the estimated latency of the current configuration.
#define kAudioStreamProperty_Latency				CFSTR( "latency" )

// [Number] Sets the lowest latency the caller thinks it will need in microseconds.
#define kAudioStreamProperty_PreferredLatency		CFSTR( "preferredLatency" )

// [Number:AudioStreamType] Type of stream. See kAudioStreamType_*.
#define kAudioStreamProperty_StreamType				CFSTR( "streamType" )

// [String] Name for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadName				CFSTR( "threadName" )

// [Number] Priority for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadPriority			CFSTR( "threadPriority" )

// [Boolean] Enable support for fine-grained skew compensation. Default is false.
#define kAudioStreamProperty_VarispeedEnabled		CFSTR( "varispeedEnabled" )

// [Number:double] Fine-grained sample rate in Hz for use when varispeed is enabled. Useful for skew compensation.
#define kAudioStreamProperty_VarispeedRate			CFSTR( "varispeedRate" )

// [Boolean] Use voice processing output to perform echo cancellation, etc.
#define kAudioStreamProperty_Voice					CFSTR( "voice" )

// [Number:double] Gets/sets the volume of the stream. Value is a linear 0.0-1.0 volume.
#define kAudioStreamProperty_Volume					CFSTR( "volume" )

// Type of stream.
typedef uint32_t		AudioStreamType;
#define kAudioStreamType_Invalid		  0 // Reserved for an invalid type.
#define kAudioStreamType_MainAudio		100 // RTP payload type for low-latency audio input/output.
#define kAudioStreamType_AltAudio		101 // RTP payload type for low-latency UI sounds, alerts, etc. output.
#define kAudioStreamType_MainHighAudio	102 // RTP payload type for high-latency audio output. UDP.

// Internals

CFTypeRef	_AudioStreamCopyProperty( CFTypeRef inObject, CFStringRef inKey, OSStatus *outErr );
typedef CFTypeRef ( *_AudioStreamCopyProperty_f )( CFTypeRef inObject, CFStringRef inKey, OSStatus *outErr );
OSStatus	_AudioStreamSetProperty( CFTypeRef inObject, CFStringRef inKey, CFTypeRef inValue );
typedef OSStatus ( *_AudioStreamSetProperty_f )( CFTypeRef inObject, CFStringRef inKey, CFTypeRef inValue );

CFObjectDefineStandardAccessors( AudioStreamRef, AudioStreamProperty, _AudioStreamCopyProperty, _AudioStreamSetProperty )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamRampVolume
	@abstract	Ramps the volume to a final volume over the specified time.
*/
OSStatus
	AudioStreamRampVolume( 
		AudioStreamRef		inStream, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue );
typedef OSStatus
	( *AudioStreamRampVolume_f )( 
		AudioStreamRef		inStream, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamPrepare
	@abstract	Prepares the audio stream so things like latency can be reported, but doesn't start playing audio.
*/
OSStatus	AudioStreamPrepare( AudioStreamRef inStream );
typedef OSStatus ( *AudioStreamPrepare_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStart
	@abstract	Starts the stream (callbacks will start getting invoked after this).
*/
OSStatus	AudioStreamStart( AudioStreamRef inStream );
typedef OSStatus ( *AudioStreamStart_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStop
	@abstract	Stops the stream. No callbacks will be received after this returns.
*/
void	AudioStreamStop( AudioStreamRef inStream, Boolean inDrain );
typedef void ( *AudioStreamStop_f )( AudioStreamRef inStream, Boolean inDrain );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamTest
	@internal
	@abstract	Unit test.
*/
OSStatus	AudioStreamTest( Boolean inInput );

#ifdef __cplusplus
}
#endif

#endif // __AudioUtils_h__
