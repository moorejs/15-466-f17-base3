#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "../MeshBuffer.hpp"

struct Button {
	glm::vec2 pos;
	glm::vec2 rad;
	std::string label;
	glm::vec3 color;
	bool hover = false;
	bool selected = false;

	std::function<bool()> isEnabled;
	std::function<void()> onFire;

	bool contains(const glm::vec2& point) const {
		return point.x > pos.x - rad.x && point.x < pos.x + rad.x && point.y > pos.y - rad.y && point.y < pos.y + rad.y;
	}
};

class ButtonGroup {
 public:
	ButtonGroup() : selected(0) {}

	Button* add(const std::string& label, const glm::vec3 color) {
		buttons.emplace_back(std::make_unique<Button>());

		buttons.back()->label = label;
		buttons.back()->color = color;

		return buttons.back().get();
	};

	// center's buttons that take the full width of the screen
	void layoutHorizontal(float height, float y, float padding = 0.02f) {
		float fullWidth = 2.0f / buttons.size() - 2.0f * padding;
		float width = fullWidth / 2.0f;	// width from center

		float x = -1.0f + width + padding;
		for (auto& button : buttons) {
			button->rad = {width, height};
			button->pos = {x, y};
			x += fullWidth + padding;
		}

		firePrev = [&](int selected) -> int {
			if (selected == 0) {
				return buttons.size() - 1;
			}
			return selected - 1;
		};
		fireNext = [&](int selected) -> int { return (selected + 1) % buttons.size(); };
	};

	void layoutVertical(float height, float width, float padding = 0.02f) {
		float overallHeight = (height + padding) * buttons.size();
		float y = 0.0f - overallHeight / 2.0f + padding;

		for (auto& button : buttons) {
			button->rad = {width, height};
			button->pos = {0.0f, y};
			y += height + padding;
		}
		firePrev = [&](int selected) -> int {
			if (selected == 0) {
				return buttons.size() - 1;
			}
			return selected - 1;
		};
		fireNext = [&](int selected) -> int { return (selected + 1) % buttons.size(); };
	}

	void checkMouseHover(float x, float y) {
		for (int i = 0; i < buttons.size(); i++) {
			if (buttons[i]->contains({x, y})) {
				buttons[i]->hover = true;
				setSelected(i);
			} else {
				buttons[i]->hover = false;
			}
		}
	}

	// fires onFire if enabled and selected/hovered
	void onClick() {
		for (auto& button : buttons) {
			if (button->hover) {
				if (button->isEnabled()) {
					button->onFire();
				}
				return;
			}
		}
	}

	void onKey() {
		// if no button is hovered
		buttons[selected]->onFire();
	}

	void onPrev() { setSelected(firePrev(selected)); }
	void onNext() { setSelected(fireNext(selected)); }

	void draw(const MeshBuffer::Mesh& mesh, GLint mvp, GLint color) {}

	std::vector<std::unique_ptr<Button>> buttons;

	std::function<int(int)> fireNext;
	std::function<int(int)> firePrev;

 private:
	void setSelected(int val) {
		buttons[selected]->selected = false;
		selected = val;
		buttons[selected]->selected = true;
	}

	int selected;
};