#pragma once

#include "Stream.h"

#include <memory>
#include <SDL/SDL_net.h>

class UDPStream : public Stream {
private:
	UDPsocket sock;
	IPaddress srvadd;
public:
	UDPStream();
	UDPStream(const char* ip, uint16_t port);
	~UDPStream();

	void open(const char* ip, uint16_t port);
	void send(uint8_t *data, size_t size) override;
	void close() override;
};