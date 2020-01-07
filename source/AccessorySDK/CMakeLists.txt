#begin of CMakeLists.txt
cmake_minimum_required( VERSION 2.8.10 )
cmake_policy( SET CMP0004 OLD )

#setting module name, must use the same value if parent module exists
set ( MODULE_NAME CoreUtils)

#setting module version variable
set( CMAKE_INSTALL_RPATH_USE_LINK_PATH on )
set( CMAKE_VERBOSE_MAKEFILE off  )

if( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	FIND_PACKAGE( Subversion )
	if( SUBVERSION_FOUND )
		Subversion_WC_INFO( ${CMAKE_SOURCE_DIR}/../ ${MODULE_NAME} )
		set( REVERSION ${${MODULE_NAME}_WC_REVISION} )
	endif( SUBVERSION_FOUND)
else( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	set( REVERSION 0 )
endif( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )

set( SOVERSION            0                                    )
set( VERSION              "${SOVERSION}.${REVERSION}"                   )

include( FindPkgConfig )
pkg_check_modules( ${MODULE_NAME} REQUIRED
	libiconv
	libudev
	glib-2.0
)
foreach( CFLAG ${${MODULE_NAME}_CFLAGS} )
	set( ${MODULE_NAME}_EXTRA_CFLAGS "${${MODULE_NAME}_EXTRA_CFLAGS} ${CFLAG}" )
endforeach( CFLAG )

#adding link parameters from libraries dependency
foreach( LDFLAG ${${MODULE_NAME}_LDFLAGS} )
	set( ${MODULE_NAME}_EXTRA_LDFLAGS "${${MODULE_NAME}_EXTRA_LDFLAGS} ${LDFLAG}" )
endforeach( LDFLAG )	

#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_NAME}_EXTRA_CFLAGS}" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_NAME}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -DCFCOMPAT_NOTIFICATIONS_ENABLED=0 -DCFL_BINARY_PLISTS=1 -DAIRPLAY_THREADED_MAIN=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCFLITE_ENABLED=1 -DDEBUG_CF_OBJECTS_ENABLED=1 -DDEBUG_EXPORT_ERROR_STRINGS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_MD5_UTILS=1 -DAES_UTILS_HAS_GLADMAN_GCM=0 -DAES_UTILS_USE_GLADMAN_AES=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_SHA_UTILS=1 -DTARGET_NO_OPENSSL=1 -DUSE_VIA_ACE_IF_PRESENT=0" )
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DNS_SD_LIBDISPATCH=0 -DASYNC_CONNECTION_BONJOUR=0 -DBONJOUR_ENABLED=0" ) 
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAIRPLAY_DEVICE_NAME=\"\\\"Media-Hub v2.0\\\"\"" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTOMMATH=1" )
IF(GCOV)
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF()
set(
	CARPLAY_LIB_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AESUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ArrayUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AsyncConnection.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AtomicUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AudioUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/Base64Utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/BonjourAdvertiser.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/BonjourBrowser.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFCompat.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLite.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLiteBinaryPlist.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLitePreferencesFile.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLiteRunLoopSelect.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFPrefUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ChaCha20Poly1305.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ChecksumUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CommandLineUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CryptoHashUtils.c

	${CMAKE_CURRENT_SOURCE_DIR}/External/Curve25519/curve25519-donna.c

	${CMAKE_CURRENT_SOURCE_DIR}/Support/DataBufferUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DebugIPCUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DebugServices.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DispatchLite.c

	${CMAKE_CURRENT_SOURCE_DIR}/EasyConfig/EasyConfigUtils.c	
	${CMAKE_CURRENT_SOURCE_DIR}/External/Ed25519/ed25519.c
	
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HIDUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HIDUtilsLinux.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPClient.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPMessage.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPServer.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/IEEE80211Utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/IPCUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/JSONUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/KeychainLiteFile.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/KeychainUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/LogUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MathUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MD5Utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MFiSAP.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MFiServerPlatformLinux.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MiscUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetTransportChaCha20Poly1305.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetworkChangeListener.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NTPUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/PairingUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/PrintFUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/RandomNumberUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ScreenUtils.c
#${CMAKE_CURRENT_SOURCE_DIR}/Support/ScreenUtilsDLL.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SerialPortUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SHAUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SRPUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/StringUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SystemUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TestUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ThreadUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TickUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TimeUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TLVUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/URLUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/utfconv.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/UUIDUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/XPCLite.c
	${CMAKE_CURRENT_SOURCE_DIR}/Support/XPCUtils.c

	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aes_modes.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aescrypt.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aeskey.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aestab.c

	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_fast_mp_invmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_fast_mp_montgomery_reduce.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_fast_s_mp_mul_digs.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_fast_s_mp_mul_high_digs.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_fast_s_mp_sqr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_2expt.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_abs.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_add.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_add_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_clamp.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_clear.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_clear_multi.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_cmp.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_cmp_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_cmp_mag.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_cnt_lsb.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_copy.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_count_bits.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_div.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_div_2.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_div_2d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_div_3.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_div_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_dr_is_modulus.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_dr_reduce.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_dr_setup.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_exch.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_exptmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_exptmod_fast.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_grow.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_init.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_init_copy.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_init_multi.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_init_size.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_invmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_invmod_slow.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_karatsuba_mul.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_karatsuba_sqr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_lshd.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mod_2d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mod_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_montgomery_calc_normalization.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_montgomery_reduce.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_montgomery_setup.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mul.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mul_2.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mul_2d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mul_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_mulmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_prime_is_divisible.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_prime_is_prime.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_prime_miller_rabin.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_prime_rabin_miller_trials.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_read_unsigned_bin.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_2k.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_2k_l.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_2k_setup.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_2k_setup_l.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_is_2k.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_is_2k_l.c

	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_reduce_setup.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_rshd.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_set_int.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_set.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_sqr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_sqrmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_sub_d.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_sub.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_to_unsigned_bin.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_toom_mul.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_toom_sqr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_unsigned_bin_size.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_mp_zero.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_prime_tab.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_reverse.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_add.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_exptmod.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_mul_digs.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_mul_high_digs.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_sqr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bn_s_mp_sub.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/bncore.c

	
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP/cstr.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP/srp.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP/srp6_client.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP/srp6_server.c
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP/t_math.c

)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/EasyConfig
	${CMAKE_CURRENT_SOURCE_DIR}/Sources
	${CMAKE_CURRENT_SOURCE_DIR}/Support
	${CMAKE_CURRENT_SOURCE_DIR}/External/Curve25519
	${CMAKE_CURRENT_SOURCE_DIR}/External/Ed25519
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES
	${CMAKE_CURRENT_SOURCE_DIR}/External/Small25519
	${CMAKE_CURRENT_SOURCE_DIR}/External/SRP
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath
#	${CMAKE_CURRENT_SOURCE_DIR}/CoreUtils
)
add_library(
	${MODULE_NAME}
	MODULE
	${CARPLAY_LIB_SOURCE}
)
target_link_libraries(
	${MODULE_NAME}
	${${MODULE_NAME}_EXTRA_LDFLAGS}
	"-ldns_sd"
)

set_target_properties(
	${MODULE_NAME}
	PROPERTIES VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)


install(
	FILES
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AESUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ArrayUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AsyncConnection.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AtomicUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/AudioUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/Base64Utils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFCompat.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLite.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFLiteBinaryPlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Support/CFPrefUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CFUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ChaCha20Poly1305.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ChecksumUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CommandLineUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CommonServices.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/CryptoHashUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/Curve25519/curve25519-donna.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DataBufferUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DebugIPCUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DebugServices.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DispatchLite.h
	${CMAKE_CURRENT_SOURCE_DIR}/EasyConfig/EasyConfigUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/Ed25519/ed25519.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HIDUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPClient.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPMessage.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPServer.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/HTTPUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/IEEE80211Utils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/IPCUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/JSONUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/KeychainUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/LogUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MathUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MD5Utils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MFiSAP.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/MiscUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetTransportChaCha20Poly1305.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NetworkChangeListener.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/NTPUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/PairingUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/PrintFUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/RandomNumberUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ScreenUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SerialPortUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SHAUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SRPUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/StringUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/SystemUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TestUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/ThreadUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TickUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TimeUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/TLVUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/URLUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/utfconv.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/UUIDUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/XPCLite.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/XPCUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/DispatchLite.h
	${CMAKE_CURRENT_SOURCE_DIR}/Support/BonjourAdvertiser.h	
	${CMAKE_CURRENT_SOURCE_DIR}/Support/BonjourBrowser.h

	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aes.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/brg_endian.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/brg_types.h
	DESTINATION include/CoreUtils
)
install(
	FILES
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/aes.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/brg_endian.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/GladmanAES/brg_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/External/LibTomMath/tommath.h
	DESTINATION include
)
install(
	TARGETS
	${MODULE_NAME}
	DESTINATION     lib	
)

install( 
	FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkg-config/lib${MODULE_NAME}.pc       # from
	DESTINATION		lib/pkgconfig/                                  # to
)

