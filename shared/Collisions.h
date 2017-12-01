#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <stdio.h>

struct Ray{
  glm::vec3 start,dir;
  Ray(){
    start = glm::vec3();
    dir = glm::vec3(0,0,1); //why not
  }
  Ray(glm::vec3 start,glm::vec3 dir){
    this->start = start;
    this->dir = dir;
  }
};

struct BBox{
	glm::vec3 min,max,center;
	BBox(){min = max = center = glm::vec3();}
	BBox(glm::vec3 Min,glm::vec3 Max){
		min = Min;
		max = Max;
		center = 0.5f*(Max+Min);
	}
	bool intersects(Ray r);
};

struct BBox2D{
	glm::vec2 minPt,maxPt,center;
	BBox2D(){minPt = maxPt = center = glm::vec2();}
	BBox2D(glm::vec2 Min,glm::vec2 Max){
		minPt = Min;
		maxPt = Max;
		center = 0.5f*(Max+Min);
	}
};

struct Hit{
	bool hit;
	glm::vec2 pt;
	BBox2D closest;
	Hit(){hit=false;}
	Hit(glm::vec2 pt){hit=true;this->pt = pt;}
	Hit(glm::vec2 pt,BBox2D box){hit=true;this->pt=pt;closest=box;}
};

class Collision{
public:
	BBox2D board;
	std::vector<BBox2D> inBounds;

	Collision(BBox2D board){
		this->board = board;
	}
	void addBounds(BBox2D box){
		inBounds.push_back(box);
	}
	Hit checkHit(glm::vec2 pt){
		return checkHit(BBox2D(pt,pt));
	}
	
	Hit AABBCollision(BBox2D bcheck, BBox2D box){
		if(bcheck.minPt.x < box.maxPt.x && 
			bcheck.maxPt.x > box.minPt.x &&
			bcheck.minPt.y < box.maxPt.y &&
			bcheck.maxPt.y > box.minPt.y){
			return Hit(glm::vec2(bcheck.center));
		}else return Hit();
		return Hit();
	}

	Hit checkHit(BBox2D box){
		if(AABBCollision(box,board).hit){
		}else{
			return Hit(box.center);
		}

		Hit closest = Hit();
		for(auto& bbox : inBounds){
			Hit cur = AABBCollision(box,bbox);
			if(cur.hit){
				if(!closest.hit ||
					(glm::length(cur.pt-box.center)<glm::length(closest.pt-box.center))){
					closest = cur;
					closest.closest = bbox;
				}
			}
		}
		return closest;
	}

	
};
