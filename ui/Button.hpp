#pragma once

#include <functional>
#include <string>

#include <glm/glm.hpp>

struct Button {
	glm::vec2 pos;
	glm::vec2 rad;
	std::string label;
	glm::vec3 color;
	bool hover = false;

	std::function<bool()> isEnabled;
	std::function<void()> onFire;

	bool contains(const glm::vec2& point) const {
		return point.x > pos.x - rad.x && point.x < pos.x + rad.x && point.y > pos.y - rad.y && point.y < pos.y + rad.y;
	}
};