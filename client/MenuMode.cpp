#include "MenuMode.hpp"

#include "GLProgram.hpp"
#include "GLVertexArray.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

//---------- resources ------------
Load<MeshBuffer> menuMeshes(LoadTagInit, []() { return new MeshBuffer("menu.p"); });

// Attrib locations in menu_program:
GLint menuProgramPosition = -1;
// Uniform locations in menu_program:
GLint menuProgramMVP = -1;
GLint menuProgramColor = -1;

// Menu program itself:
Load<GLProgram> menuProgram(LoadTagInit, []() {
	GLProgram* ret = new GLProgram(
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"}\n",
			"#version 330\n"
			"uniform vec3 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = vec4(color, 1.0);\n"
			"}\n");

	menuProgramPosition = (*ret)("Position");
	menuProgramMVP = (*ret)["mvp"];
	menuProgramColor = (*ret)["color"];

	return ret;
});

// Binding for using menu_program on menu_meshes:
Load<GLVertexArray> menuBinding(LoadTagDefault, []() {
	GLVertexArray* ret = new GLVertexArray(
			GLVertexArray::make_binding(menuProgram->program, {
																														{menuProgramPosition, menuMeshes->Position},
																												}));
	return ret;
});

//----------------------
MenuMode::MenuMode() {}

bool MenuMode::handle_event(SDL_Event const& e, glm::uvec2 const& window_size) {
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_ESCAPE) {
			Mode::set_current(nullptr);
			return true;
		} else if (e.key.keysym.sym == SDLK_UP) {
			menuButtons.onPrev();
			return true;
		} else if (e.key.keysym.sym == SDLK_DOWN) {
			menuButtons.onNext();
			return true;
		} else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
			menuButtons.onKey();
			return true;
		}
	}
	return false;
}

void MenuMode::update(float elapsed) {
	bounce += elapsed / 0.7f;
	bounce -= std::floor(bounce);
}

void MenuMode::draw(glm::uvec2 const& drawable_size) {
	float aspect = drawable_size.x / float(drawable_size.y);
	// scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(1.0f / aspect, 1.0f);
	glm::mat4 projection = glm::mat4(glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
																	 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	float total_height = 0.0f;
	for (auto const& choice : choices) {
		total_height += choice.height + 2.0f * choice.padding;
	}

	menuButtons.draw();

	glUseProgram(menuProgram->program);
	glBindVertexArray(menuBinding->array);

	// character width and spacing helpers:
	// (...in terms of the menu font's default 3-unit height)
	auto width = [](char a) {
		if (a == 'I')
			return 1.0f;
		else if (a == 'L')
			return 2.0f;
		else if (a == 'M' || a == 'W')
			return 4.0f;
		else
			return 3.0f;
	};
	auto spacing = [](char a, char b) { return 1.0f; };

	float select_bounce = std::abs(std::sin(bounce * 3.1515926f * 2.0f));

	float y = 0.5f * total_height;
	for (auto const& choice : choices) {
		y -= choice.padding;
		y -= choice.height;

		bool is_selected = (&choice - &choices[0] == selected);
		std::string label = choice.label;

		if (is_selected) {
			label = "*" + label + "*";
		}

		float total_width = 0.0f;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0)
				total_width += spacing(label[i - 1], label[i]);
			total_width += width(label[i]);
		}
		if (is_selected) {
			total_width += 2.0f * select_bounce;
		}

		float x = -0.5f * total_width;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0)
				x += spacing(label[i - 1], label[i]);
			if (is_selected && (i == 1 || i + 1 == label.size())) {
				x += select_bounce;
			}

			if (label[i] != ' ') {
				float s = choice.height * (1.0f / 3.0f);
				glm::mat4 mvp = projection * glm::mat4(glm::vec4(s, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, s, 0.0f, 0.0f),
																							 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(s * x, y, 0.0f, 1.0f));
				glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(menuProgramColor, 1.0f, 1.0f, 1.0f);

				MeshBuffer::Mesh const& mesh = menuMeshes->lookup(label.substr(i, 1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(label[i]);
		}

		y -= choice.padding;
	}
}
