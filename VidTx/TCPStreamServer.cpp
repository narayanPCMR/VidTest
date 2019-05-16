#include "pch.h"
#include "TCPStreamServer.h"

TCPStreamServer::TCPStreamServer() {}

TCPStreamServer::TCPStreamServer(uint16_t port, bool useHTTP) {
	this->open(port, useHTTP);
}

TCPStreamServer::~TCPStreamServer() {
	this->close();
}

void TCPStreamServer::open(uint16_t port, bool useHTTP) {
	this->acceptT = std::thread([=]() {
		IPaddress ip;
		TCPsocket s;

		this->port = port;
		this->run = true;

		if (SDLNet_ResolveHost(&ip, NULL, port) == -1) {
			return;
		}

		s = SDLNet_TCP_Open(&ip);

		std::string HTTP_HDR;
		HTTP_HDR += "HTTP/1.1 200 OK\r\n";
		HTTP_HDR += "Server: Simple Server!\r\n";
		HTTP_HDR += "\r\n";

		while (run) {
			TCPsocket c = SDLNet_TCP_Accept(s);
			if (c != NULL) {
				if (useHTTP)
					SDLNet_TCP_Send(c, HTTP_HDR.c_str(), HTTP_HDR.length());
				clients.push_back(c);
			}
			_sleep(10);
		}
	});
}

void TCPStreamServer::send(uint8_t *data, size_t size) {
	for (std::vector<TCPsocket>::iterator c = clients.begin(); c != clients.end();) {
		int len = SDLNet_TCP_Send(*c, data, size);
		if (len < size)
			c = clients.erase(c);
		else
			c++;
	}
}

void TCPStreamServer::close() {
	this->run = false;
	for (TCPsocket c : clients)
		SDLNet_TCP_Close(c);
	this->acceptT.join();
}

