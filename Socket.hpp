#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <iostream>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h> // getaddrinfo
#include <sys/socket.h> // getaddrinfo
#include <netdb.h> // getaddrinfo
#include <netinet/in.h>
#include <arpa/inet.h>

#include "queue/readerwriterqueue.h"

using moodycamel::ReaderWriterQueue;
using moodycamel::BlockingReaderWriterQueue;

struct Packet {
	uint8_t header;
	std::vector<uint8_t> payload;
};

enum MessageType {
	STAGING_PLAYER_CONNECT,
	STAGING_PLAYER_DISCONNECT,
	STAGING_VOTE_TO_START,
	STAGING_VETO_START,
	STAGING_START_GAME,
	INPUT,
};

struct SimpleMessage {
	MessageType type : 8;
	uint8_t id;

	static Packet* pack(MessageType type, uint8_t id = 255) {
		Packet* packet = new Packet();

		packet->payload.emplace_back(type);
		if (id != 255) {
			packet->payload.emplace_back(id);
		}

		packet->header = packet->payload.size();

		return packet;
	}

	static const SimpleMessage* unpack(const std::vector<uint8_t>& payload) {
		return reinterpret_cast<const SimpleMessage*>(payload.data());
	}
};

class Socket {

	int fd;
	std::atomic<bool> connected;

public:
	// no move or copying
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;
	Socket(Socket&& other) = delete;
	Socket& operator=(Socket&&) = delete;

	void close() {
		::close(fd);
	}

	bool isConnected() {
		return connected;
	}

	// connect client
	static Socket* connect(const std::string& hostname, const std::string& port) {
		// get sockaddr, IPv4 or IPv6:
		static auto get_in_addr = [](struct sockaddr* sa) -> void* {
			if (sa->sa_family == AF_INET) {
				return &(((struct sockaddr_in*)sa)->sin_addr);
			}
			return &(((struct sockaddr_in6*)sa)->sin6_addr);
		};

		// Basic client code structure from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleclient
		int sockfd;
		struct addrinfo hints, *servinfo, *p;
		int rv;
		char s[INET6_ADDRSTRLEN];

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if ((rv = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return nullptr;
		}

		// loop through all the results and connect to the first we can
		for (p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("client: socket");
				continue;
			}

			if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				::close(sockfd);
				perror("client: connect");
				continue;
			}

			break;
		}

		if (p == nullptr) {
			fprintf(stderr, "client: failed to connect\n");
			return nullptr;
		}

		inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
		printf("client: connecting to %s\n", s);

		freeaddrinfo(servinfo);	// all done with this structure

		return new Socket(sockfd);
	}

	ReaderWriterQueue<Packet*> readQueue;
	BlockingReaderWriterQueue<Packet*> writeQueue;

private:
	Socket(int fd)
		: fd(fd),
			connected(true),
			readThread([&]() {
				while (true) {
					if (!connected) {
						std::cout << "Disconnected, exiting read thread" << std::endl;
						return;
					}

					readQueue.enqueue(getPacket());
				}
			}),
			writeThread([&]() {
				while (true) {
					Packet* packet;
					writeQueue.wait_dequeue(packet);

					if (!packet) {
						continue;
					}

					if (!connected) {
						delete packet;
						std::cout << "Disconnected, not writing packet and exiting write thread" << std::endl;
						return;
					}

					sendPacket(packet);
				}
			})
	{
		readThread.detach();
		writeThread.detach();
	}

	std::thread readThread;
	std::thread writeThread;

	// recv that does error checking and connection checking
	int recv(void* buffer, size_t size) {
		int n = ::recv(fd, buffer, size, 0);

		if (n < 0) {
			perror("recv");
		}

		if (n == 0) {
			connected = false;
		}

		return n;
	}

	// send that does error checking and connection checking
	int send(void* buffer, size_t size) {
		int n = ::send(fd, buffer, size, 0);

		if (n < 0) {
			perror("send");
		}

		if (n == 0) {
			connected = false;
		}

		return n;
	}

	// get complete packet
	Packet* getPacket() {
		int so_far;
		Packet* packet = new Packet();

		// get header (could be multiple bytes in future)
		so_far = 0;
		do {
			int n = recv(&packet->header + so_far, (sizeof packet->header) - so_far);
			so_far += n;

			if (n == 0) {
				delete packet;
				return nullptr;
			}
		} while (so_far < sizeof packet->header);

		assert(packet->header > 0);

		// get payload
		packet->payload.resize(packet->header);
		so_far = 0;
		do {
			int n = recv(packet->payload.data() + so_far, packet->header - so_far);
			so_far += n;

			if (n == 0) {
				delete packet;
				return nullptr;
			}
		} while (so_far < packet->header);
		packet->payload.resize(packet->header);

		return packet;
	}

	// sends all data in vector
	bool sendPacket(Packet* packet) {
		// send header (could be multiple bytes in future)
		int n = send(&packet->header, 1);
		if (n <= 0) {
			return false;
		}

		int so_far = 0;
		do {
			int m = send(packet->payload.data() + so_far, packet->payload.size() - so_far);
			so_far += m;
			if (m <= 0) {
				return false;
			}
		} while (so_far < packet->payload.size());

		return true;
	}
};