#include "GameMode.hpp"
#include "MenuMode.hpp"
#include "Socket.hpp"
#include "StagingMode.hpp"
#include "load_save_png.hpp"

#include "GL.hpp"
#include "Load.hpp"
#include "Mode.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include "../shared/Server.hpp"
#include "Sounds.h"
#include "ui/Button.hpp"

std::string remoteAddress;
std::string port;
unsigned crowdSize;

GLuint tex;
GLuint vao;
GLuint buffer;

// shader program:
GLuint program = 0;
GLuint program_Position = 0;
GLuint program_TexCoord = 0;
GLuint program_Color = 0;
GLuint program_mvp = 0;
GLuint program_tex = 0;

/*
static GLuint compile_shader(GLenum type, std::string const& source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);
*/
int main(int argc, char** argv) {
	// Sound::init(argc, argv);

	std::string title = "Odd One Out";

	struct {
		int width = 720;
		int height = 480;
		int minWidth = 720 / 10;
		int minHeight = 480 / 10;
	} config;

	remoteAddress = (argc > 1) ? argv[1] : "localhost";
	port = (argc > 2) ? argv[2] : "3490";
	crowdSize = (argc > 3) ? std::stoi(argv[3]) : 200;

	//------------  initialization ------------

	// Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	// Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	// create window:
	SDL_Window* window =
			SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config.width, config.height,
											 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	SDL_SetWindowMinimumSize(window, config.minWidth, config.minHeight);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	// Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

#ifdef _WIN32
	// On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
#endif

	// Hide mouse cursor (note: showing can be useful for debugging):
	// SDL_ShowCursor(SDL_DISABLE);

	//------------ load all assets -----------
	call_load_functions();
	/*
	{	// compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
																					"#version 330\n"
																					"uniform mat4 mvp;\n"
																					"in vec4 Position;\n"
																					"in vec2 TexCoord;\n"
																					"in vec4 Color;\n"
																					"out vec2 texCoord;\n"
																					"out vec4 color;\n"
																					"void main() {\n"
																					"	gl_Position = mvp * Position;\n"
																					"	color = Color;\n"
																					"	texCoord = TexCoord;\n"
																					"}\n");

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
																						"#version 330\n"
																						"uniform sampler2D tex;\n"
																						"in vec4 color;\n"
																						"in vec2 texCoord;\n"
																						"out vec4 fragColor;\n"
																						"void main() {\n"
																						"	fragColor = texture(tex, texCoord) * color;\n"
																						"}\n");

		program = link_program(fragment_shader, vertex_shader);

		// look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U)
			throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1U)
			throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U)
			throw std::runtime_error("no attribute named Color");

		// look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U)
			throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1U)
			throw std::runtime_error("no uniform named tex");
	}

	// vertex buffer:
	buffer = 0;
	{	// create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	// vertex array object:
	vao = 0;
	{	// create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE,
													sizeof(glm::vec2) + sizeof(glm::vec2) + sizeof(glm::u8vec4), (GLbyte*)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE,
													sizeof(glm::vec2) + sizeof(glm::vec2) + sizeof(glm::u8vec4), (GLbyte*)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
													sizeof(glm::vec2) + sizeof(glm::vec2) + sizeof(glm::u8vec4),
													(GLbyte*)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	// load png from game 1 code
	tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0, 0);
	{	// load texture 'tex':
		std::vector<uint32_t> data;
		if (!load_png("ui.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		} else {
			std::cout << "loaded ui.png with size " << tex_size.x << " " << tex_size.y << " " << data[0] << " " << data[1]
								<< std::endl;
		}
		// create a texture object:
		glGenTextures(1, &tex);
		// bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		// upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		// set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}*/

	//------------ set up modes -----------
	std::shared_ptr<GameMode> game = std::make_shared<GameMode>();
	std::shared_ptr<StagingMode> staging = std::make_shared<StagingMode>();
	std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

	glm::vec3 color = {0.5f, 1.0f, 0.0f};

	Button* btn;
	btn = menu->menuButtons.add("PLAY LOCAL", color);
	btn->isEnabled = []() { return false; };
	btn->onFire = [&]() {
		std::thread([]() { Server server; }).detach();

		// wait for server to start accepting connections
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		staging->reset(true);
		Mode::set_current(staging);
	};

	btn = menu->menuButtons.add("PLAY ONLINE", color);
	btn->isEnabled = []() { return true; };
	btn->onFire = [&]() {
		staging->reset(false);
		Mode::set_current(staging);
	};

	btn = menu->menuButtons.add("QUIT", color);
	btn->isEnabled = []() { return true; };
	btn->onFire = [&]() { Mode::set_current(nullptr); };

	menu->menuButtons.layoutVertical(0.092f, 0.65f, -0.2f);

	staging->enterGame = [&](Socket* sock, std::unique_ptr<StagingMode::StagingState> stagingState) {
		game->sock = sock;
		game->reset(std::move(stagingState));
		Mode::set_current(game);
	};
	staging->showMenu = [&]() {};

	std::string startingMode = argc > 4 ? argv[4] : "";
	std::string position = argc > 5 ? argv[5] : "";

	std::cout << "Starting mode: " << startingMode << std::endl;
	if (startingMode == "staging") {
		staging->reset(false);
		Mode::set_current(staging);

		if (position == "robber") {
			SDL_SetWindowPosition(window, 0, 0);
		} else {
			SDL_SetWindowPosition(window, config.width, 0);
		}

		std::thread setup([&]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			if (position == "robber") {
				// staging->robberBtn.onFire();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				// staging->startBtn.onFire();
			} else {
				// staging->copBtn.onFire();
			}
		});
		setup.detach();

	} else if (startingMode == "game") {
		// TODO: this isn't connected to the server
		// game->reset();
		// Mode::set_current(game);
	} else {
		Mode::set_current(menu);
	}

	//------------ game loop ------------

	glm::uvec2 window_size, drawable_size;
	auto on_resize = [&]() {
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		window_size = glm::uvec2(w, h);
		SDL_GL_GetDrawableSize(window, &w, &h);
		drawable_size = glm::uvec2(w, h);
		glViewport(0, 0, drawable_size.x, drawable_size.y);
	};
	on_resize();

	typedef std::chrono::duration<int, std::ratio<1, 60>> frameDuration;
	auto delta = frameDuration(1);
	float dt = 1.0f / 60.0f;
	while (Mode::current) {
		auto startTime = std::chrono::steady_clock::now();

		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			// handle resizing:
			if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				on_resize();
			}
			// handle input:
			if (Mode::current && Mode::current->handle_event(evt, window_size)) {
				// mode handled it; great
			} else if (evt.type == SDL_QUIT) {
				Mode::set_current(nullptr);
				break;
			}
		}
		if (!Mode::current)
			break;

		Mode::current->update(dt);
		if (!Mode::current)
			break;

		// draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		Mode::current->draw(drawable_size);

		SDL_GL_SwapWindow(window);

		std::this_thread::sleep_until(startTime + delta);
	}

	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;
	// Sound::cleanup();

	return 0;
}
/*
static GLuint compile_shader(GLenum type, std::string const& source) {
	GLuint shader = glCreateShader(type);
	GLchar const* str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
*/