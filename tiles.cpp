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
    {0, 0},   //AIR (no texture)
    {16, 0},  //STONE
    {0, 0},   //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {80, 16}, //LOG
    {64, 48}, //LEAF
    {64, 0},   //WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16}, //BEDROCK
    {0, 352}
  },
  //Side textures
  {
    {0, 0},   //AIR (no texture)
    {16, 0},  //STONE
    {48, 0},  //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {64, 16}, //LOG
    {64, 48}, //LEAF
    {64, 0},   //WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16},  //BEDROCK
    {0, 0}
  },
  //Bottom textures
  {
    {0, 0},   //AIR (no texture)
    {16, 0},  //STONE
    {32, 0},  //GRASS
    {32, 32}, //COAL
    {16, 32}, //IRON
    {0, 32},  //GOLD
    {32, 48}, //DIAMOND
    {80, 16}, //LOG
    {64, 48}, //LEAF
    {64, 0},   //WATER (no texture)
    {32, 16}, //SAND
    {16, 48}, //GLASS
    {80, 32}, //OBSIDIAN
    {32, 64}, //QUARTZ
    {16, 16}, //BEDROCK
    {0, 0}
  }
};

vec4 sample(Block block, Side side, float x, float y, float z)
{
  x = fmodf(x, 1);
  y = 1 - fmodf(y, 1);
  z = fmodf(z, 1);
  int tx = 0;
  int ty = 0;
  if(x == 0 && side == SIDE)
  {
    //z,y give texcoords
    tx = z * 16;
    ty = y * 16;
  }
  else if(z == 0 && side == SIDE)
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

