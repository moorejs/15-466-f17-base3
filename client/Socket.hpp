#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <iostream>

#include <arpa/inet.h>
#include <netdb.h>	// getaddrinfo
#include <netinet/in.h>
#include <sys/socket.h>	// getaddrinfo
#include <sys/types.h>	 // getaddrinfo
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../shared/queue/readerwriterqueue.h"

#include "../shared/State.hpp"

using moodycamel::BlockingReaderWriterQueue;
using moodycamel::ReaderWriterQueue;

class Socket {
	int fd;
	std::atomic<bool> connected;
	// TODO: probably can use std::memory_order_relaxed for operations on connected
	// http://en.cppreference.com/w/cpp/atomic/atomic/store

 public:
	// no move or copying
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;
	Socket(Socket&& other) = delete;
	Socket& operator=(Socket&&) = delete;

	void close() { ::close(fd); }

	bool isConnected() { return connected; }

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
				}) {
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
		unsigned int so_far;
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

		unsigned int so_far = 0;
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
