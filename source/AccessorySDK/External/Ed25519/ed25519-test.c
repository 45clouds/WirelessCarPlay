/*
	File:    	ed25519-test.c
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
	
	Copyright (C) 2011-2015 Apple Inc. All Rights Reserved.
*/

#include "CommonServices.h"
#include "DebugServices.h"
#include "ed25519.h"
#include "Small25519.h"
#include "StringUtils.h"

// Thin abstraction for different implementations.

typedef void	( *ed25519_make_key_pair_f )( uint8_t outPK[ 32 ], uint8_t outSK[ 32 ] );
typedef void	( *ed25519_sign_f )( uint8_t outSig[ 64 ], const void *inMsg, size_t inLen, const uint8_t inPK[ 32 ], const uint8_t inSK[ 32 ] );
typedef int		( *ed25519_verify_f )( const void *inMsg, size_t inLen, const uint8_t inSig[ 64 ], const uint8_t inPK[ 32 ] );

typedef struct
{
	const char *				label;
	ed25519_make_key_pair_f		make_key_pair_f;
	ed25519_sign_f				sign_f;
	ed25519_verify_f			verify_f;
	
}	ed25519_f;

static ed25519_f	kEd25519_f		= { "ref",			ed25519_make_key_pair_ref,		ed25519_sign_ref,		ed25519_verify_ref };
static ed25519_f	kSmall25519_f	= { "small",		ed25519_make_key_pair_small,	ed25519_sign_small,		ed25519_verify_small };

// Test vector.

typedef struct
{
	const char *		sk;
	const char *		pk;
	const char *		msg;
	const char *		sig;
	
}	Ed25519TestVector;

// Prototypes.

static OSStatus	_ed25519_test_basic( const ed25519_f *inF, int inPerf );
static OSStatus	_ed25519_test_file( const ed25519_f *inF, const char *inPath, int inLog );
static OSStatus	_ed25519_test_one_vector( const Ed25519TestVector *inTV );

