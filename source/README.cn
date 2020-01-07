# Carplay
hear is Carplay AccessorySDK


Carplay的sdk希望有大神可以移植成功
1. R12d 插件源码结构与R11之间的差异

R12d源码结构与R11有差异较大，去掉了IPC方式，增加了AccessorySDK、Transport目录， 并且AccessorySDK目录下也有个makefile。之前有的目录内容也完全不一样了，支持无线CarPlay方式，要求使用mDNSResponder-567版本的Bonjour。

环境配置及R11源码编译的方式请参考博主以前的博文：CarPlay for Android： Bonjour 及 插件源码移植问题分析 ：http://blog.csdn.net/romantic_energy/article/details/466803552. R12d 插件源码移植到Linux

把插件源码拷贝到Linux环境下，插件根目录下有源码目录和Examples，把源码目录修改为CarPlay_Plugin，把Bonjour源码拷贝到插件目录下，去掉版本号，改名为mDNSResponder。修改完成后，插件根目录下有3个目录：CarPlay_Plugin、Examples、mDNSResponder.首先编译mDNSResponder：shell进入mDNSResponder\mDNSPosix目录下，执行make，环境搭建好了的话，此步骤没有问题。然后编译插件源码：shell进入CarPlay_Plugin\PlatformPOSIX目录下，执行make，报以下错误：cc1: warnings being treated as errorsIn file included from ***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/CoreUtils/CFCompat.h:8,

from ***/CarPlay_Plugin/PlatformPOSIX/../Sources/AirPlaySettings.h:9,

from ***/CarPlay_Plugin/PlatformPOSIX/../Sources/AirPlaySettings.c:5:***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/CoreUtils/CommonServices.h:1638: error: "__SIZEOF_INT128__" is not definedmake: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/obj/AirPlaySettings.so.o] Error 1cc1: warnings being treated as errors意为将编译警告视为错误，因为无法去掉所有警告，将makefile中的COMMON_WARNINGS

+= -Werror行注释掉， 即：#COMMON_WARNINGS

