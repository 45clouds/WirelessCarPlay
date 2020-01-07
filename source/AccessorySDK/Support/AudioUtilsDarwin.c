/*
	File:    	AudioUtilsDarwin.c
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
*/

#include "AudioUtils.h"

#include <stdlib.h>

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "CommonServices.h"
#include "CoreAudioUtils.h"
#include "DebugServices.h"
#include "SoftLinking.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	External
//===========================================================================================================================

// AudioToolbox

SOFT_LINK_FRAMEWORK( Frameworks, AudioToolbox )

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphAddNode, 
	OSStatus, 
	( AUGraph inGraph, const AudioComponentDescription *inDescription, AUNode *outNode ), 
	( inGraph, inDescription, outNode ) )
#define AUGraphAddNode		soft_AUGraphAddNode

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphClose, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphClose		soft_AUGraphClose

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphConnectNodeInput, 
	OSStatus, 
	( AUGraph inGraph, AUNode inSourceNode, UInt32 inSourceOutputNumber, AUNode inDestNode, UInt32 inDestInputNumber ), 
	( inGraph, inSourceNode, inSourceOutputNumber, inDestNode, inDestInputNumber ) )
#define AUGraphConnectNodeInput		soft_AUGraphConnectNodeInput

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphInitialize, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphInitialize		soft_AUGraphInitialize

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphGetIndNode, 
	OSStatus, 
	( AUGraph inGraph, UInt32 inIndex, AUNode *outNode ), 
	( inGraph, inIndex, outNode ) )
#define AUGraphGetIndNode		soft_AUGraphGetIndNode

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphGetNodeCount, 
	OSStatus, 
	( AUGraph inGraph, UInt32 *outNumberOfNodes ), 
	( inGraph, outNumberOfNodes ) )
#define AUGraphGetNodeCount		soft_AUGraphGetNodeCount

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphNodeInfo, 
	OSStatus, 
	( AUGraph inGraph, AUNode inNode, AudioComponentDescription *outDescription, AudioUnit *outAudioUnit ), 
	( inGraph, inNode, outDescription, outAudioUnit ) )
#define AUGraphNodeInfo		soft_AUGraphNodeInfo

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphOpen, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphOpen		soft_AUGraphOpen

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphSetNodeInputCallback, 
	OSStatus, 
	( AUGraph inGraph, AUNode inDestNode, UInt32 inDestInputNumber, const AURenderCallbackStruct *inInputCallback ), 
	( inGraph, inDestNode, inDestInputNumber, inInputCallback ) )
#define AUGraphSetNodeInputCallback		soft_AUGraphSetNodeInputCallback

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphStop, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphStop		soft_AUGraphStop

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphStart, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphStart		soft_AUGraphStart

