#include "pch.h"
#include "FTDIStream.h"
#include <iostream>
#include <wtypes.h>

FTDIStream::FTDIStream() {}

FTDIStream::FTDIStream(unsigned int device, unsigned long baud) {
	this->open(device, baud);
}

FTDIStream::~FTDIStream() {
	this->close();
}

bool FTDIStream::tryConnect() {
	if (FT_Open(this->devIdx, &this->ftdhdl) != FT_OK) {
		failCount++;
		return false;
	}

	FT_ResetPort(this->ftdhdl);
	FT_SetBitMode(this->ftdhdl, 0x00, FT_BITMODE_RESET);
	FT_SetBaudRate(this->ftdhdl, this->baudRate);
	failCount = 0;

	return true;
}

void FTDIStream::writeBuffer(DWORD bytes) {
	DWORD bWritten = 0;
	FT_STATUS wStat;
	bool lock = false;

	if (wBuffer.size() > bytes)
		lock = true;

	if (lock)
		writeLock.lock();

	do {
		wStat = FT_Write(this->ftdhdl, &wBuffer[0], bytes, &bWritten);

		if (wStat != FT_OK) {
			if (!this->tryConnect() && this->failCount > 20) {
				this->failCount = 0;
				break;
			}
			_sleep(100);
		}
	} while (wStat != FT_OK);

	if (bWritten <= bytes) {
		wBuffer.erase(wBuffer.begin(), wBuffer.begin() + bytes);
	}

	if (lock)
		writeLock.unlock();
}

void FTDIStream::open(unsigned int device, unsigned long baud) {
	this->devIdx = device;
	this->baudRate = baud;

	this->run = false;
	if(this->ftThead.joinable())
		this->ftThead.join();

	this->tryConnect();

	this->ftThead = std::thread([=]() {
		this->run = true;
		while (run) {
			if (wBuffer.size() >= 65536)
				this->writeBuffer(65536);
			else if (wBuffer.size() > 0)
				this->writeBuffer(wBuffer.size());

			_sleep(1);
		}
	});
}

void FTDIStream::send(uint8_t *data, size_t size) {
	writeLock.lock();
	wBuffer.insert(wBuffer.end(), data, &data[size-1]);
	writeLock.unlock();
}

void FTDIStream::close() {
	this->run = false;
	if (this->ftThead.joinable())
		this->ftThead.join();
	FT_Close(this->ftdhdl);
}
