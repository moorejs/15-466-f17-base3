#pragma once

#include "Mode.hpp"
#include "Random.hpp"
#include "Socket.hpp"
#include "ui/Button.hpp"

#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

struct GameSettings {
	float POWER_TIMEOUT = 15.0f;

	int seed;
	bool localMultiplayer;

	bool clientSidePrediction = false;
};

struct StagingMode : public Mode {
	StagingMode();
	virtual ~StagingMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	/* Closes existing connection to the server, reset state, and connects to server */
	void reset();

	Socket* sock;

	std::mt19937 twister;
	UniformRealDistribution<float> dist;
	std::function<float()> rand = [&]() -> float { return dist(twister); };

	struct StagingState {
		enum Role {
			NONE,
			ROBBER,
			COP,
		};

		struct Client {
			int id;
			std::string name;
			Role role;
		};

		Client* player;	// no ownership
		Client* robber;	// no ownership
		int undecided;
		std::unordered_map<int, std::unique_ptr<Client>> players;
		bool starting;

	} stagingState;

	Button robberBtn;
	Button copBtn;
	Button startBtn;
	std::vector<Button*> buttons;

	std::function<void(Socket*, int)> enterGame;
	std::function<void()> showMenu;
};
