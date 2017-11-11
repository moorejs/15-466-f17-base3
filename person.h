#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Scene.hpp"
#include "Collisions.h"

#include <stdlib.h>
#include <time.h>
#include <functional>

class Person{
public:
	static std::vector<Person*> people;
	static std::function<float()> random;
	/* TODO: fuuuuuture
	struct Outfit{
		Scene::Object* head, body, legs;
	};
	static std::vector<Outfit*> outfits;
	void changeOutfit();*/

	bool humanControlled;
	bool isMoving;
	bool isVisible;
	bool isAI;
	glm::vec3 pos,vel,acc;
	Scene::Object* meshObject;
	glm::quat rot;
	
	Person(){}
	
	Person(Scene::Object* obj){
		meshObject = obj;
		rot = obj->transform.rotation;
		humanControlled = false;
		isMoving = true;
		isVisible = true;
		isAI = false;
		vel = glm::vec3();
		acc = glm::vec3();
		placeInScene();
	}

	void placeInScene();
	void move(float eps,Collision* col=NULL);
	void draw();

	static void moveAll(float eps,Collision* col=NULL){
		for(auto const& person : people) person->move(eps,col);
	}
	static void freezeAll(){
		for(auto const& person : people) person->isMoving = false;
	}
	static void unfreezeAll(){
		for(auto const& person : people) person->isMoving = true;
	}
protected:
	void checkCollision();
};

Person* makeAI(Scene::Object* obj);
