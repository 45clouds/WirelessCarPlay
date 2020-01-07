/*
	File:    	MFiServerPlatformSTM32F2xx.c
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
	
	Copyright (C) 2012-2013 Apple Inc. All Rights Reserved.
	
	STM32f2xx platform plugin for MFi-SAP authentication/encryption.
	
	This defaults to the MFi auth IC being at device address of 0x22 (RST pulled high).
	This can be overridden in the makefile with the following:
	
	CFLAGS += -DMFI_AUTH_DEVICE_ADDRESS=0x10 # MFi auth IC at address 0x10 (RST pulled low).
*/

#include "MFiSAP.h"

#include "stm32f2xx_gpio.h"
#include "stm32f2xx_i2c.h"
#include "stm32f2xx.h"
#include "tx_api.h"

//===========================================================================================================================
//	BCMUSI11 Hardware Constants
//===========================================================================================================================

#define kSTM32DeviceAddress				0xA0

#define CP_I2C							I2C2
#define CP_I2C_CLK						RCC_APB1Periph_I2C2
#define CP_I2C_SCL_PIN					GPIO_Pin_10				// PB10
#define CP_I2C_SCL_GPIO_PORT			GPIOB					// GPIOB
#define CP_I2C_SCL_GPIO_CLK				RCC_AHB1Periph_GPIOB
#define CP_I2C_SCL_SOURCE				GPIO_PinSource10
#define CP_I2C_SCL_AF					GPIO_AF_I2C2
#define CP_I2C_SDA_PIN					GPIO_Pin_11				// PB11
#define CP_I2C_SDA_GPIO_PORT			GPIOB					// GPIOB
#define CP_I2C_SDA_GPIO_CLK				RCC_AHB1Periph_GPIOB
#define CP_I2C_SDA_SOURCE				GPIO_PinSource11
#define CP_I2C_SDA_AF					GPIO_AF_I2C2
#define CP_RST_PIN						GPIO_Pin_2				// PB02
#define CP_RST_GPIO_PORT				GPIOB					// GPIOB
#define CP_RST_GPIO_CLK					RCC_AHB1Periph_GPIOB
#define CP_RST_SOURCE					GPIO_PinSource2
#define CP_I2C_SPEED					100000					// 100kHz

// DMA

#define CP_I2C_DMA						DMA1
#define CP_I2C_DMA_CHANNEL				DMA_Channel_7
#define CP_I2C_DMA_STREAM_TX			DMA1_Stream7
#define CP_I2C_DMA_STREAM_RX			DMA1_Stream2
#define CP_I2C_DMA_CLK					RCC_AHB1Periph_DMA1
#define CP_I2C_DR_ADDRESS				( (uint32_t) &( CP_I2C->DR ) ) // Set to the data register of the CP I2C bus
#define CP_USE_DMA

#define CP_TX_DMA_FLAG_FEIF				DMA_FLAG_FEIF7
#define CP_TX_DMA_FLAG_DMEIF			DMA_FLAG_DMEIF7
#define CP_TX_DMA_FLAG_TEIF				DMA_FLAG_TEIF7
#define CP_TX_DMA_FLAG_HTIF				DMA_FLAG_HTIF7
#define CP_TX_DMA_FLAG_TCIF				DMA_FLAG_TCIF7
#define CP_RX_DMA_FLAG_FEIF				DMA_FLAG_FEIF2
#define CP_RX_DMA_FLAG_DMEIF			DMA_FLAG_DMEIF2
#define CP_RX_DMA_FLAG_TEIF				DMA_FLAG_TEIF2
#define CP_RX_DMA_FLAG_HTIF				DMA_FLAG_HTIF2
#define CP_RX_DMA_FLAG_TCIF				DMA_FLAG_TCIF2

#define CP_DIRECTION_TX					0
#define CP_DIRECTION_RX					1

// Maximum Timeout values for flags and events waiting loops. These timeouts are not based on accurate values, they 
// just guarantee that the application will not remain stuck if the I2C communication is corrupted.

