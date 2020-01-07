/*
	File:    	IEEE80211Utils.c
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
	
	Copyright (C) 2008-2015 Apple Inc. All Rights Reserved.
*/

#include "IEEE80211Utils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "DataBufferUtils.h"
#include "StringUtils.h"
#include "TLVUtils.h"

#define PTR_LEN_LEN( SRC, END )		( SRC ), (size_t)( ( END ) - ( SRC ) ), (size_t)( ( END ) - ( SRC ) )

//===========================================================================================================================
//	IEGet
//===========================================================================================================================

OSStatus
	IEGet( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inID, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext );
OSStatus
	IEGet( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inID, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	return( TLV8Get( inSrc, inEnd, inID, outPtr, outLen, outNext ) );
}

//===========================================================================================================================
//	IEGetNext
//===========================================================================================================================

OSStatus
	IEGetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t *			outID, 
		const uint8_t **	outData, 
		size_t *			outLen, 
		const uint8_t **	outNext );
OSStatus
	IEGetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t *			outID, 
		const uint8_t **	outData, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	return( TLV8GetNext( inSrc, inEnd, outID, outData, outLen, outNext ) );
}

//===========================================================================================================================
//	IEGetVendorSpecific
//===========================================================================================================================

OSStatus
	IEGetVendorSpecific( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint32_t			inVID, 
		const uint8_t **	outData, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	const uint8_t *		ptr;
	const uint8_t *		next;
	uint8_t				eid;
	size_t				len;
	uint32_t			vid;
	
	// Vendor-specific IE's have the following format (IEEE 802.11-2007 section 7.3.2.26):
	//
	//		<1:eid=221> <1:length> <3:oui> <1:type> <length - 4:data>
	
	for( ptr = inSrc; ( inEnd - ptr ) >= 2; ptr = next )
	{
		eid  = ptr[ 0 ];
		len  = ptr[ 1 ];
		next = ptr + 2 + len;
		if( eid != kIEEE80211_EID_Vendor )
		{
			continue;
		}
		if( ( next < inSrc ) || ( next > inEnd ) )
		{
			dlog( kLogLevelNotice, "### Overlong vendor IE len:\n%1.1H\n", PTR_LEN_LEN( inSrc, inEnd ) );
			return( kSizeErr );
		}
		if( len < 4 )
		{
			dlog( kLogLevelNotice, "### Short vendor IE:\n%1.1H\n", PTR_LEN_LEN( inSrc, inEnd ) );
			continue;
		}
		
		vid = (uint32_t)( ( ptr[ 2 ] << 24 ) | ( ptr[ 3 ] << 16 ) | ( ptr[ 4 ] << 8 ) | ptr[ 5 ] );
		if( vid != inVID )
		{
			continue;
		}
		
		*outData = ptr + 6; // Skip eid, len, oui, and type.
		*outLen  = len - 4; // Skip oui and type (length doesn't include the eid and len).
		if( outNext ) *outNext = next;
		return( kNoErr );
	}
	if( ptr != inEnd )
	{
		dlog( kLogLevelNotice, "### Bad vendor IE len:\n%1.1H\n", PTR_LEN_LEN( inSrc, inEnd ) );
	}
	return( kNotFoundErr );
}

//===========================================================================================================================
//	IECopyCoalescedVendorSpecific
//===========================================================================================================================

OSStatus
	IECopyCoalescedVendorSpecific( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint32_t			inVID, 
		uint8_t **			outPtr, 
		size_t *			outLen )
{
	OSStatus			err;
	uint8_t *			buf;
	size_t				totalLen;
	uint8_t *			tmp;
	const uint8_t *		ptr;
	size_t				len;
	
	buf = NULL;
	totalLen = 0;
	
	while( ( err = IEGetVendorSpecific( inSrc, inEnd, inVID, &ptr, &len, &inSrc ) ) == kNoErr )
	{
		tmp = (uint8_t *) malloc_compat( totalLen + len + 1 ); // +1 to avoid malloc( 0 ) being undefined.
		require_action( tmp, exit, err = kNoMemoryErr );
		if( buf )
		{
			memcpy( tmp, buf, totalLen );
			free_compat( buf );
		}
		buf = tmp;
		memcpy( buf + totalLen, ptr, len );
		totalLen += len;
	}
	require_quiet( buf, exit );
	
	*outPtr = buf;
	*outLen = totalLen;
	buf = NULL;
	err = kNoErr;
	
exit:
	if( buf ) free_compat( buf );
	return( err );
}

