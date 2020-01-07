/*
	File:    	AirPlayReceiverSessionPriv.h
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayReceiverSessionPriv_h__
#define	__AirPlayReceiverSessionPriv_h__

#include <CoreUtils/AESUtils.h>
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/DataBufferUtils.h>
#include <CoreUtils/HTTPClient.h>
#include <CoreUtils/MathUtils.h>
#include <CoreUtils/MiscUtils.h>
#include <CoreUtils/NetUtils.h>
#include <CoreUtils/ThreadUtils.h>

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionScreen.h"
#include "AirPlayUtils.h"
#include "AirTunesClock.h"
#include "dns_sd.h"

#if( TARGET_OS_POSIX )
	#include <sys/types.h>
	
	#include <sys/socket.h>
	#include <net/if.h>
#endif

		#include "APSAudioConverter.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

	#define kAirTunesDupWindowSize		512 // Size of the dup checking window. Must be >= the max number of retransmits.

// AirPlayAudioStats

typedef struct
{
	EWMA_FP_Data		bufferAvg;
	uint32_t			lostPackets;
	uint32_t			unrecoveredPackets;
	uint32_t			latePackets;
	char				ifname[ IF_NAMESIZE + 1 ];
	
}	AirPlayAudioStats;

extern AirPlayAudioStats		gAirPlayAudioStats;

// AirPlayRTPBuffer

typedef struct AirPlayRTPBuffer *	AirPlayRTPBufferRef;
struct AirPlayRTPBuffer
{
	AirPlayRTPBufferRef		next;				// PRIVATE: Ptr to the next buffer in the list.
	void *					internalContext;	// PRIVATE: Ptr to context used by the AirPlay library.
	int32_t					retainCount;		// PRIVATE: Number of references to this buffer.
	uint32_t				state;				// State variable app code can use.
	uint8_t *				readPtr;			// Ptr to the buffer we're currently read into.
	size_t					readLen;			// Number of bytes to read in the current state.
	size_t					readOffset;			// Buffer offset where we're currently reading.
	uint8_t					extraBuf[ 256 ];	// Header for any frame wrapping the RTP packet (e.g. RTP-over-TCP).
	RTPHeader				rtpHeader;			// RTP header.
	uint8_t *				payloadPtr;			// READ ONLY: malloc'd ptr for payload data.
	size_t					payloadLen;			// Number of valid bytes in payloadPtr.
	size_t					payloadMaxLen;		// PRIVATE: Max number of bytes payloadPtr can hold.
	void *					userContext;		// Context pointer for app use.
};

// AirTunesBufferNode

typedef struct AirTunesBufferNode	AirTunesBufferNode;
struct AirTunesBufferNode
{
	AirTunesBufferNode *		next;	// Next node in circular list when busy. Next node in null-terminated list when free.
	AirTunesBufferNode *		prev;	// Note: only used when node is on the busy list (not needed for free nodes).
	const RTPPacket *			rtp;	// Ptr to RTP packet header.
	uint8_t *					ptr;	// Ptr to RTP payload. Note: this may not point to the beginning of the payload.
	size_t						size;	// Size of RTP payload. Note: this may be less than the full payload size.
	uint8_t *					data;	// Buffer for the entire RTP packet. All node ptrs point within this buffer.
	uint32_t					ts;		// RTP timestamp where "ptr" points. Updated when processing partial packets.
};

// AirTunesRetransmitNode

typedef struct AirTunesRetransmitNode	AirTunesRetransmitNode;
struct AirTunesRetransmitNode
{
	AirTunesRetransmitNode *		next;
	uint16_t						seq;			// Sequence number of the packet that needs to be retransmitted.
	uint16_t						tries;			// Number of times this retransmit request has been tried (starts at 1).
	uint64_t						startNanos;		// When the retransmit request started being tracked.
	uint64_t						sentNanos;		// When the retransmit request was last sent.
	uint64_t						nextNanos;		// When the retransmit request should be sent next.
#if( DEBUG )
	uint32_t						tryNanos[ 16 ];	// Age at each retransmit try.
#endif
};

// AirTunesRetransmitHistoryNode

#if( DEBUG )
typedef struct
{
	const char *		reason;
	uint16_t			seq;
	uint16_t			tries;
	uint64_t			finalNanos;
	uint32_t			tryNanos[ countof_field( AirTunesRetransmitNode, tryNanos ) ];
	
}	AirTunesRetransmitHistoryNode;
#endif

// AirTunesSource

typedef struct
{
	// Periodic operations
	
	unsigned int				receiveCount;					// Number of packets received (and passed some validity checks).
	unsigned int				lastReceiveCount;				// Receive count at our last activity check.
	unsigned int				activityCount;					// Number of times control channel messages received.
	unsigned int				lastActivityCount;				// Activity count at our last activity check.
	uint64_t					lastActivityTicks;				// Ticks when we last detected activity.
	uint64_t					maxIdleTicks;					// Number of ticks we can be idle before timing out.
	uint64_t					perSecTicks;					// Number of ticks in a second.
	uint64_t					perSecLastTicks;				// Ticks when we did "per second" processing.
	uint64_t					lastIdleLogTicks;				// Ticks when we last logged about being idle.
	uint64_t					idleLogIntervalTicks;			// Number of ticks between idle logs.
	
	// Time Sync
	
	uint32_t					rtcpTILastTransmitTimeHi;		// Upper 32 bits of transmit time of last RTCP TI request.
	uint32_t					rtcpTILastTransmitTimeLo;		// Lower 32 bits of transmit time of last RTCP TI request.	
	unsigned int				rtcpTISendCount;				// Number of RTCP TI requests we've sent.
	unsigned int				rtcpTIResponseCount;			// Number of RTCP TI responses to our requests we've received.
	unsigned int				rtcpTIStepCount;				// Number of times the clock had to be stepped.
	
	double						rtcpTIClockRTTAvg;				// Avg round-trip time.
	double						rtcpTIClockRTTMin;				// Min round-trip time.
	double						rtcpTIClockRTTMax;				// Max round-trip time.
	double						rtcpTIClockRTTLeastBad;			// Best RTT we've received that was still considered "bad".
	int							rtcpTIClockRTTBadCount;			// Number of consecutive bad RTT's we've received.
	uint32_t					rtcpTIClockRTTOutliers;			// Total number of huge RTT's we've received.
	
	double						rtcpTIClockOffsetArray[ 3 ];	// Last N NTP clock offsets between us and the server.
	unsigned int				rtcpTIClockOffsetIndex;			// Circular index into the clock offset history array.
	double						rtcpTIClockOffsetAvg;			// Moving average of clock offsets.
	double						rtcpTIClockOffsetMin;			// Minimum clock offset.
	double						rtcpTIClockOffsetMax;			// Maximum clock offset.
	double						rtcpTIClockOffsetLeastBad;		// Best clock offset we've received that was still considered "bad".
	Boolean						rtcpTIResetStats;				// True if the clock offset stats need to be reset.
	
	uint32_t					rtpOffsetActive;				// RTP offset actively in use (different when deferring).
	uint32_t					rtpOffset;						// Current RTP timestamp offset between us and the server.
	Boolean						rtpOffsetApply;					// True when we need to apply a new RTP offset.
	uint32_t					rtpOffsetApplyTimestamp;		// Sender timestamp when we should apply our new RTP offset.
	uint32_t					rtpOffsetLast;					// Last RTP timestamp offset between us and the server.
	uint32_t					rtpOffsetAvg;					// Avg RTP timestamp offset between us and the server.
	uint32_t					rtpOffsetMaxDelta;				// Max delta between the last and instantaneous RTP offsets.
	uint32_t					rtpOffsetMaxSkew;				// Max delta between the last and current RTP timestamp offsets.
	uint32_t					rtpOffsetSkewResetCount;		// Number of timeline resets due to too much skew.
	
	// RTCP Retransmissions
	
	AirTunesRetransmitNode *	rtcpRTListStorage;				// Storage for list of pending retransmit requests.
	AirTunesRetransmitNode *	rtcpRTFreeList;					// Head of list of free retransmit nodes.
	AirTunesRetransmitNode *	rtcpRTBusyList;					// Head of list of outstanding retransmit requests.
	Boolean						rtcpRTDisable;					// If true, don't send any retransmits.
	int64_t						rtcpRTMinRTTNanos;				// Smallest RTT we've seen.
	int64_t						rtcpRTMaxRTTNanos;				// Largest RTT we've seen.
	int64_t						rtcpRTAvgRTTNanos;				// Moving average RTT.
	int64_t						rtcpRTDevRTTNanos;				// Mean deviation RTT.
	int64_t						rtcpRTTimeoutNanos;				// Current retransmit timeout.
	uint32_t					retransmitSendCount;			// Number of retransmit requests we sent.
	uint32_t					retransmitReceiveCount;			// Number of retransmit responses we sent.
	uint32_t					retransmitFutileCount;			// Number of futile retransmit responses we've received.
	uint32_t					retransmitNotFoundCount;		// Number of retransmit responses received without a request (late).
	uint64_t					retransmitMinNanos;				// Min nanoseconds for a retransmit response.
	uint64_t					retransmitMaxNanos;				// Max nanoseconds for a retransmit response.
	uint64_t					retransmitAvgNanos;				// Average nanoseconds for a retransmit response.
	uint64_t					retransmitRetryMinNanos;		// Min milliseconds for a retransmit response after a retry.
	uint64_t					retransmitRetryMaxNanos;		// Max milliseconds for a retransmit response after a retry.
	uint32_t					retransmitMaxLoss;				// Max contiguous loss to try to recover.
	uint32_t					maxBurstLoss;					// Largest contiguous number of lost packets.
	uint32_t					bigLossCount;					// Number of times we abort retransmits requests because the loss was too big.
	
}	AirTunesSource;

// AirPlayAudioStreamContext

typedef struct
{
	uint64_t	hostTime;
	uint32_t	sampleTime;
	
}	AirPlayTimestampTuple;

typedef struct SendAudioContextList SendAudioContextList;
TAILQ_HEAD( SendAudioContextList, SendAudioContext );

typedef struct
{
	AirPlayReceiverSessionRef		session;					// Session owning this stream.
	AirPlayStreamType				type;						// Type of stream.
	const char *					label;						// Name of the stream (for logging).
	uint64_t						connectionID;				// Connection ID of the stream.
	
	SocketRef						cmdSock;					// Socket for sending commands to the audio thread.
	SocketRef						dataSock;					// Socket for sending main audio input and receiving main output packets.
	pthread_t						thread;						// Thread for receiving and processing packets.
	pthread_t *						threadPtr;					// Ptr to the packet thread. NULL if thread isn't running.
	RTPJitterBufferContext			jitterBuffer;				// Buffer for processing packets.
	AirPlayCompressionType			compressionType;			// Compression type for input and output.
	uint32_t						sampleRate;					// Sample rate for configured input and output (e.g. 44100).
	uint32_t						channels;					// Number channels (e.g. 2 for stereo).
	uint32_t						bytesPerUnit;				// Number of bytes per unit (e.g. 4 for 16-bit stereo).
    uint32_t						framesPerPacket;			// Number of frames per RTP packet.
	uint32_t						bitsPerSample;				// Number of bits in each sample (e.g. 16 for 16-bit samples).
	uint64_t						rateUpdateNextTicks;		// Next UpTicks when we should we should update the rate estimate.
	uint64_t						rateUpdateIntervalTicks;	// Delay between rate updates.
	AirPlayTimestampTuple			rateUpdateSamples[ 30 ];	// Sample history for rate estimates.
	uint32_t						rateUpdateCount;			// Total number of samples we've recorded for rate updates.
	Float32							rateAvg;					// Moving average of the estimated sample rate.
	AirPlayTimestampTuple			zeroTime;					// Periodic sample timestamp.
	pthread_mutex_t					zeroTimeLock;				// Mutex to protect access to zeroTime.
	pthread_mutex_t *				zeroTimeLockPtr;			// Ptr to the zeroTime mutex. NULL if invalid.
	uint32_t						sendErrors;					// Number of send errors that occurred.
	ChaChaPolyCryptor				outputCryptor;				// ChaCha20_Poly1305 cryptor for incoming audio packets.
	SendAudioContextList			sendAudioList;
	pthread_t						sendAudioThread;            // Thread to offload sending audio data.
	pthread_t *						sendAudioThreadPtr;         // Ptr to sendAudioThread when valid.
	pthread_cond_t					sendAudioCond;              // Condition to signal when there is audio to send.
	pthread_cond_t *				sendAudioCondPtr;           // Ptr to sendAudioCond when valid.
	pthread_mutex_t					sendAudioMutex;             // Mutex for signaling audioSendCond.
	pthread_mutex_t *				sendAudioMutexPtr;          // Ptr to sendAudioMutex when valid.
	Boolean							sendAudioDone;              // Sentinal for terminating sendAudioThread.
	MirroredRingBuffer				inputRing;					// Ring buffer for processing audio input.
	sockaddr_ip						inputAddr;					// Address and port to send audio input packets to.
	socklen_t						inputAddrLen;				// Valid length of inputAddr.
	uint16_t						inputSeqNum;				// Last RTP sequence number we've sent.
	uint32_t						inputTimestamp;				// Last RTP timestamp sent.
	ChaChaPolyCryptor				inputCryptor;				// ChaCha20_Poly1305 cryptor for input audio.
	AudioConverterRef				inputConverter;				// Converter for encoding audio.
	const uint8_t *					inputDataPtr;				// Ptr to the data to encode from converter callback.
	const uint8_t *					inputDataEnd;				// End of the data to encode.
	uint32_t						sampleTimeOffset;
	
}	AirPlayAudioStreamContext;

// AirPlayReceiverSession

struct AirPlayReceiverSessionPrivate
{
	CFRuntimeBase					base;						// CF type info. Must be first.
	dispatch_queue_t				queue;						// Internal queue used by the session.
	AirPlayReceiverServerRef		server;						// Pointer to the server that owns this session.
	AirPlayReceiverConnectionRef	connection;					// Control connection for the session
	void *							platformPtr;				// Pointer to the platform-specific data.
	AirPlayReceiverSessionDelegate	delegate;					// Hooks for delegating functionality to external code.
	char							clientOSBuildVersion[ 32 ];
	pthread_mutex_t					mutex;
	pthread_mutex_t *				mutexPtr;
	dispatch_source_t				periodicTimer;				// Timer for periodic tasks.
	
	NetTransportType				transportType;				// Network transport type for the session.
	sockaddr_ip						peerAddr;					// Address of the sender.
	uint8_t							sessionUUID[ 16 ];			// Random UUID for this AirPlay session.
	OSStatus						startStatus;				// Status of starting the session (i.e. if session failed).
	uint64_t						clientDeviceID;				// Unique device ID of the client sending to us.
	uint8_t							clientIfMACAddr[ 6 ];		// Client's MAC address of the interface this session is connected on.
	uint64_t						clientSessionID;			// Unique session ID from the client.
	uint32_t						clientVersion;				// Source version of the client or 0 if unknown.
	uint64_t						sessionTicks;				// Ticks when this session was started.
	uint64_t						playTicks;					// Ticks when playback started.
	AirPlayCompressionType			compressionType;
	AES_CBCFrame_Context			decryptorStorage;			// Used for decrypting audio content.
	AES_CBCFrame_Context *			decryptor;					// Ptr to decryptor or NULL if content is not encrypted.
	uint8_t							aesSessionKey[ 16 ];
	uint8_t							aesSessionIV[ 16 ];
	AirTunesSource					source;
	Boolean							screen;						// True if AirPlay Screen. False if normal AirPlay Audio.
	uint32_t						framesPerPacket;
	int								audioQoS;					// QoS to use for audio control and data.
	
	// Control/Events
	
	Boolean							controlSetup;				// True if control is set up.
	Boolean							useEvents;					// True if the client supports events.
	Boolean							sessionStarted;				// True if the session has been started (e.g. RECORD received).
	dispatch_queue_t				eventQueue;					// Internal queue used by eventClient.
	HTTPClientRef					eventClient;				// Client for sending RTSP events back to the sender.
	SocketRef						eventSock;					// Socket for accepting an RTSP event connection from the sender.
	int								eventPort;					// Port we're listening on for an RTSP event connection.
	Boolean							sessionIdle;				// True if no stream is setup, ie. no audio and video.
	
	PairingSessionRef				pairVerifySession;			// PairingSession to derive encryption/decription keys for individual streams
	// Main/AltAudio
	
	AirPlayAudioStreamContext		mainAudioCtx;				// Context for main audio input and output.
	AirPlayAudioStreamContext		altAudioCtx;				// Context for alt audio output.
	
	// GeneralAudio
	
	int								rtpAudioPort;				// Port we're listening on for RTP audio packets.
	int								redundantAudio;				// If > 0, redundant audio packets are being sent.
	Boolean							rtpAudioDupsInitialized;	// True if the dup checker has been initialized.
	uint16_t						rtpAudioDupsLastSeq;		// Last valid sequence number we've checked.
	uint16_t						rtpAudioDupsArray[ kAirTunesDupWindowSize ]; // Sequence numbers for duplicate checking.
	
	SocketRef						rtcpSock;					// Socket for sending and receiving RTCP packets.
	int								rtcpPortLocal;				// Port we're listening on for RTCP packets.
	int								rtcpPortRemote;				// Port of the peer sending us RTCP packets.
	sockaddr_ip						rtcpRemoteAddr;				// Address of the peer to send RTCP packets to.
	socklen_t						rtcpRemoteLen;				// Length of the sockaddr for the RTCP peer.
	Boolean							rtcpConnected;				// True if the RTCP socket is connected.
    
	uint32_t						platformAudioLatency;		// Samples of latency the platform introduces.
	uint32_t						minLatency;					// Minimum samples of latency.
	uint32_t						maxLatency;					// Maximum samples of latency.
	
	// NTP Time Sync
	AirTunesClockRef				airTunesClock;				// NTP Synchronizer clock
	SocketRef						timingSock;
	int								timingPortLocal;			// Local port we listen for time sync response packets on.
	int								timingPortRemote;			// Remote port we send time sync requests to.
	sockaddr_ip						timingRemoteAddr;			// Address of the peer to send timing packets to.
	socklen_t						timingRemoteLen;			// Length of the sockaddr for the timing peer.
	Boolean							timingConnected;			// True if the timing socket is connected.
	SocketRef						timingCmdSock;
	pthread_t						timingThread;
	pthread_t *						timingThreadPtr;
	
	// Keep Alive
	SocketRef						keepAliveSock;
	int								keepAlivePortLocal;			// Local port we listen for keep alive packets on.
	SocketRef						keepAliveCmdSock;
	pthread_t						keepAliveThread;
	pthread_t *						keepAliveThreadPtr;

	// Buffering
	
	uint32_t						nodeCount;					// Number of buffer nodes.
	size_t							nodeBufferSize;				// Number of bytes of data each node can hold.
	uint8_t *						nodeBufferStorage;			// Backing store for all the node buffers.
	AirTunesBufferNode *			nodeHeaderStorage;			// Array backing store for all the node headers.
	AirTunesBufferNode				busyListSentinelStorage;	// Dummy head node to maintain a circular list with a sentinel.
	AirTunesBufferNode *			busyListSentinel;			// Ptr to dummy node...saves address-of calculations.
	AirTunesBufferNode *			freeList;					// List of free nodes. Standard head pointer, null tail list.
	uint32_t						busyNodeCount;				// Number of nodes currently in use.
	uint8_t *						decodeBuffer;
	size_t							decodeBufferSize;
	uint8_t *						readBuffer;
	size_t							readBufferSize;
	uint8_t *						skewAdjustBuffer;			// Temporary buffer for doing skew compensation.
	size_t							skewAdjustBufferSize;
	
	// Audio
	
	uint32_t						audioLatencyOffset;			// Timestamp offset to compensate for latency.
	Boolean							flushing;					// Flushing is in progress.
	uint64_t						flushRecentTicks;			// Number of ticks to consider a flush "recent".
	uint64_t						flushLastTicks;				// Ticks when the last flush occurred.
	uint32_t						flushTimeoutTS;				// Stay in flush mode until this timestamp is received.
	uint32_t						flushUntilTS;				// Flush packets with timestamps earlier than this.
	uint16_t						lastRTPSeq;					// Last RTP sequence number received.
	uint32_t						lastRTPTS;					// Last RTP timestamp received.
	Boolean							lastPlayedValid;			// true if lastPlayed info is valid.
	uint32_t						lastPlayedTS;				// Last played RTP timestamp (sender/packet timeline).
	uint16_t						lastPlayedSeq;				// Last played RTP sequence number.
	
	AudioConverterRef				audioConverter;				// Audio converter for decoding packets.
	const uint8_t *					encodedDataPtr;				// Ptr to the data to decode from converter callback.
	const uint8_t *					encodedDataEnd;				// End of the data to decode.
	AudioStreamPacketDescription	encodedPacketDesc;			// Used for passing packet info back to the converter.
	uint32_t						compressionPercentAvg;		// EWMA of the compression percent we're getting.
	Boolean							stutterCreditPending;		// True if the next normal glitch will be ignored.
	int								glitchTotal;				// Total number of glitches in the session.
	int								glitchLast;					// Number of glitches when we last checked.
	int								glitchyPeriods;				// Number of periods with glitches.
	int								glitchTotalPeriods;			// Number of periods (with or without glitches).
	uint64_t						glitchNextTicks;			// Next ticks to check the glitch counter.
	uint64_t						glitchIntervalTicks;		// Number of ticks between glitch counter checks.
	
	// Screen
	
	Boolean							screenInitialized;			// true=Screen fully initialized;
	AirPlayReceiverSessionScreenRef	screenSession;
	SocketRef						screenSock;					// Socket for receiving screen data.
	pthread_t						screenThread;				// Thread for receiving and processing packets.
	pthread_t *						screenThreadPtr;			// Ptr to the packet thread. NULL if thread isn't running.

};

void AirPlayReceiverSessionSetUserVersion( AirPlayReceiverSessionRef inSession, uint32_t userVersion );

//===========================================================================================================================
//	Debugging
//===========================================================================================================================

#if( DEBUG )
	extern int		gAirTunesDebugLogAllSkew;
	
	OSStatus	AirTunesDebugControl( const char *inCmd, CFStringRef *outOutput );
	void		AirTunesDebugControl_ResetDebugStats( void );
	OSStatus	AirTunesDebugPerf( int inPollIntervalSeconds, AirPlayReceiverSessionRef inSession );
#endif

OSStatus	AirTunesDebugShow( const char *inCmd, CFStringRef *outOutput );
OSStatus	AirTunesDebugAppendShowData( const char *inCmd, DataBuffer *inDB );

#ifdef __cplusplus
}
#endif

#endif	// __AirPlayReceiverSessionPriv_h__
