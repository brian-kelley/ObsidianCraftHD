#ifndef TILES_H
#define TILES_H

#include "glmHeaders.hpp"
#include <iostream>

using std::cout;

#define NUM_TILES 16

//4-bit block IDs
enum
{
  AIR,
  STONE,
  DIRT,
  COAL,
  IRON,
  GOLD,
  DIAMOND,
  LOG,
  LEAF,
  WATER,
  SAND,
  GLASS,
  OBSIDIAN,
  QUARTZ,
  BEDROCK,
  UNUSED_TILE
};

typedef unsigned char byte;
typedef byte Block;

//Specular coefficients
extern float ks[NUM_TILES];
//Diffuse coefficients
extern float kd[NUM_TILES];
//Indices of refraction
extern float refractIndex[NUM_TILES];

enum Side
{
  TOP,
  SIDE,
  BOTTOM
};

void initAtlas();

//Get the color of fragment at world position x, y, z
vec4 sample(Block block, Side side, float x, float y, float z);
bool isTransparent(Block block);

#endif

