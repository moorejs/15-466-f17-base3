#include "StagingMode.hpp"

#include "Load.hpp"
#include "GLProgram.hpp"
#include "GLVertexArray.hpp"
#include "MeshBuffer.hpp"

#include <glm/gtc/type_ptr.hpp>

Load<MeshBuffer> staging_meshes(LoadTagInit, [](){
	return new MeshBuffer("menu.p");
});

//Attrib locations in staging_program:
GLint staging_program_Position = -1;
//Uniform locations in staging_program:
GLint staging_program_mvp = -1;
GLint staging_program_color = -1;

//Menu program itself:
Load<GLProgram> staging_program(LoadTagInit, [](){
	GLProgram *ret = new GLProgram(
		"#version 330\n"
		"uniform mat4 mvp;\n"
		"in vec4 Position;\n"
		"void main() {\n"
		"	gl_Position = mvp * Position;\n"
		"}\n"
	,
		"#version 330\n"
		"uniform vec3 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = vec4(color, 1.0);\n"
		"}\n"
	);

	staging_program_Position = (*ret)("Position");
	staging_program_mvp = (*ret)["mvp"];
	staging_program_color = (*ret)["color"];

	return ret;
});

//Binding for using staging_program on staging_meshes:
Load<GLVertexArray> staging_binding(LoadTagDefault, [](){
	GLVertexArray *ret = new GLVertexArray(GLVertexArray::make_binding(staging_program->program, {
		{staging_program_Position, staging_meshes->Position},
	}));
	return ret;
});

StagingMode::StagingMode()
{
	starting = false;
	sock = nullptr;

	int seed = 10;
	twister.seed(seed);

	std::cout << "seed " << seed << " produces " << rand() << " " << rand() << " " << rand() << std::endl;
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
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (!sock) {
			reset();
		}

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
	static float retryConnection = 0.0f;
	static float retryConnectionLimit = 2.0f;
	if (!sock) {
		retryConnection += elapsed;
		if (retryConnection > retryConnectionLimit) {
			reset();
			retryConnection = 0.0f;
			std::cout << "trying to reconnect" << std::endl;
		};
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
void StagingMode::draw(glm::uvec2 const& drawable_size) {
	float aspect = drawable_size.x / float(drawable_size.y);
	//scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(1.0f / aspect, 1.0f);
	glm::mat4 projection = glm::mat4(
		glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);

	glUseProgram(staging_program->program);
	glBindVertexArray(staging_binding->array);

	// centered for now
	// TODO: could cache parts of this?
	static auto draw_word = [&projection](const std::string& word, float y) {
		//character width and spacing helpers:
		// (...in terms of the menu font's default 3-unit height)
		auto width = [](char a) {
			if (a == 'I') return 1.0f;
			else if (a == 'L') return 2.0f;
			else if (a == 'M' || a == 'W') return 4.0f;
			else return 3.0f;
		};
		auto spacing = [](char a, char b) {
			return 1.0f;
		};

		float total_width = 0.0f;
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0) total_width += spacing(word[i-1], word[i]);
			total_width += width(word[i]);
		}

		float x = -0.5f * total_width;
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0) {
				x += spacing(word[i], word[i-1]);
			}

			if (word[i] != ' ') {
				float s = 0.1f * (1.0f / 3.0f);
				glm::mat4 mvp = projection * glm::mat4(
					glm::vec4(s, 0.0f, 0.0f, 0.0f),
					glm::vec4(0.0f, s, 0.0f, 0.0f),
					glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
					glm::vec4(s * x, y, 0.0f, 1.0f)
				);
				glUniformMatrix4fv(staging_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(staging_program_color, 1.0f, 1.0f, 1.0f);

				MeshBuffer::Mesh const &mesh = staging_meshes->lookup(word.substr(i, 1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(word[i]);
		}
	};

	// TODO: message queue with timeouts, relatively simple?
	if (!sock) {
		draw_word("NOT CONNECTED TO SERVER", -0.95f);
	} else {
		draw_word("CONNECTED TO SERVER", -0.95f);

		if (!starting) {
			draw_word("CLICK TO START", 0.0f);
		} else {
			draw_word("CLICK TO VETO START", 0.0f);
			draw_word("STARTING IN FIVE SECONDS", 0.3f);
		}
	}
}