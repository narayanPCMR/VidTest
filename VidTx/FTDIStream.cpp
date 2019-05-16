#include "pch.h"
#include "FTDIStream.h"
#include <iostream>

FTDIStream::FTDIStream() {}

FTDIStream::FTDIStream(unsigned int device, unsigned long baud) {
	this->open(device, baud);
}

FTDIStream::~FTDIStream() {
	this->close();
}

void FTDIStream::open(unsigned int device, unsigned long baud) {
	this->devIdx = device;
	this->baudRate = baud;

	if (FT_Open(device, &this->ftdhdl) != FT_OK)
		return;

	FT_ResetPort(this->ftdhdl);
	FT_SetBitMode(this->ftdhdl, 0X00, FT_BITMODE_RESET);
	FT_SetBaudRate(this->ftdhdl, baud);
}

void FTDIStream::send(uint8_t *data, size_t size) {
	BYTE *buf = data;
	DWORD len = size;
	FT_STATUS status;
	DWORD written;

	for (;;) {
		status = FT_Write(this->ftdhdl, buf, len, &written);
		if (status != FT_OK) {
			std::cout << std::endl << "NOT OK! Code: " << status << std::endl;
			std::cout << "Size:" << size << std::endl;
			this->close();
			this->open(this->devIdx, this->baudRate);

			return;
		}
		if (written == 0) {
			std::cout << "Timeout!" << std::endl;
			return;
		}
		if (written == len)
			return;

		len -= written;
		buf += written;
	}
}

void FTDIStream::close() {
	FT_Close(this->ftdhdl);
}