#define CP_FLAG_TIMEOUT					( (uint32_t) 0x1000 )
#define CP_LONG_TIMEOUT					( (uint32_t)( 10 * CP_FLAG_TIMEOUT ) )
#define CP_RESET_DETECTION				( (uint16_t) 1250 )	// 10us at 120MHz clock rate.
#define kSTM32OneSecondTimeout			120000000			// 1 second at 120MHz clock rate.

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kMFiAuthOverallMaxTries					3		// Number of times to try a logical group of I2C transactions.
#define kMFiAuthIndividualMaxTries				20		// Number of times to try a single I2C transaction.
#define kMFiAuthRetryDelayTicks					10		// 10 ms.

#define kMFiAuthDeviceAddress				getenv("MH_I2C_DEV_ADDR")?(guint)atoi( getenv("MH_I2C_DEV_ADDR")) : 0x22
//#if( defined( MFI_AUTH_DEVICE_ADDRESS ) )
//	#define kMFiAuthDeviceAddress				MFI_AUTH_DEVICE_ADDRESS
//#else
//	#define kMFiAuthDeviceAddress				0x22	// RST has a pull-up resistor, which decides the address.
//#endif

#define kMFiAuthReg_AuthControlStatus			0x10
	#define kMFiAuthFlagError						0x80
	#define kMFiAuthControl_GenerateSignature		1

#define kMFiAuthReg_SignatureSize				0x11
#define kMFiAuthReg_SignatureData				0x12
#define kMFiAuthReg_ChallengeSize				0x20
#define kMFiAuthReg_ChallengeData				0x21
#define kMFiAuthReg_DeviceCertificateSize		0x30
#define kMFiAuthReg_DeviceCertificateData1		0x31

//===========================================================================================================================
//	Internals
//===========================================================================================================================

static void _InitI2C( void );
static OSStatus
	_DoI2CCommand(
		uint8_t			inRegister, 
		const uint8_t * inWritePtr, 
		size_t			inWriteLen, 
		uint8_t *		inReadBuf, 
		size_t			inReadLen );
static OSStatus	_DoI2CSetRegister( uint8_t inRegister, int *outActualTries );
static OSStatus	_DoI2CReadData( uint8_t *inReadBuf, size_t inReadLen, int *outActualTries );
static OSStatus	_DoI2CWriteData( const uint8_t *inWritePtr, size_t inWriteLen );
static OSStatus _DoI2CWaitForDMAStatus( DMA_Stream_TypeDef *inDMA, uint32_t inStatus, uint32_t inTimeout, OSStatus inFailError );
static OSStatus _DoI2CWaitForEvent( uint32_t inEvent, uint32_t inTimeout, OSStatus inFailError );
static void		_DoSoftReset( void );

static bool				gMFiInitialized		= false;
static uint8_t *		gMFiCertificatePtr	= NULL;
static size_t			gMFiCertificateLen	= 0;
static DMA_InitTypeDef	CP_DMA_InitStructure;

//===========================================================================================================================
//	MFiPlatform_Initialize
//===========================================================================================================================

OSStatus	MFiPlatform_Initialize( void )
{
	if( !gMFiInitialized )
	{
		_InitI2C();
		gMFiInitialized = true;
	}
	
	// Cache the certificate at startup since the certificate doesn't change and this saves ~200 ms each time.
	
	MFiPlatform_CopyCertificate( &gMFiCertificatePtr, &gMFiCertificateLen );
	return( kNoErr );
}

//===========================================================================================================================
//	MFiPlatform_Finalize
//===========================================================================================================================

