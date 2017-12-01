#include "GameMode.hpp"
#include "Draw.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include "GLProgram.hpp"
#include "GLVertexArray.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "read_chunk.hpp"

#include <algorithm>
#include <fstream>

#include <glm/gtc/type_ptr.hpp>

#include "../shared/Collisions.h"
#include "../shared/person.h"
#include "Sounds.h"

bool isSnapshotOn = false;
bool isTestimonyShowing = false;
float randX;
float randY;
std::string testimony_text = "";
SDL_TimerID snapshot_timer;
SDL_TimerID reset_snapshot_timer;
SDL_TimerID testimony_timer;
SDL_TimerID reset_testimony_timer;
SDL_TimerID reset_scale_timer;
time_t game_start_time;
float game_duration = 120;

float player_move_speed = 0.1f;

Uint32 snapshot_delay = 20000;
Uint32 testimony_delay = 12000;
Uint32 testimony_reset_delay = 4000;
Uint32 snapshot_reset_delay = 4000;
Collision collisionFramework = Collision(BBox2D(glm::vec2(-1.92, -7.107), glm::vec2(6.348, 9.775)));

// Attrib locations in staging_program:
GLint word_program_Position = -1;
// Uniform locations in staging_program:
GLint word_program_mvp = -1;
GLint word_program_color = -1;	// color
Person player;

Person cop;

std::string endGameTxt = "";

bool gameResultPosted;

bool gameEnded = false;

bool stealSuccess = false;

static glm::vec2 mouse = glm::vec2(0.0f, 0.0f);

std::vector<std::string> random_testimonies = {
		"IN A RED SHIRT", "WITH BROWN HAIR", "WITH BLACK HAIR", "WITH BLUE SHOES", "IN A GRAY SUIT", "WITH A BLACK WATCH",
		"ON THE ROAD",		"WITH A HAT",			 "NEAR THE STORE",	"WITH BLOND HAIR", "WITH GLASSES"};

Uint32 resetSnapShot(Uint32 interval, void* param) {
	isSnapshotOn = false;

	if (!gameEnded) {
		Person::unfreezeAll();
		player.isMoving = true;
	}

	SDL_RemoveTimer(reset_snapshot_timer);

	return interval;
}

Uint32 enableSnapShot(Uint32 interval = 0, void* param = nullptr) {
	if (!gameEnded) {
		randX = (rand() % 10 - 10) / 10.0f;
		randY = (rand() % 10) / 10.0f;

		isSnapshotOn = true;

		Person::freezeAll();

		player.isMoving = false;

		reset_snapshot_timer = SDL_AddTimer(snapshot_reset_delay, resetSnapShot, NULL);
	}
	return interval;
}

Uint32 resetScales(Uint32 interval, void* param) {
	for (auto const& person : Person::people) {
		person->scale = glm::vec3(0.012f, 0.012f, 0.012f);
	}

	SDL_RemoveTimer(reset_scale_timer);

	return interval;
}

Uint32 resetTestimony(Uint32 interval, void* param) {
	isTestimonyShowing = false;

	SDL_RemoveTimer(reset_testimony_timer);

	return interval;
}

Uint32 showTestimony(Uint32 interval = 0, void* param = nullptr) {
	testimony_text = random_testimonies[rand() % random_testimonies.size()];

	isTestimonyShowing = true;

	reset_testimony_timer = SDL_AddTimer(testimony_reset_delay, resetTestimony, NULL);

	return interval;
}

void spawnCop() {
	cop.scale = 0.018f * glm::vec3(1, 1, 1);
	cop.pos = glm::vec3(-1.0f, 5.5f, 0.0f);
	cop.isMoving = true;
}

Load<MeshBuffer> meshes(LoadTagInit, []() { return new MeshBuffer("city.pnc"); });

Load<MeshBuffer> word_meshes(LoadTagInit, []() { return new MeshBuffer("menu.p"); });

// Menu program itself:
Load<GLProgram> word_program(LoadTagInit, []() {
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

	word_program_Position = (*ret)("Position");
	word_program_mvp = (*ret)["mvp"];
	word_program_color = (*ret)["color"];

	return ret;
});

