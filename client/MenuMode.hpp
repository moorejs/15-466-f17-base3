#pragma once

#include "Mode.hpp"
#include "ui/Button.hpp"

#include <functional>
#include <string>
#include <vector>

struct MenuMode : public Mode {
	MenuMode();
	virtual ~MenuMode() {}

	virtual bool handle_event(SDL_Event const& event, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	ButtonGroup menuButtons;

	struct Choice {
		Choice(std::string const& label_, std::function<void(Choice&)> on_select_ = nullptr)
				: label(label_), on_select(on_select_) {}
		std::string label;
		std::function<void(Choice&)> on_select;
		// height / padding give item height and padding relative to a screen of height 2:
		float height = 0.1f;
		float padding = 0.01f;
	};
	std::vector<Choice> choices;
	uint32_t selected = 0;
	float bounce = 0.0f;
};