void	MFiPlatform_Finalize( void )
{
	gMFiInitialized = false;
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
	uint8_t			tempBuf[ 32 ];
	size_t			signatureLen;
	uint8_t *		signaturePtr = NULL;
	
	// Write the digest to the device.
	// Note: writes to the size register auto-increment to the data register that follows it.
	
	require_action( inDigestLen == 20, exit, err = kSizeErr );
	tempBuf[ 0 ] = (uint8_t)( ( inDigestLen >> 8 ) & 0xFF );
	tempBuf[ 1 ] = (uint8_t)(	inDigestLen		   & 0xFF );
	memcpy( &tempBuf[ 2 ], inDigestPtr, inDigestLen );
	err = _DoI2CCommand( kMFiAuthReg_ChallengeSize, tempBuf, 2 + inDigestLen, NULL, 0 );
	require_noerr( err, exit );
	
	// Generate the signature.
	
	tempBuf[ 0 ] = kMFiAuthControl_GenerateSignature;
	err = _DoI2CCommand( kMFiAuthReg_AuthControlStatus, tempBuf, 1, NULL, 0 );
	require_noerr( err, exit );
	
	err = _DoI2CCommand( kMFiAuthReg_AuthControlStatus, NULL, 0, tempBuf, 1 );
	require_noerr( err, exit );
	require_action( !( tempBuf[ 0 ] & kMFiAuthFlagError ), exit, err = kUnknownErr );
	
	// Read the signature.
	
	err = _DoI2CCommand( kMFiAuthReg_SignatureSize, NULL, 0, tempBuf, 2 );
	require_noerr( err, exit );
	signatureLen = ( tempBuf[ 0 ] << 8 ) | tempBuf[ 1 ];
	require_action( signatureLen > 0, exit, err = kSizeErr );
	
	signaturePtr = (uint8_t *) malloc( signatureLen );
	require_action( signaturePtr, exit, err = kNoMemoryErr );
	
	err = _DoI2CCommand( kMFiAuthReg_SignatureData, NULL, 0, signaturePtr, signatureLen );
	require_noerr( err, exit );
	
	*outSignaturePtr = signaturePtr;
	*outSignatureLen = signatureLen;
	signaturePtr = NULL;
	
exit:
	if( signaturePtr ) free( signaturePtr );
	return( err );
}

//===========================================================================================================================
//	MFiPlatform_CopyCertificate
//===========================================================================================================================