// Binding for using staging_program on staging_meshes:
Load<GLVertexArray> word_binding(LoadTagDefault, []() {
	GLVertexArray* ret = new GLVertexArray(
			GLVertexArray::make_binding(word_program->program, {
																														 {word_program_Position, word_meshes->Position},
																												 }));
	return ret;
});

// Attrib locations in game_program:
GLint game_program_Position = -1;
GLint game_program_Normal = -1;
GLint game_program_Color = -1;
// Uniform locations in game_program:
GLint game_program_index = -1;
GLint game_program_mvp = -1;
GLint game_program_mv = -1;
GLint game_program_itmv = -1;
GLint game_program_roughness = -1;

// Game program itself:
Load<GLProgram> game_program(LoadTagInit, []() {
	GLProgram* ret = new GLProgram(
			"#version 330\n"
			"uniform mat4 mvp;\n"		// model positions to clip space
			"uniform mat4x3 mv;\n"	// model positions to lighting space
			"uniform mat3 itmv;\n"	// model normals to lighting space
			"uniform int index;\n"	// person class idx
			"uniform vec3 people_colors[" NUM_PLAYER_CLASSES_STR
			"];\n"
			"in vec4 Position;\n"
			"in vec3 Normal;\n"
			"in vec3 Color;\n"
			"out vec3 color;\n"
			"out vec3 normal;\n"
			"out vec3 position;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	position = mv * Position;\n"
			"	normal = itmv * Normal;\n"
			"	color = (index>=0)? people_colors[index] : Color;\n"
			"}\n",
			"#version 330\n"
			"in vec3 color;\n"
			"in vec3 normal;\n"
			"in vec3 position;\n"
			"out vec4 fragColor;\n"
			"uniform float roughness;\n"
			"void main() {\n"
			"	float shininess = pow(1024.0, 1.0 - roughness);\n"
			"	vec3 n = normalize(normal);\n"
			"	vec3 l = vec3(0.0, 0.0, 1.0);\n"
			"	l = normalize(mix(l, n, 0.2));\n"	// fake hemisphere light w/ normal bending
			"	vec3 h = normalize(n+l);\n"
			"	vec3 reflectance = \n"
			"		color.rgb\n"																					 // Lambertain Diffuse
			"		+ pow(max(0.0, dot(n, h)), shininess)\n"							 // Blinn-Phong Specular
			"		* (shininess + 2.0) / (8.0)\n"												 // normalization factor (tweaked for hemisphere)
			"		* (0.04 + (1.0 - 0.04) * pow(1.0 - dot(l,h), 5.0))\n"	// Schlick's approximation for Fresnel reflectance
			"	;\n"
			"	vec3 lightEnergy = vec3(1.0);\n"
			"	fragColor = vec4(max(dot(n,l),0.0) * lightEnergy * reflectance, 1.0);\n"
			"}\n");

	// TODO: change these to be errors once full shader is in place
	game_program_Position = (*ret)("Position");
	game_program_Normal = ret->getAttribLocation("Normal", GLProgram::MissingIsWarning);
	game_program_Color = ret->getAttribLocation("Color", GLProgram::MissingIsWarning);
	game_program_mvp = (*ret)["mvp"];
	game_program_index = ret->getUniformLocation("index", GLProgram::MissingIsWarning);
	game_program_mv = ret->getUniformLocation("mv", GLProgram::MissingIsWarning);
	game_program_itmv = ret->getUniformLocation("itmv", GLProgram::MissingIsWarning);
	game_program_roughness = (*ret)["roughness"];
	// Person::personIdx = ret->getUniformLocation("index", GLProgram::MissingIsWarning);
	// Person::colors = ret->getUniformLocation("people_colors", GLProgram::MissingIsWarning);
	return ret;
});

// Binding for using game_program on meshes:
Load<GLVertexArray> binding(LoadTagDefault, []() {
	GLVertexArray* ret =
			new GLVertexArray(GLVertexArray::make_binding(game_program->program, {{game_program_Position, meshes->Position},
																																						{game_program_Normal, meshes->Normal},
																																						{game_program_Color, meshes->Color}}));
	return ret;
});

//------------------------------

