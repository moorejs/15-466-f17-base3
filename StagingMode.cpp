#include "StagingMode.hpp"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490"

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

StagingMode::StagingMode() {

}

// Connect to server
void StagingMode::reset() {
	if (sock) {
		sock->close();
		delete sock;
	}

	// Basic client code structure from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleclient
  int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("::1", PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		// TODO: what now?
		return;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		// TODO: what now?
		return;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo);	// all done with this structure

	sock = new Socket(sockfd);
}

bool StagingMode::handle_event(SDL_Event const& event, glm::uvec2 const& window_size) {
	if (!sock) {
		return false;
	}

	if (event.type == SDL_MOUSEBUTTONDOWN) {
		Packet* input = new Packet();

		input->payload.push_back(MessageType::STAGING_VOTE_TO_START);

		input->header = input->payload.size();

		sock->writeQueue.enqueue(input);
	}

  return false;
}

void StagingMode::update(float elapsed) {
	if (!sock) {
		return;
	}

	static int counter = 0;
	counter++;
	if (counter % 240 == 0) {
		std::cout << "connected? " << (sock->isConnected() ? "true" : "false") << std::endl;
	}

	Packet* out;
	while (sock->readQueue.try_dequeue(out)) {
		if (!out) {
			std::cout << "Bad packet from server" << std::endl;
			continue;
		}

		switch (out->payload.at(0)) { // message type
			case MessageType::STAGING_PLAYER_CONNECT: {
				// TODO: this should be done in an unpacking function
				ConnectMessage* msg = reinterpret_cast<ConnectMessage*>(out->payload.data());
				std::cout << "Player " << (int)msg->id+1 << " joined the game." << std::endl;

				break;
			}

			case MessageType::STAGING_VOTE_TO_START: {
				VoteToStartMessage* msg = reinterpret_cast<VoteToStartMessage*>(out->payload.data());
				std::cout << "Player " << (int)msg->id+1 << " voted to start the game." << std::endl;

				break;
			}

			default: {
				std::cout << "Recieved unknown staging message type: " << out->payload.at(0) <<  std::endl;
				std::cout << "Contents from server: ";
				for (const auto& thing : out->payload) {
					printf("%x ", thing);
				}
				std::cout << std::endl;

				break;
			}

		}

		delete out;
	}
}
void StagingMode::draw(glm::uvec2 const& drawable_size) {}