OSStatus	MFiPlatform_CopyCertificate( uint8_t **outCertificatePtr, size_t *outCertificateLen )
{
	OSStatus		err;
	uint8_t			tempBuf[ 2 ];
	size_t			certificateLen;
	uint8_t *		certificatePtr = NULL;
	
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
	
	err = _DoI2CCommand( kMFiAuthReg_DeviceCertificateSize, NULL, 0, tempBuf, 2 );
	require_noerr( err, exit );
	certificateLen = ( tempBuf[ 0 ] << 8 ) | tempBuf[ 1 ];
	require_action( certificateLen > 0, exit, err = kSizeErr );
	
	certificatePtr = (uint8_t *) malloc( certificateLen );
	require_action( certificatePtr, exit, err = kNoMemoryErr );
	
	// Note: reads from the data1 register auto-increment to other data registers that follow it.
	
	err = _DoI2CCommand( kMFiAuthReg_DeviceCertificateData1, NULL, 0, certificatePtr, certificateLen );
	if( err ) free( certificatePtr );
	require_noerr( err, exit );
	
	*outCertificatePtr = certificatePtr;
	*outCertificateLen = certificateLen;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_InitI2C
//===========================================================================================================================

static void	_InitI2C( void )
{
	GPIO_InitTypeDef		GPIO_InitStructure;
	I2C_InitTypeDef			I2C_InitStructure;
	
	_DoSoftReset();
	
	// Enable the I2C, SDA, SCL, and SYSCFG clocks.
	
	RCC_APB1PeriphClockCmd( CP_I2C_CLK, ENABLE );
	RCC_AHB1PeriphClockCmd( CP_I2C_SCL_GPIO_CLK | CP_I2C_SDA_GPIO_CLK, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_SYSCFG, ENABLE );
	
	// Reset the I2C clock.
	
	RCC_APB1PeriphResetCmd( CP_I2C_CLK, ENABLE );
	RCC_APB1PeriphResetCmd( CP_I2C_CLK, DISABLE );
	
	// ----------------------------------------------------------------------------------------------------------------------
	// GPIO Configuration
	// ----------------------------------------------------------------------------------------------------------------------
	
	// Configure SDA and SCL as I2C pins.
	
	GPIO_PinAFConfig( CP_I2C_SCL_GPIO_PORT, CP_I2C_SCL_SOURCE, CP_I2C_SCL_AF );
	GPIO_PinAFConfig( CP_I2C_SDA_GPIO_PORT, CP_I2C_SDA_SOURCE, CP_I2C_SDA_AF );
	
	// Initialize the shared GPIO configuration.
	
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	
	// Initialize SCL & SDA.
	
	GPIO_InitStructure.GPIO_Pin = CP_I2C_SCL_PIN;
	GPIO_Init( CP_I2C_SCL_GPIO_PORT, &GPIO_InitStructure );
	GPIO_InitStructure.GPIO_Pin = CP_I2C_SDA_PIN;
	GPIO_Init( CP_I2C_SDA_GPIO_PORT, &GPIO_InitStructure );
	
	// ----------------------------------------------------------------------------------------------------------------------
	// DMA Configuration
	// ----------------------------------------------------------------------------------------------------------------------
	
	// Enable the DMA clock.
	
	RCC_AHB1PeriphClockCmd( CP_I2C_DMA_CLK, ENABLE );
	
	// Configure the DMA streams for operation with the CP.
	
	CP_DMA_InitStructure.DMA_Channel			= CP_I2C_DMA_CHANNEL;
	CP_DMA_InitStructure.DMA_PeripheralBaseAddr	= CP_I2C_DR_ADDRESS;
	CP_DMA_InitStructure.DMA_Memory0BaseAddr	= (uint32_t) 0;					// Configured during communication
	CP_DMA_InitStructure.DMA_DIR				= DMA_DIR_MemoryToPeripheral;	// Configured during communication.
	CP_DMA_InitStructure.DMA_BufferSize			= 0xFFFF;						// Configured during communication.
	CP_DMA_InitStructure.DMA_PeripheralInc		= DMA_PeripheralInc_Disable;
	CP_DMA_InitStructure.DMA_MemoryInc			= DMA_MemoryInc_Enable;
	CP_DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_Byte;
	CP_DMA_InitStructure.DMA_MemoryDataSize		= DMA_MemoryDataSize_Byte;
	CP_DMA_InitStructure.DMA_Mode				= DMA_Mode_Normal;
	CP_DMA_InitStructure.DMA_Priority			= DMA_Priority_VeryHigh;
	CP_DMA_InitStructure.DMA_FIFOMode			= DMA_FIFOMode_Enable;
	CP_DMA_InitStructure.DMA_FIFOThreshold		= DMA_FIFOThreshold_Full;
	CP_DMA_InitStructure.DMA_MemoryBurst		= DMA_MemoryBurst_Single;
	CP_DMA_InitStructure.DMA_PeripheralBurst	= DMA_PeripheralBurst_Single;
	
	// Clear any pending flags, disable, and clear the Tx DMA channel.
	
	DMA_ClearFlag( CP_I2C_DMA_STREAM_TX, CP_TX_DMA_FLAG_FEIF | CP_TX_DMA_FLAG_DMEIF | CP_TX_DMA_FLAG_TEIF | 
		CP_TX_DMA_FLAG_HTIF | CP_TX_DMA_FLAG_TCIF );
	DMA_Cmd( CP_I2C_DMA_STREAM_TX, DISABLE );
	DMA_DeInit( CP_I2C_DMA_STREAM_TX );
	
	// Clear any pending flags, disable, and clear the Rx DMA channel.
	
	DMA_ClearFlag( CP_I2C_DMA_STREAM_RX, CP_RX_DMA_FLAG_FEIF | CP_RX_DMA_FLAG_DMEIF | CP_RX_DMA_FLAG_TEIF | 
		CP_RX_DMA_FLAG_HTIF | CP_RX_DMA_FLAG_TCIF );
	DMA_Cmd( CP_I2C_DMA_STREAM_RX, DISABLE );
	DMA_DeInit( CP_I2C_DMA_STREAM_RX );
	
	// ----------------------------------------------------------------------------------------------------------------------
	// I2C Configuration
	// ----------------------------------------------------------------------------------------------------------------------
	
	// Initialize the InitStruct for the CP.
	
	I2C_InitStructure.I2C_Mode					= I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle				= I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1			= kSTM32DeviceAddress;
	I2C_InitStructure.I2C_Ack					= I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress	= I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed			= CP_I2C_SPEED;
	
	// Enable and initialize the I2C bus.
	
	I2C_Cmd( CP_I2C, ENABLE );
	I2C_Init( CP_I2C, &I2C_InitStructure );
	
	// Enable DMA on the I2C bus.
	
	I2C_DMACmd( CP_I2C, ENABLE );
}

//===========================================================================================================================
//	_DoI2CCommand
//===========================================================================================================================

static OSStatus
	_DoI2CCommand( 
		uint8_t			inRegister, 
		const uint8_t * inWritePtr, 
		size_t			inWriteLen, 
		uint8_t *		inReadBuf, 
		size_t			inReadLen )
{
	OSStatus		err;
	int				overallTries, tries;
	
	for( overallTries = 1; overallTries <= kMFiAuthOverallMaxTries; ++overallTries )
	{
		err = _DoI2CSetRegister( inRegister, &tries );
		if( err )
		{
			dlog( kLogLevelWarning,  "### Set register to 0x%02X failed: %d\n", inRegister, (int) err );
			continue;
		}
		if( tries > 1 ) dlog( kLogLevelInfo, "Took %d tries to set register to 0x%02X\n", tries, inRegister );
		
		if( inReadBuf )
		{
			err = _DoI2CReadData( inReadBuf, inReadLen, &tries );
			if( err )
			{
				dlog( kLogLevelInfo, "### Read %d bytes from register 0x%02X failed: %d\n", (int) inReadLen, inRegister, (int) err );
				continue;
			}
			if( tries > 1 )
				dlog( kLogLevelInfo, "Took %d tries to read %d bytes from register 0x%02X\n", tries, (int) inReadLen, inRegister );
		}
		else
		{
			err = _DoI2CWriteData( inWritePtr, inWriteLen );
			if( err )
			{
				dlog( kLogLevelInfo, "### Write %d bytes to register 0x%02X failed: %d\n", (int) inWriteLen, inRegister, (int) err );
				continue;
			}
		}
		break;
	}
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_DoI2CSetRegister
//===========================================================================================================================

static OSStatus	_DoI2CSetRegister( uint8_t inRegister, int *outActualTries )
{
	OSStatus		err;
	int				tries;
	
	// Send the write address, retrying if we receive a NACK from the CP.
	
	err = kNoErr;
	for( tries = 1; tries <= kMFiAuthIndividualMaxTries; ++tries )
	{
		// Wait 10ms before every try.
		
		tx_thread_sleep( kMFiAuthRetryDelayTicks );
		
		// Send START condition.
		
		I2C_GenerateSTART( CP_I2C, ENABLE );
		err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_MODE_SELECT, CP_FLAG_TIMEOUT, kOpenErr );
		if( err ) continue;
		
		// Send the write address for the CP.
		
		I2C_Send7bitAddress( CP_I2C, kMFiAuthDeviceAddress, I2C_Direction_Transmitter );
		err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, CP_LONG_TIMEOUT, kAddressErr );
		if( !err ) break;
	}
	require_noerr( err, exit );
	
	// Send the register: only one byte address
	
	I2C_SendData( CP_I2C, inRegister );
	err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_BYTE_TRANSMITTED, CP_FLAG_TIMEOUT, kWriteErr );
	require_noerr( err, exit );
	
	if( outActualTries ) *outActualTries = tries;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_DoI2CReadData
