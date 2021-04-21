/*
	File:    	AudioConverterDLL.c
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
	
	AudioConverter adapter that delegates functionality to a DLL.
	
	This defaults to loading the DLL from "libAudioConverter.so".
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DAUDIO_CONVERTER_DLL_PATH\"/some/other/path/libAudioConverter.so\"
*/

#include "AudioConverter.h"
#include "CommonServices.h"
#include "DebugServices.h"

#include <dlfcn.h>

//===========================================================================================================================
//	AudioConverter
//===========================================================================================================================

#if( defined( AUDIO_CONVERTER_DLL_PATH ) )
	#define kAudioConverterDLLPath		AUDIO_CONVERTER_DLL_PATH
#else
	#define kAudioConverterDLLPath		"libAudioConverter.so"
#endif

#define FIND_SYM( NAME )	(NAME ## _f)(uintptr_t) dlsym( me->dllHandle, # NAME );

struct AudioConverterPrivate
{
	void *								context;	// Context for DLLs.
	void *								dllHandle;	// Handle to the DLL implementing the internals.
	AudioConverterNew_f					new_f;
	AudioConverterDispose_f				dispose_f;
	AudioConverterReset_f				reset_f;
	AudioConverterSetProperty_f			setProperty_f;
	AudioConverterFillComplexBuffer_f	fillComplexBuffer_f;
};

//===========================================================================================================================
//	AudioConverterNew
//===========================================================================================================================

OSStatus	AudioConverterNew(
				const AudioStreamBasicDescription * inSourceFormat,
				const AudioStreamBasicDescription * inDestinationFormat,
				AudioConverterRef *outConverter )
{
	OSStatus			err;
	AudioConverterRef		me;

	me = calloc(1, sizeof( *me ));
	require_action( me, exit, err = kNoMemoryErr );
	
	// Note: this uses RTLD_NODELETE to avoid re-initialization issues with global log categories if the DLL is unloaded 
	// and reloaded. Log categories we know about are removed on finalize, but DLL developers may not be as thorough.
	
	me->dllHandle = dlopen( kAudioConverterDLLPath, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE );
	require_action( me->dllHandle, exit, err = kPathErr );

	me->new_f					= FIND_SYM( AudioConverterNew );
	me->dispose_f				= FIND_SYM( AudioConverterDispose );
	me->reset_f					= FIND_SYM( AudioConverterReset );
	me->setProperty_f			= FIND_SYM( AudioConverterSetProperty );
	me->fillComplexBuffer_f		= FIND_SYM( AudioConverterFillComplexBuffer );

	if( me->new_f )
	{
		AudioConverterRef context;
		err = me->new_f( inSourceFormat, inDestinationFormat, &context );
		require_noerr( err, exit );
		AudioConverterSetContext( me, context );
	}
	
	*outConverter = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	AudioConverterDispose
//===========================================================================================================================
OSStatus	AudioConverterDispose( AudioConverterRef me )
{
	if( me->dispose_f )
	{
		me->dispose_f( me->context );
	}
	if( me->dllHandle )
	{
		dlclose( me->dllHandle );
		me->dllHandle = NULL;
	}
	return kNoErr;
}

//===========================================================================================================================
//	AudioConverterReset
//===========================================================================================================================
OSStatus	AudioConverterReset( AudioConverterRef inAudioConverter )
{
	return( inAudioConverter->reset_f ? inAudioConverter->reset_f( inAudioConverter->context ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioConverterSetProperty
//===========================================================================================================================
OSStatus	AudioConverterSetProperty(
				AudioConverterRef			inAudioConverter,
				AudioConverterPropertyID	inPropertyID,
				uint32_t					inSize,
				const void *				inData )
{
	return( inAudioConverter->setProperty_f ? inAudioConverter->setProperty_f( inAudioConverter->context, inPropertyID, inSize, inData ) : kUnsupportedErr );
}

//===========================================================================================================================
//	AudioConverterFillComplexBuffer
//===========================================================================================================================
OSStatus	AudioConverterFillComplexBuffer(
				AudioConverterRef			inAudioConverter,
				AudioConverterComplexInputDataProc	inInputDataProc,
				void *								inInputDataProcUserData,
				uint32_t *							ioOutputDataPacketSize,
				AudioBufferList *					outOutputData,
				AudioStreamPacketDescription *		outPacketDescription )
{
	return( inAudioConverter->fillComplexBuffer_f ? inAudioConverter->fillComplexBuffer_f( inAudioConverter->context, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription ) : kUnsupportedErr );
}
//===========================================================================================================================
//	ScreenStreamGetContext
//===========================================================================================================================

void *	AudioConverterGetContext( AudioConverterRef me )
{
	return( me->context );
}

//===========================================================================================================================
//	ScreenStreamSetContext
//===========================================================================================================================

void	AudioConverterSetContext( AudioConverterRef me, void *inContext )
{
	me->context = inContext;
}

