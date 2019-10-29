// VidTx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//TODO: Refer to https://github.com/leixiaohua1020/simplest_ffmpeg_mem_handler/blob/master/simplest_ffmpeg_mem_transcoder/simplest_ffmpeg_mem_transcoder.cpp
//TODO: Byte (de)stuffing


/*
			 AVIOContext -> [File / UDP / TCP / FTDI]
				  |
			AVFormatContext-----.
								|
								|
			.--- AVCodec----.	|
			|				|	|
	AVCodecContext			AVStream
		|		|
		|		|
	AVFrame -> AVPacket
		|  (After encoding)
		|
		|
	uint8_t* (Frame Buffer)
*/

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

//"Modern" version
static void fill_yuv_image(AVFrame* pict, int frame_index, int width, int height) {
	int x, y, i;

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}
/*
int allocFrames() {
	//Allocate frame
	pOutFrame = av_frame_alloc();

	//Set up output frame
	pOutFrame->width = OUTWIDTH;
	pOutFrame->height = OUTHEIGHT;
	pOutFrame->format = OUTPXFMT;

	//Allocate output buffer
	int numBytes = avpicture_get_size(OUTPXFMT, OUTWIDTH, OUTHEIGHT);
	pOutFrameBuffer = (uint8_t *)av_malloc(numBytes);

	//Fill the output frame
	avpicture_fill((AVPicture *)pOutFrame, pOutFrameBuffer, OUTPXFMT, OUTWIDTH, OUTHEIGHT);

	return numBytes;
}
*/

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

class Writer {
private:
	AVIOContext* pAVIO;
	uint8_t* outWriteBuffer;
	//FILE* f;
public:
	Writer() {
		//f = fopen("D:\\test.flv", "wb");
		outWriteBuffer = (uint8_t*)av_malloc(1024);

		pAVIO = avio_alloc_context(outWriteBuffer, sizeof(outWriteBuffer), AVIO_FLAG_WRITE, this,
			nullptr,

			[](void* opaque, unsigned char* buf, int buf_size) -> int {
				outStream->send(buf, buf_size);

				return buf_size;//fwrite(buf, 1, buf_size, ((Writer*)opaque)->f);
			},

			[](void* opaque, int64_t offset, int whence) -> int64_t {
				return offset;//fseek(((Writer*)opaque)->f, offset, whence);
			}
		);
	}

	~Writer() {
		av_freep(&outWriteBuffer);
		//fclose(f);
	}

	AVIOContext* getIOContext() {
		return pAVIO;
	}
};

class Muxer {
private:
	AVFormatContext* pFormatCtx;
public:
	Muxer(Writer* wr, const char* ext) {
		pFormatCtx = avformat_alloc_context();
		pFormatCtx->oformat = av_guess_format(NULL, ext, NULL);
		pFormatCtx->pb = wr->getIOContext();
	}

	void WriteHeader() {
		avformat_write_header(pFormatCtx, nullptr);
	}

	void WriteTrailer() {
		av_write_trailer(pFormatCtx);
	}

	void WritePacket(AVPacket* pck) {
		if (pck == nullptr) return;
		av_interleaved_write_frame(pFormatCtx, pck);
	}

	AVFormatContext* getFormatContext() {
		return pFormatCtx;
	}
};

class Frame {
private:
	AVFrame* pFrame;
	uint8_t* pFrameBuffer;
	int pFrameBufferSize;
public:
	Frame(int width, int height, int format) {
		pFrame = av_frame_alloc();
		pFrame->width = width;
		pFrame->height = height;
		pFrame->format = format;
		pFrame->pts = 0;

		pFrameBufferSize = avpicture_get_size(OUTPXFMT, OUTWIDTH, OUTHEIGHT);
		pFrameBuffer = (uint8_t*)av_malloc(pFrameBufferSize);

		//Fill the output frame
		//av_image_fill_arrays
		avpicture_fill((AVPicture*)pFrame, pFrameBuffer, OUTPXFMT, OUTWIDTH, OUTHEIGHT);
	}

	void setPTS(int pts) {
		pFrame->pts = pts;
	}

	AVFrame* getFrame() {
		return pFrame;
	}

	int getWidth() {
		return pFrame->width;
	}

	int getHeight() {
		return pFrame->height;
	}

	int getFormat() {
		return pFrame->format;
	}

