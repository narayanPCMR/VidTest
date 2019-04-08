// VidTx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <algorithm>
#include <chrono>

//LibAV headers (using C)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

//Load LibAV Libraries, only for VC
#ifdef _MSC_VER
#pragma comment(lib, "libav\\avformat.lib")
#pragma comment(lib, "libav\\avcodec.lib")
#pragma comment(lib, "libav\\avutil.lib")
#pragma comment(lib, "libav\\swscale.lib")
#endif

//SDLNet for transmitting over UDP
//TODO: Use FTDI Serial over Laser
#ifdef _MSC_VER
#pragma comment(lib, "sdl_net/SDL2_net.lib")
#endif
#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#include <SDL/SDL_net.h>


/* Input video */
const char* fileName = "C:\\Users\\naray\\Downloads\\Bad Apple PV.mp4";

//Video Metadata
#define OUTWIDTH 640
#define OUTHEIGHT 480
#define OUTCODEC AV_CODEC_ID_MPEG4
#define OUTPXFMT AV_PIX_FMT_YUV420P

//Comment to transcode at unlimited speed
#define THROTTLE

#define HBRate


//Output bit rate. Higher, the better quality and shorter transcode period, but also higher bandwidth
#ifdef HBRate
const int outBitRate = 40000000;
#else
const int outBitRate = 320000;
#endif

//Input codec and format
AVCodec *pInputCodec;
AVCodecContext *pInputCodecContext;
AVFormatContext *pInputFormatCtx;
struct SwsContext *sws_ctx;

//Output codec and format
AVCodec *outCodec;
AVCodecContext *pOutputCodecContext;

//Index of video stream in input file
unsigned int videoStream;

//Input and output frames
AVFrame *pFrame, *pOutFrame;

uint8_t *pOutFrameBuffer;

//UDP Socket
UDPsocket sock;
IPaddress srvadd;

struct avpkt {
	uint8_t sig[3] = {0x01, 0x88, 0x28};
	uint8_t part;
	uint8_t totalparts;
	uint64_t totalsize;
	uint8_t data[32700];
	uint64_t size;
} pack;


class Timer
{
public:
	Timer() : beg_(clock_::now()) {}
	void reset() { beg_ = clock_::now(); }
	double elapsed() const {
		return std::chrono::duration_cast<second_>
			(clock_::now() - beg_).count();
	}

private:
	typedef std::chrono::high_resolution_clock clock_;
	typedef std::chrono::duration<double, std::ratio<1> > second_;
	std::chrono::time_point<clock_> beg_;
};


/*
Function to send raw data over UDP
data - buffer to send
size - length of the buffer
*/
void sendrData(UDPsocket *s, uint8_t *data, uint64_t size) {
	UDPpacket **pck;
	uint64_t dtPos = 0, dtSize = 0;
	const int maxPks = 64;

	if (!(pck = SDLNet_AllocPacketV(maxPks, 32768))) {
		fprintf(stderr, "SDLNet_AllocPacket: %s\n", SDLNet_GetError());
		return;
	}

	pack.part = 0;
	pack.totalsize = size;
	
	while (dtPos < size) {
		dtSize = size - dtPos;
		if (dtSize > sizeof(pack.data))
			dtSize = sizeof(pack.data);

		pack.size = dtSize;
		memcpy_s(pack.data, sizeof(pack.data), data+dtPos, dtSize);

		//Sending
		memcpy_s(pck[pack.part]->data, pck[pack.part]->maxlen, &pack, sizeof(pack));
		pck[pack.part]->len = sizeof(pack);
		
		pck[pack.part]->channel = -1;
		pck[pack.part]->address.host = srvadd.host;
		pck[pack.part]->address.port = srvadd.port;

		dtPos += dtSize;
		pack.part++;
	}

	for (int i = 0; i < pack.part; i++) {
		pck[i]->data[offsetof(struct avpkt, totalparts)] = pack.part;
	}

	SDLNet_UDP_SendV(*s, pck, pack.part);
	
	SDLNet_FreePacketV(pck);
}

