#include "Draw.hpp"
#include "GameMode.hpp"

#include "Load.hpp"
#include "read_chunk.hpp"
#include "MeshBuffer.hpp"
#include "GLProgram.hpp"
#include "GLVertexArray.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>


#include <fstream>
#include <algorithm>

#include "person.h"
#include "Collisions.h"

bool isSnapshotOn = false;
float randX;
float randY;
SDL_TimerID snapshot_timer;
SDL_TimerID reset_snapshot_timer;
Uint32 snapshot_delay = 10000;
Uint32 snapshot_reset_delay = 2000;
Collision collisionFramework = Collision(BBox(glm::vec2(-1.92,-7.107),glm::vec2(6.348,9.775)));

Uint32 resetSnapShot(Uint32 interval, void *param){
    isSnapshotOn = false;
    
    SDL_RemoveTimer(reset_snapshot_timer);
    
    return interval;
}

Uint32 enableSnapShot(Uint32 interval, void *param){
    
    randX = (rand()%10 - 10)/10.0f;
    randY = (rand()%10)/10.0f;
    
    isSnapshotOn = true;
    
    reset_snapshot_timer = SDL_AddTimer(snapshot_reset_delay,resetSnapShot,NULL);
    
    return interval;
}

Load<MeshBuffer> meshes(LoadTagInit, []() { return new MeshBuffer("city.pnc"); });

// Attrib locations in game_program:
GLint game_program_Position = -1;
GLint game_program_Normal = -1;
GLint game_program_Color = -1;
// Uniform locations in game_program:
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
			"	color = Color;\n"
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
	game_program_mv = ret->getUniformLocation("mv", GLProgram::MissingIsWarning);
	game_program_itmv = ret->getUniformLocation("itmv", GLProgram::MissingIsWarning);
	game_program_roughness = (*ret)["roughness"];

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

void snapShot(float x,float y){
    
    glEnable( GL_STENCIL_TEST );
    glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
    glDepthMask(GL_FALSE);
    glStencilFunc( GL_NEVER, 1, 1 );
    glStencilOp( GL_REPLACE, GL_KEEP, GL_KEEP );
    
    glStencilMask(0xFF);
    glClear(GL_STENCIL_BUFFER_BIT);
    
    Draw draw;
    
    draw.add_rectangle(glm::vec2(x,y),glm::vec2(x+0.75f,y-0.75f),glm::u8vec4(0x00, 0x00, 0x00, 0xff));
    draw.draw();
    
    glStencilMask(0x00);
    glStencilFunc( GL_EQUAL, 1, 1 );
}

GameMode::GameMode() {
	auto add_object = [&](std::string const& name, glm::vec3 const& position, glm::quat const& rotation,
												glm::vec3 const& scale) {
		scene.objects.emplace_back();
		Scene::Object& object = scene.objects.back();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;

		MeshBuffer::Mesh const& mesh = meshes->lookup(name);
		object.program = game_program->program;
		object.program_mvp = game_program_mvp;
		object.program_mv = game_program_mv;
		object.program_itmv = game_program_itmv;
		object.vao = binding->array;
		object.start = mesh.start;
		object.count = mesh.count;

		object.set_uniforms = [](Scene::Object const&) { glUniform1f(game_program_roughness, 1.0f); };
		return &object;
	};
    
    snapshot_timer = SDL_AddTimer(snapshot_delay,enableSnapShot,NULL);

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
				add_object(name, entry.position, entry.rotation, entry.scale);
			}
		}
	}
	//TODO: asset pipeline bounding boxes. Hard coded for now.
	//these are the four houses
	collisionFramework.addBounds(BBox(glm::vec2(0.719,-1.003),glm::vec2(2.763,2.991)));
	collisionFramework.addBounds(BBox(glm::vec2(4.753,-2.318),glm::vec2(6.737,-0.24)));
	collisionFramework.addBounds(BBox(glm::vec2(4.451,6.721),glm::vec2(9.929,8.799)));
	collisionFramework.addBounds(BBox(glm::vec2(0.247,7.31),glm::vec2(2.397,9.986)));

	int numPlayers = 50;
	srand(time(NULL));
	for(int i=0;i<numPlayers;i++){
		Scene::Object* obj = add_object("lowman_shoes.001",glm::vec3(),glm::angleAxis(glm::radians(90.f),glm::vec3(1,0,0)),glm::vec3(0.012,0.012,0.012));
		makeAI(obj)->placeInScene(); //Will be kept track of by class
	}
}

void GameMode::reset() {}

bool GameMode::handle_event(SDL_Event const& e, glm::uvec2 const& window_size) {
	if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
		Packet* input = new Packet();

		input->payload.push_back(MessageType::INPUT);
		input->payload.push_back(e.type == SDL_KEYUP ? 1 : 0);

		// assume these can fit in a byte each
		input->payload.push_back(e.key.keysym.sym);
		input->payload.push_back(e.key.keysym.scancode);

		input->header = input->payload.size();

		sock->writeQueue.enqueue(input);
        
        switch(e.key.keysym.sym){
            case SDLK_LEFT:
                if(isSnapshotOn==false){
                    randX = (rand()%10 - 10)/10.0f;
                    randY = (rand()%10)/10.0f;
                    
                }
                isSnapshotOn = true;
                break;
            default:
                break;
        }
	}

	// temporary mouse movement for camera
	if (e.type == SDL_MOUSEMOTION) {
		static glm::vec2 mouse = glm::vec2(0.0f, 0.0f);
		glm::vec2 old_mouse = mouse;
		mouse.x = (e.motion.x + 0.5f) / float(640) * 2.0f - 1.0f;
		mouse.y = (e.motion.y + 0.5f) / float(480) *-2.0f + 1.0f;
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			camera.elevation += -2.0f * (mouse.y - old_mouse.y);
			camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
		}
		
	}else if(e.type == SDL_KEYDOWN){
		if(e.key.keysym.sym == SDLK_TAB) camera.radius++;
		else if(e.key.keysym.sym == SDLK_LSHIFT) camera.radius--;
	}


	if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
		if (show_menu)
			show_menu();
		return true;
	}
	return false;
}

void GameMode::update(float elapsed) {
	static uint8_t const* keys = SDL_GetKeyboardState(NULL);
	(void)keys;

	Person::moveAll(elapsed,&collisionFramework);
	Packet* out;
	while (sock->readQueue.try_dequeue(out)) {
		if (!out) {
			std::cout << "Bad packet from server" << std::endl;
			continue;
		}

		std::cout << "Message from server: ";
		for (const auto& thing : out->payload) {
			printf("%x ", thing);
		}
		std::cout << std::endl;

		delete out;
	}
}

void GameMode::draw(glm::uvec2 const& drawable_size) {
	scene.camera.aspect = drawable_size.x / float(drawable_size.y);
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.near = 0.01f;

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
    
    if(isSnapshotOn){
        snapShot(randX,randY);
    }
    
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	scene.render();
    
    glDisable(GL_STENCIL_TEST);
}
