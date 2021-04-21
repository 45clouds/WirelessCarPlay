/*
	File:    	ALSAMixer.c
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
	
	Copyright (C) 2007-2017 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include <alsa/asoundlib.h>
#include <math.h>
#include <CoreUtils/MathUtils.h>

#include "ALSAMixer.h"

ulog_define( ALSAMixer, kLogLevelTrace, kLogFlags_Default, "ALSAMixer", NULL );
#define am_dlog( LEVEL, ... )		dlogc( &log_category_from_name( ALSAMixer ), (LEVEL), __VA_ARGS__ )
#define am_ulog( LEVEL, ... )		ulog( &log_category_from_name( ALSAMixer ), (LEVEL), __VA_ARGS__ )

static void _ALSADuckMixerRampVolumeTimer( void *inContext );
static void _ALSADuckMixerRampVolumeCanceled( void *inContext );

static snd_mixer_t * gMixer = NULL;
static snd_mixer_elem_t * gDuckElement = NULL;
static snd_mixer_elem_t * gFrontElement = NULL;
static snd_mixer_elem_t * gRearElement = NULL;

static dispatch_source_t rampTimer;	// Timer for ramping volume.
static double rampCurrentVolume;	// Current volume during ramping.
static double rampFinalVolume;		// Final volume to ramp to.
static double rampGain;				// Volume to add at each step.
static int rampCurrentStep;			// Current step in the ramping process.
static int rampTotalSteps;			// Total number of steps in the ramping process.

OSStatus ALSAMixerOpen( void )
{
	OSStatus							err;
	snd_mixer_selem_id_t *				sid;
	
	err = snd_mixer_open( &gMixer, 0 );
	require_noerr( err, exit );
	
	err = snd_mixer_attach( gMixer, "default" );
	require_noerr( err, exit );
	
	err = snd_mixer_selem_register( gMixer, NULL, NULL );
	require_noerr( err, exit );
	
	err = snd_mixer_load( gMixer );
	require_noerr( err, exit );
	
	snd_mixer_selem_id_alloca( &sid );

	snd_mixer_selem_id_set_name( sid, "mainsoftvol" );
	snd_mixer_selem_id_set_index( sid, 0 );
	gDuckElement = snd_mixer_find_selem( gMixer, sid );
	require_action( gDuckElement, exit, err = kNotFoundErr );

	snd_mixer_selem_id_set_name( sid, "Master" );
	snd_mixer_selem_id_set_index( sid, 0 );
	gFrontElement = snd_mixer_find_selem( gMixer, sid );
	require_action( gFrontElement, exit, err = kNotFoundErr );

	snd_mixer_selem_id_set_name( sid, "DAC3" );
	snd_mixer_selem_id_set_index( sid, 0 );
	gRearElement = snd_mixer_find_selem( gMixer, sid );
	require_action( gRearElement, exit, err = kNotFoundErr );
exit:
	return err;
}

double ALSADuckMixerGetVolume( void )
{
	double								normalizedVolume = 1.0;
	OSStatus							err;
	snd_mixer_selem_channel_id_t		channel;
	//long								minDB, maxDB, db;
	long								minVolume, maxVolume, volume;
	
	// Try db volume first so we can detect overdriving and cap it.
	
	//ALSA softvol doesn't support the dB API
	/*err = snd_mixer_selem_get_playback_dB_range( gDuckElement, &minDB, &maxDB );
	 if( !err )
	 {
		if( maxDB > 0 ) maxDB = 0; // Cap at 0 dB to avoid overdriving.
		for( channel = 0; channel < SND_MIXER_SCHN_LAST; ++channel )
		{
	 if( snd_mixer_selem_has_playback_channel( gDuckElement, channel ) )
	 {
	 err = snd_mixer_selem_get_playback_dB( gDuckElement, channel, &db );
	 if( err ) continue;
	 
	 normalizedVolume = exp10( ( db - maxDB ) / 6000.0 );
	 normalizedVolume = Clamp( normalizedVolume, 0.0, 1.0 );
	 goto exit;
	 }
		}
	 }*/
	
	// No db volume support so try linear volume.
	
	err = snd_mixer_selem_get_playback_volume_range( gDuckElement, &minVolume, &maxVolume );
	if( !err )
	{
		for( channel = 0; channel < SND_MIXER_SCHN_LAST; ++channel )
		{
			if( snd_mixer_selem_has_playback_channel( gDuckElement, channel ) )
			{
				err = snd_mixer_selem_get_playback_volume( gDuckElement, channel, &volume );
				if( err ) continue;
				
				//ALSA softvol is in dB, default config is -51dB to 0dB in 255 steps (5 steps per dB)
				float db = (volume - maxVolume) / 5.0;
				normalizedVolume = DBtoLinear( db );
				normalizedVolume = Clamp( normalizedVolume, 0.0, 1.0 );
				goto exit;
			}
		}
	}
	
	// No volume controls so assume it's always full volume and leave the result as 1.0.
