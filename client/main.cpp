#include "GameMode.hpp"
#include "MenuMode.hpp"
#include "Socket.hpp"
#include "StagingMode.hpp"

#include "GL.hpp"
#include "Load.hpp"
#include "Mode.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include "../shared/Server.hpp"
#include "Sounds.h"
#include "ui/Button.hpp"

int main(int argc, char** argv) {
	// Sound::init(argc, argv);

	std::string title = "Odd One Out";

	struct {
		int width = 720;
		int height = 480;
		int minWidth = 720 / 10;
		int minHeight = 480 / 10;
	} config;

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

	//------------ set up modes -----------
	std::shared_ptr<GameMode> game = std::make_shared<GameMode>();
	std::shared_ptr<StagingMode> staging = std::make_shared<StagingMode>();
	std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

	Button* btn;
	btn = menu->menuButtons.add("playLocal", {0.0f, 1.0f, 0.0f});
	btn->isEnabled = []() { return true; };
	btn->onFire = [&]() {
		std::thread([]() { Server server; }).detach();

		// wait for server to start accepting connections
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		staging->reset(true);
		Mode::set_current(staging);
	};

	menu->menuButtons.layoutVertical(0.092f, 1.0f);

	menu->choices.emplace_back("ODD ONE OUT");
	menu->choices.emplace_back("PLAY LOCAL", [&](MenuMode::Choice&) {
		std::thread([]() { Server server; }).detach();

		// wait for server to start accepting connections
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		staging->reset(true);
		Mode::set_current(staging);
	});
	menu->choices.emplace_back("PLAY ONLINE", [&](MenuMode::Choice&) {
		staging->reset(false);
		Mode::set_current(staging);
	});
	menu->choices.emplace_back("QUIT", [&](MenuMode::Choice&) { Mode::set_current(nullptr); });
	menu->selected = 1;

	staging->enterGame = [&](Socket* sock, std::unique_ptr<StagingMode::StagingState> stagingState) {
		game->sock = sock;
		game->reset(std::move(stagingState));
		Mode::set_current(game);
	};
	staging->showMenu = [&]() {
		menu->choices[0].label = "ODD ONE OUT";
		menu->choices[1].label = "MULTIPLAYER";
		menu->selected = 2;
		Mode::set_current(menu);
	};

	std::string startingMode = argc > 1 ? argv[1] : "";
	std::string position = argc > 2 ? argv[2] : "";

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
				staging->robberBtn.onFire();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				staging->startBtn.onFire();
			} else {
				staging->copBtn.onFire();
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