//===========================================================================================================================

static OSStatus	_DoI2CReadData( uint8_t *inReadBuf, size_t inReadLen, int *outActualTries )
{
	OSStatus		err;
	int				tries;
	uint32_t		cpTimeout;
	
	require_action( inReadBuf, exit, err = kParamErr );
	require_action( inReadLen > 0, exit, err = kParamErr );
	err = kNoErr;
	
	// Enable DMA if we're reading more than one byte.
	
	if( inReadLen > 1 )
	{
		// Enable DMA on the I2C bus.
		
		I2C_DMACmd( CP_I2C, ENABLE );
		
		// Configure the DMA Rx Stream with the buffer address and the buffer size.
		
		DMA_DeInit( CP_I2C_DMA_STREAM_RX );
		CP_DMA_InitStructure.DMA_Memory0BaseAddr	= (uint32_t)(uintptr_t) inReadBuf;
		CP_DMA_InitStructure.DMA_DIR				= DMA_DIR_PeripheralToMemory;
		CP_DMA_InitStructure.DMA_BufferSize			= (uint32_t) inReadLen;
		DMA_Init( CP_I2C_DMA_STREAM_RX, &CP_DMA_InitStructure );
		
		// Automatically send a NACK after the last byte has been read.
		
		I2C_DMALastTransferCmd( CP_I2C, ENABLE );
		
		// Enable the DMA Rx stream.
		
		DMA_Cmd( CP_I2C_DMA_STREAM_RX, ENABLE );
		err = _DoI2CWaitForDMAStatus( CP_I2C_DMA_STREAM_RX, ENABLE, CP_LONG_TIMEOUT, kNotPreparedErr );
		require_noerr( err, exit );
	}
	
	// Send the read address, retrying if we receive a NACK from the CP.
	
	for( tries = 1; tries <= kMFiAuthIndividualMaxTries; ++tries )
	{
		// Wait 10ms before every try.
		
		tx_thread_sleep( kMFiAuthRetryDelayTicks );
		
		// Send START condition a second time.
		
		I2C_GenerateSTART( CP_I2C, ENABLE );
		err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_MODE_SELECT, CP_FLAG_TIMEOUT, kOpenErr );
		if( err ) continue;
		
		// Send the CP read address and wait on it to be received.
		// If we're only reading one byte, check against the ADDR flag only.
		// Otherwise, use the predefined event for DMA.
		
		I2C_Send7bitAddress( CP_I2C, kMFiAuthDeviceAddress, I2C_Direction_Receiver );
		if( inReadLen < 2 )
		{
			cpTimeout = CP_FLAG_TIMEOUT;
			while( I2C_GetFlagStatus( CP_I2C, I2C_FLAG_ADDR ) == RESET )
			{
				if( ( cpTimeout-- ) == 0 )
				{
					err = kAddressErr;
					break;
				}
			}
		}
		else
		{
			err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, CP_LONG_TIMEOUT, kAddressErr );
		}
		if( !err ) break;
	}
	require_noerr( err, exit );
	
	// Read a single byte.
	
	if( inReadLen < 2 )
	{
		// Follow the STM32F2xx documentation and disable Acknowledgment.
		
		I2C_AcknowledgeConfig( CP_I2C, DISABLE );
		
		// Clear ADDR register by reading SR1 then SR2 register (SR1 has already been read).
		
		(void)CP_I2C->SR2;
		
		// Send STOP Condition.
		
		I2C_GenerateSTOP( CP_I2C, ENABLE );
		
		// Wait for the byte to be received.
		
		cpTimeout = CP_FLAG_TIMEOUT;
		while( I2C_GetFlagStatus( CP_I2C, I2C_FLAG_RXNE ) == RESET )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kReadErr;
				break;
			}
		}
		
		// Read the byte from the I2C register.
		
		*inReadBuf = I2C_ReceiveData( CP_I2C );
		
		// Wait to make sure that STOP control bit has been cleared.
		
		cpTimeout = CP_FLAG_TIMEOUT;
		while( CP_I2C->CR1 & I2C_CR1_STOP )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kEndingErr;
				break;
			}
		}
		
		// Enable Acknowledgment to be ready for another reception.
		
		I2C_AcknowledgeConfig( CP_I2C, ENABLE );
	}
	
	// Otherwise, use DMA.
	
	else
	{
		// Wait until the transfer has completed or timeout after one second.
		
		cpTimeout = kSTM32OneSecondTimeout;
		while( DMA_GetFlagStatus( CP_I2C_DMA_STREAM_RX, CP_RX_DMA_FLAG_TCIF ) == RESET )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kReadErr;
				break;
			}
		}
		require_noerr( err, exit );
		
		// Send I2Cx STOP Condition.
		
		I2C_GenerateSTOP( CP_I2C, ENABLE );
		
		// Disable DMA Rx Channel.
		
		DMA_Cmd( CP_I2C_DMA_STREAM_RX, DISABLE );
		err = _DoI2CWaitForDMAStatus( CP_I2C_DMA_STREAM_RX, DISABLE, CP_LONG_TIMEOUT, kEndingErr );
		require_noerr( err, exit );
		
		// Disable I2C DMA request.
		
		I2C_DMACmd( CP_I2C,DISABLE );
		
		// Clear any pending flag on Rx Stream.
		
		DMA_ClearFlag( CP_I2C_DMA_STREAM_RX, CP_RX_DMA_FLAG_FEIF | CP_RX_DMA_FLAG_DMEIF | CP_RX_DMA_FLAG_TEIF | CP_RX_DMA_FLAG_HTIF | CP_RX_DMA_FLAG_TCIF );
	}
	
	if( outActualTries ) *outActualTries = tries;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_DoI2CWriteData