SOFT_LINK_FUNCTION( AudioToolbox, AUGraphUninitialize, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define AUGraphUninitialize		soft_AUGraphUninitialize

SOFT_LINK_FUNCTION( AudioToolbox, DisposeAUGraph, 
	OSStatus, 
	( AUGraph inGraph ), 
	( inGraph ) )
#define DisposeAUGraph		soft_DisposeAUGraph

SOFT_LINK_FUNCTION( AudioToolbox, NewAUGraph, 
	OSStatus, 
	( AUGraph *outGraph ), 
	( outGraph ) )
#define NewAUGraph		soft_NewAUGraph

// AudioUnit / AudioToolbox

	
	// AudioUnit
	
	SOFT_LINK_FRAMEWORK( Frameworks, AudioUnit )
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioComponentFindNext, 
		AudioComponent, 
		( AudioComponent inComponent, const AudioComponentDescription *inDesc ), 
		( inComponent, inDesc ) )
	#define AudioComponentFindNext		soft_AudioComponentFindNext
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioComponentInstanceDispose, 
		OSStatus, 
		( AudioComponentInstance inInstance ), 
		( inInstance ) )
	#define AudioComponentInstanceDispose		soft_AudioComponentInstanceDispose
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioComponentInstanceNew, 
		OSStatus, 
		( AudioComponent inComponent, AudioComponentInstance *outInstance ), 
		( inComponent, outInstance ) )
	#define AudioComponentInstanceNew		soft_AudioComponentInstanceNew
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioOutputUnitStart, 
		OSStatus, 
		( AudioUnit ci ), 
		( ci ) )
	#define AudioOutputUnitStart		soft_AudioOutputUnitStart
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioOutputUnitStop, 
		OSStatus, 
		( AudioUnit ci ), 
		( ci ) )
	#define AudioOutputUnitStop		soft_AudioOutputUnitStop
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitGetParameter, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope, AudioUnitElement inElement, 
		  AudioUnitParameterValue *outValue ), 
		( inUnit, inID, inScope, inElement, outValue ) )
	#define AudioUnitGetParameter		soft_AudioUnitGetParameter
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitGetProperty, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void *outData, 
		  UInt32 *ioDataSize ), 
		( inUnit, inID, inScope, inElement, outData, ioDataSize ) )
	#define AudioUnitGetProperty		soft_AudioUnitGetProperty
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitGetPropertyInfo, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, 
		  UInt32 *outDataSize, Boolean *outWritable ), 
		( inUnit, inID, inScope, inElement, outDataSize, outWritable ) )
	#define AudioUnitGetPropertyInfo		soft_AudioUnitGetPropertyInfo
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitInitialize, 
		OSStatus, 
		( AudioUnit inUnit ), 
		( inUnit ) )
	#define AudioUnitInitialize		soft_AudioUnitInitialize
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitRender, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, 
		  UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData ), 
		( inUnit, ioActionFlags, inTimeStamp, inOutputBusNumber, inNumberFrames, ioData ) )
	#define AudioUnitRender		soft_AudioUnitRender
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitSetProperty, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, 
		  const void *inData, UInt32 inDataSize ), 
		( inUnit, inID, inScope, inElement, inData, inDataSize ) )
	#define AudioUnitSetProperty		soft_AudioUnitSetProperty
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitSetParameter, 
		OSStatus, 
		( AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope, AudioUnitElement inElement, 
		  AudioUnitParameterValue inValue, UInt32 inBufferOffsetInFrames ), 
		( inUnit, inID, inScope, inElement, inValue, inBufferOffsetInFrames ) )
	#define AudioUnitSetParameter		soft_AudioUnitSetParameter
	
	SOFT_LINK_FUNCTION( AudioUnit, AudioUnitUninitialize, 
		OSStatus, 
		( AudioUnit inUnit ), 
		( inUnit ) )
	#define AudioUnitUninitialize		soft_AudioUnitUninitialize


// CoreAudio

SOFT_LINK_FRAMEWORK( Frameworks, CoreAudio )

SOFT_LINK_FUNCTION( CoreAudio, AudioObjectGetPropertyData, 
	OSStatus, 
	( AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress, 
	  UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *ioDataSize, void *outData ), 
	( inObjectID, inAddress, inQualifierDataSize, inQualifierData, ioDataSize, outData ) )
#define AudioObjectGetPropertyData		soft_AudioObjectGetPropertyData

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

struct AudioStreamPrivate
{
	CFRuntimeBase					base;					// CF type info. Must be first.
	void *							context;				// Context for DLLs.
	AUGraph							graph;
	Boolean							graphOpened;
	Boolean							graphInitialized;
	Boolean							graphStarted;
	AUNode							converterNode;
	AudioUnit						converterUnit;
	AUNode							mixerNode;
	AudioUnit						mixerUnit;
	Boolean							varispeedEnabled;
	AUNode							varispeedNode;
	AudioUnit						varispeedUnit;
	Boolean							voice;					// Use the VoiceProcessing output unit.
	AUNode							outputNode;				// Note: for iOS, this is used for both input and output.
	AudioUnit						outputUnit;
	Boolean							outputStarted;
	Boolean							inputEnabled;			// True if to read data from a microphone.
	AudioUnit						inputUnit;				// Separate on OSX, but points to _outputUnit on iOS.
	uint8_t *						inputBuffer;
	size_t							inputMaxLen;
	Boolean							inputStarted;
	AudioUnitParameterValue			volume;
	CoreAudioParameterRampRef		volumeRamper;
	
	AudioStreamInputCallback_f		inputCallbackPtr;		// Function to call to write input audio.
	void *							inputCallbackCtx;		// Context to pass to audio input callback function.
	AudioStreamOutputCallback_f		outputCallbackPtr;		// Function to call to read audio to output.
	void *							outputCallbackCtx;		// Context to pass to audio output callback function.
	
	CFStringRef						audioType;				// Type of audio content. See kAudioStreamAudioType_*.
	AudioStreamBasicDescription		format;					// Format of the audio data.
	uint32_t						preferredLatencyMics;	// Max latency the app can tolerate.
};

static void	_AudioStreamGetTypeID( void *inContext );
static void	_AudioStreamFinalize( CFTypeRef inCF );
static OSStatus
	_AudioStreamInputCallBack(
		void *							inContext,
		AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *			inTimeStamp,
		UInt32							inBusNumber,
		UInt32							inNumberFrames,
		AudioBufferList *				ioData );
