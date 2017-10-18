#include "GameMode.hpp"

#include "Load.hpp"
#include "read_chunk.hpp"
#include "MeshBuffer.hpp"
#include "GLProgram.hpp"
#include "GLVertexArray.hpp"

#include <fstream>
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490"

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

// Menu program itself:
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

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

GameMode::GameMode() {
	// Basic client code structure from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleclient

	// TODO: this constructor must be called earlier than expected, the client connects immediately

	int sockfd;	//, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("::1", PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		// TODO: what now?
		return;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		// TODO: what now?
		return;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo);	// all done with this structure

	/*
	if ((numbytes = recv(sockfd, buf,  - 1, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);
	*/

	close(sockfd);

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

	};

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

	// set up for first round:
	reset();
}

void GameMode::reset() {}

bool GameMode::handle_event(SDL_Event const& e, glm::uvec2 const& window_size) {
	if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
		if (show_menu)
			show_menu();
		return true;
	}
	return false;
}

void GameMode::update(float elapsed) {
	static Uint8 const* keys = SDL_GetKeyboardState(NULL);
	(void)keys;
}

void GameMode::draw(glm::uvec2 const& drawable_size) {
	scene.camera.aspect = drawable_size.x / float(drawable_size.y);
	scene.camera.fovy = 25.0f / 180.0f * 3.1415926f;
	scene.camera.transform.rotation = glm::angleAxis(17.0f / 180.0f * 3.1415926f, glm::vec3(1.0f, 0.0f, 0.0f));
	scene.camera.transform.position = glm::vec3(0.0f, -3.3f, 10.49f);
	scene.camera.near = 4.0f;

	scene.render();
}