//===========================================================================================================================
//	IEGetAppleGeneral
//===========================================================================================================================

OSStatus	IEGetAppleGeneral( const uint8_t *inSrc, const uint8_t *inEnd, uint8_t *outProductID, uint16_t *outFlags )
{
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	
	// The Apple General IE has the following format:
	//
	//		<1:productID> <2:big endian flags>
	
	err = IEGetVendorSpecific( inSrc, inEnd, kIEEE80211_VID_AppleGeneral, &ptr, &len, NULL );
	require_noerr_quiet( err, exit );
	if( len < 3 )
	{
		dlog( kLogLevelNotice, "### Bad Apple general IE length (%zu):\n%1.1H\n", len, PTR_LEN_LEN( inSrc, inEnd ) );
		err = kSizeErr;
		goto exit;
	}
	
	*outProductID	= ptr[ 0 ];
	*outFlags		= (uint16_t)( ( ptr[ 1 ] << 8 ) | ptr[ 2 ] );
	
exit:
	return( err );
}

//===========================================================================================================================
//	IEGetDWDS
//===========================================================================================================================

OSStatus	IEGetDWDS( const uint8_t *inSrc, const uint8_t *inEnd, uint8_t *outRole, uint32_t *outFlags )
{
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	
	// The DWDS IE has the following format:
	//
	//		<1:subtype> <1:version> <1:role> <4:flags>
	
	err = IEGetVendorSpecific( inSrc, inEnd, kIEEE80211_VID_DWDS, &ptr, &len, NULL );
	require_noerr_quiet( err, exit );
	if( len < 7 )
	{
		dlog( kLogLevelNotice, "### Bad DWDS IE length (%zu):\n%1.1H\n", len, PTR_LEN_LEN( inSrc, inEnd ) );
		err = kSizeErr;
		goto exit;
	}
	if( ptr[ 0 ] != 0x00 ) // SubType
	{
		dlog( kLogLevelNotice, "### Unknown DWDS subtype: (%d)\n%1.1H\n", ptr[ 0 ], PTR_LEN_LEN( inSrc, inEnd ) );
		err = kTypeErr;
		goto exit;
	}
	if( ptr[ 1 ] != 0x01 ) // Version
	{
		dlog( kLogLevelNotice, "### Unknown DWDS version: (%d)\n%1.1H\n", ptr[ 1 ], PTR_LEN_LEN( inSrc, inEnd ) );
		err = kVersionErr;
		goto exit;
	}
	
	*outRole	= ptr[ 2 ];
	*outFlags	= (uint32_t)( ( ptr[ 3 ] << 24 ) | ( ptr[ 4 ] << 16 ) | ( ptr[ 5 ] << 8 ) | ptr[ 6 ] );	
	
exit:
	return( err );
}

//===========================================================================================================================
//	IEGetTLV16
//===========================================================================================================================

