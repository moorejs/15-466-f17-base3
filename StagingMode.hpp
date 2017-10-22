#pragma once

#include "Mode.hpp"
#include "Socket.hpp"

struct StagingMode : public Mode {
	StagingMode();
	virtual ~StagingMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	void reset();

	Socket* sock;

	bool starting;

	std::function<void(Socket*)> enterGame;
	std::function<void()> show_menu;
};
