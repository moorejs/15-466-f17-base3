#pragma once

#include <atomic>
#include <initializer_list>
#include <thread>
#include <vector>

#include <iostream>

// Some of these may be superfluous
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "queue/readerwriterqueue.h"

#include "State.hpp"

using moodycamel::BlockingReaderWriterQueue;
using moodycamel::ReaderWriterQueue;

// Thoughts: Really not happy with current serialization method but it works

/* TODO:
 * - client tries to reconnect on disconnect?
 * - allow for way to stack messages and write all at once
 * - StagingState / GameState delta
 */

class ClientSocket {
	int fd;
	std::atomic<bool> connected;

 public:
	ClientSocket(int fd)
			: fd(fd),
				connected(true),
				readThread([&]() {
					while (true) {
						if (!connected) {
							return;
						}

						Packet* packet = getPacket();
						if (!packet) {
							return;
						}
						readQueue.enqueue(packet);
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
							return;
						}

						sendPacket(packet);
					}
				}) {
		readThread.detach();
		writeThread.detach();
	}

	// no move or copying
	ClientSocket(const ClientSocket&) = delete;
	ClientSocket& operator=(const ClientSocket&) = delete;
	ClientSocket(ClientSocket&& other) = delete;
	ClientSocket& operator=(ClientSocket&&) = delete;

	void close() { ::close(fd); }

	bool isConnected() { return connected; }

	static int initServer(const std::string& port, int backlog = 10) {
		// Much of the server code here is from
		// http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleserver

		int sockfd;	// listen on sock_fd, new connection on new_fd
		struct addrinfo hints, *servinfo, *p;
		int yes = 1;
		int rv;

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;	// use my IP

		if ((rv = getaddrinfo(nullptr, port.c_str(), &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			exit(1);
		}

		// loop through all the results and bind to the first we can
		for (p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("server: socket");
				continue;
			}

			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
				perror("setsockopt");
				exit(1);
			}

			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				::close(sockfd);
				perror("server: bind");
				continue;
			}

			break;
		}

		freeaddrinfo(servinfo);	// all done with this structure

		if (p == NULL) {
			fprintf(stderr, "server: failed to bind\n");
			exit(1);
		}

		if (listen(sockfd, backlog) == -1) {
			perror("listen");
			exit(1);
		}

		printf("server: waiting for connections...\n");

		return sockfd;
	}

	// accept client
	static int accept(int sockfd) {
		static auto get_in_addr = [](struct sockaddr* sa) -> void* {
			if (sa->sa_family == AF_INET) {
				return &(((struct sockaddr_in*)sa)->sin_addr);
			}
			return &(((struct sockaddr_in6*)sa)->sin6_addr);
		};

		static auto get_in_port = [](struct sockaddr* sa) -> int {
			if (sa->sa_family == AF_INET) {
				return ntohs(((struct sockaddr_in*)sa)->sin_port);
			}
			return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
		};

		static struct sockaddr_storage their_addr;	// connector's address information
		static socklen_t sin_size = sizeof their_addr;

		int fd = ::accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
		if (fd == -1) {
			perror("accept");
			return -1;
		}

		static char s[INET6_ADDRSTRLEN];
		// convert IP to string to print
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s);
		printf("server: got connection from %s %d\n", s, get_in_port((struct sockaddr*)&their_addr));

		return fd;
	}

	ReaderWriterQueue<Packet*> readQueue;
	BlockingReaderWriterQueue<Packet*> writeQueue;

 private:
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
		unsigned so_far;
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

		unsigned so_far = 0;
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