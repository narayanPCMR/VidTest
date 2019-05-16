#pragma once

#include <stdint.h>

class Stream {
public:
	virtual void open() {};
	virtual void close() = 0;
	virtual void send(uint8_t *data, size_t size) = 0;
};
