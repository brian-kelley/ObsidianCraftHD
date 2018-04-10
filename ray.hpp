#ifndef RAY_HEADER
#define RAY_HEADER

#include <iostream>
#include <string>
#include "glmHeaders.hpp"
#include "world.hpp"

using std::string;

//Width and height of the raytraced framebuffer
extern int RAY_W;
extern int RAY_H;
extern int RAY_THREADS;
extern int RAYS_PER_PIXEL;
extern int MAX_BOUNCES;

//RAY_W * RAY_H RGBA color values
extern byte* frameBuf;

void initRay();
//if write, produce a PNG file of the framebuffer after rendering
void render(bool write, string fname = "");
//get color (light contribution) from a single ray
vec3 trace(vec3 origin, vec3 direction, bool& exact);
vec3 collideRay(vec3 origin, vec3 direction, ivec3& block, vec3& normal, Block& prevMat, Block& nextMat, bool& escape);
vec3 waterNormal(vec3 position);
vec3 processEscapedRay(vec3 pos, vec3 direction, vec3 color, int bounces, bool& exact);
//Is there a direct path from given position to the sun?
//If pos is underwater, has to use monte carlo method to decide
//Otherwise, trace direct ray and see if it hits anything
bool visibleFromSun(vec3 pos);
void toggleFancy();

std::ostream& operator<<(std::ostream& os, vec3 v);
std::ostream& operator<<(std::ostream& os, vec4 v);

#endif

