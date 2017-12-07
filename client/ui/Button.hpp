#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../GLProgram.hpp"
#include "../GLVertexArray.hpp"
#include "../Load.hpp"
#include "../MeshBuffer.hpp"

/*extern GLuint vao;
extern GLuint buffer;
extern GLuint program;
extern GLuint program_Position;
extern GLuint program_TexCoord;
extern GLuint program_Color;
extern GLuint program_mvp;
extern GLuint program_tex;

extern GLuint tex;*/

extern Load<MeshBuffer> menuMeshes;
extern Load<GLProgram> menuProgram;
extern Load<GLVertexArray> menuBinding;
extern GLint menuProgramPosition;
extern GLint menuProgramMVP;
extern GLint menuProgramColor;
extern std::function<void(const std::string&, float, float, float)> drawWord;

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

	void draw(float size) {
		static int frames = 0;
		/*
		struct Vertex {
			Vertex(glm::vec2 const& Position_, glm::vec2 const& TexCoord_, glm::u8vec4 const& Color_)
					: Position(Position_), TexCoord(TexCoord_), Color(Color_) {}
			glm::vec2 Position;
			glm::vec2 TexCoord;
			glm::u8vec4 Color;
		};

		std::vector<Vertex> verts;

		static auto draw_sprite = [&verts](glm::vec2 const& min_uv, glm::vec2 const& max_uv, glm::vec2 const& rad,
																			 glm::vec2 const& at, float angle = 0.0f) {
			glm::u8vec4 tint = glm::u8vec4(0x7f, 0x7f, 0x7f, 0x7f);
			glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
			glm::vec2 up = glm::vec2(-right.y, right.x);

			verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
			verts.emplace_back(verts.back());
			verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
			verts.emplace_back(at + right * rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
			verts.emplace_back(at + right * rad.x + up * rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
			verts.emplace_back(verts.back());
		};
		// top left corner
		draw_sprite({0.0f, 0.0f}, {1.0f, 1.0f}, rad + glm::vec2(-0.05f, 0.0f), pos);

		glUseProgram(program);

		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

		glBindTexture(GL_TEXTURE_2D, tex);
		glBindVertexArray(vao);

		glUniform1i(program_tex, tex);

		glm::mat4 mvp = glm::mat4(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
															glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

		glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());*/

		static const MeshBuffer::Mesh& rect = menuMeshes->lookup("Button");

		glm::vec3 drawColor = color;

		glm::vec2 outlineSize = glm::vec2(0.005f, 0.0075f);
		if (selected) {
			frames++;
			outlineSize += outlineSize.x / 2.0f * std::sin(1.5f * (frames / 60.0f) * 2.0f * 3.14159f) + outlineSize.x / 2.0f;
			drawColor *= 1.25f;
		}
		if (!isEnabled()) {
			drawColor *= 0.15f;
		}

		glm::mat4 mvp =
				glm::mat4(glm::vec4(rad.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, rad.y, 0.0f, 0.0f),
									glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(pos.x, pos.y, -0.05f, 1.0f)	// z is back to show text
				);
		glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniform3f(menuProgramColor, drawColor.x, drawColor.y, drawColor.z);
		glDrawArrays(GL_TRIANGLES, rect.start, rect.count);

		mvp = glm::mat4(glm::vec4(rad.x + outlineSize.x, 0.0f, 0.0f, 0.0f),
										glm::vec4(0.0f, rad.y + outlineSize.y, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
										glm::vec4(pos.x, pos.y, -0.05f, 1.0f)	// z is back to show text
		);
		glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniform3f(menuProgramColor, 1.0f, 1.0f, 1.0f);
		glDrawArrays(GL_TRIANGLES, rect.start, rect.count);

		drawWord(label, pos.x, pos.y, size);
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

		setSelected(0);
		firePrev = [&](int selected) -> int {
			if (selected == 0) {
				return buttons.size() - 1;
			}
			return selected - 1;
		};
		fireNext = [&](int selected) -> int { return (selected + 1) % buttons.size(); };
	};

	void layoutVertical(float height, float width, float baseY = 0.0f, float padding = 0.05f) {
		float overallHeight = (height + padding) * buttons.size();
		float y = baseY + overallHeight / 2.0f + padding;

		for (auto& button : buttons) {
			button->rad = {width, height};
			button->pos = {0.0f, y};
			y -= 2 * height + padding;
		}

		setSelected(0);
		firePrev = [&](int selected) -> int {
			if (selected == 0) {
				return buttons.size() - 1;
			}
			return selected - 1;
		};
		fireNext = [&](int selected) -> int { return (selected + 1) % buttons.size(); };
	}

	void checkMouseHover(float x, float y) {
		for (int i = 0; i < (int)buttons.size(); i++) {
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
		if (buttons[selected]->isEnabled()) {
			buttons[selected]->onFire();
		}
	}

	void onPrev() { setSelected(firePrev(selected)); }
	void onNext() { setSelected(fireNext(selected)); }

	void draw(float fontSize = 1.0f) {
		glUseProgram(menuProgram->program);
		glBindVertexArray(menuBinding->array);

		for (auto& button : buttons) {
			button->draw(fontSize);
		}
	}

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
