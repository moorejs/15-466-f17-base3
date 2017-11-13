#pragma once

#include "Mode.hpp"

#include "Random.hpp"
#include "Scene.hpp"
#include "Socket.hpp"
#include "ui/Button.hpp"

#include <functional>
#include <random>

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	void reset(int seed);	// reset the game state

	Scene::Object* addObject(std::string const& name,
													 glm::vec3 const& position,
													 glm::quat const& rotation,
													 glm::vec3 const& scale);

	std::mt19937 twister;
	UniformRealDistribution<float> dist;
	std::function<float()> rand = [&]() -> float { return dist(twister); };

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

	struct {
		float radius = 10.0f;
		float elevation = 0.75f;
		float azimuth = 3.1f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;

	bool did_end = false;

	Button activateSnapshot;

	Socket* sock;
};
