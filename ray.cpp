#include "ray.hpp"
#include "world.hpp"
#include "player.hpp"
#include <cstdlib>
#include <iostream>
#include <pthread.h>

using std::ostream;
using std::cout;

byte* frameBuf;

ostream& operator<<(ostream& os, vec3 v)
{
  os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
  return os;
}

ostream& operator<<(ostream& os, vec4 v)
{
  os << '(' << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ')';
  return os;
}

void renderPixel(int x, int y)
{
  //find ray by inverse projecting two points in NDC
  //one on near plane, one on far plane
  vec4 backWorld(
      ((float) x / RAY_W) * 2 - 1,
      ((float) y / RAY_H) * 2 - 1,
      -1, 1);
  backWorld = projInv * backWorld;
  backWorld /= backWorld.w;
  backWorld = viewInv * backWorld;
  vec4 frontWorld(
      ((float) x / RAY_W) * 2 - 1,
      ((float) y / RAY_H) * 2 - 1,
      1, 1);
  frontWorld = projInv * frontWorld;
  frontWorld /= frontWorld.w;
  frontWorld = viewInv * frontWorld;
  vec3 direction = glm::normalize(vec3(frontWorld) - vec3(backWorld));
  vec3 color(0, 0, 0);
  for(int j = 0; j < RAYS_PER_PIXEL; j++)
  {
    color += trace(vec3(backWorld), direction);
  }
  color /= RAYS_PER_PIXEL;
  //clamp colors and convert to integer components
  byte* pixel = frameBuf + 4 * (x + y * RAY_W);
  pixel[0] = fmin(color.x, 1) * 255;
  pixel[1] = fmin(color.y, 1) * 255;
  pixel[2] = fmin(color.z, 1) * 255;
  pixel[3] = 255;
}

//worker function for threads
void* renderRange(void* data)
{
  int begin = ((int*) data)[0];
  int end = ((int*) data)[1];
  for(int i = begin; i < end; i++)
  {
    renderPixel(i % RAY_W, i / RAY_W);
  }
  return NULL;
}

void render()
{
  int workRanges[RENDER_THREADS + 1];
  for(int i = 0; i <= RENDER_THREADS; i++)
  {
    workRanges[i] = RAY_W * RAY_H * i / RENDER_THREADS;
  }
  pthread_t threads[RENDER_THREADS];
  //launch threads
  for(int i = 0; i < RENDER_THREADS; i++)
  {
    pthread_create(threads + i, NULL, renderRange, &workRanges[i]);
  }
  //then wait for all to terminate
  for(int i = 0; i < RENDER_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }
}

vec3 trace(vec3 origin, vec3 direction)
{
  vec3 blockIter((int) origin.x, (int) origin.y, (int) origin.z);
  //iterate through blocks, finding the faces that player is looking through
  int bounces = 0;
  //color components take on the product of texture components
  vec3 color(1, 1, 1);
  while(bounces < MAX_BOUNCES)
  {
    vec3 nextBlock = blockIter;
    //where ray intersects nextBlock
    vec3 intersect;
    //normal at point of intersection
    vec3 normal;
    //this loop is only to provide "break"
    while(1)
    {
      if(direction.x > 0)
      {
        //does origin + t * direction pass through the +x face?
        //note: +x face is at blockIter.x + 1
        //just solve for y, z at the point the ray gets to blockIter.x + 1
        intersect = origin + direction * (blockIter.x + 1 - origin.x) / direction.x;
        if(intersect.y >= blockIter.y && intersect.y < blockIter.y + 1 &&
           intersect.z >= blockIter.z && intersect.z < blockIter.z + 1)
        {
          nextBlock.x += 1;
          normal = vec3(-1, 0, 0);
          break;
        }
      }
      if(direction.x < 0)
      {
        //-x face
        intersect = origin + direction * (blockIter.x - origin.x) / direction.x;
        if(intersect.y >= blockIter.y && intersect.y < blockIter.y + 1 &&
           intersect.z >= blockIter.z && intersect.z < blockIter.z + 1)
        {
          nextBlock.x -= 1;
          normal = vec3(1, 0, 0);
          break;
        }
      }
      if(direction.y > 0)
      {
        //+y face
        intersect = origin + direction * (blockIter.y + 1 - origin.y) / direction.y;
        if(intersect.x >= blockIter.x && intersect.x < blockIter.x + 1 &&
           intersect.z >= blockIter.z && intersect.z < blockIter.z + 1)
        {
          nextBlock.y += 1;
          normal = vec3(0, -1, 0);
          break;
        }
      }
      if(direction.y < 0)
      {
        //-y face
        intersect = origin + direction * (blockIter.y - origin.y) / direction.y;
        if(intersect.x >= blockIter.x && intersect.x < blockIter.x + 1 &&
           intersect.z >= blockIter.z && intersect.z < blockIter.z + 1)
        {
          nextBlock.y -= 1;
          normal = vec3(0, 1, 0);
          break;
        }
      }
      if(direction.z > 0)
      {
        //+z face
        intersect = origin + direction * (blockIter.z + 1 - origin.z) / direction.z;
        if(intersect.x >= blockIter.x && intersect.x < blockIter.x + 1 &&
           intersect.y >= blockIter.y && intersect.y < blockIter.y + 1)
        {
          nextBlock.z += 1;
          normal = vec3(0, 0, -1);
          break;
        }
      }
      if(direction.z < 0)
      {
        //-z face
        intersect = origin + direction * (blockIter.z - origin.z) / direction.z;
        if(intersect.x >= blockIter.x && intersect.x < blockIter.x + 1 &&
           intersect.y >= blockIter.y && intersect.y < blockIter.y + 1)
        {
          nextBlock.z -= 1;
          normal = vec3(0, 0, 1);
          break;
        }
      }
      //shouldn't ever get here:
      //a ray entering a block must exit through exactly one face
      return vec3(0, 0, 0);
      assert(false);
    }
    if(nextBlock.x < -10 || nextBlock.x > chunksX * 16 + 10
        || nextBlock.y < -10 || nextBlock.y > chunksY * 16 + 10
        || nextBlock.z < -10 || nextBlock.z > chunksZ * 16 + 10)
    {
      //ray escaped without hitting anything; return sky color
      if(bounces == 0)
      {
        return vec3(135 / 255.0, 206 / 255.0, 250 / 255.0);
      }
      //otherwise, return value obtained from bouncing off materials
      return color;
    }
    Block block = getBlock(nextBlock.x, nextBlock.y, nextBlock.z);
    if(block)
    {
      //hit a block: sample texture at point of intersection
      vec4 texel;
      if(normal.y > 0)
        texel = sample(block, TOP, intersect.x, intersect.y, intersect.z);
      else if(normal.y < 0)
        texel = sample(block, BOTTOM, intersect.x, intersect.y, intersect.z);
      else
        texel = sample(block, SIDE, intersect.x, intersect.y, intersect.z);
      if(texel.w > 0.5)
        return vec3(texel);
      /*
      //multiply current color by texel
      color.x *= texel.x;
      color.y *= texel.y;
      color.z *= texel.z;
      //set new ray position and direction based on reflection
      direction = reflect(direction, normal);
      bounces++;
      */
    }
    origin = intersect;
    blockIter = nextBlock;
  }
  //light bounced too many times without reaching light source:
  //no light contributed from this ray
  return vec3(0, 0, 0);
}

vec3 reflect(vec3 ray, vec3 normal)
{
  return ray + 2.0f * normal * glm::dot(ray, normal);
}

