/*
	File:    	MFiServerPlatformLinux.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ‚ÄùPublic 
	Software‚Ä? and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in¬†consideration of your agreement to abide by them, Apple grants
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
	fixes or enhancements to Apple in connection with this software (‚ÄúFeedback‚Ä?, you hereby grant to
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
	
	Linux platform plugin for MFi-SAP authentication/encryption.
	
	This defaults to using i2c bus /dev/i2c-1 with an MFi auth IC device address of 0x11 (RST pulled high).
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DMFI_AUTH_DEVICE_PATH=\"/dev/i2c-0\"		# MFi auth IC on i2c bus /dev/i2c-0.
	CFLAGS += -DMFI_AUTH_DEVICE_ADDRESS=0x10			# MFi auth IC at address 0x10 (RST pulled low).
*/

#include "MFiSAP.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "TickUtils.h"

#include <glib.h>
//===========================================================================================================================
//	Constants
//===========================================================================================================================

//#if( defined( MFI_AUTH_DEVICE_PATH ) )
//	#define kMFiAuthDevicePath					MFI_AUTH_DEVICE_PATH
//#else
//	#define kMFiAuthDevicePath					"/dev/i2c-1"
//#endif

//#if( defined( MFI_AUTH_DEVICE_ADDRESS ) )
//	#define kMFiAuthDeviceAddress				MFI_AUTH_DEVICE_ADDRESS
//#else
//	#define kMFiAuthDeviceAddress				0x11
//#endif

#define kMFiAuthDevicePath					getenv("MH_APPLE_AUTH_COPROCESSOR_ADDRESS")?getenv("MH_APPLE_AUTH_COPROCESSOR_ADDRESS") : "/dev/i2c-1"
#define kMFiAuthDeviceAddress				getenv("MH_I2C_DEV_ADDR")?(guint)atoi( getenv("MH_I2C_DEV_ADDR")) : 0x11

#define kMFiAuthRetryDelayMics					5000 // 5 ms.

#define kMFiAuthReg_AuthControlStatus			0x10
	#define kMFiAuthFlagError						0x80
	#define kMFiAuthControl_GenerateSignature		1
#define kMFiAuthReg_SignatureSize				0x11
#define kMFiAuthReg_SignatureData				0x12
#define kMFiAuthReg_ChallengeSize				0x20
#define kMFiAuthReg_ChallengeData				0x21
#define kMFiAuthReg_DeviceCertificateSize		0x30
#define kMFiAuthReg_DeviceCertificateData1		0x31 // Note: auto-increments so next read is Data2, Data3, etc.

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus
	_DoI2C( 
		int				inFD, 
		uint8_t			inRegister, 
		const uint8_t *	inWritePtr, 
		size_t			inWriteLen, 
		uint8_t *		inReadBuf, 
		size_t			inReadLen );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static uint8_t *		gMFiCertificatePtr	= NULL;
static size_t			gMFiCertificateLen	= 0;

//===========================================================================================================================
//	MFiPlatform_Initialize
//===========================================================================================================================

OSStatus	MFiPlatform_Initialize( void )
{
	// Cache the certificate at startup since the certificate doesn't change and this saves ~200 ms each time.
	
	MFiPlatform_CopyCertificate( &gMFiCertificatePtr, &gMFiCertificateLen );
	return( kNoErr );
}

//===========================================================================================================================
//	MFiPlatform_Finalize
//===========================================================================================================================

void	MFiPlatform_Finalize( void )
{
	ForgetMem( &gMFiCertificatePtr );
	gMFiCertificateLen = 0;
}

//===========================================================================================================================
//	MFiPlatform_CreateSignature
//===========================================================================================================================

