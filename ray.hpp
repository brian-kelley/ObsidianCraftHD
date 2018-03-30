#ifndef RAY_HEADER
#define RAY_HEADER

#include "glmHeaders.hpp"
#include "world.hpp"

//Width and height of the raytraced framebuffer
#define RAY_W 480
#define RAY_H 320
#define RAY_THREADS 4
extern int RAYS_PER_PIXEL;
extern int MAX_BOUNCES;

//RAY_W * RAY_H RGBA color values
extern byte* frameBuf;

void initRay();
//if write, produce a PNG file of the framebuffer after rendering
void render(bool write);
//get light contribution from a single ray
vec3 trace(vec3 origin, vec3 direction, bool& exact);
vec3 reflect(vec3 ray, vec3 normal);
vec3 refract(vec3 ray, vec3 normal, float n1, float n2);
vec3 scatter(vec3 direction, vec3 normal, Block material);
vec3 getWaterNormal(vec3 position);

void toggleFancy();

#endif

