#pragma once

#include "Stream.h"
#include <algorithm>
#include <ftdi/ftd2xx.h>
#include <thread>
#include <vector>

class FTDIStream : public Stream {
private:
	FT_HANDLE ftdhdl = NULL;
	std::thread ftThead;
	unsigned int devIdx = 0;
	unsigned long baudRate = 0;

	std::vector<uint8_t> wBuffer;
	uint16_t wBufferPos = 0;
public:
	FTDIStream();
	FTDIStream(unsigned int device, unsigned long baud);
	~FTDIStream();

	void open(unsigned int device, unsigned long baud);
	void send(uint8_t *data, size_t size) override;
	void close() override;
};
