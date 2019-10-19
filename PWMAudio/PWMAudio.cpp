// PWMAudio.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#ifdef _MSC_VER
#pragma comment(lib, "ffmpeg/avformat.lib")
#pragma comment(lib, "ffmpeg/avcodec.lib")
#pragma comment(lib, "ffmpeg/avutil.lib")
#pragma comment(lib, "ffmpeg/swscale.lib")
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

const char* fileName = "D:\\Downloads\\Junko_Ohashi_-_Telephone_Number_(mp3.pm).mp3";

AVCodec* pInputCodec;
AVCodecParameters* pCodecPar;
AVCodecContext* pInputCodecContext;
AVFormatContext* pInputFormatCtx;
AVFrame* pFrame;
AVPacket* inPkt;

int audioStream;

int main() {
	if (avformat_open_input(&pInputFormatCtx, fileName, NULL, 0) != 0)
		return false;
	
	if (avformat_find_stream_info(pInputFormatCtx, 0) < 0)
		return false;

	audioStream = INT_MAX;
	for (unsigned int i = 0; i < pInputFormatCtx->nb_streams; i++) {
		if (pInputFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = i;
			break;
		}
	}
	if (audioStream == INT_MAX)
		return false;

	pCodecPar = pInputFormatCtx->streams[audioStream]->codecpar;
	pInputCodec = avcodec_find_decoder(pCodecPar->codec_id);
	pInputCodecContext = avcodec_alloc_context3(pInputCodec);
	
	if (pInputCodec == NULL)
		return false;

	if (avcodec_open2(pInputCodecContext, pInputCodec, 0) < 0)
		return false;

	if (pInputCodecContext->time_base.num > 1000 && pInputCodecContext->time_base.den == 1)
		pInputCodecContext->time_base.den = 1000;

	pFrame = av_frame_alloc();
	inPkt = av_packet_alloc();

	FILE* f;
	fopen_s(&f, "D:\\m.raw", "wb");

	float out = 0.0f;
	double ctr = 0;

	while (av_read_frame(pInputFormatCtx, inPkt) >= 0) {
		if (inPkt->stream_index != audioStream) continue;

		int ret = avcodec_send_packet(pInputCodecContext, inPkt);
		while (ret >= 0) {
			ret = avcodec_receive_frame(pInputCodecContext, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			else if (ret < 0)
				return EXIT_FAILURE;

			size_t sz = pFrame->linesize[0];
			uint8_t* buffer = (uint8_t*)av_malloc(sz);
			int bufferPos = 0;
			
			for (int i = 0; i < sz; i += 4) {
				float l, r, mono;

				uint8_t* a = (uint8_t*)& l;
				uint8_t* b = (uint8_t*)& r;
				
				for (int y = 0; y < 4; y++) {
					*(a++) = pFrame->data[0][i + y];
					*(b++) = pFrame->data[1][i + y];
				}

				mono = (l + r) / 2.0f;

				out = (float)std::sin(ctr * 4.0 * 2.0 * 3.1415926) / 2.0f;
				out += mono / 2.0f;
				
				ctr += (double)sz / (double)pFrame->sample_rate;

				memcpy_s(buffer+i, 4, (uint8_t*)& out, sizeof(out));
			}

			if(f != nullptr)
				fwrite(buffer, 1, sz, f);

			av_free(buffer);
		}
	}

	if (f != nullptr)
		fclose(f);

	return 0;
}
