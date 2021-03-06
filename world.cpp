#include "world.hpp"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>

static Block* linearWorld = nullptr;

using std::cout;

//Have INV_W x INV_H inventory grid
//Eventually, have at least a 4x4x4 chunks (64^3 blocks) world
Chunk chunks[chunksX][chunksY][chunksZ];

void setBlock(Block b, int x, int y, int z)
{
  if(!blockInBounds(x, y, z))
  {
    return;
  }
  Chunk* chunk = &chunks[x / 16][y / 16][z / 16];
  chunk->blocks[x % 16][y % 16][z % 16] = b;
  if(linearWorld)
  {
    //world was already generated, need to keep numFilled and linearWorld
    //up to date
    if(b == getBlockFast(x, y, z))
      return;
    const int wy = chunksY * 16;
    const int wz = chunksZ * 16;
    linearWorld[x * wy * wz + y * wz + z] = b;
    if(b == AIR)
      chunk->numFilled--;
    else
      chunk->numFilled++;
  }
}

Block getBlockFast(int x, int y, int z)
{
  const int wy = chunksY * 16;
  const int wz = chunksZ * 16;
  return linearWorld[x * wy * wz + y * wz + z];
}

Block getBlock(int x, int y, int z)
{
  if(!blockInBounds(x, y, z))
  {
    if(y < seaLevel)
      return WATER;
    else
      return AIR;
  }
  return chunks[x / 16][y / 16][z / 16].blocks[x % 16][y % 16][z % 16];
}

bool blockInBounds(int x, int y, int z)
{
  return x >= 0 && y >= 0 && z >= 0 &&
    x < 16 * chunksX && y < 16 * chunksY && z < 16 * chunksZ;
}

//Seed rng with unique hash of block coordinates, combined with octave value
void srandBlockHash(int x, int y, int z, int octave)
{
  srand(SEED ^ (4 * (x + y * (chunksX * 16 + 1) + z * (chunksX * 16 * chunksY * 16 + 1)) + octave));
}

//replace all "replace" blocks with "with", in ellipsoidal region
void replaceEllipsoid(Block replace, Block with, int x, int y, int z, float rx, float ry, float rz)
{
  for(int lx = x - rx; lx <= x + rx + 1; lx++)
  {
    for(int ly = y - ry; ly <= y + ry + 1; ly++)
    {
      for(int lz = z - rz; lz <= z + rz + 1; lz++)
      {
        if(!blockInBounds(lx, ly, lz))
          continue;
        if(getBlock(lx, ly, lz) != replace)
          continue;
        //compute weighted distance squared from ellipsoid center to block center
        float dx = lx - x;
        float dy = ly - y;
        float dz = lz - z;
        float distSq = (dx * dx / (rx * rx)) + (dy * dy / (ry * ry)) + (dz * dz / (rz * rz));
        if(distSq > 1)
          continue;
        setBlock(with, lx, ly, lz);
      }
    }
  }
}

static void setNumFilled()
{
  for(int i = 0; i < totalChunks; i++)
  {
    Chunk* c = ((Chunk*) chunks) + i;
    Block* b = (Block*) (c->blocks);
    c->numFilled = 0;
    for(int j = 0; j < 4096; j++)
    {
      if(b[j] != AIR)
        c->numFilled++;
    }
  }
}

void flatGen()
{
  int wx = chunksX * 16;
  int wy = chunksY * 16;
  int wz = chunksZ * 16;
  for(int i = 0; i < wx; i++)
  {
    for(int j = 0; j < wy; j++)
    {
      for(int k = 0; k < wz; k++)
      {
        setBlock(AIR, i, j, k);
      }
    }
  }
  for(int i = 0; i < wx; i++)
  {
    for(int j = 0; j < wz; j++)
    {
      setBlock(LOG, i, 0, j);
    }
  }
  setNumFilled();
}