+= -Werror修改后执行make，报以下错误：Linking (unknown-Release) \033[0;35mlibAirPlay.so\033[0m/usr/bin/ld: ***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/obj/AirPlayNTPClient.o: relocation R_X86_64_32 against `gLogCategory_AirPlayNTPClientCore' can not be used when making a shared object; recompile with -fPIC***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/obj/AirPlayNTPClient.o: could not read symbols: Bad valuecollect2: ld returned 1 exit statusmake: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/libAirPlay.so] Error 1按编译器提升：recompile with -fPIC即增加一行： COMMONFLAGS

+= -fPIC 后make clean, 然后继续make编译，会得到以下错误：Compiling (unknown-Release-shared) \033[0;35mAudioUtilsStub.c\033[0mIn file included from ***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/CoreUtils/CFUtils.h:10,

from ***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/CoreUtils/AudioUtils.h:12,

from ***/CarPlay_Plugin/PlatformPOSIX/../Support/AudioUtilsStub.c:5:***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/CoreUtils/CommonServices.h:1638: warning: "__SIZEOF_INT128__" is not defined***/CarPlay_Plugin/PlatformPOSIX/../Support/AudioUtilsStub.c:673: error: expected identifier or '(' before '{' tokenmake: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-unknown/obj/AudioUtilsStub.so.o] Error 1Compiling (unknown-Release-shared)意为为未知系统编译，说明我们make的时候带的参数不够，查看makefile后发现，应该使用make os=linux.查看/CarPlay_Plugin/PlatformPOSIX/../Support/AudioUtilsStub.c 673行，发现此行多了一个";"号，即：APSAudioSessionAudioFormat

APSAudioSessionGetSupportedFormats( AudioStreamType inStreamType, CFStringRef inAudioType );{

...}去掉";"号即可；make clean 后执行 make os=linux, 提升以下错误：Making (linux-Release) \033[0;35mlibCoreUtils.so\033[0mmake[1]: Entering directory `***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX'Compiling (lin

推荐：移植Android时关于Linux中MACHINE_START的一点探讨

在嵌入式Linux中内核移植产品代码分支时往往会遇到以下一个内核代码结构： MACHINE_START(OPT, "OMAP4 opmex tablet")

/* Maintainer: Vincent - SUNSEA OPMEX

ux--Release-shared) \033[0;35mAsyncConnection.c\033[0m***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/AsyncConnection.c:36: fatal error: dns_sd.h: No such file or directorycompilation terminated.make[1]: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/obj/AsyncConnection.so.o] Error 1make[1]: Leaving directory `***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX'make: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/libCoreUtils.so] Error 2阅读makefile后发现没有指定Bonjour的路径，于是在makefile中增加include 目录：INCLUDES
+= -I$(SRCROOT)/../mDNSResponder/mDNSShared重新make后还是报相同的错误，久寻未果，后发现报错的makefile是/CarPlay_Plugin/AccessorySDK/PlatformPOSIX中的makefile，应该是此makefile中的目录不对，遂及把/CarPlay_Plugin/AccessorySDK/PlatformPOSIX中的makefile中的MDNSROOT

= $(SRCROOT)/../mDNSResponder改为：MDNSROOT

= $(SRCROOT)/../../mDNSResponder后此处编译通过。但是还是有其他错误：cc1: warnings being treated as errors***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/CFLite.c: In function 'CFLRetain':***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/CFLite.c:400: error: implicit declaration of function 'atomic_add_and_fetch_32'***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/CFLite.c: In function 'CFLRuntimeRegisterClass':***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/CFLite.c:3359: error: implicit declaration of function 'atomic_bool_compare_and_swap_32'***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/CFLite.c:3370: error: implicit declaration of function 'atomic_read_write_barrier'make[1]: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/obj/CFLite.so.o] Error 1make[1]: Leaving directory `***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX'make: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/libCoreUtils.so] Error 2在/CarPlay_Plugin/AccessorySDK/PlatformPOSIX中的makefile中增加COMMONFLAGS

+= -DAtomicUtils_HAS_SYNC_BUILTINS=1COMMONFLAGS

+= -fPIC后解决。继续编译后，报以下错误:Compiling (linux--Release-shared) \033[0;35mChaCha20Poly1305.c\033[0m***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/ChaCha20Poly1305.c: In function '_chacha20_xor':***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/ChaCha20Poly1305.c:584: error: subscripted value is neither array nor pointer***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX/../Support/ChaCha20Poly1305.c:585: error: subscripted value is neither array nor pointermake[1]: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/obj/ChaCha20Poly1305.so.o] Error 1make[1]: Leaving directory `***/CarPlay_Plugin/AccessorySDK/PlatformPOSIX'make: *** [***/CarPlay_Plugin/PlatformPOSIX/../build/Release-linux/libCoreUtils.so] Error 2经过阅读代码和查阅网络资料后发现应该是cpu支持SIMD优化，但是编译器不支持，这里描述可能不准确，属于编译优化的问题，不够清楚，但是修改ChaCha20Poly1305.c中的代码：#if( TARGET_HAS_NEON || ( TARGET_HAS_SSE >= SSE_VERSION( 2, 0 ) ) )

#define CHACHA20_SIMD

1#else

#define CHACHA20_SIMD

0#endif为：//#if( TARGET_HAS_NEON || ( TARGET_HAS_SSE >= SSE_VERSION( 2, 0 ) ) )//

#define CHACHA20_SIMD

1//#else

#define CHACHA20_SIMD

0//#endif即不使用CHACHA20_SIMD优化后，编译通过。至此，所有的编译目标都已经被成功编译出，分别是以下文件：airplayutil

libAirPlay.so

libAirPlaySupport.so

libAudioStream.so

libCoreUtils.so

libScreenStream.so


