// LiveStream.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

//Library Load, only for VC
#ifdef _MSC_VER
#pragma comment(lib, "libav\\avformat.lib")
#pragma comment(lib, "libav\\avcodec.lib")
#pragma comment(lib, "libav\\avutil.lib")
#pragma comment(lib, "libav\\swscale.lib")
#pragma comment (lib, "Ws2_32.lib")
#endif

//LibAV headers (using C)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <map>
#include <algorithm>
#include <thread>

#if defined(_WIN64) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

AVFormatContext *pFormatCtx;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVFrame *pFrame;
AVPacket *pPacket;
uint8_t *buffer;
int buffSize;

HANDLE EvFrame;

std::vector<std::thread> userPool;

class iNET {
	int iResult;
	WSADATA wsaData;
	SOCKET listenSock;
	struct addrinfo hints;
	struct addrinfo *result = NULL;
public:
	iNET() {
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			std::cout << "WSAStartup failed with error: " << iResult << std::endl;
			return;
		}

		listenSock = INVALID_SOCKET;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		iResult = getaddrinfo(NULL, "9876", &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			return;
		}
	}

	int sockListen() {
		// Create a SOCKET for connecting to server
		listenSock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (listenSock == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			return 1;
		}

		// Setup the TCP listening socket
		iResult = bind(listenSock, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("bind failed with error: %d\n", WSAGetLastError());
			freeaddrinfo(result);
			closesocket(listenSock);
			WSACleanup();
			return 1;
		}

		freeaddrinfo(result);

		iResult = listen(listenSock, SOMAXCONN);
		if (iResult == SOCKET_ERROR) {
			printf("listen failed with error: %d\n", WSAGetLastError());
			closesocket(listenSock);
			WSACleanup();
			return 1;
		}
	}

	void sockStopListen() {
		closesocket(listenSock);
	}

	SOCKET acceptSock() {
		SOCKET ClientSocket = accept(listenSock, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			return INVALID_SOCKET;
		}

		return ClientSocket;
	}

	static std::string readLine(SOCKET s) {
		std::string data;
		char readData;

		int ret;
		do {
			ret = recv(s, &readData, 1, 0);
			if (ret == 1) {
				data += readData;
			}

			if (data.find("\r\n", 0) != std::string::npos) return data;
		} while (ret > 0);

		return data;
	}

	~iNET() {
		WSACleanup();
	}
};


void sockThread(SOCKET userSock) {
	std::string header, hdrName, hdrVal;
	std::map<std::string, std::string> headers;

	//Parse headers
	do {
		header = iNET::readLine(userSock);

		size_t separator = header.find_first_of(':');
		if (separator == std::string::npos) {
			if (header.find_first_of("GET") != std::string::npos) continue;
		}
		hdrName = header.substr(0, separator);
		hdrVal = header.substr(separator + 1, std::string::npos);

		headers.insert(std::pair<std::string, std::string>(hdrName, hdrVal));

		if (header == "\r\n") break;
	} while (header.size() > 0);

	char sendDat[] = "HTTP/1.1 200 OK\r\n\r\n";
	send(userSock, sendDat, sizeof(sendDat), 0);

	while (true) {
		while (WaitForSingleObjectEx(EvFrame, INFINITE, TRUE) != WAIT_OBJECT_0);
			


		if (send(userSock, (char*)pPacket->data, pPacket->size, 0) < 0) break;
	}



	/*
	char sendDat[] = "HTTP/1.1 200 OK\r\n\r\n";
	int gotIt;
	send(s, sendDat, sizeof(sendDat), 0);
	//avformat_write_header();

	bool connOpen = true;
	*/


	closesocket(userSock);
}

void produceImage() {
	int ret;
	while (1) {
		//Fill some image
		for (int i = 0; i < buffSize; i++) {
			buffer[i] ++;
		}

		ret = avcodec_send_frame(pCodecCtx, pFrame);
		while (ret >= 0) {
			ret = avcodec_receive_packet(pCodecCtx, pPacket);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				fprintf(stderr, "error during encoding\n");
				break;
			}

			PulseEvent(EvFrame);

			Sleep(16);

			//printf("encoded frame (size=%5d)\n", pPacket->size);
			//if (send(s, (char*)pPacket->data, pPacket->size, 0) < 0) connOpen = false;
			av_packet_unref(pPacket);
		}
	}
}

int main() {
	av_register_all();


	pFormatCtx = avformat_alloc_context();
	pCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
	pCodecCtx = avcodec_alloc_context3(pCodec);
	pPacket = av_packet_alloc();
	av_init_packet(pPacket);


	const int fps = 30;
	pCodecCtx->width = 640;
	pCodecCtx->height = 480;
	pCodecCtx->bit_rate = 400000;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;


	AVRational tbase; tbase.num = 1; tbase.den = fps;
	pCodecCtx->time_base = tbase;
	AVRational fpsrate; fpsrate.num = fps; fpsrate.den = 1;
	pCodecCtx->framerate = fpsrate;


	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		fprintf(stderr, "could not open codec\n");
		exit(1);
	}


	//Prepare frame
	pFrame = av_frame_alloc();
	pFrame->width = pCodecCtx->width;
	pFrame->height = pCodecCtx->height;
	pFrame->format = pCodecCtx->pix_fmt;


	buffSize = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_INPUT_BUFFER_PADDING_SIZE);
	buffer = (uint8_t*)av_malloc(buffSize);
	av_image_fill_arrays(pFrame->data, pFrame->linesize, buffer, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_INPUT_BUFFER_PADDING_SIZE);

	EvFrame = CreateEvent(NULL, TRUE, FALSE, TEXT("ENTER_FRAME"));

	iNET netCon;
	
	netCon.sockListen();

	std::thread vidThread = std::thread(produceImage);

	std::vector<std::thread> users;

	std::cout << "Waiting for clients..." << std::endl;
	while (1) {
		SOCKET s = netCon.acceptSock();
		std::thread t = std::thread(sockThread);
		users.push_back(t);
	}

	CloseHandle(EvFrame);

	av_packet_free(&pPacket);
	av_free(buffer);
	av_frame_free(&pFrame);
	avcodec_free_context(&pCodecCtx);
	avformat_free_context(pFormatCtx);

    return 0;
}