void initLinearWorld()
{
  int wx = chunksX * 16;
  int wy = chunksY * 16;
  int wz = chunksZ * 16;
  linearWorld = new Block[wx * wy * wz];
  for(int i = 0; i < chunksX * 16; i++)
    for(int j = 0; j < chunksY * 16; j++)
      for(int k = 0; k < chunksZ * 16; k++)
      {
        linearWorld[i * wy * wz + j * wz + k] = getBlock(i, j, k);
      }
}

void terrainGen()
{
  int wx = chunksX * 16;
  int wy = chunksY * 16;
  int wz = chunksZ * 16;
  /*  build some fractal noise in-place (with already allocated 4 bits per block)
      always clamp values to [0, 16)
      first, fill with some random vals (with a small maximum, like 2)
      then, repeatedly sample some small 3D region (-50%), scale its values up (+50%), and add it back to original
      */
  //Base fractal noise
  //Each chunk computed independently (TODO: whole terrain gen should independent of neighbors in final product)
  //sample at 4 octaves: 0, 1, 2, 3
  //octave i has amplitude 2^i and frequency 2^(-i)
  //that way, absolute max value possible is 15 (perfect for 4-bit range)
  //assume octave 0 has sample points every 2 blocks
  for(int c = 0; c < chunksX * chunksY * chunksZ; c++)
  {
    int cx = c % chunksX;
    int cy = (c / chunksX) % chunksY;
    int cz = c / (chunksX * chunksY);
    for(int i = 0; i < 16; i++)
    {
      for(int j = 0; j < 16; j++)
      {
        for(int k = 0; k < 16; k++)
        {
          int x = cx * 16 + i;
          int y = cy * 16 + j;
          int z = cz * 16 + k;
          float dist = sqrtf(powf(x - wx / 2, 2) + powf(y - wy / 2, 2));
          float radius = std::min(wx / 2, wz / 2);
          if(dist < radius * 0.2)
            setBlock(4, x, y, z);
          else if(dist < radius * 0.5)
            setBlock(2, x, y, z);
          else if(dist < radius * 0.7)
            setBlock(1, x, y, z);
          else
            setBlock(0, x, y, z);
        }
      }
    }
    for(int octave = 2; octave < 4; octave++)
    {
      int amplitude = 1 << octave;
      //period = distance between samples (must evenly divide 16)
      int period = 2 * (1 << octave);
      //freq = samples per chunk length
      int freq = 16 / period;
      //loop over sample cubes, then loop over blocks within sample cubes and lerp its value from this octave
      for(int sample = 0; sample < freq * freq * freq; sample++)
      {
        int sx = sample % freq;
        int sy = (sample / freq) % freq;
        int sz = sample / (freq * freq);
        int bx = 16 * cx + sx * period;
        int by = 16 * cy + sy * period;
        int bz = 16 * cz + sz * period;
        //samples at the 8 sampling cube corners
        int samples[8];
        srandBlockHash(bx + period, by + period, bz + period, octave);
        samples[0] = rand() % (amplitude + 1);
        srandBlockHash(bx, by + period, bz + period, octave);
        samples[1] = rand() % (amplitude + 1);
        srandBlockHash(bx + period, by, bz + period, octave);
        samples[2] = rand() % (amplitude + 1);
        srandBlockHash(bx, by, bz + period, octave);
        samples[3] = rand() % (amplitude + 1);
        srandBlockHash(bx + period, by + period, bz, octave);
        samples[4] = rand() % (amplitude + 1);
        srandBlockHash(bx, by + period, bz, octave);
        samples[5] = rand() % (amplitude + 1);
        srandBlockHash(bx + period, by, bz, octave);
        samples[6] = rand() % (amplitude + 1);
        srandBlockHash(bx, by, bz, octave);
        samples[7] = rand() % (amplitude + 1);
        for(int x = 0; x < period; x++)
        {
          for(int y = 0; y < period; y++)
          {
            for(int z = 0; z < period; z++)
            {
              //values proportional volume in cuboid between opposite corner and interpolation point
              int v = 0;
              v += samples[0] * x * y * z;
              v += samples[1] * (period - x) * y * z;
              v += samples[2] * x * (period - y) * z;
              v += samples[3] * (period - x) * (period - y) * z;
              v += samples[4] * x * y * (period - z);
              v += samples[5] * (period - x) * y * (period - z);
              v += samples[6] * x * (period - y) * (period - z);
              v += samples[7] * (period - x) * (period - y) * (period - z);
              int val = getBlock(bx + x, by + y, bz + z);
              //add the weighted average of sample cube corner values
              val += 0.5 + v / (period * period * period);
              if(val > 15)
                val = 15;
              setBlock(val, bx + x, by + y, bz + z);
            }
          }
        }
      }
    }
  }
  //add a small adjustment value that decreases with altitude
  //underground should be mostly solid, above sea level should be mostly empty
  for(int x = 0; x < wx; x++)
  {
    for(int y = 0; y < wy; y++)
    {
      for(int z = 0; z < wz; z++)
      {
        float shift = wy / 2 - y;
        if(y > wy / 2)
          shift = 1 + shift * 0.1;
        else
          shift = shift * 0.7;
        int val = getBlock(x, y, z);
        val += shift;
        if(val < 0)
          val = 0;
        if(val > 15)
          val = 15;
        setBlock(val, x, y, z);
      }
    }
  }
  //run a few sweeps of a smoothing function
  //basically gaussian blur, but in-place
  for(int sweep = 0; sweep < 8; sweep++)
  {
    for(int x = 0; x < wx; x++)
    {
      for(int y = 0; y < wy; y++)
      {
        for(int z = 0; z < wz; z++)
        {
          //test neighbors around
          //note: values outside world have value 0
          //since threshold is 8, dividing sum of neighbor values by 26 and then comparing against same threshold provides smoothing function
          int neighborVals = 0;
          int samples = 0;
          for(int tx = -1; tx <= 1; tx++)
          {
            for(int ty = -1; ty <= 1; ty++)
            {
              for(int tz = -1; tz <= 1; tz++)
              {
                if(blockInBounds(tx, ty, tz))
                {
                  neighborVals += getBlock(x + tx, y + ty, z + tz);
                  samples++;
                }
              }
            }
          }
          //get sample value as rounded-to-nearest average of samples
          //kill tiny floating islands
          if(samples < 5)
            neighborVals = 0;
          neighborVals = (neighborVals + samples / 2) / samples;
          if(neighborVals >= 8 + rand() % 2)
            setBlock(13, x, y, z);
          else
            setBlock(4, x, y, z);
        }
      }
    }
  }
  //now, set each block above a threshold to stone, and each below to air
  for(int x = 0; x < wx; x++)
  {
    for(int y = 0; y < wy; y++)
    {
      for(int z = 0; z < wz; z++)
      {
        int val = getBlock(x, y, z);
        if(val >= 6)
        {
          setBlock(STONE, x, y, z);
        }
        else
          setBlock(AIR, x, y, z);
      }
    }
  }
  //set the bottom layer of world to bedrock
  for(int i = 0; i < wx; i++)
  {
    for(int j = 0; j < wz; j++)
    {
      setBlock(BEDROCK, i, 0, j);
    }
  }
  //set all air blocks below sea level to water
  for(int x = 0; x < wx; x++)
  {
    for(int y = 0; y < wy / 2; y++)
    {
      for(int z = 0; z < wz; z++)
      {
        if(getBlock(x, y, z) == AIR)
        {
          setBlock(WATER, x, y, z);
        }
      }
    }
  }
  //set all surface blocks to dirt
  for(int x = 0; x < wx; x++)
  {
    for(int z = 0; z < wz; z++)
    {
      //trace down from sky to find highest block
      for(int y = wy - 1; y >= 0; y--)
      {
        Block b = getBlock(x, y, z);
        if(b == STONE)
        {
          setBlock(DIRT, x, y, z);
          break;
        }
      }
    }
  }
  //set all solid blocks near water to sand
  for(int x = 0; x < wx; x++)
  {
    for(int y = 0; y < wy; y++)
    {
      for(int z = 0; z < wz; z++)
      {
        if(getBlock(x, y, z) != AIR && getBlock(x, y, z) != WATER)
        {
          //look around a 3x1x3 region for water blocks
          bool nearWater = false;
          for(int i = -2; i <= 2; i++)
          {
            for(int j = -2; j <= 2; j++)
            {
              for(int k = -2; k <= 2; k++)
              {
                if(getBlock(x + i, y + j, z + k) == WATER)
                {
                  nearWater = true;
                }
              }
            }
          }
          if(nearWater)
          {
            setBlock(SAND, x, y, z);
          }
        }
      }
    }
  }
  //replace some stone with ores
  //note: veins attribute is average veins per chunk in the depth range
  //configuration:
#define GRANITE_MIN_SIZE 6
#define GRANITE_MAX_SIZE 12
#define GRANITE_MIN_DEPTH wy
#define GRANITE_VEINS 0.1
#define QUARTZ_MIN_SIZE 5
#define QUARTZ_MAX_SIZE 10
#define QUARTZ_MIN_DEPTH wy / 2
#define QUARTZ_VEINS 0.1
#define COAL_MIN_SIZE 3
#define COAL_MAX_SIZE 6
#define COAL_MIN_DEPTH wy
#define COAL_VEINS 0.1
#define IRON_MIN_SIZE 2
#define IRON_MAX_SIZE 4
#define IRON_MIN_DEPTH wy / 2
#define IRON_VEINS 0.3
#define GOLD_MIN_SIZE 2
#define GOLD_MAX_SIZE 3
#define GOLD_MIN_DEPTH wy / 3
#define GOLD_VEINS 0.2
#define DIAMOND_MIN_SIZE 1
#define DIAMOND_MAX_SIZE 3
#define DIAMOND_MIN_DEPTH wy / 5
#define DIAMOND_VEINS 0.1
#define GEN_VEINS(block, minsize, maxsize, mindepth, veins) \
  { \
    float freq = veins * ((float) mindepth / wy); \
    for(int v = 0; v < chunksX * chunksY * chunksZ * freq; v++) \
    { \
      int x = rand() % wx; \
      int y = rand() % mindepth; \
      int z = rand() % wz; \
      int rx = minsize + rand() % (maxsize - minsize + 1); \
      int ry = minsize + rand() % (maxsize - minsize + 1); \
      int rz = minsize + rand() % (maxsize - minsize + 1); \
      replaceEllipsoid(STONE, block, x, y, z, rx, ry, rz); \
    } \
  }
  GEN_VEINS(QUARTZ, QUARTZ_MIN_SIZE, QUARTZ_MAX_SIZE, QUARTZ_MIN_DEPTH, QUARTZ_VEINS);
  GEN_VEINS(COAL, COAL_MIN_SIZE, COAL_MAX_SIZE, COAL_MIN_DEPTH, COAL_VEINS);
  GEN_VEINS(IRON, IRON_MIN_SIZE, IRON_MAX_SIZE, IRON_MIN_DEPTH, IRON_VEINS);
  GEN_VEINS(GOLD, GOLD_MIN_SIZE, GOLD_MAX_SIZE, GOLD_MIN_DEPTH, GOLD_VEINS);
  GEN_VEINS(DIAMOND, DIAMOND_MIN_SIZE, DIAMOND_MAX_SIZE, DIAMOND_MIN_DEPTH, DIAMOND_VEINS);
  //find random places on the surface to plant trees
  for(int tree = 0; tree < chunksX * chunksZ; tree++)
  {
    //determine if the highest block here is dirt
    int x = rand() % wx;
    int z = rand() % wz;
    bool hitDirt = false;
    int y;
    for(y = wy - 1; y >= 1; y--)
    {
      Block b = getBlock(x, y, z);
      if(b == DIRT)
        hitDirt = true;
      if(b != AIR)
        break;
    }
    if(!hitDirt)
    {
      continue;
    }
    //try to plant the tree on dirt block @ (x, y, z)
    int treeHeight = 4 + rand() % 4;
    for(int i = 0; i < treeHeight; i++)
    {
      setBlock(LOG, x, y + 1 + i, z);
    }
    //fill in vertical ellipsoid of leaves around the trunk
    //cover the top 2/3 of trunk, and extend another 1/3 above it
    //have x/z radius be half the y radius
    //loop over bounding box of the leaves
    //float ellY = y + 1 + (5.0 / 6.0) * treeHeight;
    //int ry = 2 * treeHeight / 3;
    //float rxz = 2 * ry / 3;
    replaceEllipsoid(AIR, LEAF, x, y + 1 + 0.833 * treeHeight, z, treeHeight * 0.4, treeHeight * 0.5, treeHeight * 0.4);
  }
  createTower(0.25 * (chunksX * 16), 0.25 * (chunksZ * 16));
  createCastle(0.75 * (chunksX * 16), 0.25 * (chunksZ * 16));
  setNumFilled();
  initLinearWorld();
}

