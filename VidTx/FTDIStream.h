#pragma once

#include "Stream.h"
#include <algorithm>
#include <ftdi/ftd2xx.h>

class FTDIStream : public Stream {
private:
	FT_HANDLE ftdhdl;
	unsigned int devIdx;
	unsigned long baudRate;
public:
	FTDIStream();
	FTDIStream(unsigned int device, unsigned long baud);
	~FTDIStream();

	void open(unsigned int device, unsigned long baud);
	void send(uint8_t *data, size_t size) override;
	void close() override;
};
