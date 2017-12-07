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

#include "../shared/State.hpp"
#include "../shared/person.h"
#include "Sounds.h"

/* ANIMATION CODE HERE */
#include "BoneAnimation.hpp"
#define BONE_LIMIT 40
#define STR2( X ) #X
#define STR( X ) STR2( X )

GLint animation_program_Position = -1;
GLint animation_program_Normal = -1;
GLint animation_program_Color = -1;
GLint animation_program_TexCoord = -1;
GLint animation_program_BoneWeights = -1;
GLint animation_program_BoneIndices = -1;
GLint animation_program_mvp = -1;
GLint animation_program_bones = -1;

GLint animation_program_mv = -1;
GLint animation_program_itmv = -1;
GLint animation_program_index = -1;
GLint animation_program_people_colors = -1;

//animation program
Load< GLProgram > animation_program(LoadTagInit, []() -> GLProgram const *{
	printf("reading animation program\n");
	GLProgram *ret = new GLProgram(
		"#version 330 \n"
		"uniform mat4 mvp;\n"// model positions to clip space
		"uniform mat4x3 mv;\n"// model positions to lighting space
		"uniform mat3 itmv;\n"// model normals to lighting space
		"uniform int index;\n"// person class idx
		"uniform vec3 people_colors[" STR(NUM_PLAYER_CLASSES) "];\n"
		"uniform mat4x3 bones[" STR(BONE_LIMIT) "];\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec3 Color;\n"
		"in vec2 TexCoord;\n"
		"in vec4 BoneWeights;\n"
		"in uvec4 BoneIndices;\n"
		"out vec3 normal;\n"
		"out vec3 color;\n"
		"out vec3 position;\n"
		"void main() {\n"
		"	vec3 skinned =\n" 
		"		  BoneWeights.x * ( bones[ BoneIndices.x ] * Position)\n"
		"		+ BoneWeights.y * ( bones[ BoneIndices.y ] * Position)\n"
		"		+ BoneWeights.z * ( bones[ BoneIndices.z ] * Position)\n"
		"		+ BoneWeights.w * ( bones[ BoneIndices.w ] * Position);\n"
		"	vec3 skinned_normal = \n"
		"		inverse(transpose(BoneWeights.x * mat3(bones[ BoneIndices.x ])\n"
		"		                + BoneWeights.y * mat3(bones[ BoneIndices.y ])\n"
		"		                + BoneWeights.z * mat3(bones[ BoneIndices.z ])\n"
		"		                + BoneWeights.w * mat3(bones[ BoneIndices.w ]))) * Normal;\n"
		"	gl_Position = mvp * vec4(skinned, 1.0);\n"
		"	normal = itmv*skinned_normal;\n"
		"	position = mv * vec4(skinned, 1.0);\n"
		"	color = people_colors[index];\n"
		"}\n"
		,
		"#version 330 \n"
		"in vec3 color;\n"
		"in vec3 normal;\n"
		"in vec3 position;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	float shininess = 1.0;\n" //No roughness psh
		"	vec3 n = normalize(normal);\n"
		"	vec3 l = vec3(0.0, 0.0, 1.0);\n"
		"	l = normalize(mix(l, n, 0.2));\n"// fake hemisphere light w/ normal bending
		"	vec3 h = normalize(n+l);\n"
		"	vec3 reflectance = \n" 
		"		color.rgb\n"// Lambertain Diffuse
		"		+ pow(max(0.0, dot(n, h)), shininess)\n"// Blinn-Phong Specular
		"		* (shininess + 2.0) / (8.0)\n"// normalization factor (tweaked for hemisphere)
		"		* (0.04 + (1.0 - 0.04) * pow(1.0 - dot(l,h), 5.0));\n"// Schlick's approximation for Fresnel reflectance
		"	vec3 lightEnergy = vec3(1.0);\n"
		"	fragColor = vec4(max(dot(n,l),0.0) * lightEnergy * reflectance, 1.0);\n"
		"}\n"
	);
	animation_program_Position    = (*ret)("Position");
	animation_program_Normal      = ret->getAttribLocation("Normal", GLProgram::MissingIsWarning);
	animation_program_Color       = ret->getAttribLocation("Color", GLProgram::MissingIsWarning);
	animation_program_TexCoord    = ret->getAttribLocation("TexCoord", GLProgram::MissingIsWarning);
	animation_program_BoneWeights = ret->getAttribLocation("BoneWeights", GLProgram::MissingIsWarning);
	animation_program_BoneIndices = ret->getAttribLocation("BoneIndices", GLProgram::MissingIsWarning);

	animation_program_mvp           = (*ret)["mvp"];
	animation_program_bones         = ret->getUniformLocation("bones[0]", GLProgram::MissingIsWarning);
	animation_program_mv            = ret->getUniformLocation("mv",GLProgram::MissingIsWarning);
	animation_program_itmv          = ret->getUniformLocation("itmv",GLProgram::MissingIsWarning);
	animation_program_index         = ret->getUniformLocation("index",GLProgram::MissingIsWarning);
	animation_program_people_colors = ret->getUniformLocation("people_colors[0]",GLProgram::MissingIsWarning);

	printf("end read\n");
	return ret;
});
Load<BoneAnimation> animation(LoadTagInit, [](){
	BoneAnimation* ret = new BoneAnimation("human.blob");
	assert(ret->bones.size() < BONE_LIMIT);
	printf("found %d bones\n",(int)ret->bones.size());
	std::cout << "Animations has " << ret->frame_bones.size() / ret->bones.size() << " frames and " << ret->vertex_count << " vertices." << std::endl;
	return ret;
});
//Binding for using animation_program on animation:
Load< GLVertexArray > animation_binding(LoadTagDefault, [](){
	printf("binding vars...\n");
	GLVertexArray *ret = new GLVertexArray(GLVertexArray::make_binding(animation_program->program, {
		{animation_program_Position, animation->Position},
		{animation_program_Normal, animation->Normal},
		{animation_program_Color, animation->Color},
		{animation_program_TexCoord, animation->TexCoord},
		{animation_program_BoneWeights, animation->BoneWeights},
		{animation_program_BoneIndices, animation->BoneIndices}
	}));
	printf("binding complete\n");
	return ret;
});

