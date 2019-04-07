/* Original license message */

/**************************************\
* bmp2nes.c                            *
* Windows .bmp to NES .chr converter   *
*                                      ********************************\
* Copyright 2000 Damian Yerrick                                        *
*                                                                      *
* This program is free software; you can redistribute it and/or modify *
* it under the terms of the GNU General Public License as published by *
* the Free Software Foundation; either version 2 of the License, or    *
* (at your option) any later version.                                  *
*                                                                      *
* This program is distributed in the hope that it will be useful,      *
* but WITHOUT ANY WARRANTY; without even the implied warranty of       *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
* GNU General Public License for more details.                         *
*                                                                      *
* You should have received a copy of the GNU General Public License    *
* along with this program; if not, write to the Free Software          *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
* Or view the License online at http://www.gnu.org/copyleft/gpl.html   *
*                                                                      *
* Damian Yerrick's World Wide Web pages are located at                 *
*   http://come.to/yerrick                                             *
*                                                                      *
\**********************************************************************/

//This code *should* be portable

//TODO: Palette swap to save atleast four frames in one!

//Library Load, only for VC
#ifdef _MSC_VER
#pragma comment(lib, "libav\\avformat.lib")
#pragma comment(lib, "libav\\avcodec.lib")
#pragma comment(lib, "libav\\avutil.lib")
#pragma comment(lib, "libav\\swscale.lib")
#endif

#include "Vid2NesFMV.h"

//How many bits in a tile have to be different in order to encode it
//Set to anything above 127 to disable fuzzy tile matching
#define MATCH_THRES 128

//Number of seconds of footage to encode (0 for entire)
#define DUR_RENDER 0

#define FRAMEW 256
#define FRAMEH 240

#define FRAMESKIP 4

#define MORECOLORS true
#define DOPIXELATE false

//Standard Libraries
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <map>
#include <algorithm>

//LibAV headers (using C)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

void convert_RGB2CHR(AVFrame *pixelData, unsigned int width, unsigned int height, unsigned char *pixData) {
	unsigned char curByte, *pixPtr = pixData;

	unsigned int x, y, cx, cy, cz, pixcolor;

	//Raw RGB plane data
	uint8_t *rgbData = pixelData->data[0];

	for (y = 0; y < 240; y += 8) {
		for (x = 0; x < 256; x += 8) {
			for (cz = 0; cz < 2; cz++) {
				for (cy = y; cy < y + 8; cy++) {
					curByte = 0;
					for (cx = x; cx < x + 8; cx++) {
						//Shift in one bit per pixel (of each bit plane)
						curByte <<= 1;

						//Current pixel color
#if DOPIXELATE
						int rgbPixel = ((cy % 2 == 0 ? cy : (cy - 1))*width + (cx % 2 == 0 ? cx : (cx - 1))) * 3;
#else
						int rgbPixel = (cy*width + cx) * 3;
#endif
						//Greyscale
						if (cx < width && cy < height)
							pixcolor = (rgbData[rgbPixel] + rgbData[rgbPixel + 1] + rgbData[rgbPixel + 2]) / 3;
						else
							pixcolor = 0;

						//Set bit plane pixel
						curByte |= ((pixcolor / (MORECOLORS ? 64 : 128)) >> cz) & 1;
					}

					//Write the 8px line
					*(pixPtr++) = curByte;
				}
			}
		}
	}
}


struct Tile {
	uint8_t plane0[8], plane1[8];
};

int lookFor(Tile *list, int size, Tile value) {
	//Phase 1: find exact match
	for (int i = 0; i < size; i++)
		if (memcmp(&list[i], &value, sizeof(Tile)) == 0) return i;

	return -1;
}


