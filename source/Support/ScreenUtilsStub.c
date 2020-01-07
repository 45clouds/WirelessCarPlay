/*
	File:    	ScreenUtilsStub.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public
	Software�? and is further subject to your agreement to the following additional terms, and your
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
	fixes or enhancements to Apple in connection with this software (“Feedback�?, you hereby grant to
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
*/

#include <CoreUtils/ScreenUtils.h>

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/CFUtils.h>
#include <CoreUtils/TickUtils.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <dev_carplay.h>
#if( !SCREEN_STREAM_DLL )
	#include CF_HEADER
	#include CF_RUNTIME_HEADER
	#include LIBDISPATCH_HEADER
#endif
extern MHDevCarplay * carplayObject;
//===========================================================================================================================
//	ScreenStream
//===========================================================================================================================

#if( SCREEN_STREAM_DLL )
typedef struct ScreenStreamImp *			ScreenStreamImpRef;
struct ScreenStreamImp
#else
typedef struct ScreenStreamPrivate *		ScreenStreamImpRef;
struct ScreenStreamPrivate
#endif
{
#if( !SCREEN_STREAM_DLL )
	CFRuntimeBase		base;					// CF type info. Must be first.
#endif
	CFDataRef			avccData;				// AVCC data for the stream.
	int					widthPixels;			// Width of the screen in pixels.
	int					heightPixels;			// Height of the screen in pixels.

	int leftPixels;
	int topPixels;

	GstElement * pipeline;
	GstElement * src;
	uint64_t startticks;

	GMainContext * context;
	GMainLoop * mainloop;

	GstElement * video_sink;
};

#if( SCREEN_STREAM_DLL )
	#define _ScreenStreamGetImp( STREAM )		( (ScreenStreamImpRef) ScreenStreamGetContext( (STREAM) ) )
#else
	#define _ScreenStreamGetImp( STREAM )		(STREAM)
#endif

#if( !SCREEN_STREAM_DLL )
	static void	_ScreenStreamGetTypeID( void *inContext );
	static void	_ScreenStreamFinalize( CFTypeRef inCF );
#endif

#if( !SCREEN_STREAM_DLL )
static dispatch_once_t			gScreenStreamInitOnce = 0;
static CFTypeID					gScreenStreamTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kScreenStreamClass = 
{
	0,						// version
	"ScreenStream",			// className
	NULL,					// init
	NULL,					// copy
	_ScreenStreamFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};
#endif

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( ScreenStream, kLogLevelNotice, kLogFlags_Default, "ScreenStream", NULL );
#define ss_dlog( LEVEL, ... )		dlogc( &log_category_from_name( ScreenStream ), (LEVEL), __VA_ARGS__ )
#define ss_ulog( LEVEL, ... )		ulog( &log_category_from_name( ScreenStream ), (LEVEL), __VA_ARGS__ )

#if( !SCREEN_STREAM_DLL )
//===========================================================================================================================
//	ScreenStreamGetTypeID
//===========================================================================================================================

CFTypeID	ScreenStreamGetTypeID( void )
{
	dispatch_once_f( &gScreenStreamInitOnce, NULL, _ScreenStreamGetTypeID );
	return( gScreenStreamTypeID );
}

static void _ScreenStreamGetTypeID( void *inContext )
{
	(void) inContext;
	
	gScreenStreamTypeID = _CFRuntimeRegisterClass( &kScreenStreamClass );
	check( gScreenStreamTypeID != _kCFRuntimeNotATypeID );
}
void _enoughData( GstElement * src, gpointer user_data )
{
	( void )src;
	( void )user_data;
	g_message( "video %s", __func__ );
}
GstElement * _src;
/* 
	${CMAKE_CURRENT_SOURCE_DIR}/../Support
 * ===  function  ======================================================================
 *         name:  setupsrc
 *  description:  
 * =====================================================================================
 */