void snapShot(float x, float y, Scene* scene) {
	if (!gameEnded) {
		glEnable(GL_STENCIL_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		glStencilFunc(GL_NEVER, 1, 1);
		glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);

		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);

		Draw draw;

		scene->camera.fovy = glm::radians(40.0f);

		draw.add_rectangle(glm::vec2(x, y), glm::vec2(x + 0.75f, y - 0.75f), glm::u8vec4(0x00, 0x00, 0x00, 0xff));
		draw.draw();

		glStencilMask(0x00);
		glStencilFunc(GL_EQUAL, 1, 1);
	}
}

GameMode::GameMode() {
	sock = nullptr;

	camera.elevation = 1.00833f;
	camera.azimuth = 3.15f;

	// snapshot_timer = SDL_AddTimer(snapshot_delay, enableSnapShot, NULL);

	// testimony_timer = SDL_AddTimer(testimony_delay, showTestimony, NULL);

	time(&game_start_time);

	{	// read scene:
		std::ifstream file("city.scene", std::ios::binary);

		std::vector<char> strings;
		// read strings chunk:
		read_chunk(file, "str0", &strings);

		{	// read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 48, "Scene entry should be packed");

			std::vector<SceneEntry> data;
			read_chunk(file, "scn0", &data);

			for (auto const& entry : data) {
				if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
					throw std::runtime_error("index entry has out-of-range name begin/end");
				}
				std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
				addObject(name, entry.position, entry.rotation, entry.scale);
			}
		}
	}
	// TODO: asset pipeline bounding boxes. Hard coded for now.
	// these are the four houses
	collisionFramework.addBounds(BBox2D(glm::vec2(0.719, -1.003), glm::vec2(2.763, 2.991)));
	collisionFramework.addBounds(BBox2D(glm::vec2(4.753, -2.318), glm::vec2(6.737, -0.24)));
	collisionFramework.addBounds(BBox2D(glm::vec2(4.451, 6.721), glm::vec2(9.929, 8.799)));
	collisionFramework.addBounds(BBox2D(glm::vec2(0.247, 7.31), glm::vec2(2.397, 9.986)));

	snapshotBtn.pos = glm::vec2(-0.5f + 0.025f, -0.92f);
	snapshotBtn.rad = glm::vec2(0.45f, 0.06f);
	snapshotBtn.label = "SNAPSHOT";
	snapshotBtn.isEnabled = [&]() -> bool { return !gameEnded && state.powerTimer > settings.POWER_TIMEOUT; };
	snapshotBtn.onFire = [&]() {
		// sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_SELECT_POWER, {0}));
		enableSnapShot();
		state.powerTimer = 0;
	};
	snapshotBtn.color = glm::vec3(0.1f, 0.6f, 0.1f);

	anonymousTipBtn.pos = glm::vec2(0.5f - 0.025f, -0.92f);
	anonymousTipBtn.rad = glm::vec2(0.45f, 0.06f);
	anonymousTipBtn.label = "ANON TIP";
	anonymousTipBtn.isEnabled = [&]() -> bool { return !gameEnded && state.powerTimer > settings.POWER_TIMEOUT; };
	anonymousTipBtn.onFire = [&]() {
		// sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_SELECT_POWER, {0}));
		showTestimony();
		state.powerTimer = 0;
	};
	anonymousTipBtn.color = glm::vec3(0.1f, 0.6f, 0.1f);
}

// https://www.bensound.com/royalty-free-music/track/summer
Sound bgmusic;
void GameMode::reset(const GameSettings& gameSettings) {
	settings = gameSettings;

	twister.seed(settings.seed);
	// bgmusic = Sound("../sounds/bensound-summer.wav", true);
	// bgmusic.play();
	int numPlayers = 250;

	Person::random = rand;
	for (int i = 0; i <= numPlayers; i++) {
		Person* cur;
		if (i == numPlayers) {
			player = Person();
			cur = &player;
		} else if (i == numPlayers - 1) {
			cop = Person();
			cur = &cop;
		} else
			cur = makeAI();
		cur->rot = glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0));
		cur->scale = 0.012f * glm::vec3(1, 1, 1);

		/*
				cur->meshObject.program = game_program->program;
				cur->meshObject.program_mvp = game_program_mvp;
				cur->meshObject.program_mv = game_program_mv;
				cur->meshObject.program_itmv = game_program_itmv;
				cur->meshObject.vao = binding->array;
				cur->meshObject.start = mesh.start;
				cur->meshObject.count = mesh.count;
				cur->meshObject.set_uniforms = [](Scene::Object const&) { glUniform1f(game_program_roughness, 1.0f); };*/
		cur->placeInScene((i >= numPlayers - 1) ? NULL : &collisionFramework);

		// cop
		if (i == numPlayers - 1) {
			cur->scale = 0.0f * glm::vec3(1.0f);
		}
	}
}

