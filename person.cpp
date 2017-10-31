#include "person.h"
#include <stdio.h>

std::vector<Person*> Person::people = std::vector<Person*>();




glm::vec3 randVel(){
	const static float maxVel = 2.f;
	float rand1 = 2*(float(rand())/RAND_MAX-0.5);
	float rand2 = 2*(float(rand())/RAND_MAX-0.5);
	return glm::vec3(maxVel*rand1,maxVel*rand2,0);
}

Person* makeAI(Scene::Object* obj){
	Person* pers = (Person*) malloc(sizeof(Person));
	*pers  = Person(obj);	
	Person::people.push_back(pers);
	pers->vel = randVel();
	pers->isAI = true;
	return pers;
}

void Person::placeInScene(){
	pos = glm::vec3();
	meshObject->transform.position = pos;
}
void Person::move(float eps,Collision* col){
	Hit hit;
	glm::vec2 newpt = glm::vec2(pos.x+vel.x*eps,pos.y+vel.y*eps);
	if(col && (hit=col->checkHit(newpt)).hit){
			vel = isAI? randVel() : glm::vec3();
			acc = glm::vec3();
	}else{
		pos += vel*eps;
		vel += acc*eps;
		if(!isAI) vel *= 0.999;
	}

	meshObject->transform.position = pos;
}
