#
#	File:    	Makefile
#	Package: 	CarPlay Communications Plug-in.
#	Abstract: 	n/a 
#	Version: 	280.33.8
#	
#	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
#	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
#	Apple software is governed by and subject to the terms and conditions of your MFi License,
#	including, but not limited to, the restrictions specified in the provision entitled ”Public 
#	Software”, and is further subject to your agreement to the following additional terms, and your 
#	agreement that the use, installation, modification or redistribution of this Apple software
#	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
#	please do not use, install, modify or redistribute this Apple software.
#	
#	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
#	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive 
#	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use, 
#	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and 
#	redistribute the Apple Software, with or without modifications, in binary form. While you may not 
#	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
#	form, you must retain this notice and the following text and disclaimers in all such redistributions
#	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
#	used to endorse or promote products derived from the Apple Software without specific prior written
#	permission from Apple. Except as expressly stated in this notice, no other rights or licenses, 
#	express or implied, are granted by Apple herein, including but not limited to any patent rights that
#	may be infringed by your derivative works or by other works in which the Apple Software may be 
#	incorporated.  
#	
#	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug 
#	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
#	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use, 
#	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
#	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products 
#	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you 
#	acknowledge and agree that Apple may exercise the license granted above without the payment of 
#	royalties or further consideration to Participant.
#	
#	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR 
#	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
#	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
#	IN COMBINATION WITH YOUR PRODUCTS.
#	
#	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES 
#	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
#	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
#	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE 
#	POSSIBILITY OF SUCH DAMAGE.
#	
#	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
#
#	Build options
#	-------------
#	debug		-- 1=Compile in debug code, asserts, etc. 0=Strip out debug code for a release build.
#	linux		-- 1=Build for Linux.
#	nv			-- 1=Build for NVIDIA Jetson reference board.
#	openssl		-- 1=Use OpenSSL for AES, SHA*, etc. 0=Compile in AES, SHA*, etc. code directly.
#	qnx			-- 1=Build for QNX.
#	stub		-- 1=Build AudioUtils/ScreenUtils stub DLLs.
#	verbose		-- 1=Produce verbose output.
#