void createTower(int x, int z)
{
  const int xsize = 16;
  const int zsize = 12;
  const Block floorMaterial = QUARTZ;
  const Block wallMaterial = OBSIDIAN;
  //find elevation at (x, z)
  int elev = chunksY * 16;
  int lox = x - xsize / 2;
  int hix = x + xsize / 2;
  int loz = z - zsize / 2;
  int hiz = z + zsize / 2;
  int maxHeight = 30;
  const int floorHeight = 5;
  for(; elev >= 0 && getBlock(x, elev, z) == AIR; elev--);
  //build a "foundation" of stone at the base 
  for(int i = lox; i <= hix; i++)
  {
    for(int j = loz; j <= hiz; j++)
    {
      for(int k = 0; k <= elev; k++)
      {
        setBlock(STONE, i, k, j);
      }
    }
  }
  //add floors of the tower up to sky limit
  int floorElev = elev;
  while(floorElev + floorHeight <= elev + maxHeight)
  {
    for(int i = lox; i <= hix; i++)
    {
      for(int j = loz; j <= hiz; j++)
      {
        for(int k = floorElev; k <= floorElev + floorHeight; k++)
        {
          //how many walls does (i, k, j) intersect?
          int wallIntersect = 0;
          bool inFloor = false;
          if(i == lox || i == hix)
            wallIntersect++;
          if(j == loz || j == hiz)
            wallIntersect++;
          if(k == floorElev || k == floorElev + floorHeight)
          {
            wallIntersect++;
            inFloor = true;
          }
          if(wallIntersect >= 2)
          {
            //in frame (edges)
            setBlock(wallMaterial, i, k, j);
          }
          else if(wallIntersect == 1)
          {
            if(inFloor)
              setBlock(floorMaterial, i, k, j);
            else
              setBlock(GLASS, i, k, j);
          }
          else
          {
            setBlock(AIR, i, k, j);
          }
        }
      }
    }
    floorElev += floorHeight;
  }
}

