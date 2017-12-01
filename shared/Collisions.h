#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <stdio.h>

struct BBox{
	glm::vec2 minPt,maxPt,center;
	BBox(){minPt = maxPt = center = glm::vec2();}
	BBox(glm::vec2 Min,glm::vec2 Max){
		minPt = Min;
		maxPt = Max;
		center = 0.5f*(Max+Min);
	}
};

struct Hit{
	bool hit;
	glm::vec2 pt;
	BBox closest;
	Hit(){hit=false;}
	Hit(glm::vec2 pt){hit=true;this->pt = pt;}
	Hit(glm::vec2 pt,BBox box){hit=true;this->pt=pt;closest=box;}
};

class Collision{
public:
	BBox board;
	std::vector<BBox> inBounds;

	Collision(BBox board){
		this->board = board;
	}
	void addBounds(BBox box){
		inBounds.push_back(box);
	}
	Hit checkHit(glm::vec2 pt){
		return checkHit(BBox(pt,pt));
	}
	
	Hit AABBCollision(BBox bcheck, BBox box){
		if(bcheck.minPt.x < box.maxPt.x && 
			bcheck.maxPt.x > box.minPt.x &&
			bcheck.minPt.y < box.maxPt.y &&
			bcheck.maxPt.y > box.minPt.y){
			return Hit(glm::vec2(bcheck.center));
		}else return Hit();
		return Hit();
	}

	Hit checkHit(BBox box){
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