/* END ANIMATION CODE */


extern Load<MeshBuffer> menuMeshes;
extern Load<GLProgram> menuProgram;
extern Load<GLVertexArray> menuBinding;
extern GLint menuProgramPosition;
extern GLint menuProgramMVP;
extern GLint menuProgramColor;

bool isSnapshotOn = false;
bool isTestimonyShowing = false;
float randX;
float randY;
std::string testimony_text = "";
time_t game_start_time;
float game_duration = 120;

float player_move_speed = 0.1f;

Uint32 snapshot_delay = 20000;
Uint32 testimony_delay = 12000;
Uint32 testimony_reset_delay = 4000;
Uint32 snapshot_reset_delay = 4000;

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

void spawnCop() {
	cop.scale = 0.18f * glm::vec3(1, 1, 1);
	cop.pos = glm::vec3(-1.0f, 5.5f, 0.0f);
	cop.isMoving = true;
}

Load<MeshBuffer> meshes(LoadTagInit, []() { return new MeshBuffer("city.pnc"); });

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
GLint game_program_people_colors = -1;

// Game program itself:
Load<GLProgram> game_program(LoadTagInit, []() {
	GLProgram* ret = new GLProgram(
			"#version 330\n"
			"uniform mat4 mvp;\n"		// model positions to clip space
			"uniform mat4x3 mv;\n"	// model positions to lighting space
			"uniform mat3 itmv;\n"	// model normals to lighting space
			"uniform int index;\n"	// person class idx
			"uniform vec3 people_colors[" STR(NUM_PLAYER_CLASSES)
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
	game_program_people_colors = ret->getUniformLocation("people_colors", GLProgram::MissingIsWarning);
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

template <class callable, class... arguments>
void later(int after, callable&& f, arguments&&... args);

void robSomeone() {
	Person* closestPerson = nullptr;
	double minDistance = 1000;

	for (auto& person : Person::people) {
		float distancex = pow(person->pos.x - player.pos.x, 2.0f);
		float distancey = pow(person->pos.y - player.pos.y, 2.0f);

		double calcdistance = pow(distancex + distancey, 0.5f);

		if (calcdistance < minDistance) {
			closestPerson = person;
			minDistance = calcdistance;
		}
	}

	if (closestPerson && minDistance < 0.4f) {
		closestPerson->rob();
		later(2000, [closestPerson]() {
			closestPerson->scale = Person::BASE_SCALE;
			closestPerson->robbed = false;
		});
	}
}

void searchSomeone() {
	float distancex = pow(cop.pos.x - player.pos.x, 2.0f);
	float distancey = pow(cop.pos.y - player.pos.y, 2.0f);

	double calcdistance = pow(distancex + distancey, 0.5f);

	if (calcdistance < 0.3f) {
		player.scale = 0.16f * glm::vec3(1, 1, 1);
		std::cout << "Thief Found" << std::endl;
	} else {
		std::cout << "Thief NOT Found. Cops lose!" << std::endl;
	}
}

GameMode::GameMode() {
	sock = nullptr;

	camera.elevation = 1.00833f;
	camera.azimuth = 3.15f;

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

	auto isEnabled = [&]() -> bool { return !gameEnded && state.powerTimer > staging->settings.POWER_TIMEOUT; };

	Button* btn;
	glm::vec3 color = {0.1f, 0.6f, 0.1f};

	btn = copButtons.add("SNAPSHOT", color);
	btn->isEnabled = isEnabled;
	btn->onFire = [&]() { sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_ACTIVATE_POWER, {Power::SNAPSHOT})); };

	btn = copButtons.add("ANON TIP", color);
	btn->isEnabled = isEnabled;
	btn->onFire = [&]() { sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_ACTIVATE_POWER, {Power::ANON_TIP})); };

	btn = copButtons.add("ROAD BLOCK", color);
	btn->isEnabled = isEnabled;
	btn->onFire = [&]() { sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_ACTIVATE_POWER, {Power::ROADBLOCK})); };

	// make this on a row of it's own or something
	btn = copButtons.add("deploy", color);
	btn->isEnabled = isEnabled;
	btn->onFire = [&]() { sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_ACTIVATE_POWER, {Power::DEPLOY})); };

	copButtons.layoutHorizontal(0.06f, -0.92f);
}