void createCastle(int x, int z)
{
  //dimensions should be odd and divisible by 7
  const int xsize = 21;
  const int zsize = 35;
  const int height = 15;
  //find elevation at (x, z)
  int elev = chunksY * 16;
  int lox = x - xsize / 2;
  int hix = x + xsize / 2;
  int loz = z - zsize / 2;
  int hiz = z + zsize / 2;
  for(; elev >= 0 && getBlock(x, elev, z) == AIR; elev--);
  //build foundation of obsidian
  for(int i = lox - 10; i <= hix + 10; i++)
  {
    for(int j = loz - 10; j <= hiz + 10; j++)
    {
      for(int k = 0; k <= elev; k++)
      {
        //moat
        if(k >= elev - 5 &&
            (i >= lox - 7 && i <= hix + 7 && j >= loz - 7 && j <= hiz + 7) &&
            ((i < lox - 2 && i >= lox - 7) || (j < loz - 2 && j >= loz - 7) || (i > hix + 2 && i <= hix + 7) || (j > hiz + 2 && j <= hiz + 7)))
          setBlock(WATER, i, k, j);
        else
          setBlock(OBSIDIAN, i, k, j);
      }
      //clear space above platform
      for(int k = elev + 1; k < chunksY * 16; k++)
      {
        setBlock(AIR, i, k, j);
      }
    }
  }
  //build stone walls
  for(int i = lox; i <= hix; i++)
  {
    for(int j = elev; j <= elev + height + (i % 2 ? 1 : 0); j++)
    {
      setBlock(STONE, i, j, loz);
      setBlock(STONE, i, j, hiz);
    }
    if((i - lox) % 7 == 3)
    {
      //add a reinforcing rib outside the wall
      for(int j = elev; j <= elev + height - 2; j++)
      {
        setBlock(OBSIDIAN, i, j, loz - 1);
        setBlock(OBSIDIAN, i, j, hiz + 1);
        if(j < elev + height / 2)
        {
          setBlock(OBSIDIAN, i, j, loz - 2);
          setBlock(OBSIDIAN, i, j, hiz + 2);
        }
      }
    }
  }
  for(int i = loz; i <= hiz; i++)
  {
    for(int j = elev; j <= elev + height + (i % 2 ? 1 : 0); j++)
    {
      setBlock(STONE, lox, j, i);
      setBlock(STONE, hix, j, i);
    }
    if((i - loz) % 7 == 3)
    {
      //add a reinforcing rib outside the wall
      for(int j = elev; j <= elev + height - 2; j++)
      {
        setBlock(OBSIDIAN, lox - 1, j, i);
        setBlock(OBSIDIAN, hix + 1, j, i);
        if(j < elev + height / 2)
        {
          setBlock(OBSIDIAN, lox - 2, j, i);
          setBlock(OBSIDIAN, hix + 2, j, i);
        }
      }
    }
  }
  //wooden bridge over moat
  int bridgeZ = (loz + hiz) / 2;
  for(int i = lox - 10; i < lox; i++)
  {
    for(int k = bridgeZ - 2; k <= bridgeZ + 2; k++)
    {
      setBlock(LOG, i, elev + 1, k);
      //clear space above
      for(int j = elev + 2; j < elev + height; j++)
      {
        setBlock(AIR, i, j, k);
      }
    }
  }
  //create a gateway at end of bridge
  for(int i = bridgeZ - 2; i <= bridgeZ + 2; i++)
  {
    for(int j = elev + 1; j < elev + 7 - std::abs(bridgeZ - i); j++)
    {
      setBlock(AIR, lox, j, i);
    }
  }
  //wooden roof
  for(int i = lox + 1; i <= hix - 1; i++)
  {
    for(int k = loz + 1; k <= hiz - 1; k++)
    {
      setBlock(LOG, i, elev + height - 1, k);
    }
  }
}

void printWorldComposition()
{
  int counts[16] = {0};
  for(int i = 0; i < chunksX * chunksY * chunksZ; i++)
  {
    Chunk* chunk = ((Chunk*) chunks) + i;
    Block* blocks = (Block*) chunk->blocks;
    for(int j = 0; j < 4096; j++)
    {
      counts[blocks[j]]++;
    }
  }
  int total = chunksX * chunksY * chunksZ * 4096;
  cout << "World composition:\n";
  for(int i = 0; i < 16; i++)
  {
    cout << 100.0 * counts[i] / total << "% type " << i << '\n';
  }
}

