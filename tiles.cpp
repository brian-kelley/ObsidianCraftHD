#include "tiles.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static int atlasW;
static int atlasH;
static unsigned char* image;

void initAtlas()
{
  int components = 0;
  image = stbi_load("../atlas.png", &atlasW, &atlasH, &components, 0);
  if(!image)
  {
    puts("Failed to load textuer atlas");
    exit(1);
  }
  if(components != 4)
  {
    puts("Error: texture atlas must have 4 color components (8-bit RGBA)");
    exit(1);
  }
}

/*
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
  BEDROCK
*/

//top, side, and bottom texture coordinates for each block

static int texcoords[3][NUM_TILES][2] =
{
  //Top textures
  {
    {16, 112},   //AIR (no texture)
    {16, 0},  //STONE
    {0, 0},   //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {80, 16}, //LOG
    {64, 48}, //LEAF
    {16, 112},//WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16}, //BEDROCK
    {0, 0}
  },
  //Side textures
  {
    {16, 112},//AIR (no texture)
    {16, 0},  //STONE
    {48, 0},  //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {64, 16}, //LOG
    {64, 48}, //LEAF
    {16, 112},   //WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16},  //BEDROCK
    {0, 0}
  },
  //Bottom textures
  {
    {16, 112},//AIR (no texture)
    {16, 0},  //STONE
    {32, 0},  //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {80, 16}, //LOG
    {64, 48}, //LEAF
    {16, 112},//WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16}, //BEDROCK
    {0, 0}
  }
};

float specularity[NUM_TILES] =
{
  0,        //AIR
  0.6,      //STONE
  0.3,      //DIRT
  0.5,      //COAL
  0.5,      //IRON
  0.5,      //GOLD
  0.5,      //DIAMOND
  0.4,      //LOG
  0.3,      //LEAF
  0.99,      //WATER
  0.6,      //SAND
  1.0,      //GLASS
  0.9,      //OBSIDIAN
  0.9,      //QUARTZ
  0.0,      //BEDROCK
  0.0       //unused
};

//note: values for opaque blocks are never used
float refractIndex[NUM_TILES] = 
{
  1,        //AIR
  1,
  1,
  1,
  1,
  1,
  2.417,    //DIAMOND
  1,
  1,
  1.333,    //WATER
  1,
  1.517,    //GLASS
  1.5,      //OBSIDIAN
  1.46,     //QUARTZ
  1,
  1
};

float ipart(float);
float fpart(float);

vec4 sample(Block block, Side side, float x, float y, float z)
{
  x = fpart(x);
  y = 1 - fpart(y);
  z = fpart(z);
  float eps = 1e-8;
  int tx = 0;
  int ty = 0;
  if(x < eps && side == SIDE)
  {
    //z,y give texcoords
    tx = z * 16;
    ty = y * 16;
  }
  else if(z < eps && side == SIDE)
  {
    //x,y
    tx = x * 16;
    ty = y * 16;
  }
  else
  {
    //top, bottom: xz
    tx = x * 16;
    ty = z * 16;
  }
  //(tx, ty) gives offset with tile, so add tile coordinate
  tx += texcoords[side][block][0];
  ty += texcoords[side][block][1];
  //sample texture
  vec4 color;
  unsigned char* pixel = &image[4 * (tx + ty * atlasW)];
  color.x = pixel[0] / 255.0f;
  color.y = pixel[1] / 255.0f;
  color.z = pixel[2] / 255.0f;
  color.w = pixel[3] / 255.0f;
  return color;
}