//===========================================================================================================================

static OSStatus	_DoI2CWriteData( const uint8_t *inWritePtr, size_t inWriteLen )
{
	OSStatus		err;
	uint32_t		cpTimeout;
	
	require_action( inWritePtr, exit, err = kParamErr );
	require_action( inWriteLen > 0, exit, err = kParamErr );
	err = kNoErr;
	
	// Enable DMA if we're writing more than one byte.
	
	if( inWriteLen > 1 )
	{
		// Enable DMA on the I2C bus.

		I2C_DMACmd( CP_I2C, ENABLE );
		
		// Configure the DMA Tx Stream with the buffer address and the buffer size.
		
		DMA_DeInit( CP_I2C_DMA_STREAM_TX );
		CP_DMA_InitStructure.DMA_Memory0BaseAddr	= (uint32_t)(uintptr_t) inWritePtr;
		CP_DMA_InitStructure.DMA_DIR				= DMA_DIR_MemoryToPeripheral;
		CP_DMA_InitStructure.DMA_BufferSize			= (uint32_t) inWriteLen;
		DMA_Init( CP_I2C_DMA_STREAM_TX, &CP_DMA_InitStructure );
		
		// Enable the DMA Tx stream.
		
		DMA_Cmd( CP_I2C_DMA_STREAM_TX, ENABLE );
		err = _DoI2CWaitForDMAStatus( CP_I2C_DMA_STREAM_TX, ENABLE, CP_LONG_TIMEOUT, kNotPreparedErr );
		require_noerr( err, exit );
		
		// Wait until the transfer has completed or timeout after one second.
		
		cpTimeout = kSTM32OneSecondTimeout;
		while( DMA_GetFlagStatus( CP_I2C_DMA_STREAM_TX, CP_TX_DMA_FLAG_TCIF ) == RESET )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kWriteErr;
				break;
			}
		}
		require_noerr( err, exit );
		
		// Disable I2C DMA request.
		
		I2C_DMACmd( CP_I2C, DISABLE );
		
		// Wait until the last byte has been sent.
		
		cpTimeout = CP_LONG_TIMEOUT;
		while( !I2C_GetFlagStatus( CP_I2C, I2C_FLAG_BTF ) )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kWriteErr;
				break;
			}
		}
		require_noerr( err, exit );
		
		// Send I2Cx STOP Condition.
		
		I2C_GenerateSTOP( CP_I2C, ENABLE );
		
		// Disable DMA Tx Channel.
		
		DMA_Cmd( CP_I2C_DMA_STREAM_TX, DISABLE );
		err = _DoI2CWaitForDMAStatus( CP_I2C_DMA_STREAM_TX, DISABLE, CP_LONG_TIMEOUT, kEndingErr );
		require_noerr( err, exit );
		
		// Clear any pending flag on Rx Stream
		
		DMA_ClearFlag( CP_I2C_DMA_STREAM_TX, CP_TX_DMA_FLAG_FEIF | CP_TX_DMA_FLAG_DMEIF | CP_TX_DMA_FLAG_TEIF | CP_TX_DMA_FLAG_HTIF | CP_TX_DMA_FLAG_TCIF );
	}
	else
	{
		// Send the register: only one byte address.
		
		I2C_SendData( CP_I2C, *inWritePtr );
		err = _DoI2CWaitForEvent( I2C_EVENT_MASTER_BYTE_TRANSMITTED, CP_FLAG_TIMEOUT, kWriteErr );
		require_noerr( err, exit );
		
		// Send STOP Condition.
		
		I2C_GenerateSTOP( CP_I2C, ENABLE );
		
		// Wait to make sure that STOP control bit has been cleared.
		
		cpTimeout = CP_FLAG_TIMEOUT;
		while( CP_I2C->CR1 & I2C_CR1_STOP )
		{
			if( ( cpTimeout-- ) == 0 )
			{
				err = kEndingErr;
				break;
			}
		}
	}
	require_noerr( err, exit );
	
