#include "pch.h"
#include "UDPStream.h"

UDPStream::UDPStream() {}

UDPStream::UDPStream(const char* ip, uint16_t port) {
	this->open(ip, port);
}

UDPStream::~UDPStream() {
	this->close();
}

void UDPStream::open(const char* ip, uint16_t port) {
	//Open a UDP socket, listening to all IPs
	if (!(this->sock = SDLNet_UDP_Open(0)))
		return;
	if (SDLNet_ResolveHost(&this->srvadd, ip, port))
		return;

	//To simulate data loss
	SDLNet_UDP_SetPacketLoss(this->sock, 0);
}

void UDPStream::send(uint8_t *data, size_t size) {
	UDPpacket *pck;
	uint64_t dtPos = 0, dtSize = 0;

	if (!(pck = SDLNet_AllocPacket(32768)))
		return;

	while (dtPos < size) {
		dtSize = size - dtPos;
		if (dtSize > 32768)
			dtSize = 32768;

		//Sending
		memcpy_s(pck->data, pck->maxlen, data + dtPos, dtSize);
		pck->len = dtSize;

		pck->address.host = this->srvadd.host;
		pck->address.port = this->srvadd.port;

		SDLNet_UDP_Send(this->sock, -1, pck);

		dtPos += dtSize;
	}

	SDLNet_FreePacket(pck);
}

void UDPStream::close() {
	SDLNet_UDP_Close(this->sock);
}
