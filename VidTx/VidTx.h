#pragma once

#include <iostream>
#include <algorithm>

#define NOMINMAX
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#include "WriteStream.h"
#include "UDPStream.h"
#include "FTDIStream.h"
#include "TCPStreamServer.h"
#include "Timer.h"

//LibAV headers (using C)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

//Output Stream
WriteStream* outStream;