int main() {
	//Init all LibAV components
	av_register_all();

	//Load the video
	if (avformat_open_input(&pInputFormatCtx, fileName, NULL, 0) != 0)
		return EXIT_FAILURE;

	//Find video Stream
	if (avformat_find_stream_info(pInputFormatCtx, 0) < 0)
		return EXIT_FAILURE;
	videoStream = INT_MAX;
	for (int i = 0; i < pInputFormatCtx->nb_streams; i++) {
		if (pInputFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == INT_MAX)
		return EXIT_FAILURE; // Didn't find a video stream

	//Create and Open Codec
	pInputCodecContext = pInputFormatCtx->streams[videoStream]->codec;
	pInputCodec = avcodec_find_decoder(pInputCodecContext->codec_id);

	if (pInputCodec == NULL)
		return EXIT_FAILURE; // Codec not found

	if (avcodec_open2(pInputCodecContext, pInputCodec, 0) < 0)
		return EXIT_FAILURE; // Could not open codec

	if (pInputCodecContext->time_base.num > 1000 && pInputCodecContext->time_base.den == 1)
		pInputCodecContext->time_base.den = 1000;

	//Allocate frames
	pFrame = av_frame_alloc();
	pOutFrame = av_frame_alloc();

	/* Set up output frame */
	pOutFrame->width = OUTWIDTH;
	pOutFrame->height = OUTHEIGHT;
	pOutFrame->format = OUTPXFMT;

	//Allocate output buffer
	int numBytes = avpicture_get_size(OUTPXFMT, OUTWIDTH, OUTHEIGHT);
	pOutFrameBuffer = (uint8_t *)av_malloc(numBytes);

	//Fill the output frame
	avpicture_fill((AVPicture *)pOutFrame, pOutFrameBuffer, OUTPXFMT, OUTWIDTH, OUTHEIGHT);


	/* UDP Socket */
	//Init SDLNet
	SDLNet_Init();

	//Open a UDP socket at port 2000, listening to all IPs
	if (!(sock = SDLNet_UDP_Open(0))) {
		fprintf(stderr, "SDLNet_UDP_Open: %s\n", SDLNet_GetError());
		return EXIT_FAILURE;
	}
	if (SDLNet_ResolveHost(&srvadd, "127.0.0.1", 2000)) {
		fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return EXIT_FAILURE;
	}

	//To simulate data loss
	SDLNet_UDP_SetPacketLoss(sock, 0);

	/* Output video */
	//This should be same on receiving end
	AVRational avr;
	avr.den = 30;
	avr.num = 1;
	outCodec = avcodec_find_encoder(OUTCODEC);
	pOutputCodecContext = avcodec_alloc_context3(outCodec);
	pOutputCodecContext->bit_rate = outBitRate;
	pOutputCodecContext->width = OUTWIDTH;
	pOutputCodecContext->height = OUTHEIGHT;
	pOutputCodecContext->pix_fmt = OUTPXFMT;
	pOutputCodecContext->time_base = avr;
	
	if (avcodec_open2(pOutputCodecContext, outCodec, NULL) < 0) {
		fprintf(stderr, "could not open codec\n");
		return EXIT_FAILURE;
	}

	//To convert the video to suitable format
	sws_ctx = sws_getContext(
		pInputCodecContext->width,
		pInputCodecContext->height,
		pInputCodecContext->pix_fmt,
		OUTWIDTH,
		OUTHEIGHT,
		OUTPXFMT,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
	);

	//Allocate Packets
	AVPacket *inPkt, *outPkt;
	inPkt = av_packet_alloc();
	outPkt = av_packet_alloc();

	std::cout << "Beginning transcoding..." << std::endl;

	Timer t, updateTim;

	int mxSize = 0, totalSize = 0, lastPkSize = 0;
	int frames = 0;


	//Transcode loop
	while (av_read_frame(pInputFormatCtx, inPkt) >= 0) {
		t.reset();
		double elapsed = 0.0;

		if (inPkt->stream_index == videoStream) {
			int ret = avcodec_send_packet(pInputCodecContext, inPkt);
			while (ret >= 0) {
				ret = avcodec_receive_frame(pInputCodecContext, pFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				else if (ret < 0) {
					fprintf(stderr, "error during encoding\n");
					return EXIT_FAILURE;
				}

				/* Post processing */

				//Scale the frame to suit the output
				sws_scale(
					sws_ctx,
					(uint8_t const * const *)pFrame->data,
					pFrame->linesize, 0, pInputCodecContext->height,
					pOutFrame->data, pOutFrame->linesize);
				
				//Encode the new frame
				ret = avcodec_send_frame(pOutputCodecContext, pOutFrame);
				
				while (ret >= 0) {
					ret = avcodec_receive_packet(pOutputCodecContext, outPkt);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						break;
					else if (ret < 0) {
						fprintf(stderr, "error during encoding\n");
						return EXIT_FAILURE;
					}

					//Record peak data size
					mxSize = std::max(mxSize, outPkt->size);
					totalSize += outPkt->size;
					lastPkSize = outPkt->size;

					//Send the frame packet
					sendrData(&sock, outPkt->data, outPkt->size);

					frames++;

					elapsed = t.elapsed() * 1000.0;
					
					av_packet_unref(outPkt);
				}
			}

			if (updateTim.elapsed() > 0.25) {
				int average = totalSize / frames;
				std::cout << "\r" << "Frame " << frames << " of size " << lastPkSize << " Bytes, Max: " << mxSize << " Bytes, " << totalSize << " Bytes total, Average " << average << " Bytes per frame, took " << elapsed << "ms";
				updateTim.reset();
			}

#ifdef THROTTLE
			//Sleep to maintain framerate if possible
			_sleep((unsigned long)std::max(1000 * avr.num / avr.den - elapsed, 0.0));
#endif
		}

		av_packet_unref(inPkt);
	}

	//Exit
	avcodec_free_context(&pInputCodecContext);
	sws_freeContext(sws_ctx);
	avcodec_free_context(&pOutputCodecContext);

	av_frame_free(&pFrame);
	av_frame_free(&pOutFrame);

	av_free(pOutFrameBuffer);

	av_packet_free(&inPkt);
	av_packet_free(&outPkt);

	SDLNet_UDP_Close(sock);

	return EXIT_SUCCESS;
}
