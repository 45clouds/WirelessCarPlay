/*
	File:    	MFiServerPlatformQNX.c
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
	
	QNX platform plugin for MFi-SAP authentication/encryption.
	
	This defaults to using i2c bus /dev/i2c99 with an MFi auth IC device address of 0x11 (RST pulled high).
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DMFI_AUTH_DEVICE_PATH=\"/dev/i2c99\"		# MFi auth IC on i2c bus /dev/i2c99.
	CFLAGS += -DMFI_AUTH_DEVICE_ADDRESS=0x10			# MFi auth IC at address 0x10 (RST pulled low).
*/

#include "MFiSAP.h"

#include <devctl.h>
#include <hw/i2c.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "TickUtils.h"

//===========================================================================================================================
//	Constants
//===========================================================================================================================

//#if( defined( MFI_AUTH_DEVICE_PATH ) )
//	#define kMFiAuthDevicePath					MFI_AUTH_DEVICE_PATH
//#else
//	#define kMFiAuthDevicePath					"/dev/i2c99"
//#endif

//#if( defined( MFI_AUTH_DEVICE_ADDRESS ) )
//	#define kMFiAuthDeviceAddress				MFI_AUTH_DEVICE_ADDRESS
//#else
//	#define kMFiAuthDeviceAddress				0x11
//#endif

#define kMFiAuthDevicePath					getenv("MH_APPLE_AUTH_COPROCESSOR_ADDRESS")?getenv("MH_APPLE_AUTH_COPROCESSOR_ADDRESS") : "/dev/i2c99"
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

static OSStatus	_ReadI2C( int inFD, uint8_t inRegister, uint8_t *inReadBuf, size_t inReadLen );
static OSStatus	_WriteI2C( int inFD, uint8_t inRegister, const uint8_t *inWritePtr, size_t inWriteLen );

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
	Boolean			locked = false;
	uint8_t			buf[ 32 ];
	size_t			signatureLen;
	uint8_t *		signaturePtr;
	
	dlog( kLogLevelVerbose, "MFi auth create signature\n" );
	
	fd = open( kMFiAuthDevicePath, O_RDWR );
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	err = devctl( fd, DCMD_I2C_LOCK, NULL, 0, NULL );
	require_noerr( err, exit );
	locked = true;
	
	// Write the data to sign.
	// Note: writes to the size register auto-increment to the data register that follows it.
	
	require_action( inDigestLen == 20, exit, err = kSizeErr );
	buf[ 0 ] = (uint8_t)( ( inDigestLen >> 8 ) & 0xFF );
	buf[ 1 ] = (uint8_t)(   inDigestLen        & 0xFF );
	memcpy( &buf[ 2 ], inDigestPtr, inDigestLen );
	err = _WriteI2C( fd, kMFiAuthReg_ChallengeSize, buf, 2 + inDigestLen );
	require_noerr( err, exit );
	
	// Generate the signature.
	
	buf[ 0 ] = kMFiAuthControl_GenerateSignature;
	err = _WriteI2C( fd, kMFiAuthReg_AuthControlStatus, buf, 1 );
	require_noerr( err, exit );
	
	err = _ReadI2C( fd, kMFiAuthReg_AuthControlStatus, buf, 1 );
	require_noerr( err, exit );
	require_action( !( buf[ 0 ] & kMFiAuthFlagError ), exit, err = kUnknownErr );
	
	// Read the signature.
	
	err = _ReadI2C( fd, kMFiAuthReg_SignatureSize, buf, 2 );
	require_noerr( err, exit );
	signatureLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
	require_action( signatureLen > 0, exit, err = kSizeErr );
	
	signaturePtr = (uint8_t *) malloc( signatureLen );
	require_action( signaturePtr, exit, err = kNoMemoryErr );
	
	err = _ReadI2C( fd, kMFiAuthReg_SignatureData, signaturePtr, signatureLen );
	if( err ) free( signaturePtr );
	require_noerr( err, exit );
	
	dlog( kLogLevelVerbose, "MFi auth created signature:\n%.2H\n", signaturePtr, (int) signatureLen, (int) signatureLen );
	*outSignaturePtr = signaturePtr;
	*outSignatureLen = signatureLen;
	
exit:
	if( locked )	devctl( fd, DCMD_I2C_UNLOCK, NULL, 0, NULL );
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
	Boolean			locked = false;
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
	
	err = devctl( fd, DCMD_I2C_LOCK, NULL, 0, NULL );
	require_noerr( err, exit );
	locked = true;
	
	// Read the certificate.
	// Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.
	
	err = _ReadI2C( fd, kMFiAuthReg_DeviceCertificateSize, buf, 2 );
	require_noerr( err, exit );
	certificateLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
	require_action( certificateLen > 0, exit, err = kSizeErr );
	
	certificatePtr = (uint8_t *) malloc( certificateLen );
	require_action( certificatePtr, exit, err = kNoMemoryErr );
	
	err = _ReadI2C( fd, kMFiAuthReg_DeviceCertificateData1, certificatePtr, certificateLen );
	if( err ) free( certificatePtr );
	require_noerr( err, exit );
	
	dlog( kLogLevelVerbose, "MFi auth copy certificate done: %zu bytes\n", certificateLen );
	*outCertificatePtr = certificatePtr;
	*outCertificateLen = certificateLen;
	
