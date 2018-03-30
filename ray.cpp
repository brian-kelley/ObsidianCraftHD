#include "ray.hpp"
#include "world.hpp"
#include "player.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <ctime>
#include <pthread.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using std::ostream;
using std::cout;
using std::string;

byte* frameBuf;

extern double currentTime;
int RAYS_PER_PIXEL = 1;
int MAX_BOUNCES = 1;
bool fancy = false;

const vec3 skyBlue(125 / 255.0, 196 / 255.0, 240 / 255.0);

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
    bool exact = false;
    color += trace(vec3(backWorld), direction, exact);
    if(exact)
    {
      color *= RAYS_PER_PIXEL;
      break;
    }
  }
  color /= RAYS_PER_PIXEL;
  //clamp colors and convert to 8-bit integer components
  byte* pixel = frameBuf + 4 * (x + y * RAY_W);
  pixel[0] = fmin(color.x, 1) * 255;
  pixel[1] = fmin(color.y, 1) * 255;
  pixel[2] = fmin(color.z, 1) * 255;
  pixel[3] = 255;
}

//worker function for threads
void* renderRange(void* data)
{
  //what is my thread index
  int index = *((int*) data);
  //range of pixels this thread is responsible for
  int workBegin = RAY_W * RAY_H * index / RAY_THREADS;
  int workEnd = RAY_W * RAY_H * (index + 1) / RAY_THREADS;
  for(int i = workBegin; i < workEnd; i++)
  {
    renderPixel(i % RAY_W, i / RAY_W);
  }
  return NULL;
}

//Round down to integer
float ipart(float in)
{
  return floorf(in);
}

//fpart(1.3) = 0.3
//fpart(-0.3) = 0.7
float fpart(float in)
{
  return in - ipart(in);
}