OSStatus
	IEGetTLV16( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint32_t			inVID, 
		uint16_t			inAttrID, 
		void *				inBuf, 
		size_t				inBufLen, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		ptr;
	const uint8_t *		end;
	size_t				len;
	uint8_t *			dst;
	uint8_t *			lim;
	Boolean				gotHeader;
	uint16_t			attrID;
	size_t				attrLen;
	
	src = (const uint8_t *) inSrc;
	dst = (uint8_t *) inBuf;
	lim = dst + inBufLen;
	
	// 16-bit TLVs are a little tricky because an attribute may span multiple IE's. It is further complicated by the 
	// attribute header only being in the first IE for the attribute so if we find a 16-bit TLV IE, but it's not the 
	// one we want, we can't just check the next IE blindly because we may misinterpret the next IE as being a header. 
	// So we have to walk to the end of it.
	//
	// TLV16 attributes are a vendor-specific IE with the following format (WPS 1.0 spec sections 7.1 and 7.2):
	//
	// 		<1:eid=221> <1:length> <4:oui=00 50 F2 04> <2:attrID> <2:dataLength> <Min(dataLength, 0xFF - 8):data>
	//
	// TLV16 attributes may span multiple IE's if they won't fit in a single 255 byte IE data section.
	// Subsequent IE's to have their data section concatenated have the following format:
	//
	// 		<1:eid=221> <1:length> <4:oui=00 50 F2 04> <Min(dataLength, 0xFF - 4):data>
	
	gotHeader = false;
	attrID    = 0;
	attrLen   = 0;
	for( ;; )
	{
		err = IEGetVendorSpecific( src, inEnd, inVID, &ptr, &len, &src );
		require_noerr_quiet( err, exit );
		
		if( !gotHeader )
		{
			if( len < 4 ) { dlog( kLogLevelNotice, "### Short TLV16:\n%1.1H\n", PTR_LEN_LEN( inSrc, inEnd ) ); continue; }
			attrID  = (uint16_t)( ( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
			attrLen = (uint16_t)( ( ptr[ 2 ] << 8 ) | ptr[ 3 ] );
			ptr += 4;
			len -= 4;
			gotHeader = true;
		}
		if( attrID == inAttrID )
		{
			if( inBuf )
			{
				end = ptr + len;
				while( ( ptr < end ) && ( dst < lim ) )
				{
					*dst++ = *ptr++;
				}
				require_action_quiet( ptr == end, exit, err = kNoSpaceErr );
			}
			else
			{
				dst += len;
			}
		}
		attrLen -= Min( len, attrLen );
		if( attrLen == 0 )
		{
			if( attrID == inAttrID ) break;
			gotHeader = false;
		}
	}
	err = kNoErr;
	
exit:
	*outLen = (size_t)( dst - ( (uint8_t *) inBuf ) ); // Note: this works even if inBuf is NULL.
	if( outNext ) *outNext = src;
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	IEBufferAppendIE
//===========================================================================================================================

OSStatus	IEBufferAppendIE( IEBuffer *inBuf, uint8_t inEID, const void *inData, size_t inLen )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	
	require_noerr_action_quiet( inBuf->firstErr, exit2, err = inBuf->firstErr );
	
	// IEEE 802.11 IE's are in the following format:
	//
	// 		<1:eid> <1:length> <length:data>
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inData );
	require_action( inLen <= 0xFF, exit, err = kSizeErr );
	require_action( ( inBuf->len + 1 + 1 + inLen ) < sizeof( inBuf->buf ), exit, err = kSizeErr );
	
	inBuf->buf[ inBuf->len++ ] = inEID;
	inBuf->buf[ inBuf->len++ ] = (uint8_t) inLen;
	
	src = (const uint8_t *) inData;
	end = src + inLen;
	while( src < end ) inBuf->buf[ inBuf->len++ ] = *src++;
	err = kNoErr;
	
exit:
	if( !inBuf->firstErr ) inBuf->firstErr = err;
	
exit2:
	return( err );
}

//===========================================================================================================================
//	IEBufferStartVendorIE
//===========================================================================================================================

OSStatus	IEBufferStartVendorIE( IEBuffer *inBuf, uint32_t inVID )
{
	OSStatus			err;
	
	require_noerr_action_quiet( inBuf->firstErr, exit2, err = inBuf->firstErr );
	
	// IEEE 802.11 vendor-specific IE's are in the following format:
	//
	// 		<1:eid=0xDD> <1:length> <3:oui> <1:type> <length - 4:data>
	
	require_action( ( inBuf->len + 1 + 1 + 3 + 1 ) < sizeof( inBuf->buf ), exit, err = kSizeErr );
	
	inBuf->buf[ inBuf->len++ ]	= kIEEE80211_EID_Vendor;
	inBuf->savedOffset			= inBuf->len;
	inBuf->buf[ inBuf->len++ ]	= 0; // Placeholder to update when the IE ends.
	inBuf->buf[ inBuf->len++ ]	= (uint8_t)( ( inVID >> 24 ) & 0xFF );
	inBuf->buf[ inBuf->len++ ]	= (uint8_t)( ( inVID >> 16 ) & 0xFF );
	inBuf->buf[ inBuf->len++ ]	= (uint8_t)( ( inVID >>  8 ) & 0xFF );
	inBuf->buf[ inBuf->len++ ]	= (uint8_t)(   inVID         & 0xFF );
	err = kNoErr;
	
exit:
	if( !inBuf->firstErr ) inBuf->firstErr = err;
	
exit2:
	return( err );
}

//===========================================================================================================================
//	IEBufferEndVendorIE
//===========================================================================================================================

OSStatus	IEBufferEndVendorIE( IEBuffer *inBuf )
{
	OSStatus			err;
	
	require_noerr_action_quiet( inBuf->firstErr, exit2, err = inBuf->firstErr );
	require_action( inBuf->savedOffset > 0, exit, err = kNotPreparedErr );
	
	inBuf->buf[ inBuf->savedOffset ] = (uint8_t)( ( inBuf->len - inBuf->savedOffset ) - 1 );
	inBuf->savedOffset = 0;
	err = kNoErr;
	
exit:
	if( !inBuf->firstErr ) inBuf->firstErr = err;
	
exit2:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	DataBuffer_AppendIE
//===========================================================================================================================

OSStatus	DataBuffer_AppendIE( DataBuffer *inDB, uint8_t inEID, const void *inData, size_t inLen )
{
	OSStatus			err;
	uint8_t *			dst;
	const uint8_t *		src;
	const uint8_t *		end;
	
	// IEEE 802.11 IE's are in the following format:
	//
	// 		<1:eid> <1:length> <length:data>
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inData );
	require_action( inLen <= 0xFF, exit, err = kSizeErr );
	
	err = DataBuffer_Grow( inDB, 1 + 1 + inLen, &dst ); // eid + len + data.
	require_noerr( err, exit );
	
	*dst++ = inEID;
	*dst++ = (uint8_t) inLen;
	
	src = (const uint8_t *) inData;
	end = src + inLen;
	while( src < end ) *dst++ = *src++;
	check( dst == DataBuffer_GetEnd( inDB ) );
	
exit:
	if( !inDB->firstErr ) inDB->firstErr = err;
	return( err );
}

//===========================================================================================================================
//	DataBuffer_AppendAppleGeneralIE
//===========================================================================================================================

OSStatus
	DataBuffer_AppendAppleGeneralIE( 
		DataBuffer *	inDB, 
		uint8_t			inProductID, 
		uint16_t		inFlags, 
		const uint8_t	inRadio1BSSID[ 6 ], uint8_t inRadio1Channel, 
		const uint8_t	inRadio2BSSID[ 6 ], uint8_t inRadio2Channel )
{
	OSStatus		err;
	uint8_t			buf[ 19 ];
	size_t			len;
	
	require_action( inRadio1BSSID || ( inRadio1Channel == 0 ), exit, err = kParamErr );
	require_action( inRadio2BSSID || ( inRadio2Channel == 0 ), exit, err = kParamErr );
	
	// The Apple General IE has the following format:
	//
	//		<1:productID> <2:big endian flags> [sub IEs]
	
	len = 0;
	buf[ len++ ] = inProductID;
	buf[ len++ ] = (uint8_t)( inFlags >> 8 );
	buf[ len++ ] = (uint8_t)( inFlags & 0xFF );
	
	if( ( inRadio1Channel != 0 ) && ( inRadio2Channel != 0 ) )
	{
		buf[ len++ ] = kAppleGeneralIE_SubEID_ChannelInfo;
		buf[ len++ ] = 14; // 2 * <6:BSSID> + <1:channel>
		
		memcpy( &buf[ len ], inRadio1BSSID, 6 );
		len += 6;
		buf[ len++ ] = inRadio1Channel;
		
		memcpy( &buf[ len ], inRadio2BSSID, 6 );
		len += 6;
		buf[ len++ ] = inRadio2Channel;
		
		check( len == 19 );
	}
	
	err = DataBuffer_AppendVendorIE( inDB, kIEEE80211_VID_AppleGeneral, buf, len );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	DataBuffer_AppendVendorIE
//===========================================================================================================================

OSStatus	DataBuffer_AppendVendorIE( DataBuffer *inDB, uint32_t inVID, const void *inData, size_t inLen )
{
	OSStatus			err;
	size_t				originalLen;
	uint8_t *			dst;
	const uint8_t *		src;
	size_t				len;
	
	originalLen = DataBuffer_GetLen( inDB );
	
	// IEEE 802.11 vendor-specific IE's are in the following format:
	//
	// 		<1:eid> <1:length> <3:oui> <1:type> <length - 4:data>
	//
	// Vendor IEs larger than the max of 255 bytes (249 of payload) may be split across multiple IEs.
	// When split, each IE contains <1:eid> <1:length> <3:oui> <1:type> followed by up to 249 bytes
	// of payload. When read back, the payloads are concatenated to reconstruct the original IE.
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inData );
	src = (const uint8_t *) inData;
	do
	{
		len = Min( inLen, 249 ); // Max of 255 - 1 (eid) - 1 (len) - 4 (vid) = 249.
		
		err = DataBuffer_Grow( inDB, 1 + 1 + 4 + len, &dst ); // eid + len + vid + data.
		require_noerr( err, exit );
		
		*dst++ = kIEEE80211_EID_Vendor;
		*dst++ = (uint8_t)( 4 + len );
		*dst++ = (uint8_t)( ( inVID >> 24 ) & 0xFF );
		*dst++ = (uint8_t)( ( inVID >> 16 ) & 0xFF );
		*dst++ = (uint8_t)( ( inVID >>  8 ) & 0xFF );
		*dst++ = (uint8_t)(   inVID         & 0xFF );
		memcpy( dst, src, len );
		src   += len;
		inLen -= len;
		dst   += len;
		check( dst == DataBuffer_GetEnd( inDB ) );
		
	}	while( inLen > 0 );
	
exit:
	if( err )				inDB->bufferLen = originalLen; // Restore on errors.
	if( !inDB->firstErr )	inDB->firstErr = err;
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "SHAUtils.h"

//===========================================================================================================================
//	IEEE80211Utils_Test
//===========================================================================================================================

OSStatus	IEEE80211Utils_Test( int inPrint )
{
	OSStatus			err;
	IEBuffer			ieBuf;
	uint8_t				bits[ BitArray_MaxBytes( kAppleDeviceIEFlagBit_TotalBits ) ];
	DataBuffer			db;
	uint8_t				buf[ 256 ];
	DataBuffer			db2;
	uint8_t				buf2[ 256 ];
	uint16_t			u16;
	size_t				len;
	uint8_t				productID;
	uint16_t			flags;
	const uint8_t *		src;
	const uint8_t *		end;
	const uint8_t *		ptr;
	const char *		str;
	uint8_t *			mem;
	
	// IE Building
	
	IEBufferInit( &ieBuf );
	IEBufferStartVendorIE( &ieBuf, kIEEE80211_VID_AppleDevice );
	
	IEBufferAppendIE( &ieBuf, kAppleDeviceIE_Name, "My Device", kSizeCString );
	
	BitArray_Clear( bits, sizeof( bits ) );
	BitArray_SetBit( bits, kAppleDeviceIEFlagBit_AirPlay );
	BitArray_SetBit( bits, kAppleDeviceIEFlagBit_Unconfigured );
	BitArray_SetBit( bits, kAppleDeviceIEFlagBit_MFiConfigurationV1 );
	IEBufferAppendIE( &ieBuf, kAppleDeviceIE_Flags, bits, BitArray_MinBytes( bits, sizeof( bits ) ) );
	
	IEBufferAppendIE( &ieBuf, kAppleDeviceIE_Model, "AirPlay1,1", kSizeCString );
	IEBufferAppendIE( &ieBuf, kAppleDeviceIE_DeviceID, "\x11\x22\x33\x44\x55\x66", 6 );
	
	IEBufferEndVendorIE( &ieBuf );
	require_noerr_action( ieBuf.firstErr, exit, err = ieBuf.firstErr );
	if( inPrint )
	{
		dlog( kLogLevelNotice, "Full IE:\n%{tlv8}\n%.2H\n", 
			kAppleDeviceIEDescriptors, ieBuf.buf, (int) ieBuf.len, 
			ieBuf.buf, (int) ieBuf.len, (int) ieBuf.len );
	}
	
	err = IEGetVendorSpecific( ieBuf.buf, ieBuf.buf + ieBuf.len, kIEEE80211_VID_AppleDevice, &src, &len, NULL );
	require_noerr( err, exit );
	end = src + len;
	
	err = TLV8Get( src, end, kAppleDeviceIE_Name, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 9, exit, err = -1 );
	require_action( memcmp( ptr, "My Device", len ) == 0, exit, err = -1 );
	
	err = TLV8Get( src, end, kAppleDeviceIE_Flags, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 1, exit, err = -1 );
	require_action( memcmp( ptr, "\xE0", len ) == 0, exit, err = -1 );
	
	err = TLV8Get( src, end, kAppleDeviceIE_Model, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( ptr, "AirPlay1,1", len ) == 0, exit, err = -1 );
	
	err = TLV8Get( src, end, kAppleDeviceIE_DeviceID, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 6, exit, err = -1 );
	require_action( memcmp( ptr, "\x11\x22\x33\x44\x55\x66", len ) == 0, exit, err = -1 );
	
	// IE Building 2
	
	DataBuffer_Init( &db, buf, 10, 1024 );
	
	u16 = kAppleGeneralIE_Flags_Unconfigured | kAppleGeneralIE_Flags_WPSCapable | kAppleGeneralIE_Flags_SAWCapable;
	err = DataBuffer_AppendAppleGeneralIE( &db, 107, u16, NULL, 0, NULL, 0 );
	require_noerr( err, exit );
	
	err = DataBuffer_AppendIE( &db, kIEEE80211_EID_SSID, "my network", kSizeCString );
	require_noerr( err, exit );
	
	err = DataBuffer_Commit( &db, NULL, NULL );
	require_noerr( err, exit );
	
	err = TLV8Get( DataBuffer_GetPtr( &db ), DataBuffer_GetEnd( &db ), kIEEE80211_EID_Vendor, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 7, exit, err = -1 );
	require_action( memcmp( ptr, "\x00\x03\x93\x01\x6B\x00\xA2", 7 ) == 0, exit, err = -1 );
	
	err = TLV8Get( DataBuffer_GetPtr( &db ), DataBuffer_GetEnd( &db ), kIEEE80211_EID_SSID, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( ptr, "my network", 10 ) == 0, exit, err = -1 );
	
	//
	// IE Parsing
	//
	
	err = IEGetAppleGeneral( DataBuffer_GetPtr( &db ), DataBuffer_GetEnd( &db ), &productID, &flags );
	require_noerr( err, exit );
	require_action( productID == 107, exit, err = kResponseErr );
	u16 = kAppleGeneralIE_Flags_Unconfigured | kAppleGeneralIE_Flags_WPSCapable | kAppleGeneralIE_Flags_SAWCapable;
	require_action( flags == u16, exit, err = kResponseErr );
	
	DataBuffer_Free( &db );
	
	//
	// Big IEs
	//
	
	DataBuffer_Init( &db, buf, 10, 1024 );
	
	err = DataBuffer_AppendIE( &db, kIEEE80211_EID_SSID, "My Custom SSID", kSizeCString );
	require_noerr( err, exit );
	
	// Build the EasyConnect IE and add it as a sub IE.
	
	DataBuffer_Init( &db2, buf2, 10, 1024 );
	
	err = DataBuffer_AppendIE( &db2, 0 /* name */, 
		"This is a test of a really long name intended to fill a buffer", 
		kSizeCString );
	require_noerr( err, exit );
	
	err = DataBuffer_AppendIE( &db2, 1 /* model */, 
		"This is a test of a really long model intended to fill a buffer", 
		kSizeCString );
	require_noerr( err, exit );
	
	err = DataBuffer_AppendIE( &db2, 2 /* OffsetSubTypes */, 
		"_somesubtype1,_somesubtype2,_somesubtype3,_somesubtype4,_somesubtype5", 
		kSizeCString );
	require_noerr( err, exit );
	
	err = DataBuffer_AppendIE( &db2, 3 /* SearchTypes */, 
		"_servicetype1._tcp,_servicetype2._tcp,_servicetype3._tcp,_servicetype4._tcp,_servicetype5._tcp", 
		kSizeCString );
	require_noerr( err, exit );
	
	err = DataBuffer_AppendVendorIE( &db, 0x0017F203 /* Old EasyConnect */, 
		DataBuffer_GetPtr( &db2 ), DataBuffer_GetLen( &db2 ) );
	require_noerr( err, exit );
	
	DataBuffer_Free( &db2 );
	
	err = DataBuffer_AppendIE( &db, 16, "My Challenge Text", kSizeCString );
	require_noerr( err, exit );
	
	// Verify the IEs.
	
	str = "My Custom SSID";
	src = DataBuffer_GetPtr( &db );
	end = DataBuffer_GetEnd( &db );
	err = TLV8Get( src, end, kIEEE80211_EID_SSID, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	err = IECopyCoalescedVendorSpecific( src, end, 0x0017F203 /* Old EasyConnect */, &mem, &len );
	require_noerr( err, exit );
	src = mem;
	end = src + len;
	
	str = "This is a test of a really long name intended to fill a buffer";
	err = TLV8Get( src, end, 0 /* name */, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	str = "This is a test of a really long model intended to fill a buffer";
	err = TLV8Get( src, end, 1 /* model */, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	str = "_somesubtype1,_somesubtype2,_somesubtype3,_somesubtype4,_somesubtype5";
	err = TLV8Get( src, end, 2 /* OffsetSubTypes */, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	str = "_servicetype1._tcp,_servicetype2._tcp,_servicetype3._tcp,_servicetype4._tcp,_servicetype5._tcp";
	err = TLV8Get( src, end, 3 /* SearchTypes */, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	free_compat( mem );
	
	str = "My Challenge Text";
	src = DataBuffer_GetPtr( &db );
	end = DataBuffer_GetEnd( &db );
	err = TLV8Get( src, end, 16, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == strlen( str ), exit, err = -1 );
	require_action( memcmp( ptr, str, len ) == 0, exit, err = -1 );
	
	DataBuffer_Free( &db );
	
	// WPA_PSK_Derive
	
	memset( buf, 0, 32 );
	WPA_PSK_Derive( "password", kSizeCString, "IEEE", kSizeCString, buf );
	require_action( memcmp( buf, 
		"\xf4\x2c\x6f\xc5\x2d\xf0\xeb\xef\x9e\xbb\x4b\x90\xb3\x8a\x5f\x90"
		"\x2e\x83\xfe\x1b\x13\x5a\x70\xe2\x3a\xed\x76\x2e\x97\x10\xa1\x2e", 32 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 32 );
	WPA_PSK_Derive( "ThisIsAPassword", kSizeCString, "ThisIsASSID", kSizeCString, buf );
	require_action( memcmp( buf, 
		"\x0d\xc0\xd6\xeb\x90\x55\x5e\xd6\x41\x97\x56\xb9\xa1\x5e\xc3\xe3"
		"\x20\x9b\x63\xdf\x70\x7d\xd5\x08\xd1\x45\x81\xf8\x98\x27\x21\xaf", 32 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 32 );
	WPA_PSK_Derive( "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kSizeCString, "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ", kSizeCString, buf );
	require_action( memcmp( buf, 
		"\xbe\xcb\x93\x86\x6b\xb8\xc3\x83\x2c\xb7\x77\xc2\xf5\x59\x80\x7c"
		"\x8c\x59\xaf\xcb\x6e\xae\x73\x48\x85\x00\x13\x00\xa9\x81\xcc\x62", 32 ) == 0, exit, err = -1 );
	
exit:
	printf( "IEEE80211Utils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