Scene::Object* GameMode::addObject(std::string const& name,
																	 glm::vec3 const& position,
																	 glm::quat const& rotation,
																	 glm::vec3 const& scale) {
	scene.objects.emplace_back();
	Scene::Object& object = scene.objects.back();
	object.transform.position = position;
	object.transform.rotation = rotation;
	object.transform.scale = scale;

	MeshBuffer::Mesh const& mesh = meshes->lookup(name);
	object.program = game_program->program;
	object.program_mvp = game_program_mvp;
	object.classIndex = game_program_index;
	object.program_mv = game_program_mv;
	object.program_itmv = game_program_itmv;
	object.vao = binding->array;
	object.start = mesh.start;
	object.count = mesh.count;

	object.set_uniforms = [](Scene::Object const&) { glUniform1f(game_program_roughness, 1.0f); };
	return &object;
}

void GameMode::endGame() {
	if (gameEnded)
		return;
	gameEnded = true;
	Person::freezeAll();
	player.isMoving = false;
	spawnCop();
}

Ray GameMode::getCameraRay(glm::vec2 mouse) {
	glm::vec4 screenMouseH = glm::vec4(mouse.x, mouse.y, 0.5f, 1);
	glm::mat4 projMat = scene.camera.make_projection();
	glm::mat4 camMat = scene.camera.transform.make_world_to_local();

	glm::mat4 screenMat = projMat * camMat;

	glm::vec3 cameraCenter = scene.camera.transform.position;
	glm::vec4 mouseWorldH = glm::inverse(screenMat) * screenMouseH;
	glm::vec3 mouseWorld = glm::vec3(mouseWorldH.x, mouseWorldH.y, mouseWorldH.z) / mouseWorldH.w;

	Ray r = Ray(cameraCenter, glm::normalize(mouseWorld - cameraCenter));

	return r;
}
bool GameMode::handle_event(SDL_Event const& e, glm::uvec2 const& window_size) {
	if (sock && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)) {
		static constexpr int up = SDL_SCANCODE_W;
		static constexpr int down = SDL_SCANCODE_S;
		static constexpr int left = SDL_SCANCODE_A;
		static constexpr int right = SDL_SCANCODE_D;

		switch (e.key.keysym.scancode) {
			case up:
				sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {0}));
				break;

			case down:
				sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {1}));
				break;

			case left:
				sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {2}));
				break;

			case right:
				sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {3}));
				break;

			default:
				break;
		}
	}

	double minDistance = 1000;
	auto* closestPerson = new Person;

	if (e.type == SDL_KEYDOWN) {
		switch (e.key.keysym.sym) {
			case SDLK_LSHIFT:

				for (auto person : Person::people) {
					float distancex = pow(person->pos.x - player.pos.x, 2.0f);
					float distancey = pow(person->pos.y - player.pos.y, 2.0f);

					double calcdistance = pow(distancex + distancey, 0.5f);

					if (calcdistance < minDistance) {
						closestPerson = &*person;
						minDistance = calcdistance;
					}
				}

				if (minDistance < 0.4f) {
					closestPerson->scale = glm::vec3(0.02f, 0.02f, 0.02f);
					reset_scale_timer = SDL_AddTimer(2000, resetScales, NULL);
				}

				break;
			case SDLK_SPACE:
				// Catch thief
				if (gameEnded) {
					float distancex = pow(cop.pos.x - player.pos.x, 2.0f);
					float distancey = pow(cop.pos.y - player.pos.y, 2.0f);

					double calcdistance = pow(distancex + distancey, 0.5f);

					if (calcdistance < 0.3f) {
						player.scale = 0.016f * glm::vec3(1, 1, 1);
						std::cout << "Thief Found"
											<< "\n";
					} else {
						std::cout << "Thief NOT Found. Cops lose!"
											<< "\n";
					}

				}
				// DEPLOY COP
				else {
					endGame();
				}
				break;
			default:
				break;
		}
	}
	// temporary mouse movement for camera
	else if (e.type == SDL_MOUSEMOTION) {
		glm::vec2 old_mouse = mouse;
		mouse.x = (e.motion.x + 0.5f) / float(2 * 960) * 2.0f - 1.0f;
		mouse.y = (e.motion.y + 0.5f) / float(2 * 600) * -2.0f + 1.0f;
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			camera.elevation += -2.0f * (mouse.y - old_mouse.y);
			camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
		}
		snapshotBtn.hover = snapshotBtn.contains(mouse);
		anonymousTipBtn.hover = anonymousTipBtn.contains(mouse);
	} else if (e.type == SDL_MOUSEBUTTONDOWN) {
		Ray r = getCameraRay(mouse);
		for (auto const& person : Person::people) {
			glm::vec3 pos = person->pos;
			BBox box = BBox(glm::vec3(pos.x - 0.1, pos.y - 0.1, 0), glm::vec3(pos.x + 0.1, pos.y + 0.1, 2));
			if (box.intersects(r)) {
				person->scale = glm::vec3(0.1, 0.1, 0.1);
				// TODO: Make person large?
			}
		}

		if (snapshotBtn.hover && snapshotBtn.isEnabled()) {
			snapshotBtn.onFire();
		}
		if (anonymousTipBtn.hover && anonymousTipBtn.isEnabled()) {
			anonymousTipBtn.onFire();
		}
		////
		//		glm::mat4 world_to_camera = scene.camera.transform.make_world_to_local();
		//		glm::mat4 world_to_clip = scene.camera.make_projection() * world_to_camera;
		//
		//        glm::vec4 point2d = scene.camera.make_projection() * glm::vec4(player.pos.x, player.pos.y, 0, 1);

		//        int winX =  (((point2d.x + 1) * 320) - 0.5f);
		//        int winY =  (((point2d.y - 1) * -200) - 0.5f);
		//        std::cout << winX << " " << winY << "\n";

	} else if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_TAB)
			camera.radius++;
		// else if(e.key.keysym.sym == SDLK_LSHIFT) camera.radius--;
		else if (e.key.keysym.sym == SDLK_LSHIFT)
			std::cout << camera.elevation << " " << camera.azimuth << "\n";
	}

	return false;
}