void render(bool write)
{
  //wake up all the worker threads so they can 
  int threadIDs[RAY_THREADS];
  for(int i = 0; i < RAY_THREADS; i++)
  {
    threadIDs[i] = i;
  }
  pthread_t threads[RAY_THREADS];
  //launch threads
  for(int i = 0; i < RAY_THREADS; i++)
  {
    pthread_create(threads + i, NULL, renderRange, &threadIDs[i]);
  }
  //then wait for all to terminate
  for(int i = 0; i < RAY_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(write)
  {
    string fname = "ochd_" + std::to_string(time(NULL) % 10000) + ".png";
    stbi_write_png(fname.c_str(), RAY_W, RAY_H, 4, frameBuf, 4 * RAY_W);
  }
}

vec3 rayCubeIntersect(vec3 p, vec3 dir, vec3& norm, vec3 cube, float size)
{
  if(dir.x > 0)
  {
    //does origin + t * direction pass through the +x face?
    //note: +x face is at blockIter.x + 1
    //just solve for y, z at the point the ray gets to blockIter.x + 1
    vec3 intersect = p + dir * (cube.x + size - p.x) / dir.x;
    if(intersect.y >= cube.y && intersect.y <= cube.y + size &&
        intersect.z >= cube.z && intersect.z <= cube.z + size)
    {
      norm = vec3(-1, 0, 0);
      return intersect;
    }
  }
  if(dir.x < 0)
  {
    vec3 intersect = p + dir * (cube.x - p.x) / dir.x;
    if(intersect.y >= cube.y && intersect.y <= cube.y + size &&
        intersect.z >= cube.z && intersect.z <= cube.z + size)
    {
      norm = vec3(1, 0, 0);
      return intersect;
    }
  }
  if(dir.y > 0)
  {
    vec3 intersect = p + dir * (cube.y + size - p.y) / dir.y;
    if(intersect.x >= cube.x && intersect.x <= cube.x + size &&
        intersect.z >= cube.z && intersect.z <= cube.z + size)
    {
      norm = vec3(0, -1, 0);
      return intersect;
    }
  }
  if(dir.y < 0)
  {
    vec3 intersect = p + dir * (cube.y - p.y) / dir.y;
    if(intersect.x >= cube.x && intersect.x <= cube.x + size &&
        intersect.z >= cube.z && intersect.z <= cube.z + size)
    {
      norm = vec3(0, 1, 0);
      return intersect;
    }
  }
  if(dir.z > 0)
  {
    vec3 intersect = p + dir * (cube.z + size - p.z) / dir.z;
    if(intersect.x >= cube.x && intersect.x <= cube.x + size &&
        intersect.y >= cube.y && intersect.y <= cube.y + size)
    {
      norm = vec3(0, 0, -1);
      return intersect;
    }
  }
  if(dir.z < 0)
  {
    vec3 intersect = p + dir * (cube.z - p.z) / dir.z;
    if(intersect.x >= cube.x && intersect.x <= cube.x + size &&
        intersect.y >= cube.y && intersect.y <= cube.y + size)
    {
      norm = vec3(0, 0, 1);
      return intersect;
    }
  }
  assert(false);
  return vec3(0, 0, 0);
}

vec3 trace(vec3 origin, vec3 direction, bool& exact)
{
  exact = false;
  float eps = 1e-8;
  //sunlight direction
  vec3 sunlight = glm::normalize(vec3(0.3, -1, 0.1));
  //cosine of half the angular size of the sun
  //closer to 1 means smaller sun
  //(real life value is about 0.99996, too small for this)
  float cosSunRadius = 0.995;
  //iterate through blocks, finding the faces that player is looking through
  int bounces = 0;
  //color components take on the product of texture components
  vec3 color(1, 1, 1);
  while(bounces < MAX_BOUNCES)
  {
    //set blockIter to the block that ray is entering
    vec3 blockIter(ipart(origin.x + eps), ipart(origin.y + eps), ipart(origin.z + eps));
    Block prevMaterial = getBlock(blockIter.x, blockIter.y, blockIter.z);
    if(fpart(origin.x) < eps && direction.x < 0)
      blockIter.x -= 1;
    if(fpart(origin.y) < eps && direction.y < 0)
      blockIter.y -= 1;
    if(fpart(origin.z) < eps && direction.z < 0)
      blockIter.z -= 1;
    //get chunk that ray is entering,
    //and check if chunk is empty
    int cx = blockIter.x / 16;
    int cy = blockIter.y / 16;
    int cz = blockIter.z / 16;
    bool chunkInBounds = cx >= 0 && cy >= 0 && cz >= 0 &&
      cx < chunksX && cy < chunksY && cz < chunksZ;
    bool emptyChunk = !chunkInBounds || (chunkInBounds && chunks[cx][cy][cz].numFilled == 0);
    //the origin (corner) of chunk that ray is in (or about to enter, if on boundary)
    vec3 chunkOrigin(16 * cx, 16 * cy, 16 * cz);
    //point of intersection with next cube face (block or chunk)
    vec3 intersect;
    //normal at point of intersection
    vec3 normal;
    if(emptyChunk)
      intersect = rayCubeIntersect(origin, direction, normal, chunkOrigin, 16);
    else
      intersect = rayCubeIntersect(origin, direction, normal, blockIter, 1);
    vec3 nextBlock(ipart(intersect.x + eps), ipart(intersect.y + eps), ipart(intersect.z + eps));
    if(fpart(nextBlock.x) < eps && direction.x < 0)
      nextBlock.x -= 1;
    if(fpart(nextBlock.y) < eps && direction.y < 0)
      nextBlock.y -= 1;
    if(fpart(nextBlock.z) < eps && direction.z < 0)
      nextBlock.z -= 1;
    if(nextBlock.x < -8 || nextBlock.x > chunksX * 16 + 8
        || nextBlock.y < -8 || nextBlock.y > chunksY * 16 + 8
        || nextBlock.z < -8 || nextBlock.z > chunksZ * 16 + 8)
    {
      //ray escaped world without hitting anything
      //return the sun color or the sky color
      if(bounces == 0)
      {
        exact = true;
        if(glm::dot(direction, -sunlight) > cosSunRadius)
          return vec3(1, 1, 0.7);
        else
          return skyBlue;
      }
      //otherwise, return combined value obtained from bouncing off materials
      //increase sharply if pointing directly at the sun
      //otherwise, scale according to y component of direction
      if(glm::dot(direction, -sunlight) > cosSunRadius)
      {
        return color * 2.0f;
      }
      else
      {
        return color * 0.3f;
      }
    }
    Block nextMaterial = getBlock(nextBlock.x, nextBlock.y, nextBlock.z);
    if(prevMaterial != nextMaterial)
    {
      //hit a block: sample texture at point of intersection
      vec4 texel;
      if(normal.y > 0)
        texel = sample(nextMaterial, TOP, intersect.x, intersect.y, intersect.z);
      else if(normal.y < 0)
        texel = sample(nextMaterial, BOTTOM, intersect.x, intersect.y, intersect.z);
      else
        texel = sample(nextMaterial, SIDE, intersect.x, intersect.y, intersect.z);
      //depending on transparency of sampled texel,
      //choose randomly whether to reflect or refract
      //TODO: fresnel equations will be applied here
      exact = true;
      //show water as a flat blue color, since water has no texture
      if(nextMaterial == WATER)
        return vec3(0, 0, 0.8);
      return vec3(texel);
      /*
      if((float(rand()) / RAND_MAX) < texel.w)
      {
        //reflect
        //multiply current color by texel,
        //but reduce contribution after each bounce
        float k = 1.0f / (bounces + 1) / (bounces + 1);
        color *= 1 - k;
        color += k * vec3(texel);
        //set new ray position and direction based on reflection
        direction = scatter(direction, normal, block);
        origin = intersect;
        bounces++;
      }
      else
      {
        //refract
        cout << "refracting\n";
        float n1 = refractIndex[prevBlock];
        float n2 = refractIndex[block];
        origin = intersect;
        direction = refract(direction, normal, n1, n2);
        cout << "Direction now " << direction << '\n';
        blockIter = nextBlock;
        bounces++;
      }
      */
    }
    else
    {
      //didn't hit anything, so continue through whatever
      //transparent medium ray was already in
      origin = intersect;
    }
  }
  //light bounced too many times without reaching light source,
  //so no light contributed from this ray
  return vec3(0, 0, 0);
}

vec3 reflect(vec3 ray, vec3 normal)
{
  return ray - 2.0f * normal * glm::dot(ray, normal);
}

vec3 scatter(vec3 direction, vec3 normal, Block material)
{
  //compute random scatter vector r
  vec3 r = glm::normalize(vec3(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX));
  if(glm::dot(r, normal) < 0)
    r = -r;
  //compute the specular reflectino vector s
  vec3 s = reflect(direction, normal);
  //combine r and s based on material specularity
  float spec = specularity[material];
  return glm::normalize(spec * s + (1 - spec) * r);
}

vec3 refract(vec3 ray, vec3 normal, float n1, float n2)
{
  if(glm::dot(ray, normal) < 0)
    normal = -normal;
  float r = n1/n2;
  float c = glm::dot(normal, ray);
  return r * ray + (r * c - sqrtf(1 - r * r * (1 - c * c))) * normal;
}

vec3 getWaterNormal(vec3 position)
{
  //increase this for more ripples, but 1 is probably most realistic
  const int frequency = 1;
  const double timeScale = 1;
  //since the position of water fragments is perfectly flat,
  //amplitude needs to be very small
  const double k = 0.01;
  double scaledTime = fmod(currentTime, M_PI * 2) * timeScale;
  double x = position.x * 2 * M_PI * frequency + scaledTime;
  double y = position.y * 2 * M_PI * frequency + scaledTime;
  return vec3(-k * cos(x) * cos(y), 1, k * sin(x) * sin(y));
}

void toggleFancy()
{
  fancy = !fancy;
  if(fancy)
  {
    MAX_BOUNCES = 8;
    RAYS_PER_PIXEL = 100;
  }
  else
  {
    MAX_BOUNCES = 1;
    RAYS_PER_PIXEL = 1;
  }
}

