#ifndef OC_H
#define OC_H

#include "stdio.h"
#include "assert.h"
#include "time.h"
#include "graphics.h"

typedef struct
{
  byte item;  //If == 0, no item
  byte count;
} Stack; 

void ocMain();
void renderChunk(int x, int y, int z);
byte getBlock(int x, int y, int z);
void setBlock(byte b, int x, int y, int z);
byte getBlockC(Chunk* c, int cx, int cy, int cz);
void setBlockC(byte b, Chunk* c, int cx, int cy, int cz);

#endif