void GameMode::update(float elapsed) {
	counter += elapsed;

	state.powerTimer += elapsed;

	static const uint8_t* keys = SDL_GetKeyboardState(NULL);

	if (settings.clientSidePrediction) {
		if (gameEnded)
			cop.setVel(keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_A], keys[SDL_SCANCODE_D]);
		else
			player.setVel(keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_A], keys[SDL_SCANCODE_D]);

		player.move(elapsed, &collisionFramework);
		cop.move(elapsed, &collisionFramework);
	}
	Person::moveAll(elapsed, &collisionFramework);

	time_t curtime;
	time(&curtime);
	if (!gameEnded && difftime(curtime, game_start_time) > game_duration) {
		endGame();
	}

	if (!sock) {
		return;
	}

	Packet* out;
	while (sock->readQueue.try_dequeue(out)) {
		if (!out) {
			std::cout << "Bad packet from server" << std::endl;
			continue;
		}

		switch (out->payload[0]) {
			case MessageType::GAME_ROBBER_POS: {
				player.pos.x = *reinterpret_cast<float*>(&out->payload[1]);
				player.pos.y = *reinterpret_cast<float*>(&out->payload[1 + sizeof(float)]);

				// TODO: update rotation.. might just go with a local value, also move the player locally and then snap to
				// correct value

				std::cout << "New robber position: " << player.pos.x << " " << player.pos.y << std::endl;

				break;
			}

			default: {
				// DEBUG_PRINT("Unknown game message: " << ((int)out->payload.at(0)));
				break;
			}
		}

		delete out;
	}
}

