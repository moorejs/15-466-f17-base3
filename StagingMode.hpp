#pragma once

#include "Mode.hpp"
#include "Socket.hpp"
#include "Random.hpp"

#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

struct StagingMode : public Mode {
	StagingMode();
	virtual ~StagingMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	void reset();

	Socket* sock;

	std::mt19937 twister;
	UniformRealDistribution<float> dist;
	std::function<float()> rand = [&]() -> float {
		return dist(twister);
	};

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

		Client* player = nullptr; // no ownership
		Client* robber = nullptr; // no ownership
		int undecided = 0;
		std::unordered_map<int, std::unique_ptr<Client>> players;

	} stagingState;

	// temp, will move this
	struct Button {
		glm::vec2 pos;
		glm::vec2 rad;
		std::string label;
		glm::vec3 color;
		bool hover = false;

		std::function<bool()> isEnabled;
		std::function<void()> onFire;

		bool contains(const glm::vec2& point) const {
			return point.x > pos.x - rad.x && point.x < pos.x + rad.x
			 	&& point.y > pos.y - rad.y && point.y < pos.y + rad.y;
		}
	};
	std::vector<Button> buttons;

	bool starting;

	std::function<void(Socket*)> enterGame;
	std::function<void()> show_menu;
};