exit:
	return( normalizedVolume );
}

void ALSADuckMixerSetVolume( double inVolume )
{
	OSStatus                    err;
	//long                        minDB, maxDB, newDB;
	long                        minVolume, maxVolume, newVolume;
	
	// Try db volume first so we can detect overdriving and cap it.
	
	//ALSA softvol doesn't support the dB API
	/*err = snd_mixer_selem_get_playback_dB_range( gDuckElement, &minDB, &maxDB );
	 if( !err )
	 {
		if( maxDB > 0 ) maxDB = 0; // Cap at 0 dB to avoid overdriving.
		if(      inVolume <= 0 ) newDB = minDB;
		else if( inVolume >= 1 ) newDB = maxDB;
		else
		{
	 newDB = (long)( ( 6000.0 * log10( inVolume ) ) + maxDB );
	 newDB = Clamp( newDB, minDB, maxDB );
		}
		printf("mixer %ld, has dB from %ld to %ld\n", newDB, minDB, maxDB);
		err = snd_mixer_selem_set_playback_dB_all( gDuckElement, newDB, 0 );
		if( !err ) goto exit;
	 }*/
	
	// No db volume support so try linear volume.
	
	err = snd_mixer_selem_get_playback_volume_range( gDuckElement, &minVolume, &maxVolume );
	if( !err )
	{
		//ALSA softvol is in dB, default config is -51dB to 0dB in 255 steps (5 steps per dB)
		//softvol can be configured with min_dB, max_dB, and resolution
		newVolume = maxVolume + LinearToDB( inVolume ) * 5;
		newVolume = Clamp( newVolume, minVolume, maxVolume );
		snd_mixer_selem_set_playback_volume_all( gDuckElement, newVolume );
	}
}

#define kRampSteps		16

OSStatus ALSADuckMixerRampVolume( double inFinalVolume, double inDurationSecs, dispatch_queue_t	inQueue )
{
	OSStatus					err;
	uint64_t					nanos;
	
	// $$$ TODO: Real head-unit implementations should do sample-level ramping. This current implementation with a timer
	// and setting the volume periodically is really just a hack.
	
	dispatch_source_forget( &rampTimer );
	
	rampCurrentVolume	= ALSADuckMixerGetVolume();
	rampFinalVolume		= inFinalVolume;
	rampGain			= ( inFinalVolume - rampCurrentVolume ) / kRampSteps;
	rampCurrentStep		= 0;
	rampTotalSteps		= kRampSteps;
	nanos					= (uint64_t)( ( inDurationSecs * kNanosecondsPerSecond ) / kRampSteps );
	am_dlog( kLogLevelTrace, "Ramping volume from %f to %f in %d steps over %f seconds\n",
			rampCurrentVolume, inFinalVolume, kRampSteps, inDurationSecs );
	
	rampTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inQueue ? inQueue : dispatch_get_main_queue() );
	require_action( rampTimer, exit, err = kUnknownErr );
	//dispatch_set_context( rampTimer, inStream );
	dispatch_source_set_event_handler_f( rampTimer, _ALSADuckMixerRampVolumeTimer );
	dispatch_source_set_cancel_handler_f( rampTimer, _ALSADuckMixerRampVolumeCanceled );
	dispatch_source_set_timer( rampTimer, dispatch_time( DISPATCH_TIME_NOW, nanos ), nanos, 5 * kNanosecondsPerMillisecond );
	dispatch_resume( rampTimer );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ALSADuckMixerRampVolumeTimer