INTERACTIVE := $(shell [ -t 0 ] && echo 1)
ifdef INTERACTIVE
	ColorMagenta=\033[0;35m
	ColorCyan=\033[0;36m
	ColorEnd=\033[0m
endif

# Build options

	hid					?= 1
	nohidraw			?= 1
	screen				?= 1
ifeq ($(debug),1)
	config				= Debug
else
	config				= Release
endif
ifeq ($(nv),1)
	linux				?= 1
	os					= linux
endif
ifeq ($(qnx),1)
	os					= qnx
	arch				= arm
endif
os						?= unknown
stub					?= 1
ifneq ($(verbose),1)
	quiet				= @
endif

# Build tools

ifeq ($(nv),1)
	# Use NVidia's Vibrante toolchain.

	ifndef NVTOOLSPATH
		$(error NVTOOLSPATH must be set to the location of the Vibrante toolchain)
	endif
	include $(NVTOOLSPATH)/make/nvdefs.mk

	CFLAGS				+= $(NV_PLATFORM_OPT) $(NV_PLATFORM_CFLAGS)
	CPPFLAGS			+= $(NV_PLATFORM_SDK_INC) $(NV_PLATFORM_CPPFLAGS)
	LINKFLAGS			+= $(NV_PLATFORM_SDK_LIB) $(NV_PLATFORM_LDFLAGS)
else
	ifeq ($(qnx),1)
		# Use the QNX toolchain.

		qnx_arch		= gcc_ntoarmv7le
		qnx_prefix		= ntoarmv7

		AR				= $(qnx_prefix)-ar
		CC				= qcc -V$(qnx_arch)
		CXX				= qcc -V$(qnx_arch)_cpp
		LD				= $(qnx_prefix)-ld
		NM				= $(qnx_prefix)-nm
		RANLIB			= $(qnx_prefix)-ranlib
		STRIP			= $(qnx_prefix)-strip

		CFLAGS			+= -w1
		CXXFLAGS		+= -w1
		LINKFLAGS		+= -w1
	else
		# Default toolchain.
		AR				?= ar
		CC				?= gcc
		CXX				?= g++
		LD				?= gcc
		NM				?= nm
		RANLIB			?= ranlib
		STRIP			?= strip
	endif
endif
ifeq ($(debug),1)
	# Override "STRIP" to do nothing in debug builds.
	STRIP				= :
endif

# Paths

SRCROOT					= $(CURDIR)/..
BUILDROOT				= $(SRCROOT)/build/$(config)-$(os)
OBJDIR					= $(BUILDROOT)/obj

COREUTILSROOT			?= $(SRCROOT)/AccessorySDK
MDNSROOT				?= $(SRCROOT)/../mDNSResponder

VPATH					+= $(SRCROOT)/PlatformPOSIX
VPATH					+= $(SRCROOT)/Sources
VPATH					+= $(SRCROOT)/Support
VPATH					+= $(SRCROOT)/Transport

# Includes

INCLUDES				+= -I$(SRCROOT)/Common
INCLUDES				+= -I$(SRCROOT)/Sources
INCLUDES				+= -I$(SRCROOT)/Support
INCLUDES				+= -I$(SRCROOT)/Transport
INCLUDES				+= -I$(BUILDROOT)
INCLUDES				+= -I$(BUILDROOT)/CoreUtils
INCLUDES				+= -I$(MDNSROOT)/mDNSShared

# Link flags

LINKFLAGS				+= -L$(BUILDROOT)
LINKFLAGS				+= -L$(MDNSROOT)/mDNSPosix/build/prod

# Warning flags

ifneq ($(qnx),1)
	COMMON_WARNINGS		+= -pedantic
endif
COMMON_WARNINGS			+= -W
COMMON_WARNINGS			+= -Wall
COMMON_WARNINGS			+= -Wempty-body
COMMON_WARNINGS			+= -Werror
COMMON_WARNINGS			+= -Wextra
COMMON_WARNINGS			+= -Wformat
COMMON_WARNINGS			+= -Wmissing-braces
COMMON_WARNINGS			+= -Wmissing-field-initializers
COMMON_WARNINGS			+= -Wno-cast-align
COMMON_WARNINGS			+= -Wno-unused-result
COMMON_WARNINGS			+= -Wparentheses
COMMON_WARNINGS			+= -Wpointer-sign
COMMON_WARNINGS			+= -Wshadow
COMMON_WARNINGS			+= -Wsign-compare
COMMON_WARNINGS			+= -Wswitch
COMMON_WARNINGS			+= -Wundef
COMMON_WARNINGS			+= -Wuninitialized
COMMON_WARNINGS			+= -Wunknown-pragmas
COMMON_WARNINGS			+= -Wunused-function
COMMON_WARNINGS			+= -Wunused-label
COMMON_WARNINGS			+= -Wunused-parameter
COMMON_WARNINGS			+= -Wunused-value
COMMON_WARNINGS			+= -Wunused-variable

CWARNS					+= $(COMMON_WARNINGS)
CWARNS					+= -Wmissing-prototypes

CXXWARNS				+= $(COMMON_WARNINGS)
CXXWARNS				+= -Wnon-virtual-dtor
CXXWARNS				+= -Woverloaded-virtual

# Preprocessor flags

COMMONFLAGS				+= -D__STDC_LIMIT_MACROS=1
COMMONFLAGS				+= -D_GNU_SOURCE
COMMONFLAGS				+= -DAIRPLAY_ALAC=0
COMMONFLAGS				+= -DAUDIO_CONVERTER_ALAC=0
COMMONFLAGS				+= -DAIRPLAY_MFI=1
COMMONFLAGS				+= -DCFCOMPAT_NOTIFICATIONS_ENABLED=0
COMMONFLAGS				+= -DCFL_BINARY_PLISTS=1
COMMONFLAGS				+= -DCFLITE_ENABLED=1
COMMONFLAGS				+= -DDEBUG_CF_OBJECTS_ENABLED=1
COMMONFLAGS				+= -DDEBUG_EXPORT_ERROR_STRINGS=1
ifeq ($(debug),1)
	COMMONFLAGS			+= -DDEBUG=1
else
	COMMONFLAGS			+= -DNDEBUG
endif
	COMMONFLAGS			+= -DAIRPLAY_HID_SERVER=1
ifneq ($(openssl),1)
	COMMONFLAGS			+= -DAES_UTILS_HAS_GLADMAN_GCM=0
	COMMONFLAGS			+= -DAES_UTILS_USE_GLADMAN_AES=1
	COMMONFLAGS			+= -DTARGET_HAS_MD5_UTILS=1
	COMMONFLAGS			+= -DTARGET_HAS_SHA_UTILS=1
	COMMONFLAGS			+= -DTARGET_NO_OPENSSL=1
	COMMONFLAGS			+= -DUSE_VIA_ACE_IF_PRESENT=0
endif
	COMMONFLAGS			+= -DAIRPLAY_SCREEN=1
COMMONFLAGS				+= -DAUDIO_STREAM_DLL
COMMONFLAGS				+= -DSCREEN_STREAM_DLL

# Compiler flags

CFLAGS					+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS) -Os
ifneq ($(qnx),1)
	CFLAGS				+= -fgnu89-inline
	CFLAGS				+= -std=gnu99
endif
CXXFLAGS				+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS) -Os

# Targets

TARGETS					+= $(BUILDROOT)/libAirPlaySupport.so
TARGETS					+= $(BUILDROOT)/libAirPlay.so
ifeq ($(stub),1)
	TARGETS				+= $(BUILDROOT)/libAudioStream.so
	TARGETS				+= $(BUILDROOT)/libScreenStream.so
endif
TARGETS					+= $(BUILDROOT)/airplayutil

# AirPlay Core

AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverPOSIX.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverServer.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverSession.o
	AirPlayCore_OBJS	+= $(OBJDIR)/AirPlayReceiverSessionScreen.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayUtils.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirTunesClock.o
AirPlayCore_OBJS		+= $(OBJDIR)/APAdvertiser.o
AirPlayCore_OBJS		+= $(OBJDIR)/APAdvertiserInfo.o
AirPlayCore_OBJS		+= $(OBJDIR)/CarPlayControlClient.o

# AirPlay

AirPlay_OBJS		+= $(AirPlayCore_OBJS)

# AirPlay Support

AirPlaySupport_OBJS		+= $(OBJDIR)/AirPlaySettings.so.o
AirPlaySupport_OBJS		+= $(OBJDIR)/APSAudioConverterStub.so.o
AirPlaySupport_OBJS		+= $(OBJDIR)/APSAudioSessionStub.so.o

# AirPlayUtil

AirPlayUtil_OBJS		+= $(OBJDIR)/airplayutil.o

# Audio / Screen

Audio_OBJS				+= $(OBJDIR)/AudioUtilsStub.so.o
	Screen_OBJS			+= $(OBJDIR)/ScreenUtilsStub.so.o

# Libraries

ifeq ($(qnx),1)
	LINKFLAGS			+= -lbacktrace
	LINKFLAGS			+= -lsocket
endif
LINKFLAGS				+= -lc
ifeq ($(openssl),1)
	LINKFLAGS			+= -lcrypto
endif
LINKFLAGS				+= -ldns_sd
LINKFLAGS				+= -lm

# Rules

.PHONY : all
all: $(TARGETS)

$(BUILDROOT)/airplayutil: $(AirPlayUtil_OBJS) $(BUILDROOT)/libAirPlaySupport.so $(BUILDROOT)/libAirPlay.so $(BUILDROOT)/libCoreUtils.so
	@echo "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) $(LINKFLAGS) -lAirPlaySupport -lAirPlay -lCoreUtils -o $@ $^
	$(quiet)$(STRIP) $@
	@echo "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

# Library rules.

$(BUILDROOT)/libAirPlay.so: $(AirPlay_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAirPlay.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAudioStream.so: $(Audio_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAudioStream.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAirPlaySupport.so: $(AirPlaySupport_OBJS)
	@echo "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAirPlaySupport.so -o $@ $(LINKFLAGS) $^
	$(quiet)$(STRIP) $@
	@echo "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libScreenStream.so: $(Screen_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libScreenStream.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
	
.PHONY : clean
clean:
	-rm -rf $(BUILDROOT)

$(AirPlay_OBJS):		| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AirPlaySupport_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AirPlayUtil_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(Audio_OBJS):			| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(Screen_OBJS):			| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils

$(BUILDROOT)/obj:
	$(quiet)mkdir -p $@

# General rules

$(OBJDIR)/%.o: %.c
	@echo "Compiling ($(os)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@echo "Compiling ($(os)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c $(CXXFLAGS) -o $@ $<

$(OBJDIR)/%.so.o: %.c
	@echo "Compiling ($(os)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.so.o: %.cpp
	@echo "Compiling ($(os)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c -fPIC $(CXXFLAGS) -o $@ $<

# Core Utils

arm					= 1
ifeq ($(openssl),1)
	libtommath		= 0
else
	libtommath		= 1
endif	
mfi					= 1
nvtools				= $(nv)
qnxtools			= $(qnx)

export arch arm debug nohidraw libtommath linux mfi nv nvtools openssl os qnx qnxtools
export BUILDROOT
ifeq ($(debug),1)
	export STRIP
endif

.PHONY : $(BUILDROOT)/libCoreUtils.so
$(BUILDROOT)/libCoreUtils.so: $(BUILDROOT)/CoreUtils
	@echo "Making ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(MAKE) -C $(COREUTILSROOT)/PlatformPOSIX -e CoreUtils
	@echo "$(ColorCyan)=== MAKE COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

.PHONY : $(BUILDROOT)/CoreUtils
$(BUILDROOT)/CoreUtils:
	@echo "Copying CoreUtils headers"
	$(quiet)mkdir -p $@
	$(quiet)rsync -ap "$(COREUTILSROOT)/Support/"*.h "$(BUILDROOT)/CoreUtils"
	$(quiet)rsync -ap "$(COREUTILSROOT)/External/GladmanAES/"*.h "$(BUILDROOT)/CoreUtils"
	@echo "$(ColorCyan)=== COPY COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

# Bonjour 

.PHONY : bonjour clean_bonjour
bonjour clean_bonjour:
	@echo "Making ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(MAKE) -C $(COREUTILSROOT)/PlatformPOSIX -e $@
	@echo "$(ColorCyan)=== MAKE COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
