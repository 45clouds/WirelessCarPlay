#begin of CMakeLists.txt
cmake_minimum_required( VERSION 2.8.10 )
cmake_policy( SET CMP0004 OLD )

#setting module name, must use the same value if parent module exists
set ( MODULE_NAME AirPlaySupport)

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

#setting module version variable
set( CMAKE_INSTALL_RPATH_USE_LINK_PATH on )
set( CMAKE_VERBOSE_MAKEFILE off  )

include( FindPkgConfig )

if ( SUPPORT_WIRELESS_CARPLAY )	
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIRELESS_CARPLAY=1" )
pkg_check_modules( ${MODULE_NAME} REQUIRED
	libiconv
#	libCoreUtils
	glib-2.0
	fdk-aac
	opus
)
else( SUPPORT_WIRELESS_CARPLAY )	
pkg_check_modules( ${MODULE_NAME} REQUIRED
	libiconv
#	libCoreUtils
	glib-2.0
)
endif( SUPPORT_WIRELESS_CARPLAY )	

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
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAIRPLAY_DEVICE_NAME=\"\\\"Media-Hub v2.0\\\"\"" )
IF(GCOV)
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF()
set(
	AIRPLAY_SUPPORT_LIB_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/../Sources/AirPlaySettings.c
	${CMAKE_CURRENT_SOURCE_DIR}/APSAudioConverterStub.c
	${CMAKE_CURRENT_SOURCE_DIR}/APSAudioSessionStub.c
)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../Sources
	${CMAKE_CURRENT_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
add_library(
	${MODULE_NAME}
	MODULE
	${AIRPLAY_SUPPORT_LIB_SOURCE}
)

set_target_properties(
	${MODULE_NAME}
	PROPERTIES VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)

if ( SUPPORT_WIRELESS_CARPLAY )	
target_link_libraries(
	${MODULE_NAME}
	${${MODULE_NAME}_EXTRA_LDFLAGS}
	"-lfdk-aac"
	"-lopus"
)
else( SUPPORT_WIRELESS_CARPLAY )	
target_link_libraries(
	${MODULE_NAME}
	${${MODULE_NAME}_EXTRA_LDFLAGS}
)
endif( SUPPORT_WIRELESS_CARPLAY )	

install(
	TARGETS
	${MODULE_NAME}
	DESTINATION     lib	
)


set ( MODULE_AUDIO 	AudioStream)

if( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	FIND_PACKAGE( Subversion )
	if( SUBVERSION_FOUND )
		Subversion_WC_INFO( ${CMAKE_SOURCE_DIR}/../ ${MODULE_AUDIO} )
		set( REVERSION ${${MODULE_AUDIO}_WC_REVISION} )
	endif( SUBVERSION_FOUND)
else( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	set( REVERSION 0 )
endif( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )

set( SOVERSION            0                                    )
set( VERSION              "${SOVERSION}.${REVERSION}"                   )


pkg_check_modules( ${MODULE_AUDIO} REQUIRED
	libiconv
#	libCoreUtils
	glib-2.0
	gstreamer-1.0
)
foreach( CFLAG ${${MODULE_AUDIO}_CFLAGS} )
	set( ${MODULE_AUDIO}_EXTRA_CFLAGS "${${MODULE_AUDIO}_EXTRA_CFLAGS} ${CFLAG}" )
endforeach( CFLAG )

#adding link parameters from libraries dependency
foreach( LDFLAG ${${MODULE_AUDIO}_LDFLAGS} )
	set( ${MODULE_AUDIO}_EXTRA_LDFLAGS "${${MODULE_AUDIO}_EXTRA_LDFLAGS} ${LDFLAG}" )
endforeach( LDFLAG )

#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_NAME}_EXTRA_CFLAGS}" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_AUDIO}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1" )

set(
	AUDIO_LIB_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/AudioUtilsStub.c
)

add_library(
	${MODULE_AUDIO}
	MODULE
	${AUDIO_LIB_SOURCE}
)

set_target_properties(
	${MODULE_AUDIO}
	PROPERTIES VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)

install(
	TARGETS
	${MODULE_AUDIO}
	DESTINATION     lib	
)

set ( MODULE_SCREEN 	ScreenStream)

if( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	FIND_PACKAGE( Subversion )
	if( SUBVERSION_FOUND )
		Subversion_WC_INFO( ${CMAKE_SOURCE_DIR}/../ ${MODULE_SCREEN} )
		set( REVERSION ${${MODULE_SCREEN}_WC_REVISION} )
	endif( SUBVERSION_FOUND)
else( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )
	set( REVERSION 0 )
endif( EXISTS ${CMAKE_SOURCE_DIR}/../../../../.svn )

set( SOVERSION            0                                    )
set( VERSION              "${SOVERSION}.${REVERSION}"                   )

pkg_check_modules( ${MODULE_SCREEN} REQUIRED
	libiconv
#	libCoreUtils
	glib-2.0
	gstreamer-1.0
)
foreach( CFLAG ${${MODULE_SCREEN}_CFLAGS} )
	set( ${MODULE_SCREEN}_EXTRA_CFLAGS "${${MODULE_SCREEN}_EXTRA_CFLAGS} ${CFLAG}" )
endforeach( CFLAG )

#adding link parameters from libraries dependency
foreach( LDFLAG ${${MODULE_SCREEN}_LDFLAGS} )
	set( ${MODULE_SCREEN}_EXTRA_LDFLAGS "${${MODULE_SCREEN}_EXTRA_LDFLAGS} ${LDFLAG}" )
endforeach( LDFLAG )

#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_NAME}_EXTRA_CFLAGS}" )
if ( B511 )	
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_SCREEN}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1 -DB511=1" )
else( B511 )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${MODULE_SCREEN}_EXTRA_CFLAGS} -fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1" )
endif( B511 )
set(
	SCREEN_LIB_SOURCE
	
	${CMAKE_CURRENT_SOURCE_DIR}/ScreenUtilsStub.c
)

add_library(
	${MODULE_SCREEN}
	MODULE
	${SCREEN_LIB_SOURCE}
)

set_target_properties(
	${MODULE_SCREEN}
	PROPERTIES VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)

install(
	TARGETS
	${MODULE_SCREEN}
	DESTINATION     lib	
)

install( 
	FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkg-config/lib${MODULE_NAME}.pc         # from
	${CMAKE_CURRENT_SOURCE_DIR}/pkg-config/lib${MODULE_AUDIO}.pc
	${CMAKE_CURRENT_SOURCE_DIR}/pkg-config/lib${MODULE_SCREEN}.pc       # from
	DESTINATION		lib/pkgconfig/                                  # to
)

install(
	FILES
	${CMAKE_CURRENT_SOURCE_DIR}/../Sources/AirPlaySettings.h
	${CMAKE_CURRENT_SOURCE_DIR}/APSAudioConverter.h
	${CMAKE_CURRENT_SOURCE_DIR}/APSAudioSession.h
	${CMAKE_CURRENT_SOURCE_DIR}/APSDispatchUtils.h
	${CMAKE_CURRENT_SOURCE_DIR}/CarPlayControlCommon.h

	DESTINATION include
)

	