	~Frame() {
		av_frame_free(&pFrame);
		av_freep(&pFrameBuffer);
	}
};

class Stream {
private:
	AVStream* outVideoStream;
	AVCodec* outCodec;
	AVPacket* outPkt;
	AVCodecContext* pOutputCodecContext;
	Muxer* parentMuxer;
	Frame* encFrame;
	static int DTS;

public:
	Stream(Muxer* m, AVCodecID codec, Frame* encFrame, int bitRate=3200000) {
		parentMuxer = m;
		this->encFrame = encFrame;
		outCodec = avcodec_find_encoder(codec);

		outVideoStream = avformat_new_stream(m->getFormatContext(), outCodec);
		
		AVCodecParameters* streamParameters = outVideoStream->codecpar;
		streamParameters->width = encFrame->getWidth();
		streamParameters->height = encFrame->getHeight();
		streamParameters->format = encFrame->getFormat();
		streamParameters->codec_id = codec;
		streamParameters->bit_rate = bitRate;
		streamParameters->codec_type = avcodec_get_type(codec);
		outVideoStream->id = m->getFormatContext()->nb_streams - 1;
		outVideoStream->time_base.num = 1;
		outVideoStream->time_base.den = 30;

		pOutputCodecContext = avcodec_alloc_context3(outCodec);

		avcodec_parameters_to_context(pOutputCodecContext, streamParameters);
		pOutputCodecContext->width = streamParameters->width;
		pOutputCodecContext->height = streamParameters->height;
		pOutputCodecContext->bit_rate = bitRate;
		pOutputCodecContext->time_base.num = 1;
		pOutputCodecContext->time_base.den = 30;

		switch (streamParameters->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			pOutputCodecContext->pix_fmt = (AVPixelFormat)streamParameters->format;
			break;
		case AVMEDIA_TYPE_AUDIO:
			pOutputCodecContext->sample_fmt = (AVSampleFormat)streamParameters->format;
			break;
		}

		avcodec_open2(pOutputCodecContext, outCodec, NULL);
		avcodec_parameters_from_context(streamParameters, pOutputCodecContext);

		outVideoStream->avg_frame_rate = av_inv_q(outVideoStream->time_base);

		outPkt = av_packet_alloc();
	}

	int Encode(int64_t framePos, void(*callback)(Stream*, AVPacket*)) {
		int ret;

		encFrame->getFrame()->pts += 1;//av_rescale_q(1, pOutputCodecContext->time_base, outVideoStream->time_base);

		ret = avcodec_send_frame(pOutputCodecContext, encFrame->getFrame());

		while (ret >= 0) {
			ret = avcodec_receive_packet(pOutputCodecContext, outPkt);
			
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			else if (ret < 0)
				return ret;

			callback(this, outPkt);

			av_packet_unref(outPkt);
		}

		return ret;
	}

	AVCodecContext* getCodecContext() {
		return pOutputCodecContext;
	}

	Muxer* getMuxer() {
		return parentMuxer;
	}

	~Stream() {
		avcodec_free_context(&pOutputCodecContext);
		av_free_packet(outPkt);
	}
};
int Stream::DTS = 0;

void freeAll() {
	outStream->close();
}

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

void sendPacket(Stream* s, AVPacket* pck) {
#ifdef USECONTAINER
	s->getMuxer()->WritePacket(pck);
#else
	outStream->send(pck->data, pck->size);
#endif
}

int main() {
	int frames = 0;

	initTX();

	Frame* img = new Frame(OUTWIDTH, OUTHEIGHT, OUTPXFMT);
	Frame* img2 = new Frame(640, 480, OUTPXFMT);

	Writer* w = new Writer();
	Muxer* f = new Muxer(w, OUTCNT);

	Stream* s = new Stream(f, OUTCODEC, img);
	Stream* s2 = new Stream(f, OUTCODEC, img2);

#ifdef USECONTAINER
	f->WriteHeader();
#endif

	while (frames < 3000) {
		fill_yuv_image(img->getFrame(), frames, OUTWIDTH, OUTHEIGHT);

		s->Encode(frames, sendPacket);
		//s2->Encode(frames, sendPacket);

		Sleep(10);
		frames++;
	}

#ifdef USECONTAINER
	f->WriteTrailer();
#endif

	freeAll();

	return EXIT_SUCCESS;
}
