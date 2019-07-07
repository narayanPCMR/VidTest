#pragma once

#include <stdint.h>

/* Input video */
const char* fileName = "D:\\Downloads\\Pexels Videos 1672801.mp4";

//Video Metadata
#define OUTWIDTH		1280
#define OUTHEIGHT		720
#define OUTCODEC		AV_CODEC_ID_MPEG4
#define OUTPXFMT		AV_PIX_FMT_YUV420P

#define FTDI_BAUD		2000000

//UDP TX Port/IP
const char *UDP_IP = "127.0.0.1";
const uint16_t UDP_PORT = 2000;

//Comment to transcode at unlimited speed
#define THROTTLE

//Switch high or low bitrate
unsigned int bitRate = 3200000;

//How many times to repeat the video before exitting
int repeatTimes = 2;

#define MODE_TCP		0
#define MODE_UDP		1
#define MODE_FTDI		2

//Transmission Media
#define TXMODE			MODE_UDP
