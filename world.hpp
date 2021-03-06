#ifndef WORLD_H
#define WORLD_H

#define SEED 1332

#include "tiles.hpp"

typedef unsigned char byte;

typedef struct
{
  //16^3 blocks
  Block blocks[16][16][16];
  //number of non-air blocks
  int numFilled;
} Chunk;

// Note: 16x8x16 is the original size of MCPE worlds

#define chunksX 16
#define chunksY 8
#define chunksZ 16
#define totalChunks (chunksX * chunksY * chunksZ)
#define seaLevel (chunksY * 16 / 2)

void flatGen();
void terrainGen();
void printWorldComposition();

extern Chunk chunks[chunksX][chunksY][chunksZ];

void setBlock(Block b, int x, int y, int z);
//getBlockFast does no bounds checking
//the ray tracer can safely use this since it already checks
//for when rays escape the world
Block getBlockFast(int x, int y, int z);
Block getBlock(int x, int y, int z);
bool blockInBounds(int x, int y, int z);

void createTower(int x, int z);
void createCastle(int x, int z);

#endif