// https://www.bensound.com/royalty-free-music/track/summer
Sound bgmusic;
void GameMode::reset(std::unique_ptr<StagingMode::StagingState> stagingState) {
	staging = std::move(stagingState);

	twister.seed(staging->settings.seed);
	// bgmusic = Sound("../sounds/bensound-summer.wav", true);
	// bgmusic.play();
	int numPlayers = 250;

	std::cout << "first random values" << rand() << " " << rand() << " " << rand() << std::endl;

	Person::random = rand;
	for (int i = 0; i <= numPlayers; i++) {
		Person* cur;
		if (i == numPlayers) {
			player = Person();
			cur = &player;

			if (!staging->settings.localMultiplayer && staging->player == staging->robber) {
				player.scale *= 1.5f;
			}

		} else if (i == numPlayers - 1) {
			cop = Person();
			cur = &cop;
			cur->scale *= 0.0f;
		} else {
			cur = makeAI();
		}

		cur->rot = glm::quat();//glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
		cur->placeInScene((i >= numPlayers - 1) ? NULL : &Data::collisionFramework);
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

int frames = 0;
void GameMode::endGame() {
	std::cout << "Frames since start:" << frames << std::endl;

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
	if (e.type == SDL_KEYDOWN) {
		static constexpr int up = SDL_SCANCODE_W;
		static constexpr int down = SDL_SCANCODE_S;
		static constexpr int left = SDL_SCANCODE_A;
		static constexpr int right = SDL_SCANCODE_D;
		static constexpr int action = SDL_SCANCODE_LSHIFT;

		static constexpr int altUp = SDL_SCANCODE_UP;
		static constexpr int altDown = SDL_SCANCODE_DOWN;
		static constexpr int altLeft = SDL_SCANCODE_LEFT;
		static constexpr int altRight = SDL_SCANCODE_RIGHT;
		static constexpr int altAction = SDL_SCANCODE_RSHIFT;

		if (e.key.keysym.sym == SDLK_TAB) camera.radius++;
		else if(e.key.keysym.sym == SDLK_LSHIFT) camera.radius--;

		if (!staging->settings.localMultiplayer) {
			switch (e.key.keysym.scancode) {
				case up:
				case altUp:
					if (gameEnded || staging->player == staging->robber) {
						sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {Control::UP}));
					}
					break;

				case down:
				case altDown:
					if (gameEnded || staging->player == staging->robber) {
						sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {Control::DOWN}));
					}
					break;

				case left:
				case altLeft:
					if (gameEnded || staging->player == staging->robber) {
						sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {Control::LEFT}));
					} else {
						if (!gameEnded && staging->player != staging->robber) {
							copButtons.onPrev();
						}
					}
					break;

				case right:
				case altRight:
					if (gameEnded || staging->player == staging->robber) {
						sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {Control::RIGHT}));
					} else {
						if (!gameEnded && staging->player != staging->robber) {
							copButtons.onNext();
						}
					}
					break;

				case action:
				case altAction:
					if (gameEnded || staging->player == staging->robber) {
						sock->writeQueue.enqueue(Packet::pack(MessageType::INPUT, {Control::ACTION}));
					} else {
						if (!gameEnded && staging->player != staging->robber) {
							copButtons.onKey();
						}
					}
					break;

				default:
					break;
			}
		} else {
			// TODO: if local play, alt inputs are for cop, regular inputs are for robber.. always send
		}
	}

	// temporary mouse movement for camera
	else if (e.type == SDL_MOUSEMOTION) {
		glm::vec2 old_mouse = mouse;
		mouse.x = (e.motion.x + 0.5f) / float(window_size.x) * 2.0f - 1.0f;
		mouse.y = (e.motion.y + 0.5f) / float(window_size.y) * -2.0f + 1.0f;
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			camera.elevation += -2.0f * (mouse.y - old_mouse.y);
			camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
		}

		if (staging->settings.localMultiplayer || staging->player != staging->robber) {
			copButtons.checkMouseHover(mouse.x, mouse.y);
		}
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

		if (staging->settings.localMultiplayer || staging->player != staging->robber) {
			copButtons.onClick();
		}

		////
		//		glm::mat4 world_to_camera = scene.camera.transform.make_world_to_local();
		//		glm::mat4 world_to_clip = scene.camera.make_projection() * world_to_camera;
		//
		//        glm::vec4 point2d = scene.camera.make_projection() * glm::vec4(player.pos.x, player.pos.y, 0, 1);

		//        int winX =  (((point2d.x + 1) * 320) - 0.5f);
		//        int winY =  (((point2d.y - 1) * -200) - 0.5f);
		//        std::cout << winX << " " << winY << "\n";
	}

	return false;
}

