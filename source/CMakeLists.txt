#begin of CMakeLists.txt
cmake_minimum_required( VERSION 2.8.10 )
cmake_policy( SET CMP0004 OLD )

#setting module name, must use the same value if parent module exists
set ( MODULE_AIRPLAY_NAME AirPlay)

if( EXISTS ${CMAKE_SOURCE_DIR}/../../../.svn )
	FIND_PACKAGE( Subversion )
	if( SUBVERSION_FOUND )
		Subversion_WC_INFO( ${CMAKE_SOURCE_DIR}/../ ${MODULE_AIRPLAY_NAME} )
		set( REVERSION ${${MODULE_AIRPLAY_NAME}_WC_REVISION} )
	endif( SUBVERSION_FOUND)
else( EXISTS ${CMAKE_SOURCE_DIR}/../../../.svn )
	set( REVERSION 0 )
endif( EXISTS ${CMAKE_SOURCE_DIR}/../../../.svn )

set( SOVERSION            0                                    )
set( VERSION              "${SOVERSION}.${REVERSION}"                   )

#setting module version variable
set( CMAKE_INSTALL_RPATH_USE_LINK_PATH on )
set( CMAKE_VERBOSE_MAKEFILE off  )

#ADD_SUBDIRECTORY(AccessorySDK)
#ADD_SUBDIRECTORY(Support)

include( FindPkgConfig )
pkg_check_modules( ${MODULE_AIRPLAY_NAME} REQUIRED
	libiconv
	glib-2.0
)
foreach( CFLAG ${${MODULE_AIRPLAY_NAME}_CFLAGS} )
	set( ${MODULE_AIRPLAY_NAME}_EXTRA_CFLAGS "${${MODULE_AIRPLAY_NAME}_EXTRA_CFLAGS} ${CFLAG}" )
endforeach( CFLAG )

#adding link parameters from libraries dependency
foreach( LDFLAG ${${MODULE_AIRPLAY_NAME}_LDFLAGS} )
	set( ${MODULE_AIRPLAY_NAME}_EXTRA_LDFLAGS "${${MODULE_AIRPLAY_NAME}_EXTRA_LDFLAGS} ${LDFLAG}" )
endforeach( LDFLAG )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_AIRPLAY_NAME}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -DCFCOMPAT_NOTIFICATIONS_ENABLED=0 -DCFL_BINARY_PLISTS=1 -DAIRPLAY_THREADED_MAIN=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCFLITE_ENABLED=1 -DDEBUG_CF_OBJECTS_ENABLED=1 -DDEBUG_EXPORT_ERROR_STRINGS=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_MD5_UTILS=1 -DAES_UTILS_HAS_GLADMAN_GCM=0 -DAES_UTILS_USE_GLADMAN_AES=1" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTARGET_HAS_SHA_UTILS=1 -DTARGET_NO_OPENSSL=1 -DUSE_VIA_ACE_IF_PRESENT=0" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAIRPLAY_DEVICE_NAME=\"\\\"Media-Hub v2.0\\\"\"" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAIRPLAY_HTTP_SERVER_LEGACY" )
IF(GCOV)
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF()
set(
	AIRPLAY_LIB_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverPOSIX.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverServer.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverSession.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverSessionScreen.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayUtils.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirTunesClock.c
	${CMAKE_CURRENT_SOURCE_DIR}/Transport/APAdvertiser.c
	${CMAKE_CURRENT_SOURCE_DIR}/Transport/APAdvertiserInfo.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/CarPlayControlClient.c
	${CMAKE_CURRENT_SOURCE_DIR}/Sources/airplayutil.c
)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/Sources
	${CMAKE_CURRENT_SOURCE_DIR}/Transport
	${CMAKE_CURRENT_SOURCE_DIR}/Support
	${CMAKE_CURRENT_SOURCE_DIR}/Common
)
add_library(
	${MODULE_AIRPLAY_NAME}
	MODULE
	${AIRPLAY_LIB_SOURCE}
)

install(
	TARGETS
	${MODULE_AIRPLAY_NAME}
	DESTINATION     lib	
)

set_target_properties(
	${MODULE_AIRPLAY_NAME}
	PROPERTIES VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)

install(
		FILES
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverSession.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayCommon.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverServer.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayUtils.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayVersion.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/CarPlayControlClient.h
		${CMAKE_CURRENT_SOURCE_DIR}/Sources/AirPlayReceiverServerPriv.h
		${CMAKE_CURRENT_SOURCE_DIR}/Transport/APAdvertiser.h
		${CMAKE_CURRENT_SOURCE_DIR}/Transport/APAdvertiserInfo.h
		DESTINATION		include
)

install(
		FILES
		${CMAKE_CURRENT_SOURCE_DIR}/pkg-config/libAirPlay.pc
		DESTINATION		lib/pkgconfig/                                  # to
)




















