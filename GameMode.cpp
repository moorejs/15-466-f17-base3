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

#include "Collisions.h"
#include "person.h"



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
SDL_TimerID game_timer;

float player_move_speed = 0.1f;

Uint32 snapshot_delay = 20000;
Uint32 testimony_delay = 12000;
Uint32 testimony_reset_delay = 4000;
Uint32 snapshot_reset_delay = 4000;
Uint32 game_time = 2000;
Collision collisionFramework = Collision(BBox(glm::vec2(-1.92, -7.107), glm::vec2(6.348, 9.775)));

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

    
    if(!gameEnded){
        Person::unfreezeAll();
        player.isMoving = true;
    }

	SDL_RemoveTimer(reset_snapshot_timer);

	return interval;
}

Uint32 enableSnapShot(Uint32 interval = 0, void* param = nullptr) {
    if(!gameEnded){
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
		person->meshObject.transform.scale = glm::vec3(0.012f, 0.012f, 0.012f);
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

void spawnCop(){
    cop.meshObject.transform.scale = 0.018f*glm::vec3(1,1,1);
    cop.meshObject.transform.position = glm::vec3(-1.0f,5.5f,0.0f);
    cop.isMoving = true;
}

Uint32 endGame(Uint32 interval = 0, void* param = nullptr) {
    
    gameEnded = true;
    Person::freezeAll();
    
    player.isMoving = false;
    
    SDL_RemoveTimer(game_timer);
    
    spawnCop();
    
    return interval;
}

Load<MeshBuffer> meshes(LoadTagInit, []() { return new MeshBuffer("city.pnc"); });

Load<MeshBuffer> word_meshes(LoadTagInit, []() { return new MeshBuffer("menu.p"); });

//Menu program itself:
Load<GLProgram> word_program(LoadTagInit, [](){
		GLProgram *ret = new GLProgram(
			 "#version 330\n"
			 "uniform mat4 mvp;\n"
			 "in vec4 Position;\n"
			 "void main() {\n"
			 "	gl_Position = mvp * Position;\n"
			 "}\n"
			 ,
			 "#version 330\n"
			 "uniform vec3 color;\n"
			 "out vec4 fragColor;\n"
			 "void main() {\n"
			 "	fragColor = vec4(color, 1.0);\n"
			 "}\n"
			 );

		word_program_Position = (*ret)("Position");
		word_program_mvp = (*ret)["mvp"];
		word_program_color = (*ret)["color"];
		
		return ret;
});

//Binding for using staging_program on staging_meshes:
Load<GLVertexArray> word_binding(LoadTagDefault, [](){
	GLVertexArray *ret = new GLVertexArray(GLVertexArray::make_binding(word_program->program, {
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
			"uniform mat4 mvp;\n"	// model positions to clip space
			"uniform mat4x3 mv;\n"	// model positions to lighting space
			"uniform mat3 itmv;\n"	// model normals to lighting space
			"uniform int index;\n" //person class idx
			"uniform vec3 people_colors[" NUM_PLAYER_CLASSES_STR "];\n"
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
			"		color.rgb\n"   // Lambertain Diffuse
			"		+ pow(max(0.0, dot(n, h)), shininess)\n" // Blinn-Phong Specular
			"		* (shininess + 2.0) / (8.0)\n"// normalization factor (tweaked for hemisphere)
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
	game_program_index = ret->getUniformLocation("index",GLProgram::MissingIsWarning);
	game_program_mv = ret->getUniformLocation("mv", GLProgram::MissingIsWarning);
	game_program_itmv = ret->getUniformLocation("itmv", GLProgram::MissingIsWarning);
	game_program_roughness = (*ret)["roughness"];
	Person::personIdx = ret->getUniformLocation("index",GLProgram::MissingIsWarning);
	Person::colors = ret->getUniformLocation("people_colors",GLProgram::MissingIsWarning);
	return ret;
});

// Binding for using game_program on meshes:
Load<GLVertexArray> binding(LoadTagDefault, []() {
	GLVertexArray* ret = new GLVertexArray(GLVertexArray::make_binding(game_program->program,
							 {{game_program_Position, meshes->Position},
								{game_program_Normal, meshes->Normal},
								{game_program_Color, meshes->Color}}));
	return ret;
});

//------------------------------

void snapShot(float x, float y, Scene* scene) {
    if(!gameEnded){
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
    
    game_timer = SDL_AddTimer(game_time, endGame, NULL);

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
	collisionFramework.addBounds(BBox(glm::vec2(0.719, -1.003), glm::vec2(2.763, 2.991)));
	collisionFramework.addBounds(BBox(glm::vec2(4.753, -2.318), glm::vec2(6.737, -0.24)));
	collisionFramework.addBounds(BBox(glm::vec2(4.451, 6.721), glm::vec2(9.929, 8.799)));
	collisionFramework.addBounds(BBox(glm::vec2(0.247, 7.31), glm::vec2(2.397, 9.986)));

	snapshotBtn.pos = glm::vec2(-0.5f, -0.92f);
	snapshotBtn.rad = glm::vec2(0.5f, 0.05f);
	snapshotBtn.label = "SNAPSHOT";
	snapshotBtn.isEnabled = [&]() { return true; };	// state.powerTimer > 30.0f };
	snapshotBtn.onFire = [&]() {
		// sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_SELECT_POWER, {0}));
		enableSnapShot();
		state.powerTimer = 0;
	};

	anonymousTipBtn.pos = glm::vec2(0.5f, -0.92f);
	anonymousTipBtn.rad = glm::vec2(0.5f, 0.05f);
	anonymousTipBtn.label = "ANON TIP";
	anonymousTipBtn.isEnabled = [&]() { return true; };	// state.powerTimer > 30.0f };
	anonymousTipBtn.onFire = [&]() {
		// sock->writeQueue.enqueue(Packet::pack(MessageType::GAME_SELECT_POWER, {0}));
		showTestimony();
		state.powerTimer = 0;
	};
}

void GameMode::reset(int seed) {
	twister.seed(seed);

	int numPlayers = 50;

	Person::random = rand;
	MeshBuffer::Mesh const& mesh = meshes->lookup("lowman_shoes.001");
	for (int i=0;i<=numPlayers;i++) {
		Person* cur;
		if(i==numPlayers){
			player = Person();
			cur = &player;
		}
        else if(i==numPlayers-1){
            cop = Person();
            cur = &cop;
        }
        else cur = makeAI();
		cur->meshObject.transform.rotation = glm::angleAxis(glm::radians(90.f),glm::vec3(1,0,0));
		cur->meshObject.transform.scale = 0.012f*glm::vec3(1,1,1);
        
		cur->meshObject.program = game_program->program;
		cur->meshObject.program_mvp = game_program_mvp;
		cur->meshObject.program_mv = game_program_mv;
		cur->meshObject.program_itmv = game_program_itmv;
		cur->meshObject.vao = binding->array;
		cur->meshObject.start = mesh.start;
		cur->meshObject.count = mesh.count;
		cur->meshObject.set_uniforms = [](Scene::Object const&) { glUniform1f(game_program_roughness, 1.0f); };
		cur->placeInScene((i>=numPlayers-1)? NULL : &collisionFramework);
        
        //cop
        if(i==numPlayers-1){
            cur->meshObject.transform.scale = 0.0f*glm::vec3(1,1,1);
        }
	}
}

Scene::Object* GameMode::addObject(std::string const& name, glm::vec3 const& position, glm::quat const& rotation, glm::vec3 const& scale) {
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

bool GameMode::handle_event(SDL_Event const& e, glm::uvec2 const& window_size) {
	if (sock && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)) {
		Packet* input = new Packet();

		input->payload.push_back(MessageType::INPUT);
		input->payload.push_back(e.type == SDL_KEYUP ? 1 : 0);

		// assume these can fit in a byte each
		input->payload.push_back(e.key.keysym.sym);
		input->payload.push_back(e.key.keysym.scancode);

		input->header = input->payload.size();

		sock->writeQueue.enqueue(input);

		switch (e.key.keysym.sym) {
			case SDLK_LEFT:
				break;
			default:
				break;
		}
	}
    
    double minDistance = 1000;
    auto *closestPerson = new Person;
    
	if (e.type == SDL_KEYDOWN) {
		switch (e.key.keysym.sym) {
			case SDLK_x:
                
                
				for (auto person : Person::people) {
					float distancex = pow(person->pos.x - player.pos.x, 2.0f);
					float distancey = pow(person->pos.y - player.pos.y, 2.0f);

					double calcdistance = pow(distancex + distancey, 0.5f);
                    
                    if(calcdistance<minDistance){
                        closestPerson = &*person;
                        minDistance = calcdistance;
                    }
					
				}
                
                if(minDistance<0.4f){
                    closestPerson->meshObject.transform.scale = glm::vec3(0.02f, 0.02f, 0.02f);
                    reset_scale_timer = SDL_AddTimer(2000, resetScales, NULL);
                }
                
				break;
            case SDLK_SPACE:
                //Catch thief
                if(gameEnded && !gameResultPosted){
                    float distancex = pow(cop.pos.x - player.pos.x, 2.0f);
                    float distancey = pow(cop.pos.y - player.pos.y, 2.0f);
                    
                    double calcdistance = pow(distancex + distancey, 0.5f);
                    
                    gameResultPosted = true;
                    
                    if(calcdistance<0.3f){
                        endGameTxt = "THIEF IDENTIFIED";
                    }
                    else{
                        endGameTxt = "COPS LOST";
                    }
                    
                    player.meshObject.transform.scale = 0.018f*glm::vec3(1,1,1);
                }
                //DEPLOY COP
                else{
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
		mouse.x = (e.motion.x + 0.5f) / float(640) * 2.0f - 1.0f;
		mouse.y = (e.motion.y + 0.5f) / float(400) * -2.0f + 1.0f;
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			camera.elevation += -2.0f * (mouse.y - old_mouse.y);
			camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
		}
		snapshotBtn.hover = snapshotBtn.contains(mouse);
		anonymousTipBtn.hover = anonymousTipBtn.contains(mouse);
	} else if (e.type == SDL_MOUSEBUTTONDOWN) {
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
	static uint8_t const* keys = SDL_GetKeyboardState(NULL);
	(void)keys;
	glm::vec3 playerVel = glm::vec3();
    glm::vec3 copVel = glm::vec3();
    
	if(keys[SDL_SCANCODE_W]) playerVel += glm::vec3(1,0,0);
	if(keys[SDL_SCANCODE_S]) playerVel += glm::vec3(-1,0,0);
	if(keys[SDL_SCANCODE_A]) playerVel += glm::vec3(0,1,0);
	if(keys[SDL_SCANCODE_D]) playerVel += glm::vec3(0,-1,0);
	player.vel = glm::normalize(playerVel);
    
    if(keys[SDL_SCANCODE_UP] && gameEnded) copVel += glm::vec3(1,0,0);
    if(keys[SDL_SCANCODE_DOWN] && gameEnded) copVel += glm::vec3(-1,0,0);
    if(keys[SDL_SCANCODE_LEFT] && gameEnded) copVel += glm::vec3(0,1,0);
    if(keys[SDL_SCANCODE_RIGHT] && gameEnded) copVel += glm::vec3(0,-1,0);
    cop.vel = glm::normalize(copVel);
    cop.move(elapsed,&collisionFramework);

    
	Person::moveAll(elapsed,&collisionFramework);
	player.move(elapsed,&collisionFramework);

	if (!sock) {
		return;
	}

	Packet* out;
	while (sock->readQueue.try_dequeue(out)) {
		if (!out) {
			std::cout << "Bad packet from server" << std::endl;
			continue;
		}

		delete out;
	}
}

/*
struct Ray{glm::vec3 start,dir;};
	Ray getCameraRay(glm::vec2 pt){
		glm::vec3 localpt = focal length / pt / fov;
		glm::mat4 mat = camera.transform.get_local_to_world();
		glm::vec4 world4 = mat*glm::vec4(localpt.x,localpt.y,localpt.z,1);
		glm::vec3 world3 = glm::vec4(world4.x,world4.y,world4.z)/world4.w;

		Ray r;
		r.start = camera.transform.position;
		r.dir = glm::normalize(world3-r.start);
	}
*/
void GameMode::draw(glm::uvec2 const& drawable_size) {	
	// camera:
	scene.camera.transform.position = camera.radius * glm::vec3(
		std::cos(camera.elevation) * std::cos(camera.azimuth),
		std::cos(camera.elevation) * std::sin(camera.azimuth),
		std::sin(camera.elevation)) + camera.target;

	glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
	glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
	up = glm::normalize(up - glm::dot(up, out) * out);
	glm::vec3 right = glm::cross(up, out);

	scene.camera.transform.rotation = glm::quat_cast(glm::mat3(right, up, out));
	scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	static bool prevSnapshotOn = false;
	static float offset = rand()*0.3f; // TODO: server will just send a coord, none of this
	static glm::vec2 pos = glm::vec2(player.pos.y / -9.0f - offset, player.pos.x / 6.0f + offset);
	if (!prevSnapshotOn && isSnapshotOn) { // first frame
		offset = rand() * 0.4f + 0.1f;
		pos = glm::vec2(player.pos.y / -9.0f - offset, player.pos.x / 6.0f + offset);
	}
	if(isSnapshotOn){
			snapShot(pos.x, pos.y,&scene);
			prevSnapshotOn = true;
	} else {
		prevSnapshotOn = false;
	}
	
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	scene.render();
	Person::renderAll(scene.camera, scene.lights,player,cop);
	float aspect = drawable_size.x / float(drawable_size.y);
	//scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(0.55f / aspect, 0.55f);
	glm::mat4 projection = glm::mat4(
					 glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
					 glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
					 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
					 glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
					 );
	glUseProgram(word_program->program);
	glBindVertexArray(word_binding->array);


	static auto draw_word = [&projection](const std::string& word, float y) {
			//character width and spacing helpers:
			// (...in terms of the menu font's default 3-unit height)
			auto width = [](char a) {
					if (a == 'I') return 1.0f;
					else if (a == 'L') return 2.0f;
					else if (a == 'M' || a == 'W') return 4.0f;
					else return 3.0f;
			};
			auto spacing = [](char a, char b) {
					return 1.0f;
			};
			
			float total_width = 0.0f;
			for (uint32_t i = 0; i < word.size(); ++i) {
					if (i > 0) total_width += spacing(word[i-1], word[i]);
					total_width += width(word[i]);
			}
			
			float x = -0.5f * total_width;
			for (uint32_t i = 0; i < word.size(); ++i) {
					if (i > 0) {
							x += spacing(word[i], word[i-1]);
					}
					
					if (word[i] != ' ') {
							float s = 0.1f * (1.0f / 3.0f);
							glm::mat4 mvp = projection * glm::mat4(
										 glm::vec4(s, 0.0f, 0.0f, 0.0f),
										 glm::vec4(0.0f, s, 0.0f, 0.0f),
										 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
										 glm::vec4(s * x, y, 0.0f, 1.0f)
																										 );
							glUniformMatrix4fv(word_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
							glUniform3f(word_program_color, 1.0f, 1.0f, 1.0f);
							
							MeshBuffer::Mesh const &mesh = word_meshes->lookup(word.substr(i, 1));
							glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
					}
					
					x += width(word[i]);
			}
	};
	static auto draw_word_xy = [&projection](const std::string& word, float init_x, float y) {
			//character width and spacing helpers:
			// (...in terms of the menu font's default 3-unit height)
			auto width = [](char a) {
					if (a == 'I') return 1.0f;
					else if (a == 'L') return 2.0f;
					else if (a == 'M' || a == 'W') return 4.0f;
					else return 3.0f;
			};
			auto spacing = [](char a, char b) {
					return 1.0f;
			};
			
			float total_width = 0.0f;
			for (uint32_t i = 0; i < word.size(); ++i) {
					if (i > 0) total_width += spacing(word[i-1], word[i]);
					total_width += width(word[i]);
			}
			
			float x = -0.5f * total_width;
			for (uint32_t i = 0; i < word.size(); ++i) {
					if (i > 0) {
							x += spacing(word[i], word[i-1]);
					}
					
					if (word[i] != ' ') {
							float s = 0.1f * (1.0f / 3.0f);
							glm::mat4 mvp = projection * glm::mat4(
										 glm::vec4(s, 0.0f, 0.0f, 0.0f),
										 glm::vec4(0.0f, s, 0.0f, 0.0f),
										 glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
										 glm::vec4(2*init_x+s * x, 2*y+0.1, 0.0f, 1.0f)
																										 );
							glUniformMatrix4fv(word_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
							glUniform3f(word_program_color, 1.0f, 1.0f, 1.0f);
							
							MeshBuffer::Mesh const &mesh = word_meshes->lookup(word.substr(i, 1));
							glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
					}
					
					x += width(word[i]);
			}
	};
	
	if(isTestimonyShowing){
		draw_word("ANONYMOUS TIP", -1.25f);
		draw_word("SUSPICIOUS PERSON "+testimony_text+" REPORTED", -1.5f);
	}
	draw_word_xy(snapshotBtn.label, snapshotBtn.pos.x, snapshotBtn.pos.y);
	draw_word_xy(anonymousTipBtn.label,  anonymousTipBtn.pos.x,anonymousTipBtn.pos.y);
    
    if(gameResultPosted){
        draw_word_xy(endGameTxt,0,-0.7f);
    }
	
	
	scene.camera.aspect = drawable_size.x / float(drawable_size.y);
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.near = 0.01f;
	
	glDisable(GL_STENCIL_TEST);
}