static const Ed25519TestVector		kEd25519TestVectors[] = 
{
	// draft-josefsson-eddsa-ed25519 - Test 1
	{
		/* SK */	"9d61b19deffd5a60ba844af492ec2cc4"
					"4449c5697b326919703bac031cae7f60",
		
		/* PK */	"d75a980182b10ab7d54bfed3c964073a"
					"0ee172f3daa62325af021a68f707511a",
		
		/* Msg */	"",
		
		/* Sig */	"e5564300c360ac729086e2cc806e828a"
					"84877f1eb8e5d974d873e06522490155"
					"5fb8821590a33bacc61e39701cf9b46b"
					"d25bf5f0595bbe24655141438e7a100b"
	},
	// draft-josefsson-eddsa-ed25519 - Test 2
	{
		/* SK */	"4ccd089b28ff96da9db6c346ec114e0f"
					"5b8a319f35aba624da8cf6ed4fb8a6fb",
		
		/* PK */	"3d4017c3e843895a92b70aa74d1b7ebc"
					"9c982ccf2ec4968cc0cd55f12af4660c",
		
		/* Msg */	"72",
		
		/* Sig */	"92a009a9f0d4cab8720e820b5f642540"
					"a2b27b5416503f8fb3762223ebdb69da"
					"085ac1e43e15996e458f3613d0f11d8c"
					"387b2eaeb4302aeeb00d291612bb0c00"
	},
	// draft-josefsson-eddsa-ed25519 - Test 3
	{
		/* SK */	"c5aa8df43f9f837bedb7442f31dcb7b1"
					"66d38535076f094b85ce3a2e0b4458f7",
		
		/* PK */	"fc51cd8e6218a1a38da47ed00230f058"
					"0816ed13ba3303ac5deb911548908025",
		
		/* Msg */	"af82",
		
		/* Sig */	"6291d657deec24024827e69c3abe01a3"
					"0ce548a284743a445e3680d7db5ac3ac"
					"18ff9b538d16f290ae67f760984dc659"
					"4a7c15e9716ed28dc027beceea1ec40a"
	},
	// draft-josefsson-eddsa-ed25519 - Test 1024
	{
		/* SK */	"f5e5767cf153319517630f226876b86c"
					"8160cc583bc013744c6bf255f5cc0ee5",
		
		/* PK */	"278117fc144c72340f67d0f2316e8386"
					"ceffbf2b2428c9c51fef7c597f1d426e",
		
		/* Msg */	"08b8b2b733424243760fe426a4b54908"
					"632110a66c2f6591eabd3345e3e4eb98"
					"fa6e264bf09efe12ee50f8f54e9f77b1"
					"e355f6c50544e23fb1433ddf73be84d8"
					"79de7c0046dc4996d9e773f4bc9efe57"
					"38829adb26c81b37c93a1b270b20329d"
					"658675fc6ea534e0810a4432826bf58c"
					"941efb65d57a338bbd2e26640f89ffbc"
					"1a858efcb8550ee3a5e1998bd177e93a"
					"7363c344fe6b199ee5d02e82d522c4fe"
					"ba15452f80288a821a579116ec6dad2b"
					"3b310da903401aa62100ab5d1a36553e"
					"06203b33890cc9b832f79ef80560ccb9"
					"a39ce767967ed628c6ad573cb116dbef"
					"efd75499da96bd68a8a97b928a8bbc10"
					"3b6621fcde2beca1231d206be6cd9ec7"
					"aff6f6c94fcd7204ed3455c68c83f4a4"
					"1da4af2b74ef5c53f1d8ac70bdcb7ed1"
					"85ce81bd84359d44254d95629e9855a9"
					"4a7c1958d1f8ada5d0532ed8a5aa3fb2"
					"d17ba70eb6248e594e1a2297acbbb39d"
					"502f1a8c6eb6f1ce22b3de1a1f40cc24"
					"554119a831a9aad6079cad88425de6bd"
					"e1a9187ebb6092cf67bf2b13fd65f270"
					"88d78b7e883c8759d2c4f5c65adb7553"
					"878ad575f9fad878e80a0c9ba63bcbcc"
					"2732e69485bbc9c90bfbd62481d9089b"
					"eccf80cfe2df16a2cf65bd92dd597b07"
					"07e0917af48bbb75fed413d238f5555a"
					"7a569d80c3414a8d0859dc65a46128ba"
					"b27af87a71314f318c782b23ebfe808b"
					"82b0ce26401d2e22f04d83d1255dc51a"
					"ddd3b75a2b1ae0784504df543af8969b"
					"e3ea7082ff7fc9888c144da2af58429e"
					"c96031dbcad3dad9af0dcbaaaf268cb8"
					"fcffead94f3c7ca495e056a9b47acdb7"
					"51fb73e666c6c655ade8297297d07ad1"
					"ba5e43f1bca32301651339e22904cc8c"
					"42f58c30c04aafdb038dda0847dd988d"
					"cda6f3bfd15c4b4c4525004aa06eeff8"
					"ca61783aacec57fb3d1f92b0fe2fd1a8"
					"5f6724517b65e614ad6808d6f6ee34df"
					"f7310fdc82aebfd904b01e1dc54b2927"
					"094b2db68d6f903b68401adebf5a7e08"
					"d78ff4ef5d63653a65040cf9bfd4aca7"
					"984a74d37145986780fc0b16ac451649"
					"de6188a7dbdf191f64b5fc5e2ab47b57"
					"f7f7276cd419c17a3ca8e1b939ae49e4"
					"88acba6b965610b5480109c8b17b80e1"
					"b7b750dfc7598d5d5011fd2dcc5600a3"
					"2ef5b52a1ecc820e308aa342721aac09"
					"43bf6686b64b2579376504ccc493d97e"
					"6aed3fb0f9cd71a43dd497f01f17c0e2"
					"cb3797aa2a2f256656168e6c496afc5f"
					"b93246f6b1116398a346f1a641f3b041"
					"e989f7914f90cc2c7fff357876e506b5"
					"0d334ba77c225bc307ba537152f3f161"
					"0e4eafe595f6d9d90d11faa933a15ef1"
					"369546868a7f3a45a96768d40fd9d034"
					"12c091c6315cf4fde7cb68606937380d"
					"b2eaaa707b4c4185c32eddcdd306705e"
					"4dc1ffc872eeee475a64dfac86aba41c"
					"0618983f8741c5ef68d3a101e8a3b8ca"
					"c60c905c15fc910840b94c00a0b9d0",
		
		/* Sig */	"0aab4c900501b3e24d7cdf4663326a3a"
					"87df5e4843b2cbdb67cbf6e460fec350"
					"aa5371b1508f9f4528ecea23c436d94b"
					"5e8fcd4f681e30a6ac00a9704a188a03"
	}
};