exit:
	return err;
}

//===========================================================================================================================
//	_DoI2CWaitForDMAStatus
//===========================================================================================================================

static OSStatus _DoI2CWaitForDMAStatus( DMA_Stream_TypeDef *inDMA, uint32_t inStatus, uint32_t inTimeout, OSStatus inFailError )
{
	do
	{
		if( DMA_GetCmdStatus( inDMA ) == inStatus )
		{
			return( kNoErr );
		}
	
	}	while( inTimeout-- > 0 );
	return( inFailError );
}

//===========================================================================================================================
//	_DoI2CWaitForEvent
//===========================================================================================================================

static OSStatus _DoI2CWaitForEvent( uint32_t inEvent, uint32_t inTimeout, OSStatus inFailError )
{
	do
	{
		if( I2C_CheckEvent( CP_I2C, inEvent ) )
		{
			return( kNoErr );
		}
	
	}	while( inTimeout-- > 0 );
	return( inFailError );
}

//===========================================================================================================================
//	_DoSoftReset
//	
//	May or may not need this: we need to drive RST, SDA, and SCL to high to set the address.
//	Looks like these connections have pull up resistors, so this proabably isn't needed.
//===========================================================================================================================

static void	_DoSoftReset( void )
{
	GPIO_InitTypeDef	GPIO_InitStructure;
	uint16_t			ticks;
	
	// Enable RST, SDA, SCL, and SYSCFG clocks
	
	RCC_AHB1PeriphClockCmd( CP_RST_GPIO_CLK | CP_I2C_SCL_GPIO_CLK | CP_I2C_SDA_GPIO_CLK, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_SYSCFG, ENABLE );
	
	// Initialize RST, SDA, and SCL as GPIO Outputs
	
	GPIO_StructInit( &GPIO_InitStructure );
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL; // may need to be GPIO_PuPd_UP
	
	// Initialize SCL to HIGH
	
	GPIO_InitStructure.GPIO_Pin = CP_I2C_SCL_PIN;
	GPIO_Init( CP_I2C_SCL_GPIO_PORT, &GPIO_InitStructure );
	GPIO_SetBits( CP_I2C_SCL_GPIO_PORT, CP_I2C_SCL_PIN );
	
	// Initialize SDA to HIGH
	
	GPIO_InitStructure.GPIO_Pin = CP_I2C_SDA_PIN;
	GPIO_Init( CP_I2C_SDA_GPIO_PORT, &GPIO_InitStructure );
	GPIO_SetBits( CP_I2C_SDA_GPIO_PORT, CP_I2C_SDA_PIN );
	
	// Initialize RST
	
	GPIO_InitStructure.GPIO_Pin = CP_RST_PIN;
	GPIO_Init( CP_RST_GPIO_PORT, &GPIO_InitStructure );
	
	// Pull RST low
	
	GPIO_ResetBits( CP_RST_GPIO_PORT, CP_RST_PIN );
	
	// Wait at least 10us
	
	for( ticks = 0; ticks < CP_RESET_DETECTION; ++ticks ) {}
	
	// Pull RST high

	GPIO_SetBits( CP_RST_GPIO_PORT, CP_RST_PIN );
	
	// Wait at least 10ms
	
	tx_thread_sleep( kMFiAuthRetryDelayTicks * 2 );
}
