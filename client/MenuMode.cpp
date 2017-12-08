#include "MenuMode.hpp"

#include "GLProgram.hpp"
#include "GLVertexArray.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"

#include "DrawTex.cpp"

#include <cmath>
#include <functional>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

//---------- resources ------------
Load<MeshBuffer> menuMeshes(LoadTagInit, []() { return new MeshBuffer("menu.p"); });

std::function<void(const std::string&, float, float, float)> drawWord;

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

DrawTex drawT;

//----------------------
MenuMode::MenuMode() {
    
    
    drawT.loadTexture();
    
	drawWord = [](const std::string& word, float x, float y, float fontSize) {
		glUseProgram(menuProgram->program);
		glBindVertexArray(menuBinding->array);

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

		float total_width = 0.0f;
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0)
				total_width += spacing(word[i - 1], word[i]);
			total_width += width(word[i]) * 0.75f;
		}

		static const float height = 1.0f;
		y += -0.5f * 0.1 * height;									// center y
		x += -0.5f * total_width * 0.1f * 0.3333f;	// center x
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0) {
				x += spacing(word[i], word[i - 1]) * 0.1f * 0.3333f;
			}

			if (word[i] != ' ') {
				float s = fontSize * 0.1f * (1.0f / 3.0f);
				glm::mat4 mvp = glm::mat4(glm::vec4(s, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, s, 0.0f, 0.0f),
																	glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(x, y, 0.0f, 1.0f));
				glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(menuProgramColor, 1.0f, 1.0f, 1.0f);

				MeshBuffer::Mesh const& mesh = menuMeshes->lookup(word.substr(i, 1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(word[i]) * 0.75f * 0.1f * 0.3333f;
		}
	};
}

bool MenuMode::handle_event(SDL_Event const& event, glm::uvec2 const& window_size) {
	static glm::vec2 mouse;

	if (event.type == SDL_MOUSEMOTION) {
		mouse.x = (event.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f;
		mouse.y = (event.motion.y + 0.5f) / window_size.y * -2.0f + 1.0f;

		menuButtons.checkMouseHover(mouse.x, mouse.y);
	}

	if (event.type == SDL_MOUSEBUTTONDOWN) {
		menuButtons.onClick();
	}

	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT) {
			menuButtons.onKey();
		} else if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP ||
							 event.key.keysym.sym == SDLK_LEFT) {
			menuButtons.onPrev();
		} else if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_s ||
							 event.key.keysym.sym == SDLK_DOWN) {
			menuButtons.onNext();
		}
		return true;
	}
	return false;
}

void MenuMode::update(float elapsed) {
	bounce += elapsed / 0.7f;
	bounce -= std::floor(bounce);
}

void MenuMode::draw(glm::uvec2 const& drawable_size) {
	/*float aspect = drawable_size.x / float(drawable_size.y);
	// scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(1.0f / aspect, 1.0f);
	glm::mat4 projection = glm::mat4(glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
																	 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
*/
	float total_height = 0.0f;
	for (auto const& choice : choices) {
		total_height += choice.height + 2.0f * choice.padding;
	}
    
    
    menuButtons.draw();
//    drawWord("ODD ONE OUT", 0.0f, 0.6f, 1.0f);
    drawT.drawtexture();
    
	
    
    
	
    
    
}
