#include "StagingMode.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "GLBuffer.hpp"
#include "GLProgram.hpp"
#include "GLVertexArray.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"

#include "../shared/State.hpp"

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << __FILE__ << ":" << __LINE__ << ": " << x << std::endl
#define IF_DEBUG(x) x
#else
#define DEBUG_PRINT(x)
#define IF_DEBUG(x)
#endif

Load<MeshBuffer> staging_meshes(LoadTagInit, []() { return new MeshBuffer("menu.p"); });

extern Load<MeshBuffer> menuMeshes;

// Attrib locations in staging_program:
GLint staging_program_Position = -1;
// Uniform locations in staging_program:
GLint staging_program_mvp = -1;
GLint staging_program_color = -1;

// Menu program itself:
Load<GLProgram> staging_program(LoadTagInit, []() {
	GLProgram* ret = new GLProgram(
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"}\n",
			"#version 330\n"
			"uniform vec3 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = vec4(color, 1.0);\n"
			"}\n");

	staging_program_Position = (*ret)("Position");
	staging_program_mvp = (*ret)["mvp"];
	staging_program_color = (*ret)["color"];

	return ret;
});

// Binding for using staging_program on staging_meshes:
Load<GLVertexArray> staging_binding(LoadTagDefault, []() {
	GLVertexArray* ret = new GLVertexArray(
			GLVertexArray::make_binding(staging_program->program, {
																																{staging_program_Position, staging_meshes->Position},
																														}));
	return ret;
});

StagingMode::StagingMode() {
	DEBUG_PRINT("IN DEBUG MODE");

	sock = nullptr;

	int seed = 10;
	twister.seed(seed);

	std::cout << "seed " << seed << " produces " << rand() << " " << rand() << " " << rand() << std::endl;

	glm::vec3 btnColor = glm::vec3(0.75f, 0.0f, 0.0f);

	Button* btn;
	btn = btns.add("COP", btnColor);
	btn->isEnabled = [&]() {
		return !state->starting && state->player && state->player->role != StagingState::Role::COP;
	};
	btn->onFire = [&]() {
		sock->writeQueue.enqueue(Packet::pack(MessageType::STAGING_ROLE_CHANGE, {StagingState::Role::COP}));

		// just accept latency.. state->players[state->playerId].role = StagingState::Role::COP;
	};

	btn = btns.add("ROBBER", btnColor);
	btn->isEnabled = [&]() { return !state->starting && state->player && !state->robber; };
	btn->onFire = [&]() {
		sock->writeQueue.enqueue(Packet::pack(MessageType::STAGING_ROLE_CHANGE, {StagingState::Role::ROBBER}));

		// accepting latency for now.. state->players[state->playerId].role = StagingState::Role::ROBBER;
	};

	startBtn = btns.add("START GAME", btnColor);
	startBtn->isEnabled = [&]() { return state->players.size() >= 2 && state->undecided == 0 && state->robber; };
	startBtn->onFire = [&]() {
		Packet* out;
		if (state->starting) {
			out = Packet::pack(MessageType::STAGING_VETO_START);
		} else {
			out = Packet::pack(MessageType::STAGING_VOTE_TO_START);
		}

		sock->writeQueue.enqueue(out);
	};

	btns.layoutVertical(0.092f, 0.6f);

	state = std::make_unique<StagingState>();
}

extern std::string remoteAddress;
extern std::string port;

// Connect to server
void StagingMode::reset(bool localMultiplayer) {
	if (sock) {
		sock->close();
		delete sock;
	}

	state->robber = nullptr;
	state->player = nullptr;
	state->undecided = 0;
	state->players.clear();
	state->starting = false;

	state->settings.localMultiplayer = localMultiplayer;

	sock = Socket::connect(remoteAddress, port);
}

bool StagingMode::handle_event(SDL_Event const& event, glm::uvec2 const& window_size) {
	static glm::vec2 mouse;

	if (event.type == SDL_MOUSEMOTION) {
		mouse.x = (event.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f;
		mouse.y = (event.motion.y + 0.5f) / window_size.y * -2.0f + 1.0f;

		btns.checkMouseHover(mouse.x, mouse.y);
	}

	if (event.type == SDL_MOUSEBUTTONDOWN) {
		btns.onClick();
	}

	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT) {
			btns.onKey();
		} else if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP ||
							 event.key.keysym.sym == SDLK_LEFT) {
			btns.onPrev();
		} else if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_s ||
							 event.key.keysym.sym == SDLK_DOWN) {
			btns.onNext();
		}
		return true;
	}

	return false;
}

