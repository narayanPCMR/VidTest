#pragma once

#include "Stream.h"

#include <thread>
#include <vector>

#include <SDL2/SDL_net.h>

class TCPStreamServer : public Stream {
private:
	std::thread acceptT;
	std::vector<TCPsocket> clients;
	uint16_t port = 0;
	volatile bool run = false;
public:
	TCPStreamServer();
	TCPStreamServer(uint16_t port, bool useHTTP = false);
	~TCPStreamServer();

	void open(uint16_t port, bool useHTTP = false);
	void send(uint8_t *data, size_t size) override;
	void close() override;
};
