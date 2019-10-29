#pragma once

#include "WriteStream.h"
#include <algorithm>
#include <ftdi/ftd2xx.h>
#include <thread>
#include <vector>
#include <mutex>

class FTDIStream : public WriteStream {
private:
	FT_HANDLE ftdhdl = NULL;
	std::thread ftThead;
	unsigned int devIdx = 0;
	unsigned long baudRate = 0;
	unsigned int failCount = 0;
	volatile bool run = false;

	std::mutex writeLock;

	std::vector<uint8_t> wBuffer;
	uint16_t wBufferPos = 0;

	bool tryConnect();
	void writeBuffer(DWORD bytes);
public:
	FTDIStream();
	FTDIStream(unsigned int device, unsigned long baud);
	~FTDIStream();

	void open(unsigned int device, unsigned long baud);
	void send(uint8_t *data, size_t size) override;
	void close() override;
};