void StagingMode::update(float elapsed) {
	static float retryConnection = 0.0f;
	static float retryConnectionLimit = 4.0f;

	if (!sock) {
		retryConnection += elapsed;
		if (retryConnection > retryConnectionLimit) {
			reset(state->settings.localMultiplayer);
			retryConnection = 0.0f;
		};
		return;
	}

	static int counter = 0;
	counter++;
	if (counter % 600 == 0) {
		DEBUG_PRINT("connected? " << (sock->isConnected() ? "true" : "false"));
	}

	Packet* out;
	while (sock->readQueue.try_dequeue(out)) {
		if (!out) {
			std::cout << "Bad packet from server" << std::endl;
			continue;
		}

		static const std::vector<std::string> names = {"Name1", "Name2", "Name3"};	// temp, auto-naming;

		switch (out->payload.at(0)) {	// message type

			case MessageType::STAGING_PLAYER_CONNECT: {
				const SimpleMessage* msg = SimpleMessage::unpack(out);

				auto player = std::make_unique<StagingState::Client>();
				player->id = msg->id;
				player->role = StagingState::Role::NONE;
				player->name = names[msg->id % names.size()];

				DEBUG_PRINT("Player " << player->name << " joined the game.");

				state->players[msg->id] = std::move(player);
				state->undecided += 1;

				break;
			}

			case MessageType::STAGING_PLAYER_SYNC: {
				// player should only ever get this message once
				// contains player's id and the state of any other connected clients

				auto player = std::make_unique<StagingState::Client>();
				player->id = out->payload[1];
				player->role = StagingState::Role::NONE;
				player->name = names[player->id % names.size()];
				state->player = player.get();
				state->undecided += 1;

				DEBUG_PRINT("Assigned ID " << player->id << " by server");

				state->players[player->id] = std::move(player);

				size_t i = 2;
				while (i < out->payload.size()) {
					auto opponent = std::make_unique<StagingState::Client>();
					opponent->id = out->payload[i];
					opponent->role = static_cast<StagingState::Role>(out->payload[i + 1]);
					opponent->name = names[opponent->id % names.size()];

					if (opponent->role == StagingState::Role::ROBBER) {
						state->robber = opponent.get();
					} else if (opponent->role == StagingState::Role::NONE) {
						state->undecided += 1;
					}

					DEBUG_PRINT("Added client " << opponent->id << " with name " << opponent->name << " and role "
																			<< opponent->role);

					state->players[opponent->id] = std::move(opponent);

					i += 2;
				}

				break;
			}

			case MessageType::STAGING_ROLE_CHANGE: {
				// contains player id, new role

				auto player = state->players[out->payload[1]].get();

				if (state->robber == player && out->payload[2] != StagingState::Role::ROBBER) {
					state->robber = nullptr;
					DEBUG_PRINT("There is no longer a robber");
				}

				if (player->role == StagingState::Role::NONE) {
					state->undecided -= 1;
				}

				player->role = static_cast<StagingState::Role>(out->payload[2]);

				if (player->role == StagingState::Role::ROBBER) {
					state->robber = player;
				}

				DEBUG_PRINT("Client" << (int)out->payload[1] << " role set to" << (int)out->payload[2]);

				break;
			}

			case MessageType::STAGING_ROLE_CHANGE_REJECTION: {
				// contains id of current robber

				if (state->player->role != StagingState::Role::NONE) {
					state->player->role = StagingState::Role::NONE;
					state->undecided += 1;
				}

				DEBUG_PRINT("Client" << (int)out->payload[1] << " is currently the robber, you cannot be");

				// this should already be set through other means but whatever
				state->robber = state->players[out->payload[1]].get();

				break;
			}

			case MessageType::STAGING_VOTE_TO_START: {
				const SimpleMessage* msg = SimpleMessage::unpack(out);
				std::cout << "Player " << state->players[msg->id]->name << " voted to start the game." << std::endl;
				state->starting = true;

				startBtn->label = "VETO START";

				break;
			}

			case MessageType::STAGING_VETO_START: {
				const SimpleMessage* msg = SimpleMessage::unpack(out);
				std::cout << "Player " << state->players[msg->id]->name << " vetoed the game startBtn." << std::endl;
				state->starting = false;

				startBtn->label = "START GAME";

				break;
			}

			case MessageType::STAGING_START_GAME: {
				// contains seed

				state->settings.seed = out->payload[1];
				enterGame(sock, std::move(state));	// TODO: make sock unique_ptr and move it here

				break;
			}

			default: {
				/*std::cout << "Recieved unknown staging message type: " << out->payload.at(0) <<  std::endl;
				std::cout << "Contents from server: ";
				for (const auto& thing : out->payload) {
					printf("%x ", thing);
				}
				std::cout << std::endl;*/

				break;
			}
		}

		delete out;
	}
}
void StagingMode::draw(glm::uvec2 const& drawable_size) {
	// TODO: message queue with timeouts, relatively simple?
	if (!sock || !state->player) {
		drawWord("NOT CONNECTED", 0.0f, -0.92f, 1.0f);
	} else {
		drawWord("CONNECTED", 0.0f, -0.92f, 1.0f);

		switch (state->player->role) {
			case StagingState::Role::ROBBER: {
				drawWord("ROBBER SELECTED", 0.0f, 0.88f, 1.0f);
				break;
			}

			case StagingState::Role::COP: {
				drawWord("COP SELECTED", 0.0f, 0.88f, 1.0f);
				break;
			}

			case StagingState::Role::NONE: {
				drawWord("SELECT A ROLE", 0.0f, 0.88f, 1.0f);
				break;
			}
		}

		btns.draw();
	}
}