//===========================================================================================================================
//	ed25519_test
//===========================================================================================================================

OSStatus	ed25519_test( int inPerf )
{
	OSStatus		err;
	size_t			i;
	
	err = _ed25519_test_basic( &kEd25519_f, inPerf );
	require_noerr( err, exit );
	
	err = _ed25519_test_basic( &kSmall25519_f, inPerf );
	require_noerr( err, exit );
	
	
	for( i = 0; i < countof( kEd25519TestVectors ); ++i )
	{
		err = _ed25519_test_one_vector( &kEd25519TestVectors[ i ] );
		require_noerr( err, exit );
	}
	
exit:
	printf( "ed25519_test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_ed25519_test_basic
//===========================================================================================================================

static OSStatus	_ed25519_test_basic( const ed25519_f *inF, int inPerf )
{
	OSStatus			err;
	CFAbsoluteTime		t;
	uint8_t				pk[ 32 ];
	uint8_t				sk[ 32 ];
	uint8_t				sig[ 64 ];
	uint8_t				sig2[ 64 ];
	uint8_t				msg[ 32 ];
	int					i, n;
	
	// ed25519_make_key_pair
	
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		inF->make_key_pair_f( pk, sk );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\tm:   %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	// ed25519_sign
	
	inF->make_key_pair_f( pk, sk );
	memset( msg, 'A', sizeof( msg ) );
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		msg[ ( (size_t) i ) % sizeof( msg ) ] += 1;
		inF->sign_f( sig, msg, sizeof( msg ), pk, sk );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\ts:   %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	// ed25519_verify (passing)
	
	inF->make_key_pair_f( pk, sk );
	memset( msg, 'B', sizeof( msg ) );
	inF->sign_f( sig, msg, sizeof( msg ), pk, sk );
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		err = inF->verify_f( msg, sizeof( msg ), sig, pk );
		require_noerr( err, exit );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\tv+:  %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	// ed25519_verify (failing)
	
	inF->make_key_pair_f( pk, sk );
	memset( msg, 'C', sizeof( msg ) );
	inF->sign_f( sig, msg, sizeof( msg ), pk, sk );
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		memcpy( sig2, sig, sizeof( sig2 ) );
		sig2[ ( (size_t) i ) % sizeof( sig2 ) ] ^= 0xAA;
		err = inF->verify_f( msg, sizeof( msg ), sig2, pk );
		require_action( err != 0, exit, err = -1 );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\tv-:  %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	// ed25519_sign + ed25519_verify (passing)
	
	memset( pk, 0, sizeof( pk ) );
	memset( sk, 0, sizeof( sk ) );
	inF->make_key_pair_f( pk, sk );
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		memset( msg, 'A' + i, sizeof( msg ) );
		inF->sign_f( sig, msg, sizeof( msg ), pk, sk );
		err = inF->verify_f( msg, sizeof( msg ), sig, pk );
		require_noerr( err, exit );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\tsv+: %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	// ed25519_sign + ed25519_verify (failing)
	
	memset( pk, 0, sizeof( pk ) );
	memset( sk, 0, sizeof( sk ) );
	inF->make_key_pair_f( pk, sk );
	t = CFAbsoluteTimeGetCurrent();
	n = inPerf ? 1000 : 1;
	for( i = 0; i < n; ++i )
	{
		memset( msg, 'a', sizeof( msg ) );
		memset( sig, 0, sizeof( sig ) );
		inF->sign_f( sig, msg, sizeof( msg ), pk, sk );
		sig[ i % 64 ] ^= 1;
		err = inF->verify_f( msg, sizeof( msg ), sig, pk );
		require_action( err != 0, exit, err = -1 );
	}
	t = CFAbsoluteTimeGetCurrent() - t;
	if( inPerf ) printf( "\tsv-: %10f seconds, %5.0f microseconds per iteration, %d iterations\n", t, 1000000 * ( t / n ), n );
	
	err = kNoErr;
	
exit:
	printf( "ed25519_test (%s): %s\n", inF->label, !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	ed25519_test_file
//===========================================================================================================================

OSStatus	ed25519_test_file( const char *inPath, int inLog )
{
	OSStatus		err;
	
	err = _ed25519_test_file( &kEd25519_f, inPath, inLog );
	require_noerr( err, exit );
	
	err = _ed25519_test_file( &kSmall25519_f, inPath, inLog );
	require_noerr( err, exit );
	
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ed25519_test_file
//===========================================================================================================================

static OSStatus	_ed25519_test_file( const ed25519_f *inF, const char *inPath, int inLog )
{
	OSStatus		err;
	FILE *			f;
	char			line[ 8192 ];
	const char *	src;
	const char *	end;
	uint8_t			sk[ 32 ];
	uint8_t			pk[ 32 ];
	uint8_t			msg[ 4096 ];
	size_t			len;
	uint8_t			sig[ 64 ];
	uint8_t			sig2[ 64 ];
	size_t			i;
	size_t			lineNum;
	
	// Expected format of file (see <http://ed25519.cr.yp.to/python/sign.input> for an example):
	//
	// fields on each input line: sk, pk, m, sm
	// each field hex
	// each field colon-terminated
	// sk includes pk at end
	// sm includes m at end
	
	f = fopen( inPath, "r" );
	require_action( f, exit, err = kOpenErr );
	
	lineNum = 1;
	while( ( src = fgets( line, (int) sizeof( line ), f ) ) != NULL )
	{
		// SK
		
		for( end = src; ( *end != '\0' ) && ( *end != ':' ); ++end ) {}
		require_action( ( end - src ) == 128, exit, err = -1 );
		if( inLog > 1 ) printf( "SK:  %.64s\n", src );
		for( i = 0; ( ( end - src ) >= 2 ) && ( i < 32 ); ++i, src += 2 ) sk[ i ] = HexPairToByte( src );
		require_action( i == 32, exit, err = -1 );
		src = end + 1;
		
		// PK
		
		for( end = src; ( *end != '\0' ) && ( *end != ':' ); ++end ) {}
		require_action( ( end - src ) == 64, exit, err = -1 );
		if( inLog > 1 ) printf( "PK:  %.64s\n", src );
		for( i = 0; ( ( end - src ) >= 2 ) && ( i < 32 ); ++i, src += 2 ) pk[ i ] = HexPairToByte( src );
		require_action( i == 32, exit, err = -1 );
		require_action( src == end, exit, err = -1 );
		++src;
		
		// MSG
		
		for( end = src; ( *end != '\0' ) && ( *end != ':' ); ++end ) {}
		if( inLog > 1 ) printf( "MSG: %.*s\n", (int)( end - src ), src );
		for( i = 0; ( ( end - src ) >= 2 ) && ( i < sizeof( msg ) ); ++i, src += 2 ) msg[ i ] = HexPairToByte( src );
		len = i;
		require_action( src == end, exit, err = -1 );
		++src;
		
		// SIG
		
		for( end = src; ( *end != '\0' ) && ( *end != ':' ); ++end ) {}
		require_action( ( end - src ) >= 128, exit, err = -1 );
		if( inLog > 1 ) printf( "SIG: %.128s\n", src );
		for( i = 0; ( ( end - src ) >= 2 ) && ( i < 64 ); ++i, src += 2 ) sig[ i ] = HexPairToByte( src );
		
		// Check it
		
		memset( sig2, 'a', sizeof( sig2 ) );
		inF->sign_f( sig2, msg, len, pk, sk );
		require_action( memcmp( sig, sig2, sizeof( sig ) ) == 0, exit, err = -2 );
		err = inF->verify_f( msg, len, sig, pk );
		require_noerr( err, exit );
		if( inLog > 1 ) printf( "\n" );
		
		if( inLog == 1 )
		{
			printf( "." );
			fflush( stdout );
			if( ( lineNum > 0 ) && ( ( lineNum % 100 ) == 0 ) )
			{
				printf( "\n" );
				fflush( stdout );
			}
		}
		++lineNum;
	}
	err = kNoErr;
	
exit:
	if( f ) fclose( f );
	printf( "ed25519_test_file (%s): %s\n", inF->label, !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_ed25519_test_one_vector
//===========================================================================================================================

static OSStatus	_ed25519_test_one_vector( const Ed25519TestVector *inTV )
{
	OSStatus		err;
	uint8_t			sk[ 32 ];
	uint8_t			pk[ 32 ];
	uint8_t *		msgPtr = NULL;
	size_t			msgLen;
	uint8_t			sig[ 64 ];
	uint8_t			sig2[ 64 ];
	size_t			len;
	
	err = HexToData( inTV->sk, kSizeCString, kHexToData_DefaultFlags, sk, sizeof( sk ), NULL, &len, NULL );
	require_noerr( err, exit );
	require_action( len == sizeof( sk ), exit, err = kSizeErr );
	
	err = HexToData( inTV->pk, kSizeCString, kHexToData_DefaultFlags, pk, sizeof( pk ), NULL, &len, NULL );
	require_noerr( err, exit );
	require_action( len == sizeof( pk ), exit, err = kSizeErr );
	
	err = HexToDataCopy( inTV->msg, kSizeCString, kHexToData_DefaultFlags, &msgPtr, &msgLen, NULL );
	require_noerr( err, exit );
	
	err = HexToData( inTV->sig, kSizeCString, kHexToData_DefaultFlags, sig, sizeof( sig ), NULL, &len, NULL );
	require_noerr( err, exit );
	require_action( len == sizeof( sig ), exit, err = kSizeErr );
	
	// Signing
	
	memset( sig2, 0, sizeof( sig2 ) );
	kEd25519_f.sign_f( sig2, msgPtr, msgLen, pk, sk );
	require_action( memcmp( sig, sig2, sizeof( sig ) ) == 0, exit, err = kSignatureErr );
	
	memset( sig2, 0, sizeof( sig2 ) );
	kSmall25519_f.sign_f( sig2, msgPtr, msgLen, pk, sk );
	require_action( memcmp( sig, sig2, sizeof( sig ) ) == 0, exit, err = kSignatureErr );
	
	
	// Verifying
	
	err = kEd25519_f.verify_f( msgPtr, msgLen, sig, pk );
	require_action( err == 0, exit, err = kAuthenticationErr );
	
	err = kSmall25519_f.verify_f( msgPtr, msgLen, sig, pk );
	require_action( err == 0, exit, err = kAuthenticationErr );
	
	
exit:
	FreeNullSafe( msgPtr );
	return( err );
}
