#ifndef RAY_HEADER
#define RAY_HEADER

#include <iostream>
#include "glmHeaders.hpp"
#include "world.hpp"

//Width and height of the raytraced framebuffer
#define RAY_W 640
#define RAY_H 480
extern int RAY_THREADS;
extern int RAYS_PER_PIXEL;
extern int MAX_BOUNCES;

//RAY_W * RAY_H RGBA color values
extern byte* frameBuf;

void initRay();
//if write, produce a PNG file of the framebuffer after rendering
void render(bool write);
//get light contribution from a single ray
vec3 trace(vec3 origin, vec3 direction, bool& exact);
vec3 scatter(vec3 direction, vec3 normal, Block material);
vec3 waterNormal(vec3 position);
void toggleFancy();

std::ostream& operator<<(std::ostream& os, vec3 v);
std::ostream& operator<<(std::ostream& os, vec4 v);

#endif

