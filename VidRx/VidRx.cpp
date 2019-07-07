// VidRx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>

#include <ftdi/ftd2xx.h>

//FFmpeg headers (using C)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

//Load FFmpeg Libraries
#ifdef _MSC_VER
#pragma comment(lib, "ffmpeg/avformat.lib")
#pragma comment(lib, "ffmpeg/avcodec.lib")
#pragma comment(lib, "ffmpeg/avutil.lib")
#pragma comment(lib, "ffmpeg/swscale.lib")
#endif

//SDLNet for transmitting over TCP / UDP
#ifdef _MSC_VER
#pragma comment(lib, "sdl2/SDL2.lib")
#pragma comment(lib, "sdl2_net/SDL2_net.lib")

//GL Render engine
#ifdef _DEBUG
#pragma comment(lib, "sgle/Debug/SGLE.lib")
#else
#pragma comment(lib, "sgle/Release/SGLE.lib")
#endif

#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

//GL renderer
#include <sgle/SGLE.h>

//Video Codec
#define OUTCODEC AV_CODEC_ID_MPEG4

//Output window size
#define RENDERW 1280
#define RENDERH 720

glWindow *mainGL;
GLuint vao, vbo;
GLuint rgb_tex;
GLuint prog;

//Input codec and format
AVCodec *pCodec;
AVCodecContext *pCodecCtx;
struct SwsContext *sws_ctx = nullptr;

//Ouput frame
AVFrame *pFrame, *pOutFrame;
uint8_t *pOutFrameBuffer;
int pOutFrameBufferSize;

//Codec packet


//UDP Socket
UDPsocket sock;

AVPacket* tPck;

AVCodecParserContext* cpc;

std::vector<uint8_t> dataBuf;

void decodeFrame(AVPacket* rPacket);
/*
struct {
	uint8_t sig[3];
	uint8_t part;
	uint8_t totalparts;
	uint64_t totalsize;
	uint8_t data[32700];
	uint64_t size;
} pack;*/

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

	//Allocate a packet vector to receive
	if (!(upck = SDLNet_AllocPacket(32768))) {
		fprintf(stderr, "SDLNet_AllocPacket: %s\n", SDLNet_GetError()); exit(EXIT_FAILURE);
	}

	//Receive data packets
	int retCode;
	while ((retCode = SDLNet_UDP_Recv(sock, upck))) {
		uint8_t* in_data = upck->data;
		int in_len = upck->len;
		int len;

		while (in_len) {
			len = av_parser_parse2(cpc, pCodecCtx, &tPck->data, &tPck->size, in_data, in_len, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

			in_data += len;
			in_len -= len;

			if (tPck->size)
				decodeFrame(tPck);
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

void decodeFrame(AVPacket *rPacket) {
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
		if (sws_ctx == nullptr) {
			if (pFrame->width == 0 || pFrame->height == 0 || pFrame->format == -1)
				break;

			//To convert to RGB
			sws_ctx = sws_getContext(
				pFrame->width,
				pFrame->height,
				static_cast <AVPixelFormat>(pFrame->format),
				RENDERW,
				RENDERH,
				AV_PIX_FMT_RGB24,
				SWS_BILINEAR,
				NULL,
				NULL,
				NULL
			);
		}

		if (sws_ctx != nullptr) {
			sws_scale(
				sws_ctx,
				(uint8_t const* const*)pFrame->data,
				pFrame->linesize, 0, pFrame->height,
				pOutFrame->data, pOutFrame->linesize);

			setRGBPixels(pOutFrame->data[0], RENDERW);
		}

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

	//Create and Open Codec
	pCodec = avcodec_find_decoder(OUTCODEC);
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (OUTCODEC == AV_CODEC_ID_MPEG4) {
		pCodecCtx->width = RENDERW;
		pCodecCtx->height = RENDERH;
	}

	if (pCodec == NULL)
		return -1; // Codec not found

	if (pCodec->capabilities & AV_CODEC_CAP_TRUNCATED)
		pCodecCtx->flags |= AV_CODEC_FLAG_TRUNCATED;

	if (avcodec_open2(pCodecCtx, pCodec, 0) < 0)
		return -1; // Could not open codec

	//Input frame and RGB frame
	pFrame = av_frame_alloc();
	pOutFrame = av_frame_alloc();

	tPck = av_packet_alloc();

	cpc = av_parser_init(OUTCODEC);

	/* Set up output frame */
	pOutFrame->width = RENDERW;
	pOutFrame->height = RENDERH;
	pOutFrame->format = AV_PIX_FMT_RGB24;
	pOutFrameBufferSize = avpicture_get_size(AV_PIX_FMT_RGB24, RENDERW, RENDERH);
	pOutFrameBuffer = (uint8_t *)av_malloc(pOutFrameBufferSize);

	//Fill the output frame
	avpicture_fill((AVPicture *)pOutFrame, pOutFrameBuffer, AV_PIX_FMT_RGB24, RENDERW, RENDERH);

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
	
	if (sws_ctx != nullptr)
		sws_freeContext(sws_ctx);

	av_packet_free(&tPck);
	av_frame_free(&pFrame);
	av_frame_free(&pOutFrame);
	av_free(pOutFrameBuffer);

	SDLNet_UDP_Close(sock);
	SDL_Quit();

	return EXIT_SUCCESS;
}