void GameMode::draw(glm::uvec2 const& drawable_size) {
	screenSize = drawable_size;
	// TODO: this is a affected by retina, still need to figure this out

	// camera:
	scene.camera.transform.position =
			camera.radius * glm::vec3(std::cos(camera.elevation) * std::cos(camera.azimuth),
																std::cos(camera.elevation) * std::sin(camera.azimuth), std::sin(camera.elevation)) +
			camera.target;

	glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
	glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
	up = glm::normalize(up - glm::dot(up, out) * out);
	glm::vec3 right = glm::cross(up, out);

	scene.camera.transform.rotation = glm::quat_cast(glm::mat3(right, up, out));
	scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	static bool prevSnapshotOn = false;
	static float offset = rand() * 0.3f;	// TODO: server will just send a coord, none of this
	static glm::vec2 pos = glm::vec2(player.pos.y / -9.0f - offset, player.pos.x / 6.0f + offset);
	if (!prevSnapshotOn && isSnapshotOn) {	// first frame
		offset = rand() * 0.4f + 0.1f;
		pos = glm::vec2(player.pos.y / -9.0f - offset, player.pos.x / 6.0f + offset);
	}
	if (isSnapshotOn) {
		snapShot(pos.x, pos.y, &scene);
		prevSnapshotOn = true;
	} else {
		prevSnapshotOn = false;
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	scene.render();

	static bool colorsNotInit = false;

	static MeshBuffer::Mesh const& mesh = meshes->lookup("lowman_shoes.001");

	static auto make_local_to_parent = [](glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
		return glm::mat4(	// translate
							 glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
							 glm::vec4(position, 1.0f)) *
					 glm::mat4_cast(rotation)	// rotate
					 * glm::mat4(							 // scale
								 glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
								 glm::vec4(0.0f, 0.0f, scale.z, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	};
	static auto renderAll = [&](Scene::Camera camera, std::list<Scene::Light> lights, Person player, Person cop) {
		glm::mat4 world_to_camera = camera.transform.make_world_to_local();
		glm::mat4 world_to_clip = camera.make_projection() * world_to_camera;

		if (colorsNotInit) {
			for (int i = 0; i < NUM_PLAYER_CLASSES; i++) {
				Person::PeopleColors[i] = glm::vec3(Person::random(), Person::random(), Person::random());
			}
			colorsNotInit = false;
		}

		// Get world-space position of all lights:
		for (auto const& light : lights) {
			glm::mat4 mv = world_to_camera * light.transform.make_local_to_world();
			(void)mv;
		}

		for (unsigned int i = 0; i <= Person::people.size(); i++) {
			Person* person;
			if (i == Person::people.size())
				person = &player;
			else if (i == Person::people.size() - 1)
				person = &cop;
			else
				person = Person::people[i];

			glm::mat4 local_to_world = make_local_to_parent(person->pos, person->rot, person->scale);

			// compute modelview+projection (object space to clip space) matrix for this object:
			glm::mat4 mvp = world_to_clip * local_to_world;

			// compute modelview (object space to camera local space) matrix for this object:
			glm::mat4 mv = local_to_world;

			// NOTE: inverse cancels out transpose unless there is scale involved
			glm::mat3 itmv = glm::inverse(glm::transpose(glm::mat3(mv)));

			// set up program uniforms:
			glUseProgram(game_program->program);
			if (game_program_mvp != -1U) {
				glUniformMatrix4fv(game_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
			}
			if (game_program_mv != -1U) {
				glUniformMatrix4x3fv(game_program_mv, 1, GL_FALSE, glm::value_ptr(mv));
			}
			if (game_program_itmv != -1U) {
				glUniformMatrix3fv(game_program_itmv, 1, GL_FALSE, glm::value_ptr(itmv));
			}
			// glUniform1i(Person::personIdx, person->playerClass);
			// glUniform3fv(Person::colors, NUM_PLAYER_CLASSES, (GLfloat*)Person::PeopleColors);

			glUniform1f(game_program_roughness, 1.0f);

			glBindVertexArray(binding->array);

			// draw the object:
			glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
		}
	};
	renderAll(scene.camera, scene.lights, player, cop);

	float aspect = drawable_size.x / float(drawable_size.y);
	// scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(0.55f / aspect, 0.55f);
	glm::mat4 projection = glm::mat4(glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
																	 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	glUseProgram(word_program->program);
	glBindVertexArray(word_binding->array);

	static auto draw_word = [&projection](const std::string& word, float init_x, float y, float fontSize = 1.f,
																				glm::vec3 color = glm::vec3(1, 1, 1)) {
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

		float total_width = 0.0f;
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0)
				total_width += spacing(word[i - 1], word[i]);
			total_width += width(word[i]);
		}

		float x = -0.5f * total_width;
		for (uint32_t i = 0; i < word.size(); ++i) {
			if (i > 0) {
				x += spacing(word[i], word[i - 1]);
			}

			if (word[i] != ' ') {
				float s = fontSize * 0.1f * (1.0f / 3.0f);
				glm::mat4 mvp = projection * glm::mat4(glm::vec4(s, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, s, 0.0f, 0.0f),
																							 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
																							 glm::vec4(2 * init_x + s * x, 2 * y + 0.1, 0.0f, 1.0f));
				glUniformMatrix4fv(word_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(word_program_color, color.r, color.g, color.b);

				MeshBuffer::Mesh const& mesh = word_meshes->lookup(word.substr(i, 1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(word[i]);
		}
	};

	static const MeshBuffer::Mesh& buttonMesh = word_meshes->lookup("Button");
	static auto draw_rect = [&](const glm::vec2& pos, const glm::vec2& rad, const glm::vec3& color) {
		// scale with aspect ratio, projection matrix not applied
		glm::mat4 mvp =
				glm::mat4(glm::vec4(rad.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, rad.y, 0.0f, 0.0f),
									glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(pos.x, pos.y, -0.05f, 1.0f)	// z is back to show text
				);

		glUniformMatrix4fv(word_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniform3f(word_program_color, color.x, color.y, color.z);
		glDrawArrays(GL_TRIANGLES, buttonMesh.start, buttonMesh.count);
	};
	static auto draw_button = [&](const Button& button) {
		glm::vec3 color = button.color;
		glm::vec3 textColor = glm::vec3(1.0f);

		glm::vec2 outlineSize = glm::vec2(0.005f, 0.0075f);
		if (button.hover) {
			outlineSize += outlineSize.x / 2.0f * std::sin(1.5f * counter * 2 * 3.14159f) + outlineSize.x / 2.0f;

			color *= 1.25f;
		}
		if (!button.isEnabled()) {
			color *= 0.1f;
			textColor *= 0.5f;
		}

		draw_rect(button.pos, button.rad, color);
		draw_rect(button.pos, button.rad + outlineSize, {1.0f, 1.0f, 1.0f});
		draw_word(button.label, button.pos.x, button.pos.y, 1, textColor);
	};

	if (isTestimonyShowing) {
		draw_word("ANONYMOUS TIP", 0, 0);
		draw_word("SUSPICIOUS PERSON " + testimony_text + " REPORTED", 0, -0.5);
	}

	time_t curtime;
	time(&curtime);
	int secondsRemaining = gameEnded ? 0 : game_duration - int(difftime(curtime, game_start_time));
	char timeString[1000];
	if (secondsRemaining <= 0)
		sprintf(timeString, "TIME IS UP");
	else if (secondsRemaining > 100)
		sprintf(timeString, "HUNDRED TWENTY SECONDS LEFT");
	else if (secondsRemaining > 80)
		sprintf(timeString, "HUNDRED SECONDS LEFT");
	else if (secondsRemaining > 60)
		sprintf(timeString, "EIGHTY SECONDS LEFT");
	else if (secondsRemaining > 40)
		sprintf(timeString, "SIXTY SECONDS LEFT");
	else if (secondsRemaining > 20)
		sprintf(timeString, "FORTY SECONDS LEFT");
	else if (secondsRemaining > 10)
		sprintf(timeString, "TWENTY SECONDS LEFT");
	else {
		static char digits[10][32] = {"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN"};
		sprintf(timeString, "%s SECONDS LEFT", digits[secondsRemaining - 1]);
	}

	draw_word(timeString, 0.7, 0.8, 0.8, glm::vec3(0.4, 0.4, 0.4));
	draw_button(snapshotBtn);
	draw_button(anonymousTipBtn);

	if (gameResultPosted) {
		draw_word(endGameTxt, 0, 0);
	}

	scene.camera.aspect = drawable_size.x / float(drawable_size.y);
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.near = 0.01f;

	glDisable(GL_STENCIL_TEST);
}