static OSStatus
	_AudioStreamOutputCallBack(
		void *							inContext,
		AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *			inTimeStamp,
		UInt32							inBusNumber,
		UInt32							inNumberFrames,
		AudioBufferList *				ioData );
static uint32_t	_AudioStreamGetLatency( AudioStreamRef me, OSStatus *outErr );

static const CFRuntimeClass		kAudioStreamClass = 
{
	0,						// version
	"AudioStream",			// className
	NULL,					// init
	NULL,					// copy
	_AudioStreamFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t		gAudioStreamInitOnce	= 0;
static CFTypeID				gAudioStreamTypeID		= _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( AudioUtils, kLogLevelTrace, kLogFlags_Default, "AudioUtils", NULL );
#define as_dlog( LEVEL, ... )		dlogc( &log_category_from_name( AudioUtils ), (LEVEL), __VA_ARGS__ )
#define as_ulog( LEVEL, ... )		ulog( &log_category_from_name( AudioUtils ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	AudioStreamGetTypeID
//===========================================================================================================================

CFTypeID	AudioStreamGetTypeID( void )
{
	dispatch_once_f( &gAudioStreamInitOnce, NULL, _AudioStreamGetTypeID );
	return( gAudioStreamTypeID );
}

static void _AudioStreamGetTypeID( void *inContext )
{
	(void) inContext;
	
	gAudioStreamTypeID = _CFRuntimeRegisterClass( &kAudioStreamClass );
	check( gAudioStreamTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	AudioStreamCreate
//===========================================================================================================================

OSStatus	AudioStreamCreate( AudioStreamRef *outStream )
{
	OSStatus			err;
	AudioStreamRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AudioStreamRef) _CFRuntimeCreateInstance( NULL, AudioStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->volume = 1.0f;
	
	*outStream = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_AudioStreamFinalize
//===========================================================================================================================

static void	_AudioStreamFinalize( CFTypeRef inCF )
{
	AudioStreamRef const		me = (AudioStreamRef) inCF;
	
	ForgetCF( &me->audioType );
}

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void	AudioStreamSetInputCallback( AudioStreamRef me, AudioStreamInputCallback_f inFunc, void *inContext )
{
	me->inputCallbackPtr = inFunc;
	me->inputCallbackCtx = inContext;
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void	AudioStreamSetOutputCallback( AudioStreamRef me, AudioStreamOutputCallback_f inFunc, void *inContext )
{
	me->outputCallbackPtr = inFunc;
	me->outputCallbackCtx = inContext;
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef	_AudioStreamCopyProperty( CFTypeRef inObject, CFStringRef inProperty, OSStatus *outErr )
{
	AudioStreamRef const		me		= (AudioStreamRef) inObject;
	CFTypeRef					value	= NULL;
	OSStatus					err;
	double						d;
	uint32_t					u32;
	
	if( 0 ) {}
	
	// AudioType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		value = me->audioType;
		require_action_quiet( value, exit, err = kNotFoundErr );
		CFRetain( value );
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		value = CFDataCreate( NULL, (const uint8_t *) &me->format, (CFIndex) sizeof( me->format ) );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Input

	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
		value = me->inputEnabled ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )
	{
		u32 = _AudioStreamGetLatency( me, &err );
		require_noerr( err, exit );
		value = CFNumberCreateInt64( u32 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		value = CFNumberCreateInt64( me->preferredLatencyMics );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// VarispeedEnabled

	else if( CFEqual( inProperty, kAudioStreamProperty_VarispeedEnabled ) )
	{
		value = me->varispeedEnabled ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// Volume

	else if( CFEqual( inProperty, kAudioStreamProperty_Volume ) )
	{
		AudioUnitParameterValue		volume = 1.0f;
		
		if( me->mixerUnit )
		{
			err = AudioUnitGetParameter( me->mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, &volume );
			require_noerr( err, exit );
		}
		
		d = volume;
		value = CFNumberCreate( NULL, kCFNumberDoubleType, &d );
		require_action( value, exit, err = kUnknownErr );
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus	_AudioStreamSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue )
{
	AudioStreamRef const		me = (AudioStreamRef) inObject;
	OSStatus					err;
	double						d;
	
	if( 0 ) {}
	
	// AudioType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
		require_action( CFIsType( inValue, CFString ), exit, err = kTypeErr );
		ReplaceCF( &me->audioType, inValue );
	}
	
	// Format
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
		CFGetData( inValue, &me->format, sizeof( me->format ), NULL, &err );
		require_noerr( err, exit );
	}
	
	// Input
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
		me->inputEnabled = CFGetBoolean( inValue, NULL );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		me->preferredLatencyMics = CFGetUInt32( inValue, &err );
		require_noerr( err, exit );
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
		// We don't use a custom thread nor do we have control over the threads used by CoreAudio so do nothing here.
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		// We don't use a custom thread nor do we have control over the threads used by CoreAudio so do nothing here.
	}
	
	// VarispeedEnabled

	else if( CFEqual( inProperty, kAudioStreamProperty_VarispeedEnabled ) )
	{
		me->varispeedEnabled = CFGetBoolean( inValue, NULL );
	}
	
	// VarispeedRate

	else if( CFEqual( inProperty, kAudioStreamProperty_VarispeedRate ) )
	{
		AudioUnitParameterValue		param;
		
		require_action( me->varispeedUnit, exit, err = kNotPreparedErr );
		
		d = CFGetDouble( inValue, &err );
		require_noerr( err, exit );
		
		param = (AudioUnitParameterValue)( d / me->format.mSampleRate );
		err = AudioUnitSetParameter( me->varispeedUnit, kVarispeedParam_PlaybackRate, kAudioUnitScope_Global, 0, param, 0 );
		require_noerr( err, exit );
	}
	
	// Voice
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Voice ) )
	{
		me->voice = CFGetBoolean( inValue, NULL );
	}
	
	// Volume

	else if( CFEqual( inProperty, kAudioStreamProperty_Volume ) )
	{
		d = CFGetDouble( inValue, &err );
		require_noerr( err, exit );
		
		me->volume = (AudioUnitParameterValue) d;
		as_dlog( kLogLevelVerbose, "Setting volume to %f %s\n", me->volume, me->mixerUnit ? "" : "(deferred)" );
		if( me->mixerUnit )
		{
			err = AudioUnitSetParameter( me->mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, me->volume, 0 );
			require_noerr( err, exit );
		}
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioStreamGetLatency
//===========================================================================================================================

static uint32_t	_AudioStreamGetLatency( AudioStreamRef me, OSStatus *outErr )
{
	uint32_t						totalLatencyMics = 0;
	OSStatus						err;
	UInt32							nodeIndex, nodeCount;
	AUNode							node;
	AudioComponentDescription		desc;
	AudioUnit						unit;
	UInt32							len;
	Float64							sampleRate;
	Float64							f64;
	UInt32							u32;
	UInt32							streamIndex, streamCount;
	AudioObjectID *					streamIDs;
	uint32_t						streamLatencyMics, maxStreamLatencyMics;
	
	require_action( me->graph, exit, err = kNotPreparedErr );
	
	// Add the latency of each audio unit in the graph.
	
	nodeCount = 0;
	err = AUGraphGetNodeCount( me->graph, &nodeCount );
	check_noerr( err );
	if( err ) nodeCount = 0;
	
	for( nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex )
	{
		err = AUGraphGetIndNode( me->graph, nodeIndex, &node );
		check_noerr( err );
		if( err ) continue;
		
		err = AUGraphNodeInfo( me->graph, node, &desc, &unit );
		check_noerr( err );
		if( err ) continue;
		
		len = (UInt32) sizeof( sampleRate );
		err = AudioUnitGetProperty( unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, &len );
		check_noerr( err );
		if( err || ( sampleRate <= 0 ) ) sampleRate = 44100;
		
		len = (UInt32) sizeof( f64 );
		err = AudioUnitGetProperty( unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &f64, &len );
		check_noerr( err );
		if( !err ) totalLatencyMics += (uint32_t)( ( 1000000 * f64 ) / sampleRate );
		dlog( kLogLevelNotice, "Latency %u of %u, '%C'/'%C': %.9f\n", nodeIndex + 1, nodeCount, 
			desc.componentType, desc.componentSubType, f64 );
		
		// If it's an output unit then get the underlying device's latency as well (currently only for OS X).
		
		if( desc.componentType == kAudioUnitType_Output )
		{
			u32 = 0;
			len = (UInt32) sizeof( u32 );
			err = AudioUnitGetProperty( unit, kAudioDevicePropertyLatency, kAudioUnitScope_Output, 0, &u32, &len );
			if( !err )
			{
				totalLatencyMics += (uint32_t)( ( 1000000.0 * u32 ) / sampleRate );
				dlog( kLogLevelNotice, "Device Latency: %.9f\n", u32 / sampleRate );
			}
			
			u32 = 0;
			len = (UInt32) sizeof( u32 );
			err = AudioUnitGetProperty( unit, kAudioDevicePropertySafetyOffset, kAudioUnitScope_Output, 0, &u32, &len );
			if( !err )
			{
				totalLatencyMics += (uint32_t)( ( 1000000.0 * u32 ) / sampleRate );
				dlog( kLogLevelNotice, "Safety Offset: %.9f\n", u32 / sampleRate );
			}
			
			// Add the latency of the stream with the highest latency.
			
			maxStreamLatencyMics = 0;
			len = 0;
			err = AudioUnitGetPropertyInfo( unit, kAudioDevicePropertyStreams, kAudioUnitScope_Output, 0, &len, NULL );
			streamCount = err ? 0 : ( (UInt32)( len / sizeof( *streamIDs ) ) );
			if( streamCount > 0 )
			{
				streamIDs = (AudioStreamID *) malloc( len );
				check( streamIDs );
				if( streamIDs )
				{
					err = AudioUnitGetProperty( unit, kAudioDevicePropertyStreams, kAudioUnitScope_Output, 0, streamIDs, &len );
					check_noerr( err );
					streamCount = err ? 0 : ( (UInt32)( len / sizeof( *streamIDs ) ) );
					for( streamIndex = 0; streamIndex < streamCount; ++streamIndex )
					{
						AudioObjectPropertyAddress		addr = 
						{
							kAudioStreamPropertyLatency, 
							kAudioObjectPropertyScopeGlobal, 
							kAudioObjectPropertyElementMaster
						};
						
						u32 = 0;
						len = (UInt32) sizeof( u32 );
						err = AudioObjectGetPropertyData( streamIDs[ streamIndex ], &addr, 0, NULL, &len, &u32 );
						if( !err )
						{
							streamLatencyMics = (uint32_t)( ( 1000000.0 * u32 ) / sampleRate );
							if( streamLatencyMics > maxStreamLatencyMics ) maxStreamLatencyMics = streamLatencyMics;
							dlog( kLogLevelNotice, "Stream Latency %u of %u: %.9f\n", streamIndex + 1, streamCount, 
								u32 / sampleRate );
						}
					}
					free( streamIDs );
				}
			}
			totalLatencyMics += maxStreamLatencyMics;
		}
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( totalLatencyMics );
}

//===========================================================================================================================
//	AudioStreamRampVolume
//===========================================================================================================================

OSStatus
	AudioStreamRampVolume( 
		AudioStreamRef		me, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue )
{
	CoreAudioRampParameterForget( &me->volumeRamper );
	CoreAudioRampParameterStart( &me->volumeRamper, me->mixerUnit, kMultiChannelMixerParam_Volume, 
		kAudioUnitScope_Input, 0, (AudioUnitParameterValue) inFinalVolume, inDurationSecs, 16, inQueue,
	^{
		CoreAudioRampParameterForget( &me->volumeRamper );
	} );
	return( kNoErr );
}

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus	AudioStreamPrepare( AudioStreamRef me )
{
	OSStatus						err;
	AudioComponentDescription		desc;
	AURenderCallbackStruct			renderCB;
	UInt32							u32, len;
	AudioStreamBasicDescription		asbd;
	
	err = NewAUGraph( &me->graph );
	require_noerr( err, exit );
	
	err = AUGraphOpen( me->graph );
	require_noerr( err, exit );
	me->graphOpened = true;
	
	// Configure output.
	
	desc.componentType			= kAudioUnitType_Output;
	desc.componentSubType		= kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	err = AUGraphAddNode( me->graph, &desc, &me->outputNode );
	require_noerr( err, exit );
	
	err = AUGraphNodeInfo( me->graph, me->outputNode, NULL, &me->outputUnit );
	require_noerr( err, exit );
	
	// Configure mixer.
	
	desc.componentType			= kAudioUnitType_Mixer;
	desc.componentSubType		= kAudioUnitSubType_MultiChannelMixer;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	err = AUGraphAddNode( me->graph, &desc, &me->mixerNode );
	require_noerr( err, exit );
	
	err = AUGraphNodeInfo( me->graph, me->mixerNode, NULL, &me->mixerUnit );
	require_noerr( err, exit );
	
	u32 = 1;
	err = AudioUnitSetProperty( me->mixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, 
		&u32, (UInt32) sizeof( u32 ) );
	require_noerr( err, exit );
	
	ASBD_FillAUCanonical( &asbd, me->format.mChannelsPerFrame );
	err = AudioUnitSetProperty( me->mixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, 
		&asbd, (UInt32) sizeof( asbd ) );
	require_noerr( err, exit );
	
	err = AudioUnitSetParameter( me->mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, 1.0f, 0 );
	require_noerr( err, exit );
	
	err = AudioUnitSetParameter( me->mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, me->volume, 0 );
	require_noerr( err, exit );
	
	err = AUGraphConnectNodeInput( me->graph, me->mixerNode, 0, me->outputNode, 0 );
	require_noerr( err, exit );
	
	// Configure varispeed.
	
	if( me->varispeedEnabled )
	{
		desc.componentType			= kAudioUnitType_FormatConverter;
		desc.componentSubType		= kAudioUnitSubType_Varispeed;
		desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
		desc.componentFlags			= 0;
		desc.componentFlagsMask		= 0;
		err = AUGraphAddNode( me->graph, &desc, &me->varispeedNode );
		require_noerr( err, exit );
		
		err = AUGraphNodeInfo( me->graph, me->varispeedNode, NULL, &me->varispeedUnit );
		require_noerr( err, exit );
		
		ASBD_FillAUCanonical( &asbd, me->format.mChannelsPerFrame );
		err = AudioUnitSetProperty( me->varispeedUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, 
			&asbd, (UInt32) sizeof( asbd ) );
		require_noerr( err, exit );
		
		u32 = kRenderQuality_Max;
		err = AudioUnitSetProperty( me->varispeedUnit, kAudioUnitProperty_RenderQuality, kAudioUnitScope_Global, 0, 
			&u32, (UInt32) sizeof( u32 ) );
		require_noerr( err, exit );
		
		err = AUGraphConnectNodeInput( me->graph, me->varispeedNode, 0, me->mixerNode, 0 );
		require_noerr( err, exit );
	}
	
	// Configure converter.
	
	desc.componentType			= kAudioUnitType_FormatConverter;
	desc.componentSubType		= kAudioUnitSubType_AUConverter;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	err = AUGraphAddNode( me->graph, &desc, &me->converterNode );
	require_noerr( err, exit );
	
	err = AUGraphNodeInfo( me->graph, me->converterNode, NULL, &me->converterUnit );
	require_noerr( err, exit );
	
	require_noerr( err, exit );
	err = AudioUnitSetProperty( me->converterUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, 
		&me->format, (UInt32) sizeof( me->format ) );
	require_noerr( err, exit );
	
	ASBD_FillAUCanonical( &asbd, me->format.mChannelsPerFrame );
	if( me->varispeedEnabled )
	{
		asbd.mSampleRate = me->format.mSampleRate; // Use source sample rate so varispeed does it one step.
	}
	err = AudioUnitSetProperty( me->converterUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, 
		&asbd, (UInt32) sizeof( asbd ) );
	require_noerr( err, exit );
	
	renderCB.inputProc = _AudioStreamOutputCallBack;
	renderCB.inputProcRefCon = me;
	err = AUGraphSetNodeInputCallback( me->graph, me->converterNode, 0, &renderCB );
	require_noerr( err, exit );
	
	err = AUGraphConnectNodeInput( me->graph, me->converterNode, 0, me->varispeedNode ? me->varispeedNode : me->mixerNode, 0 );
	require_noerr( err, exit );
	
	// Configure input. Note: this doesn't make it part of the graph as you can only have one I/O unit per graph.
	// This is only needed on OSX as AURemoteIO on iOS lets you configure input and output from a single AURemoteIO.
	
	if( me->inputEnabled )
	{
		#if( TARGET_OS_MACOSX )
			AudioComponent		component;
			
			desc.componentType			= kAudioUnitType_Output;
			desc.componentSubType		= kAudioUnitSubType_HALOutput;
			desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
			desc.componentFlags			= 0;
			desc.componentFlagsMask		= 0;
			
			component = AudioComponentFindNext( NULL, &desc );
			require_noerr( err, exit );
			
			err = AudioComponentInstanceNew( component, &me->inputUnit );
			require_noerr( err, exit );
			
			err = AudioUnitInitialize( me->inputUnit );
			require_noerr( err, exit );
		#endif
		
		// Enable input I/O because input is disabled by default so we have to explicitly enable it.
		
		u32 = true;
		err = AudioUnitSetProperty( me->inputUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, 
			&u32, (UInt32) sizeof( u32 ) );
		require_noerr( err, exit );
		
		// OSX's doesn't support input and output from the same AudioUnit so disable output on the input unit.
		
		#if( TARGET_OS_MACOSX )
			u32 = false;
			err = AudioUnitSetProperty( me->inputUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, 
				&u32, (UInt32) sizeof( u32 ) );
			require_noerr( err, exit );
		#endif
		
		// Configure the current input device. iOS does this automatically so it's only needed on OSX.
		
		#if( TARGET_OS_MACOSX )
		{
			AudioObjectPropertyAddress		addr;
			AudioDeviceID					inputDevice;
			
			addr.mSelector	= kAudioHardwarePropertyDefaultInputDevice;
			addr.mScope		= kAudioObjectPropertyScopeGlobal;
			addr.mElement	= kAudioObjectPropertyElementMaster;
			u32				= (UInt32) sizeof( u32 );
			err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &addr, 0, NULL, &u32, &inputDevice );
			require_noerr( err, exit );
			
			err = AudioUnitSetProperty( me->inputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 
				0, &inputDevice, (UInt32) sizeof( inputDevice ) );
			require_noerr( err, exit );
		}
		#endif
		
		// Configure the input callback.
		
		err = AudioUnitSetProperty( me->inputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, 
			&me->format, (UInt32) sizeof( me->format ) );
		require_noerr( err, exit );
		
		u32 = 0;
		len = (UInt32) sizeof( u32 );
		err = AudioUnitGetProperty( me->inputUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, 
			&u32, &len );
		check_noerr( err );
		if( err || ( u32 < 4096 ) ) u32 = 4096;
		u32 *= asbd.mBytesPerFrame;
		
		me->inputMaxLen = u32;
		me->inputBuffer = (uint8_t *) malloc( u32 );
		require_action( me->inputBuffer, exit, err = kNoMemoryErr );
		
		renderCB.inputProc = _AudioStreamInputCallBack;
		renderCB.inputProcRefCon = me;
		err = AudioUnitSetProperty( me->inputUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, 
			&renderCB, sizeof( renderCB ) );
		require_noerr( err, exit );
	}
	
	err = AUGraphInitialize( me->graph );
	require_noerr( err, exit );
	me->graphInitialized = true;
	
exit:
	if( err ) 
	{
		as_ulog( kLogLevelNotice, "### Audio stream prepare failed: %#m\n", err );
		AudioStreamStop( me, false );
	}
	return( err );
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef me )
{
	OSStatus	err;
	
	if( !me->graph )
	{
		err = AudioStreamPrepare( me );
		require_noerr( err, exit );
	}
	if( me->graph )
	{
		if( me->outputStarted ) AUGraphStop( me->graph );
		err = AUGraphStart( me->graph );
		require_noerr( err, exit );
		me->outputStarted = true;
	}
	#if( TARGET_OS_MACOSX )
	if( me->inputUnit )
	{
		if( me->inputStarted ) AudioOutputUnitStop( me->inputUnit );
		err = AudioOutputUnitStart( me->inputUnit );
		require_noerr( err, exit );
	}
	#endif
	me->inputStarted = true;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void	AudioStreamStop( AudioStreamRef me, Boolean inDrain )
{
	OSStatus		err;
	
	(void) inDrain;
	
	CoreAudioRampParameterForget( &me->volumeRamper );
	if( me->graphStarted )
	{
		err = AUGraphStop( me->graph );
		check_noerr( err );
		me->graphStarted = false;
	}
	if( me->graphInitialized )
	{
		err = AUGraphUninitialize( me->graph );
		check_noerr( err );
		me->graphInitialized = false;
	}
	if( me->graphOpened )
	{
		err = AUGraphClose( me->graph );
		check_noerr( err );
		me->graphOpened = false;
	}
	if( me->graph )
	{
		err = DisposeAUGraph( me->graph );
		check_noerr( err );
		me->graph = NULL;
	}
	me->converterNode	= 0;
	me->converterUnit	= NULL;
	me->mixerNode		= 0;
	me->mixerUnit		= NULL;
	me->varispeedNode	= 0;
	me->varispeedUnit	= NULL;
	me->outputNode		= 0;
	me->outputUnit		= NULL;
	me->outputStarted	= false;
#if( TARGET_OS_MACOSX )
	if( me->inputUnit )
	{
		if( me->inputStarted ) AudioOutputUnitStop( me->inputUnit );
		AudioUnitUninitialize( me->inputUnit );
		AudioComponentInstanceDispose( me->inputUnit );
	}
#endif
	me->inputUnit = NULL;
	ForgetMem( &me->inputBuffer );
	me->inputStarted = false;
}

//===========================================================================================================================
//	_AudioStreamInputCallBack
//===========================================================================================================================

static OSStatus
	_AudioStreamInputCallBack(
		void *							inContext,
		AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *			inTimeStamp,
		UInt32							inBusNumber,
		UInt32							inNumberFrames,
		AudioBufferList *				ioData )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	OSStatus					err;
	uint32_t					sampleTime;
	size_t						len;
	AudioBufferList				bufferList;
	
	(void) ioData;
	
	len = inNumberFrames * me->format.mBytesPerFrame;
	require_action( len <= me->inputMaxLen, exit, err = kOverrunErr );
	bufferList.mNumberBuffers					= 1;
	bufferList.mBuffers[ 0 ].mNumberChannels	= me->format.mChannelsPerFrame;
	bufferList.mBuffers[ 0 ].mDataByteSize		= (UInt32) len;
	bufferList.mBuffers[ 0 ].mData				= me->inputBuffer;
	
	err = AudioUnitRender( me->inputUnit, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, &bufferList );
	require_noerr( err, exit );
	
	sampleTime = (uint32_t) fmod( inTimeStamp->mSampleTime, 4294967296.0 );
	me->inputCallbackPtr( sampleTime, inTimeStamp->mHostTime, me->inputBuffer, len, me->inputCallbackCtx );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioStreamOutputCallBack
//===========================================================================================================================

static OSStatus
	_AudioStreamOutputCallBack(
		void *							inContext,
		AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *			inTimeStamp,
		UInt32							inBusNumber,
		UInt32							inNumberFrames,
		AudioBufferList *				ioData )
{
	AudioStreamRef const		me = (AudioStreamRef) inContext;
	uint32_t					sampleTime;
	size_t						len;
	
	(void) ioActionFlags;
	(void) inBusNumber;
	
	sampleTime = (uint32_t) fmod( inTimeStamp->mSampleTime, 4294967296.0 );
	len = inNumberFrames * me->format.mBytesPerFrame;
	me->outputCallbackPtr( sampleTime, inTimeStamp->mHostTime, ioData->mBuffers[ 0 ].mData, len, me->outputCallbackCtx );
	return( kNoErr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AudioStreamTest
//===========================================================================================================================

#if( !EXCLUDE_UNIT_TESTS )

static void
	_AudioStreamTestInput( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext );

static void
	_AudioStreamTestOutput( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );

OSStatus	AudioStreamTest( Boolean inInput )
{
	OSStatus						err;
	SineTableRef					sineTable = NULL;
	AudioStreamRef					me = NULL;
	AudioStreamBasicDescription		asbd;
	size_t							written = 0;
	
	err = SineTable_Create( &sineTable, 44100, 800 );
	require_noerr( err, exit );
	
	err = AudioStreamCreate( &me );
	require_noerr( err, exit );
	
	if( inInput )
	{
		AudioStreamSetInputCallback( me, _AudioStreamTestInput, &written );
		_AudioStreamSetProperty( me, kAudioStreamProperty_Input, kCFBooleanTrue );
	}
	AudioStreamSetOutputCallback( me, _AudioStreamTestOutput, sineTable );
	
	ASBD_FillPCM( &asbd, 44100, 16, 16, 2 );
	err = AudioStreamPropertySetBytes( me, kAudioStreamProperty_Format, &asbd, sizeof( asbd ) );
	require_noerr( err, exit );
	
	err = AudioStreamPropertySetInt64( me, kAudioStreamProperty_PreferredLatency, 100000 );
	require_noerr( err, exit );
	
	err = AudioStreamStart( me );
	require_noerr( err, exit );
	
	sleep( 5 );
	
	if( inInput )
	{
		require_action( written > 0, exit, err = kReadErr );
	}
	
exit:
	AudioStreamForget( &me );
	if( sineTable ) SineTable_Delete( sineTable );
	printf( "AudioStreamTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

static void
	_AudioStreamTestInput( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext )
{
	size_t * const		writtenPtr = (size_t *) inContext;
	
	(void) inSampleTime;
	(void) inHostTime;
	(void) inBuffer;
	
	*writtenPtr += inLen;
}

static void
	_AudioStreamTestOutput( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext )
{
	SineTableRef const		sineTable = (SineTableRef) inContext;
	
	(void) inSampleTime;
	(void) inHostTime;
	
	SineTable_GetSamples( sineTable, 0, (int)( inLen / 4 ), inBuffer );
}

#endif // !EXCLUDE_UNIT_TESTS