int main(int argc, const char **argv) {
	//Input video
	const char* fileName = "C:\\Users\\naray\\Downloads\\Bad Apple PV.mp4";

	struct SwsContext *img_convert_ctx;
	AVFormatContext *pFormatCtx = NULL;
	unsigned int i, videoStream;
	int frameNo = 0;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVFrame *pFrame;
	AVFrame *pFrameRGB;
	AVPacket packet;
	int frameFinished;
	int numBytes;
	uint8_t *buffer;

	av_register_all();

	if (avformat_open_input(&pFormatCtx, fileName, NULL, 0) != 0)
		return -1;

	if (avformat_find_stream_info(pFormatCtx, 0) < 0)
		return -1; // Couldn't find stream information

	av_dump_format(pFormatCtx, 0, fileName, false);

	videoStream = INT_MAX;

	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == INT_MAX)
		return -1; // Didn't find a video stream


	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

	if (pCodec == NULL)
		return -1; // Codec not found

	if (avcodec_open2(pCodecCtx, pCodec, 0) < 0)
		return -1; // Could not open codec

	// Hack to correct wrong frame rates that seem to be generated by some codecs
	if (pCodecCtx->time_base.num > 1000 && pCodecCtx->time_base.den == 1)
		pCodecCtx->time_base.den = 1000;

	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();

	if (pFrameRGB == NULL)
		return -1;

	numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, FRAMEW, FRAMEH, AV_INPUT_BUFFER_PADDING_SIZE);
	buffer = (uint8_t*)av_malloc(numBytes);

	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, FRAMEW, FRAMEH, AV_INPUT_BUFFER_PADDING_SIZE);
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, FRAMEW, FRAMEH, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	pFrameRGB->width = FRAMEW;
	pFrameRGB->height = FRAMEH;

	uint8_t nesData[0x3C00], nametableMap[0x400];
	Tile *patternTable;
	uint32_t patSize = 0x100;

	patternTable = new Tile[patSize];

	memset(nesData, 0, sizeof(nesData));
	memset(nametableMap, 0, sizeof(nametableMap));
	memset(patternTable, 0, patSize * sizeof(Tile));

	std::ofstream fileP("D:\\badAppleP.dat", std::ios::binary | std::ios::out);
	std::ofstream fileC("D:\\badAppleC.dat", std::ios::binary | std::ios::out);
#ifdef _TEST_CHARS
	char tempDat[256];
#endif

	bool doDecode = true;
	int lastUsedTile = 0;
	int patternPage = 0;

	int frameFail;

	std::cout << std::endl;

	while (av_read_frame(pFormatCtx, &packet) >= 0 && doDecode) {
		if (packet.stream_index == videoStream) {
			int ret = avcodec_send_packet(pCodecCtx, &packet);
			while (ret >= 0 && doDecode) {
				ret = avcodec_receive_frame(pCodecCtx, pFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				else if (ret < 0) {
					std::cout << std::endl << "error during decoding";
					break;
				}

				//Frame has been decoded
				frameFail = 0;
				frameNo++;

				if (frameNo > 240) {
					doDecode = false;
					break;
				}

				sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				convert_RGB2CHR(pFrameRGB, pFrameRGB->width, pFrameRGB->height, nesData);

				std::map<int, int> tiletonametable;
#if 1

				bool found;
				for (int i = 0; i < 0x3C0; i++) {
					found = false;
					Tile test;
					memcpy(&test, nesData + i * sizeof(Tile), sizeof(Tile));

					int loc = lookFor(patternTable, patSize, test);
					if (loc >= 0) {
						tiletonametable.insert_or_assign(i, loc);
					} else {
						lastUsedTile++;
						patternTable[lastUsedTile * sizeof(Tile)] = test;
						tiletonametable.insert_or_assign(i, lastUsedTile);
						if (lastUsedTile > 0xFF) {
							//patternPage++;
							//std::cout << std::endl << "Page!" << std::endl;
							lastUsedTile = 0;
						}
					}
				}

				std::map<int, int>::iterator it;

				for (int y = 0; y < 30; y++) {
					for (int x = 0; x < 32; x++) {
						it = tiletonametable.find(x + y * 32);
						if (it != tiletonametable.end())
							nametableMap[x + y * 32] = it->second;
					}
				}
#else
				for (int y = 0; y < 30; y++) {
					for (int x = 0; x < 32; x++) {
						nametableMap[x + y * 32] = 0;
					}
				}
#endif

				if (frameFail) {
					std::cout << std::endl << "Frame " << frameNo << " could not be compressed!" << std::endl;
					continue;
				}

				fileP.write((char*)(nametableMap), 0x400);


				std::cout << "\r" << frameNo << " / " << pFormatCtx->streams[0]->nb_frames << " frames written";
				
				av_free_packet(&packet);
				av_packet_unref(&packet);
			}
		}
	}

	fileC.write((char*)patternTable, patSize*sizeof(Tile));

	delete[] patternTable;

	std::cout << std::endl;

	fileP.close();
	fileC.close();
	av_frame_free(&pFrameRGB);
	av_frame_free(&pFrame);
	avcodec_free_context(&pCodecCtx);
	av_free(buffer);

	return EXIT_SUCCESS;
}
