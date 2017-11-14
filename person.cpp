#include "person.h"
#include <stdio.h>


#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


std::vector<Person*> Person::people = std::vector<Person*>();
glm::vec3 Person::PeopleColors[NUM_PLAYER_CLASSES] = {glm::vec3()};
std::function<float()> Person::random;
GLint Person::personIdx=-1;
GLint Person::colors=-1;

glm::vec3 Person::randVel(){
	//civilians need to match how player moves
	float rand1 = int(3*Person::random())-1; //either -1,0,or 1
	float rand2 = int(3*Person::random())-1; //either -1,0,or 1
	time(&savedTime);
	return glm::normalize(glm::vec3(rand1,rand2,0));
}

Person* makeAI(){
	char* start = (char*) malloc(sizeof(Person));
	Person* pers = new (start) Person();
	Person::people.push_back(pers);
	pers->vel = pers->randVel();
	pers->isAI = true;
	return pers;
}

bool colorsNotInit = true;
void Person::renderAll(Scene::Camera camera, std::list< Scene::Light > lights, Person player){
	glm::mat4 world_to_camera = camera.transform.make_world_to_local();
	glm::mat4 world_to_clip = camera.make_projection() * world_to_camera;

	if(colorsNotInit){
		for(int i=0;i<NUM_PLAYER_CLASSES;i++){
			Person::PeopleColors[i] = glm::vec3(random(),random(),random());
		}
		colorsNotInit = false;
	}

	//Get world-space position of all lights:
	for (auto const &light : lights) {
		glm::mat4 mv = world_to_camera * light.transform.make_local_to_world();
		(void)mv;
	}

	for (unsigned int i=0;i<=people.size();i++){
		Person* person = (i==people.size())? &player : people[i];
		glm::mat4 local_to_world = person->meshObject.transform.make_local_to_world();

		//compute modelview+projection (object space to clip space) matrix for this object:
		glm::mat4 mvp = world_to_clip * local_to_world;

		//compute modelview (object space to camera local space) matrix for this object:
		glm::mat4 mv = local_to_world;

		//NOTE: inverse cancels out transpose unless there is scale involved
		glm::mat3 itmv = glm::inverse(glm::transpose(glm::mat3(mv)));

		//set up program uniforms:
		glUseProgram(person->meshObject.program);
		if (person->meshObject.program_mvp != -1U) {
			glUniformMatrix4fv(person->meshObject.program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (person->meshObject.program_mv != -1U) {
			glUniformMatrix4x3fv(person->meshObject.program_itmv, 1, GL_FALSE, glm::value_ptr(mv));
		}
		if (person->meshObject.program_itmv != -1U) {
			glUniformMatrix3fv(person->meshObject.program_itmv, 1, GL_FALSE, glm::value_ptr(itmv));
		}
		glUniform1i(Person::personIdx,person->playerClass);
		glUniform3fv(Person::colors, NUM_PLAYER_CLASSES, (GLfloat*)Person::PeopleColors);

		if (person->meshObject.set_uniforms) person->meshObject.set_uniforms(person->meshObject);

		glBindVertexArray(person->meshObject.vao);

		//draw the object:
		glDrawArrays(GL_TRIANGLES, person->meshObject.start, person->meshObject.count);
	}
}

#define EPS 0.01f
void Person::placeInScene(Collision* col){
	pos = glm::vec3();
	if(col){
		BBox bounds = col->board;
		do{
			pos.x = bounds.minPt.x+EPS+(Person::random()-EPS)*(bounds.maxPt.x-bounds.minPt.x);
			pos.y = bounds.minPt.y+EPS+(Person::random()-EPS)*(bounds.maxPt.y-bounds.minPt.y);
		}while(col->checkHit(glm::vec2(pos.x,pos.y)).hit);
	}
	meshObject.transform.position = pos;
	rot = meshObject.transform.rotation; //track original rotation
	playerClass = int(Person::random()*NUM_PLAYER_CLASSES); //give random player class
}

void Person::move(float eps,Collision* col){
    if(isMoving){
        Hit hit;
        glm::vec2 newpt = glm::vec2(pos.x+vel.x*eps,pos.y+vel.y*eps);
        if(col && (hit=col->checkHit(newpt)).hit){
			vel = isAI? randVel() : glm::vec3();
        }else{
            pos += vel*eps;
        }

		if(glm::length(vel)>0.1){
			float angle = atan2(vel.x,-vel.y);
			meshObject.transform.rotation = glm::normalize(glm::angleAxis(angle,glm::vec3(0,0,1))*rot);

			if(isAI){
				time_t curtime;
				time(&curtime);
				if(difftime(curtime,savedTime)>1 && random()>0.99) vel = randVel();
			}
		}

        meshObject.transform.position = pos;
    }
}
