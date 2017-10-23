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

StagingMode::StagingMode() {

}

// Connect to server
void StagingMode::reset() {
	if (sock) {
		sock->close();
		delete sock;
	}

	sock = Socket::connect("::1", "3490");
}

bool StagingMode::handle_event(SDL_Event const& event, glm::uvec2 const& window_size) {
	if (!sock) {
		return false;
	}

	if (event.type == SDL_MOUSEBUTTONDOWN) {
		Packet* out;
		if (starting) {
			out = SimpleMessage::pack(MessageType::STAGING_VETO_START);
		} else {
			out = SimpleMessage::pack(MessageType::STAGING_VOTE_TO_START);
		}

		sock->writeQueue.enqueue(out);
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
				const SimpleMessage* msg = SimpleMessage::unpack(out->payload);
				std::cout << "Player " << (int)msg->id+1 << " joined the game." << std::endl;

				break;
			}

			case MessageType::STAGING_VOTE_TO_START: {
				const SimpleMessage* msg = SimpleMessage::unpack(out->payload);
				std::cout << "Player " << (int)msg->id+1 << " voted to start the game." << std::endl;
				starting = true;

				break;
			}

			case MessageType::STAGING_VETO_START: {
				const SimpleMessage* msg = SimpleMessage::unpack(out->payload);
				std::cout << "Player " << (int)msg->id+1 << " vetoed the game start." << std::endl;
				starting = false;

				break;
			}

			case MessageType::STAGING_START_GAME: {
				enterGame(sock); // TODO: make sock unique_ptr and move it here
				// TODO: maybe should add a GameInfo struct and pass that to the game, to capture # players, etc.
				// or have the server tell us all that too, maybe easier

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