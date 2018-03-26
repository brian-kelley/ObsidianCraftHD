#ifndef RAY_HEADER
#define RAY_HEADER

#include "glmHeaders.hpp"
#include "world.hpp"

//Width and height of the raytraced framebuffer
#define RAY_W 320
#define RAY_H 200
#define RAYS_PER_PIXEL 1
//maximum number of reflections/refractions for ray to escape to the sky
#define MAX_BOUNCES 1

//RAY_W * RAY_H RGBA color values
extern byte* frameBuf;

void render();
//get light contribution from a single ray
vec3 trace(vec3 origin, vec3 direction);
vec3 reflect(vec3 ray, vec3 normal);
//vec3 refract(vec3 ray, vec3 normal, float n1, float n2);

#endif

