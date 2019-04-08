// VidRx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>

#include <ftdi/ftd2xx.h>

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


//SDLNet for receiving over UDP
//TODO: Use FTDI Serial over Laser
#ifdef _MSC_VER
#pragma comment(lib, "sdl_net/SDL2_net.lib")
#pragma comment(lib, "SGLE.lib")
#endif
#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#include <SDL/SDL_net.h>

//GL renderer
#include <SGLE.h>

//Video Metadata
#define OUTWIDTH 640
#define OUTHEIGHT 480
#define OUTCODEC AV_CODEC_ID_MPEG4
#define OUTPXFMT AV_PIX_FMT_YUV420P

//Output window size
#define RENDERW 640
#define RENDERH 480

glWindow *mainGL;
GLuint vao, vbo;
GLuint rgb_tex;
GLuint prog;

//Input codec and format
AVCodec *pCodec;
AVCodecContext *pCodecCtx;
struct SwsContext *sws_ctx;

//Ouput frame
AVFrame *pFrame, *pOutFrame;
uint8_t *pOutFrameBuffer;
int pOutFrameBufferSize;

//Codec packet
AVPacket *rPacket;

//UDP Socket
UDPsocket sock;

std::vector<uint8_t> dataBuf;

void decodeFrame(uint8_t* dataBuf, int size);

struct {
	uint8_t sig[3];
	uint8_t part;
	uint8_t totalparts;
	uint64_t totalsize;
	uint8_t data[32700];
	uint64_t size;
} pack;

void setupTextures() {
	glGenTextures(1, &rgb_tex);
	glBindTexture(GL_TEXTURE_2D, rgb_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, RENDERW, RENDERH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void setRGBPixels(uint8_t* pixels, int stride) {
	glBindTexture(GL_TEXTURE_2D, rgb_tex);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RENDERW, RENDERH, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

void gameEngine(long dT) {
	UDPpacket *upck;
	std::vector< std::vector<uint8_t>::iterator> points;

	//Allocate a packet vector to receive
	if (!(upck = SDLNet_AllocPacket(32768))) {
		fprintf(stderr, "SDLNet_AllocPacket: %s\n", SDLNet_GetError()); exit(EXIT_FAILURE);
	}
	
	//Receive data packets
	int retCode;
	while ((retCode = SDLNet_UDP_Recv(sock, upck))) {
		memcpy_s(&pack, sizeof(pack), upck->data, upck->len);

		if (pack.sig[0] != 0x01 || pack.sig[1] != 0x88 || pack.sig[2] != 0x28) {
			std::cout << "Packet header not correct" << std::endl;
		} else {
			std::cout << "Received Packet of length " << pack.size << "B of " << pack.totalsize << "B" << std::endl;

			if (pack.part == 0 && pack.totalparts == 1) {
				dataBuf.clear();
				decodeFrame(pack.data, pack.size);
				break;
			} else if (pack.part == 0) {
				dataBuf.clear();
				dataBuf.insert(dataBuf.end(), pack.data, pack.data + pack.size);
			} else {
				dataBuf.insert(dataBuf.end(), pack.data, pack.data + pack.size);

				if (pack.part == pack.totalparts - 1) {
					if (dataBuf.size() != pack.totalsize) {
						std::cout << "Missing: got " << dataBuf.size() << " bytes, expected " << pack.totalsize << std::endl;
					}

					decodeFrame(&dataBuf[0], dataBuf.size());
					dataBuf.clear();
					break;
				}
			}
		}
	}

	SDLNet_FreePacket(upck);
}

void drawEngine() {
	mainGL->useShader(prog);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rgb_tex);

	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(0);

	glDisable(GL_BLEND);
}

void decodeFrame(uint8_t* dataBuf, int size) {
	rPacket->data = dataBuf;
	rPacket->size = size;

	int ret = avcodec_send_packet(pCodecCtx, rPacket);
	while (ret >= 0) {
		ret = avcodec_receive_frame(pCodecCtx, pFrame);

		//Need more data to complete frame(s)
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			std::cout << "error during decoding" << std::endl;
			break;
		}

		//Convert YUV to RGB
		sws_scale(
			sws_ctx,
			(uint8_t const * const *)pFrame->data,
			pFrame->linesize, 0, pFrame->height,
			pOutFrame->data, pOutFrame->linesize);

		setRGBPixels(pOutFrame->data[0], RENDERW);

		av_packet_unref(rPacket);
	}
}

int main() {
	//Create a GL window
	mainGL = new glWindow("Output", RENDERW, RENDERH, false);

	//Init SDLNet
	SDLNet_Init();

	//No pesky main() re-definition!
	SDL_SetMainReady();


	/* UDP Socket */
	//Open a UDP socket at port 2000
	if (!(sock = SDLNet_UDP_Open(2000))) {
		fprintf(stderr, "SDLNet_UDP_Open: %s\n", SDLNet_GetError()); exit(EXIT_FAILURE);
	}

	//Init all LibAV components
	av_register_all();

	//Create and Open Codec
	pCodec = avcodec_find_decoder(OUTCODEC);
	pCodecCtx = avcodec_alloc_context3(pCodec);
	pCodecCtx->pix_fmt = OUTPXFMT;
	pCodecCtx->width = OUTWIDTH;
	pCodecCtx->height = OUTHEIGHT;

	if (pCodec == NULL)
		return -1; // Codec not found

	if (avcodec_open2(pCodecCtx, pCodec, 0) < 0)
		return -1; // Could not open codec

	//Allocate Packet
	rPacket = av_packet_alloc();

	//Input frame and RGB frame
	pFrame = av_frame_alloc();
	pOutFrame = av_frame_alloc();

	/* Set up output frame */
	pOutFrame->width = RENDERW;
	pOutFrame->height = RENDERH;
	pOutFrame->format = AV_PIX_FMT_RGB24;
	pOutFrameBufferSize = avpicture_get_size(AV_PIX_FMT_RGB24, RENDERW, RENDERH);
	pOutFrameBuffer = (uint8_t *)av_malloc(pOutFrameBufferSize);

	//Fill the output frame
	avpicture_fill((AVPicture *)pOutFrame, pOutFrameBuffer, AV_PIX_FMT_RGB24, RENDERW, RENDERH);

	//To convert to RGB
	sws_ctx = sws_getContext(
		OUTWIDTH,
		OUTHEIGHT,
		OUTPXFMT,
		RENDERW,
		RENDERH,
		AV_PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
	);

	setupTextures();

	prog = glWindow::LoadShaders("vtx.shader", "frag.shader");
	glUseProgram(prog);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	//Set texture to location 0
	glUniform1i(glGetUniformLocation(prog, "vid_tex"), 0);

	//Main loop
	mainGL->gameLoop(gameEngine, drawEngine);

	//Exit
	delete mainGL;

	avcodec_free_context(&pCodecCtx);
	sws_freeContext(sws_ctx);

	av_frame_free(&pFrame);
	av_frame_free(&pOutFrame);
	av_free(pOutFrameBuffer);

	av_packet_free(&rPacket);

	SDLNet_UDP_Close(sock);
	SDL_Quit();

	return EXIT_SUCCESS;
}