OSStatus
	MFiPlatform_CreateSignature( 
		const void *	inDigestPtr, 
		size_t			inDigestLen, 
		uint8_t **		outSignaturePtr,
		size_t *		outSignatureLen )
{
	OSStatus		err;
	int				fd;
	uint8_t			buf[ 32 ];
	size_t			signatureLen;
	uint8_t *		signaturePtr;
	
	dlog( kLogLevelVerbose, "MFi auth create signature\n" );
	
	fd = open( kMFiAuthDevicePath, O_RDWR );
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	err = ioctl( fd, I2C_SLAVE, kMFiAuthDeviceAddress );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	// Write the data to sign.
	// Note: writes to the size register auto-increment to the data register that follows it.
	
	require_action( inDigestLen == 20, exit, err = kSizeErr );
	buf[ 0 ] = (uint8_t)( ( inDigestLen >> 8 ) & 0xFF );
	buf[ 1 ] = (uint8_t)(   inDigestLen        & 0xFF );
	memcpy( &buf[ 2 ], inDigestPtr, inDigestLen );
	err = _DoI2C( fd, kMFiAuthReg_ChallengeSize, buf, 2 + inDigestLen, NULL, 0 );
	require_noerr( err, exit );
	
	// Generate the signature.
	
	buf[ 0 ] = kMFiAuthControl_GenerateSignature;
	err = _DoI2C( fd, kMFiAuthReg_AuthControlStatus, buf, 1, NULL, 0 );
	require_noerr( err, exit );
	
	err = _DoI2C( fd, kMFiAuthReg_AuthControlStatus, NULL, 0, buf, 1 );
	require_noerr( err, exit );
	require_action( !( buf[ 0 ] & kMFiAuthFlagError ), exit, err = kUnknownErr );
	
	// Read the signature.
	
	err = _DoI2C( fd, kMFiAuthReg_SignatureSize, NULL, 0, buf, 2 );
	require_noerr( err, exit );
	signatureLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
	require_action( signatureLen > 0, exit, err = kSizeErr );
	
	signaturePtr = (uint8_t *) malloc( signatureLen );
	require_action( signaturePtr, exit, err = kNoMemoryErr );
	
	err = _DoI2C( fd, kMFiAuthReg_SignatureData, NULL, 0, signaturePtr, signatureLen );
	if( err ) free( signaturePtr );
	require_noerr( err, exit );
	
	dlog( kLogLevelVerbose, "MFi auth created signature:\n%.2H\n", signaturePtr, (int) signatureLen, (int) signatureLen );
	*outSignaturePtr = signaturePtr;
	*outSignatureLen = signatureLen;
	
exit:
	if( fd >= 0 )	close( fd );
	if( err )		dlog( kLogLevelWarning, "### MFi auth create signature failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	MFiPlatform_CopyCertificate
//===========================================================================================================================

OSStatus	MFiPlatform_CopyCertificate( uint8_t **outCertificatePtr, size_t *outCertificateLen )
{
	OSStatus		err;
	size_t			certificateLen;
	uint8_t *		certificatePtr;
	int				fd = -1;
	uint8_t			buf[ 2 ];
	
	dlog( kLogLevelVerbose, "MFi auth copy certificate\n" );
	
	// If the certificate has already been cached then return that as an optimization since it doesn't change.
	
	if( gMFiCertificateLen > 0 )
	{
		certificatePtr = (uint8_t *) malloc( gMFiCertificateLen );
		require_action( certificatePtr, exit, err = kNoMemoryErr );
		memcpy( certificatePtr, gMFiCertificatePtr, gMFiCertificateLen );
		
		*outCertificatePtr = certificatePtr;
		*outCertificateLen = gMFiCertificateLen;
		err = kNoErr;
		goto exit;
	}
	
	fd = open( kMFiAuthDevicePath, O_RDWR );
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	err = ioctl( fd, I2C_SLAVE, kMFiAuthDeviceAddress );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	err = _DoI2C( fd, kMFiAuthReg_DeviceCertificateSize, NULL, 0, buf, 2 );
	require_noerr( err, exit );
	certificateLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
	require_action( certificateLen > 0, exit, err = kSizeErr );
	
	certificatePtr = (uint8_t *) malloc( certificateLen );
	require_action( certificatePtr, exit, err = kNoMemoryErr );
	
	// Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.
	
	err = _DoI2C( fd, kMFiAuthReg_DeviceCertificateData1, NULL, 0, certificatePtr, certificateLen );
	if( err ) free( certificatePtr );
	require_noerr( err, exit );
	
	dlog( kLogLevelVerbose, "MFi auth copy certificate done: %zu bytes\n", certificateLen );
	*outCertificatePtr = certificatePtr;
	*outCertificateLen = certificateLen;
	
exit:
	if( fd >= 0 )	close( fd );
	if( err )		dlog( kLogLevelWarning, "### MFi auth copy certificate failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_DoI2C
//===========================================================================================================================

static OSStatus
	_DoI2C( 
		int				inFD, 
		uint8_t			inRegister, 
		const uint8_t *	inWritePtr, 
		size_t			inWriteLen, 
		uint8_t *		inReadBuf, 
		size_t			inReadLen )
{
	OSStatus		err;
	uint64_t		deadline;
	int				tries;
	ssize_t			n;
	uint8_t			buf[ 1 + inWriteLen ];
	size_t			len;
	
	deadline = UpTicks() + SecondsToUpTicks( 2 );
	if( inReadBuf )
	{
		// Combined mode transactions are not supported so set the register and do a separate read.
		
		for( tries = 1; ; ++tries )
		{
			n = write( inFD, &inRegister, 1 );
			err = map_global_value_errno( n == 1, n );
			if( !err ) break;
			
			dlog( kLogLevelVerbose, "### MFi auth set register 0x%02X failed (try %d): %#m\n", inRegister, tries, err );
			usleep( kMFiAuthRetryDelayMics );
			require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
		}
		for( tries = 1; ; ++tries )
		{
			n = read( inFD, inReadBuf, inReadLen );
			err = map_global_value_errno( n == ( (ssize_t) inReadLen ), n );
			if( !err ) break;
			
			dlog( kLogLevelVerbose, "### MFi auth read register 0x%02X, %zu bytes failed (try %d): %#m\n", 
				inRegister, inReadLen, tries, err );
			usleep( kMFiAuthRetryDelayMics );
			require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
		}
	}
	else
	{
		// Gather the register and data so it can be done as a single write transaction.
		
		buf[ 0 ] = inRegister;
		memcpy( &buf[ 1 ], inWritePtr, inWriteLen );
		len = 1 + inWriteLen;
		
		for( tries = 1; ; ++tries )
		{
			n = write( inFD, buf, len );
			err = map_global_value_errno( n == ( (ssize_t) len ), n );
			if( !err ) break;
			
			dlog( kLogLevelVerbose, "### MFi auth write register 0x%02X, %zu bytes failed (try %d): %#m\n", 
				inRegister, inWriteLen, tries, err );
			usleep( kMFiAuthRetryDelayMics );
			require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
		}
	}
	
exit:
	if( err ) dlog( kLogLevelWarning, "### MFi auth register 0x%02X failed after %d tries: %#m\n", inRegister, tries, err );
	return( err );
}
