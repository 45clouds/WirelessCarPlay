#
# Top level makefile for Build & Integration.
# 
# This file is used to facilitate checking the mDNSResponder project
# directly out of CVS and submitting to B&I at Apple.
#
# The various platform directories contain makefiles or projects
# specific to that platform.
#
#    B&I builds must respect the following target:
#         install:
#         installsrc:
#         installhdrs:
#         clean:
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make

MVERS = "mDNSResponder-765.20.4"

DDNSWRITECONFIG = "$(DSTROOT)/Library/Application Support/Bonjour/ddnswriteconfig"
VER = 
ifneq ($(strip $(GCC_VERSION)),)
	VER = -- GCC_VERSION=$(GCC_VERSION)
endif
echo "VER = $(VER)"

installSome:
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild install     OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT) -target Build\ Some $(VER) 

SystemLibraries:
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild install     OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT) -target SystemLibraries $(VER) 

install:
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild install     OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT) $(VER) 
	# Make sure ddnswriteconfig is owned by root:wheel, then make it setuid root executable
	if test -e $(DDNSWRITECONFIG) ; then chown 0:80 $(DDNSWRITECONFIG) ; chmod 4555 $(DDNSWRITECONFIG) ; fi

installsrc:
	ditto . "$(SRCROOT)"

installhdrs::
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild installhdrs OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT)  -target SystemLibraries $(VER)
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild installhdrs OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT)  -target dns_services $(VER)

java:
	cd "$(SRCROOT)/mDNSMacOSX"; xcodebuild install  OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT) -target libjdns_sd.jnilib $(VER)

clean::
	echo clean
