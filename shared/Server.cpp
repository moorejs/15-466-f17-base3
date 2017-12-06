#include "Server.hpp"

using moodycamel::ReaderWriterQueue;

#ifdef DEBUG
#define DEBUG_PRINT(x) \
	std::cout << std::this_thread::get_id() << ":" << __FILE__ << ":" << __LINE__ << ": " << x << std::endl
#define IF_DEBUG(x) x
#else
#define DEBUG_PRINT(x)
#define IF_DEBUG(x)
#endif

Server::Server() {
	DEBUG_PRINT("IN DEBUG MODE");

	int sockfd = ClientSocket::initServer("3490");

	struct Client {
		uint8_t id;
		ClientSocket sock;

		enum Role {	// TODO: reuse code from client
			NONE,
			ROBBER,
			COP,
		} role = Role::NONE;

		Client(uint8_t id, int fd) : id(id), sock(fd) {}
	};

	ReaderWriterQueue<int> newClients;

	std::thread acceptThread([&sockfd, &newClients]() {
		int accepted = 0;

		while (accepted < 3) {
			int fd = ClientSocket::accept(sockfd);
			if (fd == -1) {
				continue;
			}
			newClients.enqueue(fd);

			accepted++;
		}
		std::cout << "Done accepting clients" << std::endl;
		close(sockfd);
	});
	acceptThread.detach();

	// ------- game state --------
	std::vector<std::unique_ptr<Client>> clients;

	enum State {
		STAGING,
		IN_GAME,
	} state = STAGING;

	struct StagingState {
		bool starting = false;
		float startingTimer = 0.0f;
		Client* robber = nullptr;	// everyone else assumed to be cop
		unsigned playerUnready = 0;

		// TODO: this should be in a settings struct
		const float POWER_LIMIT = 10.0f;
		const float TIME_LIMIT = 15.0f;	// TEMP, normally 150
		uint8_t seed = 200;
	} stagingState;

	struct GameState {
		Person* robber;
		Person* cop;

		bool ended = false;
		float gameTimer = 0.0f;

		float powerTimer = 0.0f;
		Power activePower = Power::NONE;
	} gameState;

	auto endGame = [&]() {
		gameState.ended = true;

		Person::freezeAll();
		gameState.robber->isMoving = false;

		gameState.cop = new Person();
		gameState.cop->pos = glm::vec3(-1.0f, 5.5f, 0.0f);
		gameState.cop->isMoving = true;
	};

	// clock timing logic from https://stackoverflow.com/a/20381816
	typedef std::chrono::duration<int, std::ratio<1, 30>> frame_duration;
	auto delta = frame_duration(1);
	float dt = 1.0f / 30.0f;

	std::thread gameLoop([&]() {
		while (true) {
			auto start_time = std::chrono::steady_clock::now();

			switch (state) {
				case STAGING: {
					// process newly accepted connections
					int fd;
					while (newClients.try_dequeue(fd)) {
						uint8_t newId = clients.size();

						std::vector<uint8_t> syncData;
						syncData.push_back(newId);
						for (auto& client : clients) {
							client->sock.writeQueue.enqueue(Packet::pack(MessageType::STAGING_PLAYER_CONNECT, {newId}));
							syncData.push_back(client->id);
							syncData.push_back(client->role);
						};

						clients.emplace_back(new Client(newId, fd));

						clients.back()->sock.writeQueue.enqueue(Packet::pack(MessageType::STAGING_PLAYER_SYNC, syncData));

						stagingState.playerUnready += 1;

						// TODO: tell new client about game settings / staging state
					}

					for (auto& client : clients) {
						if (!client->sock.isConnected()) {
							continue;
						}

						// read pending messages from clients
						Packet* out;
						while (client->sock.readQueue.try_dequeue(out)) {
							if (!out) {
								std::cout << "Bad packet from client" << std::endl;
								continue;
							}

							switch (out->payload.at(0)) {	// message type
								case MessageType::STAGING_VOTE_TO_START: {
									if (stagingState.starting) {
										break;
									}

									if (clients.size() < 2) {
										break;
									}

									if (stagingState.playerUnready > 0) {
										// TODO: error message saying not all players are ready
										break;
									}

									stagingState.starting = true;
									stagingState.startingTimer = 0.0f;
									IF_DEBUG(stagingState.startingTimer = 3.0f);

									std::cout << "Client voted to start the game" << std::endl;

									// TODO: queue up message saying player voted to start the game
									// or do it now?
									for (auto& c : clients) {
										c->sock.writeQueue.enqueue(Packet::pack(MessageType::STAGING_VOTE_TO_START, {client->id}));
									}

									break;
								}

								case MessageType::STAGING_VETO_START: {
									if (!stagingState.starting) {
										break;
									}

									stagingState.starting = false;

									std::cout << "Client vetoed the game start" << std::endl;

									// TODO: queue up message start vetod by x message
									// or do it now?
									for (auto& c : clients) {
										c->sock.writeQueue.enqueue(Packet::pack(MessageType::STAGING_VETO_START, {client->id}));
									}

									break;
								}

								case MessageType::STAGING_ROLE_CHANGE: {
									if (stagingState.starting) {
										break;
									}

									DEBUG_PRINT("client " << (int)client->id << " wants role " << int(out->payload[1]));

									if (out->payload[1] == Client::Role::ROBBER &&
											stagingState.robber) {	// can't be robber if someone else is
										client->sock.writeQueue.enqueue(
												Packet::pack(MessageType::STAGING_ROLE_CHANGE_REJECTION, {stagingState.robber->id}));
										break;
									}

									if (client->role == Client::Role::NONE) {	// client has never selected anything
										stagingState.playerUnready -= 1;
									}

									// client is no longer robber
									if (client->role == Client::Role::ROBBER && out->payload[1] != Client::Role::ROBBER) {
										stagingState.robber = nullptr;
									}

									if (out->payload[1] == Client::Role::ROBBER) {	// desires to be robber
										stagingState.robber = client.get();
									}

									client->role = static_cast<Client::Role>(out->payload[1]);

									// tell players of role change
									for (auto& c : clients) {
										c->sock.writeQueue.enqueue(
												Packet::pack(MessageType::STAGING_ROLE_CHANGE, {client->id, out->payload[1]}));
									}

									break;
								}

								default: {
									std::cout << "Unknown starting message type: " << (int)out->payload.at(0) << std::endl;
									break;
								}
							}

							delete out;
						}
					}

					if (stagingState.starting) {
						stagingState.startingTimer += dt;

						if (stagingState.startingTimer > 2.0f) {
							std::cout << "Game starting. Leaving staging." << std::endl;
							for (auto& c : clients) {
								c->sock.writeQueue.enqueue(Packet::pack(MessageType::STAGING_START_GAME, {stagingState.seed}));
							}

							gameState.robber = new Person();

							state = IN_GAME;
						}
					}

					// write state updates

					break;
				}

				case IN_GAME: {
					// we do this because we only send if a key is down one at a time
					// so we want to keep track of what keys to use what the last player setVel call
					float frameUp = false;
					float frameDown = false;
					float frameLeft = false;
					float frameRight = false;

					for (auto& client : clients) {
						// read pending messages from clients
						Packet* out;
						while (client->sock.readQueue.try_dequeue(out)) {
							if (!out) {
								std::cout << "Bad packet from client" << std::endl;
								continue;
							}

							DEBUG_PRINT("Packet " << (int)out->payload[0] << " from " << (int)client->id);

							switch (out->payload[0]) {	// message type

								case MessageType::INPUT: {
									if (client.get() == stagingState.robber || gameState.ended) {
										frameUp = frameUp || out->payload[1] == Control::UP;
										frameDown = frameDown || out->payload[1] == Control::DOWN;
										frameLeft = frameLeft || out->payload[1] == Control::LEFT;
										frameRight = frameRight || out->payload[1] == Control::RIGHT;

										DEBUG_PRINT("up " << frameUp << " down " << frameDown << " left" << frameLeft << " right"
																			<< frameRight);
									}

									break;
								}

								case MessageType::GAME_ACTIVATE_POWER: {
									if (client.get() != stagingState.robber && gameState.activePower == Power::NONE) {
										// TODO: would be nice to capture who used the power

										gameState.activePower = (Power)out->payload[1];
										gameState.powerTimer = 0.0f;
									}

									break;
								}

								default: {
									std::cout << "Unknown game message type: " << (int)out->payload[0] << std::endl;
									break;
								}
							}

							delete out;
						}
					}

					// game state updates
					gameState.gameTimer += dt;
					gameState.powerTimer += dt;

					if (gameState.powerTimer >= stagingState.POWER_LIMIT) {
						gameState.activePower = Power::NONE;
					}

					if (gameState.ended) {
						gameState.cop->setVel(frameUp, frameDown, frameLeft, frameRight);
						gameState.cop->move(dt, nullptr);
					} else {
						gameState.robber->setVel(frameUp, frameDown, frameLeft, frameRight);
						gameState.robber->move(dt, nullptr);
					}

					// write state updates
					if (gameState.ended) {
						for (auto& client : clients) {
							// TODO: only send on player move (delta compression)

							// TODO: send floats in a more legit way, this has endianness problems (maybe just add htonl() before)
							uint8_t* x = reinterpret_cast<uint8_t*>(&gameState.cop->pos.x);
							uint8_t* y = reinterpret_cast<uint8_t*>(&gameState.cop->pos.y);

							client->sock.writeQueue.enqueue(
									Packet::pack(MessageType::GAME_COP_POS, {x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]}));
						}
					} else {
						for (auto& client : clients) {
							// TODO: only send on player move (delta compression)

							// TODO: send floats in a more legit way, this has endianness problems (maybe just add htonl() before)
							uint8_t* x = reinterpret_cast<uint8_t*>(&gameState.robber->pos.x);
							uint8_t* y = reinterpret_cast<uint8_t*>(&gameState.robber->pos.y);

							client->sock.writeQueue.enqueue(
									Packet::pack(MessageType::GAME_ROBBER_POS, {x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]}));
						}
					}

					if (gameState.activePower != Power::NONE && gameState.powerTimer <= dt) {
						for (auto& client : clients) {
							client->sock.writeQueue.enqueue(Packet::pack(MessageType::GAME_ACTIVATE_POWER, {gameState.activePower}));
						}

						if (gameState.activePower == Power::DEPLOY) {
							endGame();
						}
					}

					if (!gameState.ended && gameState.gameTimer >= stagingState.TIME_LIMIT) {
						for (auto& client : clients) {
							client->sock.writeQueue.enqueue(Packet::pack(MessageType::GAME_TIME_OVER, {}));
						}
						endGame();
					}

					break;
				}
			}

			// sleep if necessary
			std::this_thread::sleep_until(start_time + delta);
		}
	});

	gameLoop.join();
}