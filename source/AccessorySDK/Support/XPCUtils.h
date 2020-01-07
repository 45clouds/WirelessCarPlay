/*
	File:    	XPCUtils.h
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
	
	Copyright (C) 2011-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__XPCUtils_h__
#define	__XPCUtils_h__

#include "CommonServices.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER
#include XPC_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if( !XPC_LITE_ENABLED )
	typedef void ( *xpc_handler_f )( xpc_object_t inObject, void *inContext );
	
	#define XPC_CONNECTION_SERVICE_LISTENER			XPC_CONNECTION_MACH_SERVICE_LISTENER
	#define XPC_CONNECTION_SERVICE_PRIVILEGED		XPC_CONNECTION_MACH_SERVICE_PRIVILEGED
	
	#define xpc_connection_create_named_service		xpc_connection_create_mach_service
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_connection_has_entitlement
	@abstract	Returns true if the connection has the specified entitlement and it's true.
*/
bool	xpc_connection_has_entitlement( xpc_connection_t inConnection, const char *inEntitlementName );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_connection_send_message_with_reply_f
	@abstract	Function callback version of xpc_connection_send_message_with_reply.
	@discussion	This allows for implementations of XPC that don't support blocks.
*/
void
	xpc_connection_send_message_with_reply_f( 
		xpc_connection_t	inCnx, 
		xpc_object_t		inMsg, 
		dispatch_queue_t	inQueue, 
		xpc_handler_f		inHandler, 
		void *				inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_connection_set_event_handler_f
	@abstract	Function callback version of xpc_connection_set_event_handler.
	@discussion	This allows for implementations of XPC that don't support blocks.
*/
void	xpc_connection_set_event_handler_f( xpc_connection_t inCnx, xpc_handler_f inHandler, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_dictionary_copy_cf_object
	@abstract	Gets a dictionary value and converts it to an equivalent CF object.
	@discussion	See CFXPCBridge.h for details on the conversion process.
*/
CF_RETURNS_RETAINED
CFTypeRef	xpc_dictionary_copy_cf_object( xpc_object_t inDict, const char *inKey, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_dictionary_set_cf_object
	@abstract	Sets a dictionary value by convert a CF object to an equivalent XPC object.
	@discussion	See CFXPCBridge.h for details on the conversion process.
*/
OSStatus	xpc_dictionary_set_cf_object( xpc_object_t inDict, const char *inKey, CFTypeRef inCFObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	xpc_send_message_sync
	@abstract	One-shot function to connect to a service, send a message, and wait for a reply.
	
	@param		inService		Name of the XPC service to send the message to.
	@param		inFlags			Flags to pass to xpc_connection_create_mach_service().
	@param		inUID			User-session UID to target. Use 0 if you're not targetting a user session.
	@param		inMsg			Message to send.
	@param		outReply		Receives reply message. May be NULL if a reply is not needed.
	
	Note: If a reply is not needed the a barrier block will be used to guarantee that the message will be sent even if
	the caller quits the process immediately after this function returns.
*/
OSStatus	xpc_send_message_sync( const char *inService, uint64_t inFlags, uid_t inUID, xpc_object_t inMsg, xpc_object_t *outReply );

#ifdef __cplusplus
}
#endif

#endif // __XPCUtils_h__