static void setupSrc( GstElement * object, GstElement * arg0, gpointer user_data )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( user_data );

	( void )object;
	( void )user_data;

	me->src	=	arg0;
	_src	=	arg0;

	g_object_set( arg0, "stream-type", GST_APP_STREAM_TYPE_STREAM,  
			"is-live", TRUE, "block", TRUE,
			"format", GST_FORMAT_TIME, NULL );

	g_object_set( arg0, "do-timestamp", TRUE, NULL );

	g_object_set( arg0, "min-latency",(gint64)0, NULL );

	g_object_set( arg0, "max-latency", (gint64)0, NULL );

	g_signal_connect( arg0, "enough-data", G_CALLBACK( _enoughData ), arg0 );
}		/* -----  end of static function setupSrc  ----- */

static gboolean
busCallback (GstBus *bus,
		GstMessage *message,
		gpointer data)
{
	if( strncmp( gst_element_get_name( message->src ), "vpudec", 6 ) == 0 )
	{
		g_object_set( message->src, "low-latency", TRUE, "dis-reorder", TRUE,
				"frame-plus", 2,
				NULL );

		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  screen_input
 *  Description:  
 * =====================================================================================
 */
static gpointer screen_input( gpointer user_data )
{
	ScreenStreamRef me	=	_ScreenStreamGetImp( user_data );
	GstBus * _bus;

	g_main_context_push_thread_default( me->context );

	_bus	=	gst_pipeline_get_bus( GST_PIPELINE( me->pipeline ) );
	gst_bus_add_watch( _bus, busCallback, NULL );
	gst_object_unref( _bus );

	g_main_loop_run( me->mainloop );

	g_main_loop_unref( me->mainloop );
	g_main_context_unref( me->context );

	return NULL;
}		/* -----  end of static function screen_input  ----- */


//===========================================================================================================================
//	ScreenStreamCreate
//===========================================================================================================================

OSStatus	ScreenStreamCreate(  ScreenStreamRef *outStream )
{
	OSStatus			err;
	ScreenStreamRef		me;
	size_t				extraLen;
	
	const char * _sinkName;
//	const char * _sinkName, *_decName, *_parseName;
	const char * _audiosinkName;
	const char * _videosinkDev;
	const char * _sinkX, * _sinkY, * _sinkW, * _sinkH;
	const char * _surfaceid;	
	g_message("%s", __func__);
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (ScreenStreamRef) _CFRuntimeCreateInstance( NULL, ScreenStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	// $$$ TODO: Other initialization goes here.
	// This function is only called when ScreenUtils is compiled into the AirPlay library.
	if( carplayObject->pb == NULL )
	{
		g_message("ScreenStreamCreate-----get pb");
		carplayObject->pb = mh_misc_get_pb_handle();
	}
	if( me->pipeline != NULL ) g_assert( 0 );
//	if( carplayObject->pb == NULL ) g_assert( 0 );
////	_sinkName	=	getenv( "MH_CARPLAY_VIDEO_SINK_NAME" );
//
////	_sinkX		=	getenv( "MH_CARPLAY_VIDEO_SINK_X" );
////	_sinkY		=	getenv( "MH_CARPLAY_VIDEO_SINK_Y" );
////	_sinkW		=	getenv( "MH_CARPLAY_VIDEO_SINK_W" );
////	_sinkH		=	getenv( "MH_CARPLAY_VIDEO_SINK_H" );
//
////	_audiosinkName	=	getenv( "MH_CARPLAY_AUDIO_SINK_NAME" );
//
////	_sink	=	gst_element_factory_make( _sinkName, _sinkName );
////
////	if( _sinkX != NULL ) me->leftPixels	=	atoi( _sinkX );
////	if( _sinkY != NULL ) me->topPixels	=	atoi( _sinkY );
////	if( _sinkW != NULL ) me->widthPixels	=	atoi( _sinkW );
////	if( _sinkH != NULL ) me->heightPixels	=	atoi( _sinkH );
//
//	_sink	=	gst_element_factory_make( carplayObject->pb->video_sink_name, carplayObject->pb->video_sink_name );
//	me->leftPixels		=	carplayObject->pb->disp_x;
//	me->topPixels		=	carplayObject->pb->disp_y;
//	if( carplayObject->pb->disp_width != 0 ) me->widthPixels	=	carplayObject->pb->disp_width;
//	if( carplayObject->pb->disp_height != 0 ) me->heightPixels	=	carplayObject->pb->disp_height;
//
//	me->pipeline	=	gst_element_factory_make( "playbin", "playbin" );
//
//	if( _sink != NULL )
//	{
//		g_object_set( _sink, "surfaceid", carplayObject->pb->surfaceid, NULL);
//		g_object_set( me->pipeline, "video-sink", _sink, NULL );
//		g_message(" video sink name = %s surfaceid = %d %d %d %d %d \n",carplayObject->pb->video_sink_name,carplayObject->pb->surfaceid,
//			carplayObject->pb->disp_x,carplayObject->pb->disp_y,carplayObject->pb->disp_width,
//			carplayObject->pb->disp_height);
//	}
	
	_sinkName	=	getenv( "MH_CARPLAY_VIDEO_SINK_NAME" );
//	_decName	=	getenv( "MH_CARPLAY_VIDEO_DEC_NAME" );
//	g_message(" video before _decName = %s\n",_decName);
//	_decName = _decName ? _decName : "omxh264dec";
//	g_message(" video after _decName = %s\n",_decName);
	
//	_parseName	=	getenv( "MH_CARPLAY_VIDEO_PARSE_NAME" );
//	g_message(" video before _parseName = %s\n",_parseName);
//	_parseName = _parseName ? _parseName : "h264parse";
//	g_message(" video after _parseName = %s\n",_parseName);

	_sinkX		=	getenv( "MH_CARPLAY_VIDEO_SINK_X" );
	_sinkY		=	getenv( "MH_CARPLAY_VIDEO_SINK_Y" );
	_sinkW		=	getenv( "MH_CARPLAY_VIDEO_SINK_W" );
	_sinkH		=	getenv( "MH_CARPLAY_VIDEO_SINK_H" );

	_surfaceid	=	getenv( "MH_CARPLAY_VIDEO_SURFACE_ID" );

	if( _sinkName != NULL )
	{
		me->video_sink	=	gst_element_factory_make( _sinkName, _sinkName );
	}
	else
	{
		if( carplayObject->pb == NULL ) g_assert( 0 );
		me->video_sink	=	gst_element_factory_make( carplayObject->pb->video_sink_name, carplayObject->pb->video_sink_name );
	}

	if( _sinkX != NULL ) 
	{
		me->leftPixels	=	atoi( _sinkX );
	}
	else
	{
		if( carplayObject->pb == NULL ) g_assert( 0 );
		me->leftPixels		=	carplayObject->pb->disp_x;
	}

	if( _sinkY != NULL ) 
	{
		me->topPixels	=	atoi( _sinkY );
	}
	else
	{
		if( carplayObject->pb == NULL ) g_assert( 0 );
		carplayObject->pb->disp_y;
	}

	if( _sinkW != NULL ) 
	{
		me->widthPixels	=	atoi( _sinkW );
	}
	else
	{
		if( carplayObject->pb == NULL ) g_assert( 0 );
		me->widthPixels	=	carplayObject->pb->disp_width;
	}

	if( _sinkH != NULL )
	{
		me->heightPixels	=	atoi( _sinkH );
	}
	else
	{
		if( carplayObject->pb == NULL ) g_assert( 0 );
		me->heightPixels	=	carplayObject->pb->disp_height;
	}

#ifdef B511 //NAGIVI
	//me->pipeline	=	gst_element_factory_make( "playbin", "playbin" );
	
	GstElement  *appsrc, *parse, *decoder;
	bool rel = FALSE;

	me->pipeline	=	gst_pipeline_new ("pipeline");
	appsrc = gst_element_factory_make ("appsrc", "source");
	parse = gst_element_factory_make ("h264parse", "parse");
	decoder = gst_element_factory_make ("omxh264dec", "decoder");

//	g_object_set( decoder, "async-depth", 2, NULL );
//	g_object_set( decoder, "live-mode", TRUE, NULL );
#else
	GstElement  *appsrc, *parse, *decoder;
	bool rel = FALSE;

//	me->pipeline	=	gst_element_factory_make( "playbin", "playbin" );

	me->pipeline	=	gst_pipeline_new ("pipeline");
	appsrc = gst_element_factory_make ("appsrc", "source");
	parse = gst_element_factory_make ("h264parse", "parse");
	decoder = gst_element_factory_make ("mfxdecode", "decoder");
//	parse = gst_element_factory_make (_parseName, "parse");
//	decoder = gst_element_factory_make (_decName, "decoder");
//	g_message(" parse = %p, decoder=%p\n",parse, decoder);

	g_object_set( decoder, "async-depth", 2, NULL );
	g_object_set( decoder, "live-mode", TRUE, NULL );
#endif

	if( me->video_sink != NULL )
	{
		if( _surfaceid != NULL )
		{
			uint32_t _num = ( uint32_t )atoi( _surfaceid );
			g_object_set( me->video_sink, "surfaceid", _num, NULL);
		}
		else
		{
			g_object_set( me->video_sink, "surfaceid", carplayObject->pb->surfaceid, NULL);
		}
		
#ifdef B511 //NAGIVI
		_videosinkDev	=	getenv( "MH_CARPLAY_VIDEO_SINK_DEVICE" );
		_videosinkDev = _videosinkDev ? _videosinkDev : "/dev/video10";
		g_message(" _videosinkDev = %s", _videosinkDev);
		g_object_set(me->video_sink, "device", _videosinkDev, NULL);
		g_object_set(me->video_sink, "sync", FALSE, NULL);
		g_object_set( me->pipeline, "video-sink", me->video_sink, NULL );
#endif
		g_message(" video sink name = %s\n",_sinkName);
	}

//	GstElement * _audio_sink;
//
//	_audio_sink	=	gst_element_factory_make( _audiosinkName, _audiosinkName );
//
//	if( _audio_sink != NULL )
//		g_object_set( me->pipeline, "audio-sink", _audio_sink, NULL );

	gst_bin_add_many (GST_BIN ( me->pipeline ), appsrc, parse, decoder, me->video_sink, NULL);
	rel = gst_element_link_many(appsrc, parse, decoder, me->video_sink, NULL);
	g_message("%s ===========================> rel = %d",__func__,rel);
	carplayObject->videoPipeline	=	me->pipeline;


	me->context	=	g_main_context_new();
	me->mainloop	=	g_main_loop_new( me->context, FALSE );

	g_thread_unref( g_thread_new( "screen_input", screen_input, me ));

	me->src	=	appsrc;
	_src	=	appsrc;

	g_object_set( appsrc, "stream-type", GST_APP_STREAM_TYPE_STREAM,  
			"is-live", TRUE, "block", TRUE,
			"format", GST_FORMAT_TIME, NULL );
	g_object_set( appsrc, "do-timestamp", TRUE, NULL );
	g_object_set( appsrc, "min-latency",(gint64)0, NULL );
	g_object_set( appsrc, "max-latency", (gint64)0, NULL );
	g_signal_connect( appsrc, "enough-data", G_CALLBACK( _enoughData ), appsrc );
//	GST_DEBUG_BIN_TO_DOT_FILE( GST_BIN(me->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "carplay_Null2Ready");

	*outStream = me;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ScreenStreamFinalize
//===========================================================================================================================

static void	_ScreenStreamFinalize( CFTypeRef inCF )
{
	ScreenStreamRef const		me = (ScreenStreamRef) inCF;
	
	DEBUG_USE_ONLY( me );
	
	g_message("%s", __func__);
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called when ScreenUtils is compiled into the AirPlay library, when the retain count of a ScreenStream 
	// object goes to zero.
	if( me->mainloop != NULL )
	{
		g_main_loop_quit( me->mainloop );
	}

	if( me->pipeline != NULL )
	{
		g_message("carplay pipeline unref");
		gst_element_set_state( me->pipeline, GST_STATE_NULL );
		gst_object_unref ( GST_OBJECT( me->pipeline ));
//		g_object_unref( me->pipeline );
	}
	ForgetCF( &me->avccData );

}
#endif

#if( SCREEN_STREAM_DLL )
//===========================================================================================================================
//	ScreenStreamInitialize
//===========================================================================================================================

OSStatus	ScreenStreamInitialize( ScreenStreamRef inStream )
{
	OSStatus				err;
	ScreenStreamImpRef		me;
	
	require_action( ScreenStreamGetContext( inStream ) == NULL, exit, err = kAlreadyInitializedErr );
	
	me = (ScreenStreamImpRef) calloc( 1, sizeof( *me ) );
	require_action( me, exit, err = kNoMemoryErr );

	me = (ScreenStreamImpRef) calloc( 1, sizeof( *me ) );
	require_action( me, exit, err = kNoMemoryErr );

	// $$$ TODO: Other initialization goes here.
	// This function is called (instead of ScreenStreamCreate()) when ScreenUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the initialization code should look very similar
	// to that in ScreenStreamCreate().
	
	ScreenStreamSetContext( inStream, me );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ScreenStreamFinalize
//===========================================================================================================================

void	ScreenStreamFinalize( ScreenStreamRef inStream )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	
	if( !me ) return;
	
	// $$$ TODO: Last chance to free any resources allocated by this object.
	// This function is called (instead of _ScreenStreamFinalize()) when ScreenUtils is built as a standalone shared object
	// that is loaded dynamically by AirPlay at runtime, so the finalization code should look very similar to that in
	// _ScreenStreamFinalize().
	// It is automatically invoked, when the retain count of an ScreenStream object goes to zero.

	ForgetCF( &me->avccData );
	free( me );
	ScreenStreamSetContext( inStream, NULL );
}
#endif

//===========================================================================================================================
//	_ScreenStreamSetProperty
//===========================================================================================================================

OSStatus
	_ScreenStreamSetProperty( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( (ScreenStreamRef) inObject );
	OSStatus						err;
	
	(void) inFlags;
	(void) inQualifier;
	
	if( 0 ) {}
	
	// AVCC
	
	else if( CFEqual( inProperty, kScreenStreamProperty_AVCC ) )
	{
		const uint8_t * _avccPtr;
		size_t _avccLen;
		GstBuffer * _buf;
		GstCaps * _caps;

		// $$$ TODO: If your video processing chain does not support AVCC, then you may need to convert this to
		// an Annex B header.  See H264ConvertAVCCtoAnnexBHeader() in MiscUtils.h.
	
		require_action( CFIsType( inValue, CFData ), exit, err = kTypeErr );
		ReplaceCF( &me->avccData, inValue );
		
		_avccPtr	=	CFDataGetBytePtr( (CFDataRef)me->avccData );
		
		_avccLen	=	CFDataGetLength( (CFDataRef)me->avccData );

//		_buf	=	gst_buffer_new_and_alloc( _avccLen );
//		memcpy( GST_BUFFER_DATA( _buf ), _avccPtr, _avccLen );

//		_caps	=	gst_caps_new_simple( "video/x-h264",
//				"codec_data", GST_TYPE_BUFFER, _buf,
//				NULL );

		_buf = gst_buffer_new_allocate (NULL, _avccLen, NULL);	
		gst_buffer_fill (_buf, 0, _avccPtr, _avccLen);

		_caps 	=	gst_caps_new_simple ("video/x-h264",
//				"parsed", G_TYPE_BOOLEAN, TRUE,
				"stream-format", G_TYPE_STRING, "avc",
				"codec_data", GST_TYPE_BUFFER, _buf,
				"alignment", G_TYPE_STRING,"au",
//				"profile", G_TYPE_STRING, "baseline",
				"width", G_TYPE_INT,  me->widthPixels,
				"height", G_TYPE_INT,  me->heightPixels,
				"framerate", GST_TYPE_FRACTION, 60, 1,
				NULL);

		g_object_set( me->src, "caps", _caps,
				NULL );

		gst_buffer_unref( _buf );
		gst_caps_unref( _caps );

#ifdef DUP_SCREEN_DATA
		fp	=	fopen( "/tmp/screen.cap", "wb" );

		fwrite( &_avccLen, sizeof( _avccLen ), 1, fp );
		fwrite( _avccPtr, _avccLen, 1, fp );
#endif

	}
	
	// Pixel dimensions
	
	else if( CFEqual( inProperty, kScreenStreamProperty_WidthPixels ) )
	{
		me->widthPixels = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, exit );
	}
	else if( CFEqual( inProperty, kScreenStreamProperty_HeightPixels ) )
	{
		me->heightPixels = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, exit );
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
//	ScreenStreamStart
//===========================================================================================================================

OSStatus	ScreenStreamStart( ScreenStreamRef inStream )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	OSStatus						err;
	const char * _sinkName;
	GstElement * _sink = NULL ;
	g_message("%s", __func__);

	// $$$ TODO: This is where the video processing chain should be started.
	// Once this function returns, ScreenStreamProcessData() will be called continuously, providing H.264 bit-stream data
	// to be decoded and displayed.

	g_object_get( me->pipeline, "video-sink", &_sink, NULL );

	if( _sink != NULL )
	{
		g_warning( "%s %d %d %d %d", gst_element_get_name( _sink ), me->leftPixels, me->topPixels, 
				me->widthPixels, me->heightPixels );
		if( g_strstr_len( gst_element_get_name( _sink ), -1, "glimagesink" ) != NULL )
		{
			g_object_set( _sink, "window-x", me->leftPixels, "window-y", me->topPixels, 
					"window-width", me->widthPixels, "window-height", me->heightPixels, NULL );
		}
		else
		if( g_strstr_len( gst_element_get_name( _sink ), -1, "mfw_isink" ) != NULL )
		{
			g_object_set( _sink, "axis-left", me->leftPixels, "axis-top", me->topPixels, 
					"disp-width", me->widthPixels, "disp-height", me->heightPixels, NULL );
		}
		else
		if( g_strstr_len( gst_element_get_name( _sink ), -1, "v4l2sink" ) != NULL )
		{
			g_message("left = %d top = %d width = %d height = %d\n",
				me->leftPixels,	me->topPixels, me->widthPixels, me->heightPixels);
//			g_object_set( _sink, "axis-left", me->leftPixels, "axis-top", me->topPixels, 
//					"disp-width", me->widthPixels, "disp-height", me->heightPixels, NULL );
			g_object_set(G_OBJECT( _sink ),
			"overlay-set-left", 	me->leftPixels,
			"overlay-set-top", me->topPixels,
			"overlay-set-width", me->widthPixels,
			"overlay-set-height",  me->heightPixels,
			"overlay-set-update", 1,
			NULL );
//			g_object_set(G_OBJECT( _sink ),
//			"overlay-set-left", 	0,
//			"overlay-set-top", 0,
//			"overlay-set-width", 1920,
//			"overlay-set-height", 1080,
//			"overlay-set-update", 1,
//			NULL );
//			g_message("%s hahahahahahahahahahahaha");
		}
		else
		if( g_strstr_len( gst_element_get_name( _sink ), -1, "mfw_v4lsink" ) != NULL )
		{
			g_object_set( _sink, "axis-left", me->leftPixels, "axis-top", me->topPixels, 
					"disp-width", me->widthPixels, "disp-height", me->heightPixels, NULL );
		}
	}

	gst_element_set_state( me->pipeline, GST_STATE_PLAYING );

	me->startticks	=	UpTicks();

	err = kNoErr;
	ss_ulog( kLogLevelNotice, "Screen stream started\n" );
//	GST_DEBUG_BIN_TO_DOT_FILE( GST_BIN(me->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "carplay_Ready2Playing");
	
	if( err )
	{
		ss_ulog( kLogLevelError, "### Start screen stream failed: %#m\n", err );
		ScreenStreamStop( inStream );
	}
	return( err );
}

//===========================================================================================================================
//	ScreenStreamStop
//===========================================================================================================================

void	ScreenStreamStop( ScreenStreamRef inStream )
{
	g_message("%s", __func__);

	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );

	// $$$ TODO: This is where the video processing chain should be stopped.
	// This function is responsible for releasing any resources allocated in ScreenStreamStart().
	gst_element_set_state( me->pipeline, GST_STATE_READY );

	ForgetCF( &me->avccData );
	ss_ulog( kLogLevelNotice, "Screen stream stopped\n" );
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  emitBuf
 *  Description:  
 * =====================================================================================
 */
static gboolean emitBuf( gpointer user_data )
{
	GstFlowReturn _ret;
	GstBuffer * _buf	=	( GstBuffer * )user_data;

	g_signal_emit_by_name( _src, "push-buffer", _buf, &_ret );

	gst_buffer_unref( _buf );

	return G_SOURCE_REMOVE;
}		/* -----  end of static function emitBuf  ----- */


//===========================================================================================================================
//	ScreenStreamProcessData
//===========================================================================================================================

OSStatus
	ScreenStreamProcessData( 
		ScreenStreamRef				inStream, 
		const uint8_t *				inData,
		size_t						inLen, 
		uint64_t					inDisplayTicks, 
		CFDictionaryRef				inOptions, 
		ScreenStreamCompletion_f	inCompletion, 
		void *						inContext )
{
	ScreenStreamImpRef const		me = _ScreenStreamGetImp( inStream );
	OSStatus						err;
	GstBuffer * _buf;
	GSource * _source;
	
	(void) inOptions;

//	_buf	=	gst_buffer_new_and_alloc( inLen );

//	memcpy( GST_BUFFER_DATA( _buf ), inData, inLen );
//	GST_BUFFER_TIMESTAMP( _buf )	=	inDisplayTicks - me->startticks;

//	guint8 * data	=	g_malloc0( inLen );
//	memcpy( data, inData, inLen );
//
//	_buf	=	gst_buffer_new();
//	gst_buffer_append_memory(_buf,
//			gst_memory_new_wrapped( GST_MEMORY_FLAG_READONLY,
//				data, inLen, 0, inLen, NULL, NULL));
//	g_free( data );

	_buf = gst_buffer_new_allocate (NULL, inLen, NULL);	
	gst_buffer_fill (_buf, 0, inData, inLen);

//	GSTy_BUFFER_PTS( _buf )	=	inDisplayTicks - me->startticks;

	_source	=	g_idle_source_new();
	g_source_set_callback( _source, emitBuf, _buf, NULL );
	g_source_attach( _source, me->context );
	g_source_unref( _source );

#ifdef DUP_SCREEN_DATA	
	fwrite( &inDisplayTicks, sizeof( inDisplayTicks ), 1, fp );
	fwrite( &inLen, sizeof( inLen ), 1, fp );
	fwrite( inData, inLen, 1, fp );

	fflush( fp );
#endif
	
	// $$$ TODO: Decode an H.264 frame and enqueue it for display at time inDisplayTicks.
	// After ScreenStreamStart() has been called, this function will be invoked any time there is a new H.264 frame
	// available for decode and display.
	// inDisplayTicks is the time, in units of ticks, that the provided frame is expected to be displayed on screen.
	// It is important that the video render chain honour this timestamp to ensure proper A/V sync.
	// Note that inDisplayTicks may need to be converted to another unit (like microseconds or nanoseconds) -- TickUtils.h
	// has a number of helper routines for doing this conversion.
	// The data will be provided in AVCC format.  If your video processing chain does not support AVCC, then the contents
	// of inData may need to be preprocessed here before sending it to the decoder.
	// This function currently expects the frame to be decoded (but not necessarily displayed) synchronously; that is,
	// when this function returns, there should be no expectation that the memory pointed to by inData will remain valid.
	(void) inData;
	(void) inLen;
	(void) inDisplayTicks;
	(void) me;

	err = kNoErr;
	
	if( inCompletion ) inCompletion( inContext );
	if( err ) ss_ulog( kLogLevelError, "### Screen stream process data failed: %#m\n", err );
	return( err );
}