void GameMode::update(float elapsed) {
	frames += 1;
	counter += elapsed;

	state.powerTimer += elapsed;

	static const uint8_t* keys = SDL_GetKeyboardState(NULL);

	if (staging->settings.clientSidePrediction) {
		if (gameEnded)
			cop.setVel(keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_A], keys[SDL_SCANCODE_D]);
		else
			player.setVel(keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_A], keys[SDL_SCANCODE_D]);

		player.move(elapsed, &Data::collisionFramework);
		cop.move(elapsed, &Data::collisionFramework);
	}
	Person::moveAll(elapsed, &Data::collisionFramework);

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

				break;
			}

			case MessageType::GAME_COP_POS: {
				cop.pos.x = *reinterpret_cast<float*>(&out->payload[1]);
				cop.pos.y = *reinterpret_cast<float*>(&out->payload[1 + sizeof(float)]);

				// TODO: update rotation.. might just go with a local value, also move the player locally and then snap to
				// correct value

				break;
			}

			case MessageType::GAME_ACTIVATE_POWER: {
				std::cout << "Activing power " << (int)out->payload[1] << std::endl;

				switch (out->payload[1]) {
					case Power::SNAPSHOT: {
						activateSnapshot();
						break;
					}

					case Power::ANON_TIP: {
						activateTestimony();
						break;
					}

					case Power::ROADBLOCK: {
						activateRoadblock(0);	// out->payload[2]);
						break;
					}

					case Power::DEPLOY: {
						short targetFrames = 2 * *reinterpret_cast<short*>(&out->payload[2]);

						std::cout << "ending in " << (targetFrames - frames) << "frames" << std::endl;
						later((targetFrames - frames) * 1000.0f * 1.0f / 60.0f, [&]() { endGame(); });	// targetFrames)

						break;
					}

					default: { std::cout << "Power not handled" << std::endl; }
				}

				// TODO: add some leniency in timing because server timing may not match up exactly
				state.powerTimer = 0.0f;

				break;
			}

			case MessageType::GAME_TIME_OVER: {
				// TODO: something special that says time is up

				short targetFrames = 2 * *reinterpret_cast<short*>(&out->payload[1]);

				std::cout << "ending in " << (targetFrames - frames) << "frames" << std::endl;
				later((targetFrames - frames) * 1000.0f * 1.0f / 60.0f, [&]() { endGame(); });	// targetFrames)

				break;
			}

			case MessageType::INPUT: {
				// this will always be action input
				if (gameEnded) {
					searchSomeone();
				} else {
					robSomeone();
				}

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

	static bool colorsNotInit = true;

	//static MeshBuffer::Mesh const& mesh = meshes->lookup("lowman_shoes.001");

	static auto make_local_to_parent = [](glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
		return glm::mat4(	// translate
							 glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
							 glm::vec4(position, 1.0f)) *
					 glm::mat4_cast(rotation)	// rotate
					 * glm::mat4(							 // scale
								 glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
								 glm::vec4(0.0f, 0.0f, scale.z, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	};

	glm::mat4 world_to_camera = scene.camera.transform.make_world_to_local();
	glm::mat4 world_to_clip = scene.camera.make_projection() * world_to_camera;
	static auto renderAll = [&](std::list<Scene::Light> lights, Person player, Person cop) {
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

		//animation setup
		std::vector< glm::mat4x3 > bone_to_world(animation->bones.size()); //needed for hierarchy
		std::vector< glm::mat4x3 > bind_to_world(animation->bones.size()); //actual uniforms
		auto const &anim = animation->lookup("walking"); //only one animation currently

		glUseProgram(animation_program->program);
		glUniform3fv(animation_program_people_colors, NUM_PLAYER_CLASSES, (GLfloat*)Person::PeopleColors);
		for (unsigned int i = 0; i <= Person::people.size(); i++) {
			Person* person;
			if (i == Person::people.size())
				person = &player;
			else if (i == Person::people.size() - 1)
				person = &cop;
			else
				person = Person::people[i];

			person->pos += glm::vec3(0,0,1);
			glm::mat4 local_to_world = make_local_to_parent(person->pos, person->rot, person->scale);
			person->pos -= glm::vec3(0,0,1);
			// compute modelview+projection (object space to clip space) matrix for this object:
			glm::mat4 mvp = world_to_clip * local_to_world;

			// compute modelview (object space to camera local space) matrix for this object:
			glm::mat4 mv = local_to_world;

			// NOTE: inverse cancels out transpose unless there is scale involved
			glm::mat3 itmv = glm::inverse(glm::transpose(glm::mat3(mv)));

			// set up program uniforms:
			glUniformMatrix4fv(animation_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
			glUniformMatrix4x3fv(animation_program_mv, 1, GL_FALSE, glm::value_ptr(mv));
			glUniformMatrix3fv(animation_program_itmv, 1, GL_FALSE, glm::value_ptr(itmv));
			glUniform1i(animation_program_index, person->playerClass);

			//Animation Stuff
			BoneAnimation::PoseBone const *frame = animation->get_frame(anim.begin + person->animationFrameIdx);
			for (uint32_t b = 0; b < animation->bones.size(); ++b) {
				BoneAnimation::PoseBone const &pose_bone = frame[b];
				BoneAnimation::Bone const &bone = animation->bones[b];

				glm::mat3 r = glm::mat3_cast(pose_bone.rotation);
				glm::mat3 rs = glm::mat3(
					r[0] * pose_bone.scale.x,
					r[1] * pose_bone.scale.y,
					r[2] * pose_bone.scale.z
				);
				glm::mat4x3 trs = glm::mat4x3(
					rs[0], rs[1], rs[2], pose_bone.position
				);
				if (bone.parent == -1U) bone_to_world[b] = trs;
				else bone_to_world[b] = bone_to_world[bone.parent] * glm::mat4(trs);
				bind_to_world[b] = bone_to_world[b]*glm::mat4(bone.inverse_bind_matrix);
			}
			glUniformMatrix4x3fv(animation_program_bones, bind_to_world.size(), GL_FALSE, glm::value_ptr(bind_to_world[0]));

			glBindVertexArray(animation_binding->array);

			glDrawArrays(GL_TRIANGLES, 0, animation->vertex_count);
		}
	};
	renderAll(scene.lights, player, cop);

	float aspect = drawable_size.x / float(drawable_size.y);
	// scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(0.55f / aspect, 0.55f);
	glm::mat4 projection = glm::mat4(glm::vec4(scale.x, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
																	 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	glUseProgram(menuProgram->program);
	glBindVertexArray(menuBinding->array);

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
				glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(menuProgramColor, color.r, color.g, color.b);

				MeshBuffer::Mesh const& mesh = menuMeshes->lookup(word.substr(i, 1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(word[i]);
		}
	};

	static const MeshBuffer::Mesh& rect = menuMeshes->lookup("Button");
	static auto drawRect = [&](const glm::mat4& mvp, const glm::vec3 color) {
		glUseProgram(menuProgram->program);
		glBindVertexArray(menuBinding->array);

		glUniformMatrix4fv(menuProgramMVP, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniform3f(menuProgramColor, color.x, color.y, color.z);

		glDrawArrays(GL_TRIANGLES, rect.start, rect.count);
	};

	for (auto& person : Person::people) {
		static const glm::vec3 iconScale = {0.01f, 0.1f, 0.1f};
		static const float height = 1.1f;	// above player

		if (person->robbed) {
			glm::mat4 local_to_world =
					make_local_to_parent(person->pos + glm::vec3(0.0f, 0.0f, height), person->rot, iconScale);
			// compute modelview+projection (object space to clip space) matrix for this object:
			glm::mat4 mvp = world_to_clip * local_to_world;

			drawRect(mvp, {0.0f, 1.0f, 0.0f});
		} else if (counter < 7.5f && person->playerClass >= 9) {	// show richest targets
			glm::mat4 local_to_world =
					make_local_to_parent(person->pos + glm::vec3(0.0f, 0.0f, height), person->rot, iconScale);
			// compute modelview+projection (object space to clip space) matrix for this object:
			glm::mat4 mvp = world_to_clip * local_to_world;

			drawRect(mvp, {1.0f, 0.0f, 0.0f});
		}
	}

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

	if (!staging->settings.localMultiplayer) {
		if (staging->player == staging->robber) {
			// robber only views
		} else {
			copButtons.draw();
		}
	} else {
		copButtons.draw();
	}

	if (gameResultPosted) {
		draw_word(endGameTxt, 0, 0);
	}

	scene.camera.aspect = drawable_size.x / float(drawable_size.y);
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.near = 0.01f;

	glDisable(GL_STENCIL_TEST);
}

void GameMode::activateSnapshot() {
	if (!gameEnded) {
		Person::freezeAll();
		player.isMoving = false;
		isSnapshotOn = true;

		later(snapshot_reset_delay, []() {
			isSnapshotOn = false;

			if (!gameEnded) {
				Person::unfreezeAll();
				player.isMoving = true;
			}
		});
	}
}

void GameMode::activateTestimony() {
	testimony_text = random_testimonies[(int)rand() % random_testimonies.size()];

	isTestimonyShowing = true;

	later(testimony_reset_delay, []() { isTestimonyShowing = false; });
}

void GameMode::activateRoadblock(int roadblock) {
	Data::collisionFramework.addBounds(Data::roadblocks[roadblock]);

	// TODO: track remaining, if none remaining disable button
}

// from https://stackoverflow.com/a/14665230
template <class callable, class... arguments>
void later(int after, callable&& f, arguments&&... args) {
	std::function<typename std::result_of<callable(arguments...)>::type()> task(
			std::bind(std::forward<callable>(f), std::forward<arguments>(args)...));

	std::thread([after, task]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(after));
		task();
	})
			.detach();
}