exit:
	if( locked )	devctl( fd, DCMD_I2C_UNLOCK, NULL, 0, NULL );
	if( fd >= 0 )	close( fd );
	if( err )		dlog( kLogLevelWarning, "### MFi auth copy certificate failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_ReadI2C
//===========================================================================================================================

static OSStatus	_ReadI2C( int inFD, uint8_t inRegister, uint8_t *inReadBuf, size_t inReadLen )
{
	OSStatus			err;
	uint64_t			deadline;
	size_t				len;
	i2c_send_t			sendHeader;
	i2c_recv_t			recvHeader;
	struct iovec		rio[ 2 ], sio[ 2 ];
	int					tries, n;
	
	deadline = UpTicks() + SecondsToUpTicks( 2 );
	do
	{
		len = Min( inReadLen, 128 ); // Restrict to 128 byte chunks to work around issues on USB-based auth IC board.
		for( tries = 1; ; ++tries )
		{
			sendHeader.slave.addr	= kMFiAuthDeviceAddress;
			sendHeader.slave.fmt	= I2C_ADDRFMT_7BIT;
			sendHeader.len			= sizeof( inRegister );
			sendHeader.stop			= true;
			SETIOV( &sio[ 0 ], &sendHeader, sizeof( sendHeader ) );
			SETIOV( &sio[ 1 ], &inRegister, sizeof( inRegister ) );
			SETIOV( &rio[ 0 ], &sendHeader, sizeof( sendHeader ) );
			
			n = 0;
			err = devctlv( inFD, DCMD_I2C_SEND, 2, 1, sio, rio, &n );
			if( !err ) break;
			
			usleep( kMFiAuthRetryDelayMics );
			require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
		}
		
		usleep( kMFiAuthRetryDelayMics );
		
		for( tries = 1; ; ++tries )
		{
			recvHeader.slave.addr	= kMFiAuthDeviceAddress;
			recvHeader.slave.fmt	= I2C_ADDRFMT_7BIT;
			recvHeader.len			= len;
			recvHeader.stop			= true;
			SETIOV( &sio[ 0 ], &recvHeader, sizeof( recvHeader ) );
			SETIOV( &rio[ 0 ], &recvHeader, sizeof( recvHeader ) );
			SETIOV( &rio[ 1 ], inReadBuf, len );
			
			n = 0;
			err = devctlv( inFD, DCMD_I2C_RECV, 1, 2, sio, rio, &n );
			if( !err && ( n == ( (int) len ) ) ) break;
			
			dlog( kLogLevelVerbose, "### MFi auth 0x%02X read register 0x%02X, %zu bytes (try %d, %d bytes) failed: %#m\n", 
				kMFiAuthDeviceAddress, inRegister, len, tries, n, err );
			usleep( kMFiAuthRetryDelayMics );
			require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
		}
		inReadBuf  += len;
		inReadLen  -= len;
		inRegister += 1;
		
	}	while( inReadLen > 0 );
	
exit:
	if( err ) dlog( kLogLevelWarning, "### MFi auth read register 0x%02X, %zu bytes failed after %d tries: %#m\n", 
		inRegister, inReadLen, tries, err );
	return( err );
}

//===========================================================================================================================
//	_WriteI2C
//===========================================================================================================================

static OSStatus	_WriteI2C( int inFD, uint8_t inRegister, const uint8_t *inWritePtr, size_t inWriteLen )
{
	OSStatus			err;
	uint64_t			deadline;
	i2c_send_t			header;
	struct iovec		rio[ 1 ], sio[ 3 ];
	int					tries, n;
	
	deadline = UpTicks() + SecondsToUpTicks( 2 );
	for( tries = 1; ; ++tries )
	{
		header.slave.addr	= kMFiAuthDeviceAddress;
		header.slave.fmt	= I2C_ADDRFMT_7BIT;
		header.len			= sizeof( inRegister ) + inWriteLen;
		header.stop			= true;
		SETIOV( &sio[ 0 ], &header,     sizeof( header ) );
		SETIOV( &sio[ 1 ], &inRegister, sizeof( inRegister ) );
		SETIOV( &sio[ 2 ], inWritePtr,	inWriteLen );
		SETIOV( &rio[ 0 ], &header,     sizeof( header ) );
		
		err = devctlv( inFD, DCMD_I2C_SEND, 3, 1, sio, rio, &n );
		if( !err && ( n == ( (int) header.len ) ) ) break;
		
		dlog( kLogLevelVerbose, "### MFi auth 0x%02X write register 0x%02X, %zu bytes (try %d, %d bytes) failed: %#m\n", 
			kMFiAuthDeviceAddress, inRegister, inWriteLen, tries, n, err );
		usleep( kMFiAuthRetryDelayMics );
		require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
	}
	
exit:
	if( err ) dlog( kLogLevelWarning, "### MFi auth write register 0x%02X, %zu bytes failed after %d tries: %#m\n", 
		inRegister, inWriteLen, tries, err );
	return( err );
}
