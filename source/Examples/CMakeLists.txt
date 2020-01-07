#begin of CMakeLists.txt
cmake_minimum_required( VERSION 2.8.10 )
cmake_policy( SET CMP0004 OLD )

#setting module name, must use the same value if parent module exists
set ( MODULE_SAMPLE AirPlay_Demo)

#setting module version variable
set( CMAKE_INSTALL_RPATH_USE_LINK_PATH on )
set( CMAKE_VERBOSE_MAKEFILE off  )

set( SOVERSION            0                                    )
set( VERSION              "${SOVERSION}.0"                   )

include( FindPkgConfig )
pkg_check_modules( ${MODULE_SAMPLE} REQUIRED
	libiconv
	libCoreUtils
	libAirPlay
	libAirPlaySupport
)
foreach( CFLAG ${${MODULE_SAMPLE}_CFLAGS} )
	set( ${MODULE_SAMPLE}_EXTRA_CFLAGS "${${MODULE_SAMPLE}_EXTRA_CFLAGS} ${CFLAG}" )
endforeach( CFLAG )

#adding link parameters from libraries dependency
foreach( LDFLAG ${${MODULE_SAMPLE}_LDFLAGS} )
	set( ${MODULE_SAMPLE}_EXTRA_LDFLAGS "${${MODULE_SAMPLE}_EXTRA_LDFLAGS} ${LDFLAG}" )
endforeach( LDFLAG )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_SAMPLE}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -DCFCOMPAT_NOTIFICATIONS_ENABLED=0 -DCFL_BINARY_PLISTS=1 -DAIRPLAY_THREADED_MAIN=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCFLITE_ENABLED=1 -DDEBUG_CF_OBJECTS_ENABLED=1 -DDEBUG_EXPORT_ERROR_STRINGS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_MD5_UTILS=1 -DAES_UTILS_HAS_GLADMAN_GCM=0 -DAES_UTILS_USE_GLADMAN_AES=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_SHA_UTILS=1 -DTARGET_NO_OPENSSL=1 -DUSE_VIA_ACE_IF_PRESENT=0" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAIRPLAY_DEVICE_NAME=\"\\\"Media-Hub v2.0\\\"\" -DMFI_AUTH_DEVICE_PATH=\"\\\"/dev/i2c-1\\\"\"" )

IF(GCOV)
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF()
set(
	AIRPLAY_DEMO_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/AppleCarPlay_AppStub.c
#	${CMAKE_CURRENT_SOURCE_DIR}/HIDKnob.c
#	${CMAKE_CURRENT_SOURCE_DIR}/HIDMediaButtons.c
#	${CMAKE_CURRENT_SOURCE_DIR}/HIDTelephony.c
#	${CMAKE_CURRENT_SOURCE_DIR}/HIDTouchScreen.c
#	${CMAKE_CURRENT_SOURCE_DIR}/HIDTouchpad.c
)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/
)
add_executable(
	${MODULE_SAMPLE}
	${AIRPLAY_DEMO_SOURCE}
)
install(
	TARGETS
	${MODULE_SAMPLE}
	DESTINATION     bin	
)









