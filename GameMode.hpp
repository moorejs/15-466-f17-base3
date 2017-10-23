#pragma once

#include "Mode.hpp"

#include "Scene.hpp"
#include "Socket.hpp"

#include <functional>

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	void reset();	// reset the game state

	// scene + references into scene for objects:
	Scene scene;

	// controls:
	struct Controls {
		SDL_Scancode left_forward;
		SDL_Scancode left_backward;
		SDL_Scancode right_forward;
		SDL_Scancode right_backward;
	};
	Controls diamond_controls = {SDL_SCANCODE_A, SDL_SCANCODE_Z, SDL_SCANCODE_S, SDL_SCANCODE_X};
	Controls solid_controls = {SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_PERIOD, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_SLASH};

	// function to call when 'ESC' is pressed:
	std::function<void()> show_menu;

	struct {
		float radius = 10.0f;
		float elevation = 0.75f;
		float azimuth = 3.1f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;

	bool did_end = false;

	// functions to call when game ends:
	std::function<void()> diamonds_wins;
	std::function<void()> solids_wins;
	std::function<void()> everyone_loses;

	Socket* sock;
};
