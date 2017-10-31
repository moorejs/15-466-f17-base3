#include "person.h"
#include <stdio.h>

std::vector<Person*> Person::people = std::vector<Person*>();
std::function<float()> Person::random;

Person* makeAI(Scene::Object* obj){
	Person* pers = (Person*) malloc(sizeof(Person));
	*pers  = Person(obj);
	Person::people.push_back(pers);
	return pers;
}

glm::vec3 randVel(){
	const static float maxVel = 2.f;
	float rand1 = 2*(Person::random()-0.5f);
	float rand2 = 2*(Person::random()-0.5f);
	return glm::vec3(maxVel*rand1,maxVel*rand2,0);
}

void Person::placeInScene(){
	pos = glm::vec3();
	vel = randVel();
	meshObject->transform.position = pos;
}
void Person::move(float eps,Collision* col){
	Hit hit;
	glm::vec2 newpt = glm::vec2(pos.x+vel.x*eps,pos.y+vel.y*eps);
	if(col && (hit=col->checkHit(newpt)).hit){
			vel = randVel();
	}else{
		pos += vel*eps;
		vel += acc*eps;
	}

	meshObject->transform.position = pos;
}
