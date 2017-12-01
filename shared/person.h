#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include "Collisions.h"

#include <stdlib.h>
#include <time.h>
#include <functional>

#define NUM_PLAYER_CLASSES 10
#define NUM_PLAYER_CLASSES_STR "10"
class Person {
 public:
	static std::vector<Person*> people;
	static std::function<float()> random;
	// static GLint personIdx, colors;
	static glm::vec3 PeopleColors[NUM_PLAYER_CLASSES];

	/* TODO: fuuuuuture
	struct Outfit{
		Scene::Object* head, body, legs;
	};
	static std::vector<Outfit*> outfits;
	void changeOutfit();*/

	int playerClass;
	bool humanControlled;
	bool isMoving;
	bool isVisible;
	bool isAI;
	time_t savedTime;
	glm::vec3 pos, vel, scale;
	glm::quat rot;

	Person() {
		humanControlled = false;
		isMoving = true;
		isVisible = true;
		isAI = false;
		vel = glm::vec3();
		playerClass = -1;
	}

	glm::vec3 randVel();
	void placeInScene(Collision* col = NULL);
	void setVel(bool up, bool down, bool left, bool right);
	void move(float eps, Collision* col = NULL);

	static void moveAll(float eps, Collision* col = NULL) {
		for (auto const& person : people)
			person->move(eps, col);
	}
	static void freezeAll() {
		for (auto const& person : people)
			person->isMoving = false;
	}
	static void unfreezeAll() {
		for (auto const& person : people)
			person->isMoving = true;
	}

 protected:
	void checkCollision();
};

Person* makeAI();
