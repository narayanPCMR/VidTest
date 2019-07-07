// VidTx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "VidTx.h"
#include "config.h"

//Load FFmpeg Libraries
#ifdef _MSC_VER
#pragma comment(lib, "ffmpeg/avformat.lib")
#pragma comment(lib, "ffmpeg/avcodec.lib")
#pragma comment(lib, "ffmpeg/avutil.lib")
#pragma comment(lib, "ffmpeg/swscale.lib")
#endif

//SDLNet for transmitting over TCP / UDP
#ifdef _MSC_VER
#pragma comment(lib, "sdl2_net/SDL2_net.lib")
#endif

#ifdef _MSC_VER
#pragma comment(lib, "ftdi/ftd2xx.lib")
#endif

//TODO: Update deprecated functions

/*
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
}*/

bool loadVideo() {
	//Load the video
	if (avformat_open_input(&pInputFormatCtx, fileName, NULL, 0) != 0)
		return false;

	//Find video Stream
	if (avformat_find_stream_info(pInputFormatCtx, 0) < 0)
		return false;

	videoStream = INT_MAX;
	for (unsigned int i = 0; i < pInputFormatCtx->nb_streams; i++) {
		if (pInputFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == INT_MAX)
		return false;

	//Create and Open Codec
	pInputCodecContext = pInputFormatCtx->streams[videoStream]->codec;
	pInputCodec = avcodec_find_decoder(pInputCodecContext->codec_id);

	if (pInputCodec == NULL)
		return false;

	if (avcodec_open2(pInputCodecContext, pInputCodec, 0) < 0)
		return false;

	if (pInputCodecContext->time_base.num > 1000 && pInputCodecContext->time_base.den == 1)
		pInputCodecContext->time_base.den = 1000;

	return true;
}

void allocFrames() {
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
}

void initTX() {
	//Init SDLNet
	SDLNet_Init();

#if TXMODE == MODE_TCP
	outStream = new TCPStreamServer(9999);
#elif TXMODE == MODE_UDP
	outStream = new UDPStream(UDP_IP, UDP_PORT);
#elif TXMODE == MODE_FTDI
	outStream = new FTDIStream(0, FTDI_BAUD);
#endif
}

bool initOPFrame() {
	//This should be same on receiving end
	avr.den = 30;
	avr.num = 1;
	outCodec = avcodec_find_encoder(OUTCODEC);
	pOutputCodecContext = avcodec_alloc_context3(outCodec);
	pOutputCodecContext->bit_rate = bitRate;
	pOutputCodecContext->width = OUTWIDTH;
	pOutputCodecContext->height = OUTHEIGHT;
	pOutputCodecContext->pix_fmt = OUTPXFMT;
	pOutputCodecContext->time_base = avr;
	pOutputCodecContext->max_b_frames = 8;

	if (avcodec_open2(pOutputCodecContext, outCodec, NULL) < 0) {
		std::cout << "Could not open codec" << std::endl;
		return false;
	}

	//To convert the video to suitable format
	sws_ctx = sws_getContext(
		pInputCodecContext->width,
		pInputCodecContext->height,
		pInputCodecContext->pix_fmt,
		OUTWIDTH,
		OUTHEIGHT,
		OUTPXFMT,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);

	return true;
}

void freeAll() {
	avcodec_free_context(&pInputCodecContext);
	sws_freeContext(sws_ctx);
	avcodec_free_context(&pOutputCodecContext);

	av_frame_free(&pFrame);
	av_frame_free(&pOutFrame);

	av_free(pOutFrameBuffer);

	av_packet_free(&inPkt);
	av_packet_free(&outPkt);

	outStream->close();
}

void allocPck() {
	inPkt = av_packet_alloc();
	outPkt = av_packet_alloc();
}

int main() {
	/* Debug information */
	Timer t, updateTim;
	int mxSize = 0, totalSize = 0, lastPkSize = 0, prevTotal = 0;
	int frames = 0;

	//Presentation Timestamp for encoder
	int pts_ct = 0;

	if (!loadVideo())
		return EXIT_FAILURE;

	allocFrames();
	
	initTX();
	
	if (!initOPFrame())
		return EXIT_FAILURE;
	
	allocPck();

	std::cout << "Beginning transcoding..." << std::endl;

	//Transcode loop
	do {
		frames = 0;
		av_seek_frame(pInputFormatCtx, videoStream, 0, 0);

		while (av_read_frame(pInputFormatCtx, inPkt) >= 0) {
			t.reset();
			double elapsed = 0.0;

			if (inPkt->stream_index == videoStream) {
				int ret = avcodec_send_packet(pInputCodecContext, inPkt);
				while (ret >= 0) {
					ret = avcodec_receive_frame(pInputCodecContext, pFrame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						break;
					else if (ret < 0)
						return EXIT_FAILURE;

					/* Post processing */

					//Scale the frame to suit the output
					sws_scale(sws_ctx,
						(uint8_t const* const*)pFrame->data,
						pFrame->linesize, 0, pInputCodecContext->height,
						pOutFrame->data, pOutFrame->linesize);

					pOutFrame->pts = pts_ct++;

					//Encode the new frame
					ret = avcodec_send_frame(pOutputCodecContext, pOutFrame);

					while (ret >= 0) {
						ret = avcodec_receive_packet(pOutputCodecContext, outPkt);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
							break;
						else if (ret < 0) {
							std::cout << "Error during encoding" << std::endl;
							return EXIT_FAILURE;
						}

						//Record peak data size
						mxSize = std::max(mxSize, outPkt->size);
						totalSize += outPkt->size;
						lastPkSize = outPkt->size;

						/* RX SOLUTION!!!! */
						/*
						AVCodecParserContext* cpc;
						cpc = av_parser_init(OUTCODEC);

						uint8_t* in_data = outPkt->data;
						int in_len = outPkt->size;

						uint8_t* data;
						int size, len;

						while (in_len) {
							len = av_parser_parse2(cpc, pOutputCodecContext, &data, &size, outPkt->data, outPkt->size, outPkt->pts, outPkt->dts, 0);

							in_data += len;
							in_len -= len;

							if (size)
								decode(data, size);
						}*/

						//Send the frame packet
						outStream->send(outPkt->data, outPkt->size);

						frames++;
						elapsed = t.elapsed() * 1000.0;

						av_packet_unref(outPkt);
					}
				}

				if (updateTim.elapsed() > 0.25 && frames > 0) {
					int average = totalSize / frames;
					std::cout << "F:" << frames << ", " << lastPkSize << "B, MAX: " << mxSize << "B, ALL: " << totalSize << "B, AVG: " << average << "B/F (" << ((totalSize - prevTotal) * avr.den / avr.num) << "B/s" << "), T:" << elapsed << "ms (potential " << (1000.0 / elapsed) << "fps)    \r";
					prevTotal = totalSize;
					updateTim.reset();
				}

#ifdef THROTTLE
				//Sleep to maintain framerate if possible
				Sleep((DWORD)std::max(1000 * avr.num / avr.den - elapsed, 0.0));
#endif
			}

			av_packet_unref(inPkt);
		}
	} while (repeatTimes--);

	freeAll();

	return EXIT_SUCCESS;
}
