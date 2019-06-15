#pragma once

#include <stdint.h>

/* Input video */
const char* fileName = "D:\\Downloads\\Pexels Videos 1672801.mp4";

//Video Metadata
#define OUTWIDTH		640
#define OUTHEIGHT		360
#define OUTCODEC		AV_CODEC_ID_MPEG4
#define OUTPXFMT		AV_PIX_FMT_YUV420P

#define FTDI_BAUD		115200

//UDP TX Port/IP
const char *UDP_IP = "192.168.1.22";
const uint16_t UDP_PORT = 2000;

//Comment to transcode at unlimited speed
#define THROTTLE

//Switch high or low bitrate
unsigned int bitRate = 32000;

int repeatTimes = 1;

#define MODE_TCP		0
#define MODE_UDP		1
#define MODE_FTDI		2

//Transmission Media
#define TXMODE			MODE_FTDI
