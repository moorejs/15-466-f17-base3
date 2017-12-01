#include "Collisions.h"

//taken from https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection but come on its ray casting
void swap(float& a, float& b){
	float saved = a;
	a=b;
	b=saved;
}
bool BBox::intersects(Ray r) { 
    float tmin = (min.x - r.start.x) / r.dir.x; 
    float tmax = (max.x - r.start.x) / r.dir.x; 
 
    if (tmin > tmax) swap(tmin,tmax);
 
    float tymin = (min.y - r.start.y) / r.dir.y; 
    float tymax = (max.y - r.start.y) / r.dir.y; 
 
    if (tymin > tymax) swap(tymin, tymax); 
 
    if ((tmin > tymax) || (tymin > tmax)) 
        return false; 
 
    if (tymin > tmin) 
        tmin = tymin; 
 
    if (tymax < tmax) 
        tmax = tymax; 
 
    float tzmin = (min.z - r.start.z) / r.dir.z; 
    float tzmax = (max.z - r.start.z) / r.dir.z; 
 
    if (tzmin > tzmax) swap(tzmin,tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) 
        return false; 
 
    if (tzmin > tmin) 
        tmin = tzmin; 
 
    if (tzmax < tmax) 
        tmax = tzmax; 
 
    return true; 
} 
