#include "person.h"
#include <stdio.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

const glm::vec3 Person::BASE_SCALE = glm::vec3(0.12f);
int Person::ANIMATION_FRAMES = 160;	// animation->frame_bones.size() / animation->bones.size()

std::vector<Person*> Person::people = std::vector<Person*>();
glm::vec3 Person::PeopleColors[NUM_PLAYER_CLASSES] = {glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,1),glm::vec3(1,1,0),glm::vec3(1,0,1),glm::vec3(0,1,1),glm::vec3(1,1,1),glm::vec3(0,0,0),glm::vec3(1,0.5,0),glm::vec3(1,0.75,0.75)};
std::string Person::colorNames[NUM_PLAYER_CLASSES] = {"RED","GREEN","BLUE","YELLOW","PURPLE","CYAN","WHITE","BLACK","ORANGE","PINK"};
std::function<float()> Person::random;
// GLint Person::personIdx = -1;
// GLint Person::colors = -1;

glm::vec3 Person::randVel() {
	// civilians need to match how player moves
	float rand1 = int(3 * Person::random()) - 1;	// either -1,0,or 1
	float rand2 = int(3 * Person::random()) - 1;	// either -1,0,or 1

	movingFrames = 0;
	// between half second and 5 seconds
	directionFrames = 30 + (int)(4.5f * 60.0f * Person::random());

	return glm::normalize(glm::vec3(rand1, rand2, 0));
}

Person* makeAI() {
	char* start = (char*)malloc(sizeof(Person));
	Person* pers = new (start) Person();
	Person::people.push_back(pers);
	pers->vel = pers->randVel();
	pers->isAI = true;
	return pers;
}

#define EPS 0.01f
void Person::placeInScene(Collision* col) {
	pos = glm::vec3();
	animationFrameIdx = 0;
	if (col) {
		BBox2D bounds = col->board;
		do {
			pos.x = bounds.minPt.x + EPS + (Person::random() - EPS) * (bounds.maxPt.x - bounds.minPt.x);
			pos.y = bounds.minPt.y + EPS + (Person::random() - EPS) * (bounds.maxPt.y - bounds.minPt.y);
		} while (col->checkHit(glm::vec2(pos.x, pos.y)).hit);
	}

	playerClass = int(Person::random() * NUM_PLAYER_CLASSES);	// give random player class
}

// set player velocity based on controls
void Person::setVel(bool up, bool down, bool left, bool right) {
	vel = glm::vec3(0.0f);

	if (up)
		vel.x += 1.0f;
	if (down)
		vel.x -= 1.0f;
	if (left)
		vel.y += 1.0f;
	if (right)
		vel.y -= 1.0f;

	if (vel.x != 0.0f || vel.y != 0.0f) {
		vel = glm::normalize(vel);
	}
}

void Person::move(float eps, Collision* col) {
	if (isMoving) {
		Hit hit;
		glm::vec2 newpt = glm::vec2(pos.x + vel.x * eps, pos.y + vel.y * eps);
		if (col && (hit = col->checkHit(newpt)).hit) {
			vel = isAI ? randVel() : glm::vec3();
			animationFrameIdx = 0;
		} else {
			pos += vel * eps;
			animationFrameIdx = (animationFrameIdx + 2) % ANIMATION_FRAMES;
		}

		if (glm::length(vel) > 0.1) {
			float angle = atan2(vel.x, -vel.y);
			rot = glm::normalize(glm::angleAxis(angle, glm::vec3(0, 0, 1)) /* *
													 glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0))*/);

			if (isAI) {
				if (movingFrames >= directionFrames) {
					vel = randVel();
				}
			}
		}
		movingFrames++;
	}
}

void Person::rob() {
	robbed = true;
	// TODO: take away money
}