//===========================================================================================================================

static void	_ALSADuckMixerRampVolumeTimer( void *inContext )
{
	(void) inContext;
	Boolean						done;
	
	rampCurrentVolume += rampGain;
	done = ( ( ++rampCurrentStep >= rampTotalSteps ) || ( rampCurrentVolume == rampFinalVolume ) );
	ALSADuckMixerSetVolume( done ? rampFinalVolume : rampCurrentVolume );
	am_dlog( kLogLevelVerbose, "Ramping volume at %d of %d, volume %f of %f\n",
			rampCurrentStep, rampTotalSteps, rampCurrentVolume, rampFinalVolume );
	if( done )
	{
		am_dlog( kLogLevelTrace, "Ramped volume to %f\n", rampFinalVolume );
		dispatch_source_forget( &rampTimer );
	}
}

//===========================================================================================================================
//	_ALSADuckMixerRampVolumeCanceled
//===========================================================================================================================

static void	_ALSADuckMixerRampVolumeCanceled( void *inContext )
{
	(void) inContext;
	
	if( ( rampCurrentStep != rampTotalSteps ) && ( rampCurrentVolume != rampFinalVolume ) )
	{
		am_dlog( kLogLevelTrace, "Ramp volume canceled at %d of %d, volume %f of %f\n",
				rampCurrentStep, rampTotalSteps, rampCurrentVolume, rampFinalVolume );
	}
}

OSStatus ALSAAdjustMainVolume( int delta )
{
	OSStatus                    err;
	long                        minVolume, maxVolume, volume;
	
	err = snd_mixer_selem_get_playback_volume_range( gFrontElement, &minVolume, &maxVolume );
	if ( err == kNoErr ) {
		//Front left
		err = snd_mixer_selem_get_playback_volume( gFrontElement, SND_MIXER_SCHN_FRONT_LEFT, &volume );
		if ( err == kNoErr )
		{
			volume += delta;
			volume = Clamp( volume, minVolume, maxVolume );
			snd_mixer_selem_set_playback_volume( gFrontElement, SND_MIXER_SCHN_FRONT_LEFT, volume );
		}
		//Front right
		err = snd_mixer_selem_get_playback_volume( gFrontElement, SND_MIXER_SCHN_FRONT_RIGHT, &volume );
		if ( err == kNoErr )
		{
			volume += delta;
			volume = Clamp( volume, minVolume, maxVolume );
			snd_mixer_selem_set_playback_volume( gFrontElement, SND_MIXER_SCHN_FRONT_RIGHT, volume );
		}
	}

	err = snd_mixer_selem_get_playback_volume_range( gRearElement, &minVolume, &maxVolume );
	if ( err == kNoErr )
	{
		//Rear left
		err = snd_mixer_selem_get_playback_volume( gRearElement, SND_MIXER_SCHN_FRONT_LEFT, &volume );
		if ( err == kNoErr )
		{
			volume += delta;
			volume = Clamp( volume, minVolume, maxVolume );
			snd_mixer_selem_set_playback_volume( gRearElement, SND_MIXER_SCHN_FRONT_LEFT, volume );
		}
		//Rear right
		err = snd_mixer_selem_get_playback_volume( gRearElement, SND_MIXER_SCHN_FRONT_RIGHT, &volume );
		if ( err == kNoErr )
		{
			volume += delta;
			volume = Clamp( volume, minVolume, maxVolume );
			snd_mixer_selem_set_playback_volume( gRearElement, SND_MIXER_SCHN_FRONT_RIGHT, volume );
		}
	}
	
	return( err );
}
