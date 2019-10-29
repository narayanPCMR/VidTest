#pragma once

#include <stdint.h>


//Video Metadata
//#define USECONTAINER
#define OUTWIDTH		320
#define OUTHEIGHT		240
#define OUTFPS			30
#define OUTCODEC		AV_CODEC_ID_MPEG4
#define OUTPXFMT		AV_PIX_FMT_YUV420P
#define OUTCNT			".ts"


#define FTDI_BAUD		2000000


//UDP TX Port/IP
const char *UDP_IP = "127.0.0.1";
const uint16_t UDP_PORT = 2000;




#define MODE_TCP		0
#define MODE_UDP		1
#define MODE_FTDI		2

//Transmission Media
#define TXMODE			MODE_UDP
