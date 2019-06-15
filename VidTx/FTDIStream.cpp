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

void FTDIStream::open(unsigned int device, unsigned long baud) {
	this->devIdx = device;
	this->baudRate = baud;

	if (FT_Open(device, &this->ftdhdl) != FT_OK)
		return;

	FT_ResetPort(this->ftdhdl);
	FT_SetBitMode(this->ftdhdl, 0x00, FT_BITMODE_RESET);
	FT_SetBaudRate(this->ftdhdl, baud);

	this->ftThead = std::thread([=]() {
		while (1) {
			while (wBuffer.size() >= 65536) {
				DWORD bWritten;

				FT_Write(this->ftdhdl, &wBuffer[0], 65536, &bWritten);

				wBuffer.erase(wBuffer.begin(), wBuffer.begin()+65536);
			}

			_sleep(1);
		}
	});
}

void FTDIStream::send(uint8_t *data, size_t size) {
	wBuffer.insert(wBuffer.end(), data, &data[size-1]);
	
	//
	/*while (pos < size) {
		memcpy_s(wBuffer, 65536, data+pos, 65536);

		pos += sizeof(wBuffer);
	}*/

	/*BYTE *buf = data;
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
	}*/
}

void FTDIStream::close() {
	FT_Close(this->ftdhdl);
}
