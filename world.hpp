#ifndef WORLD_H
#define WORLD_H

#define SEED 1337

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

#define chunksX 32
#define chunksY 32
#define chunksZ 32
#define totalChunks (chunksX * chunksY * chunksZ)

void flatGen();
void terrainGen();
void printWorldComposition();

extern Chunk chunks[chunksX][chunksY][chunksZ];

void setBlock(Block b, int x, int y, int z);
Block getBlock(int x, int y, int z);
bool blockInBounds(int x, int y, int z);

#endif

