#include "ray.hpp"
#include "world.hpp"
#include "player.hpp"
#include <cstdlib>
#include <string>
#include <sstream>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using std::ostream;
using std::cout;
using std::string;
using std::ostringstream;

byte* frameBuf;

extern double currentTime;
int RAY_W = 480;
int RAY_H = 320;
int RAY_THREADS = 4;
int RAYS_PER_PIXEL = 1;
int MAX_BOUNCES = 1;
bool fancy = false;

//#define DEBUG_OUT
#ifdef DEBUG_OUT
#define bmk(x) cout << x;
#else
#define bmk(x)
#endif

const vec3 skyBlue(140 / 255.0, 140 / 255.0, 200 / 255.0);

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

static int* workCounts;

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
  workCounts[index] = 0;
  //range of pixels this thread is responsible for
  int workBegin = RAY_W * RAY_H * index / RAY_THREADS;
  int workEnd = RAY_W * RAY_H * (index + 1) / RAY_THREADS;
  for(int i = workBegin; i < workEnd; i++)
  {
    //distribute image columns for better load balancing
    renderPixel(i / RAY_H, i % RAY_H);
    workCounts[index]++;
  }
  return NULL;
}

//Round down to integer
float ipart(float in)
{
  return floorf(in);
}

//fpart >= 0
//ipart(x) + fpart(x) = x
float fpart(float in)
{
  return in - ipart(in);
}

void render(bool write)
{
  if(RAY_THREADS == 1)
  {
    //do rendering in main thread only
    for(int y = 0; y < RAY_H; y++)
    {
      for(int x = 0; x < RAY_W; x++)
      {
        renderPixel(x, y);
      }
    }
  }
  else
  {
    int threadIDs[RAY_THREADS];
    for(int i = 0; i < RAY_THREADS; i++)
    {
      threadIDs[i] = i;
    }
    pthread_t threads[RAY_THREADS];
    workCounts = new int[RAY_THREADS];
    //launch threads
    for(int i = 0; i < RAY_THREADS; i++)
    {
      pthread_create(threads + i, NULL, renderRange, &threadIDs[i]);
    }
    //then wait for all to terminate
    while(true)
    {
      int pixelsDone = 0;
      for(int i = 0; i < RAY_THREADS; i++)
        pixelsDone += workCounts[i];
      if(pixelsDone == RAY_W * RAY_H)
        break;
      if(fancy)
      {
        cout << "Image is " << (100.0 * pixelsDone / RAY_W / RAY_H) << "% done\n";
        sleep(1);
      }
    }
    for(int i = 0; i < RAY_THREADS; i++)
    {
      pthread_join(threads[i], NULL);
    }
  }
  if(write)
  {
    ostringstream oss;
    oss << "ochd_" << (time(NULL) % 10000) << ".png";
    //need to vertically flip the image for STBI
    for(int row = 0; row < RAY_H / 2; row++)
    {
      for(int i = 0; i < 4 * RAY_W; i++)
      {
        byte& top = frameBuf[row * 4 * RAY_W + i];
        byte& bottom = frameBuf[(RAY_H - 1 - row) * 4 * RAY_W + i];
        std::swap(top, bottom);
      }
    }
    stbi_write_png(oss.str().c_str(), RAY_W, RAY_H, 4, frameBuf, 4 * RAY_W);
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
  cout << "Warning; failed to intersect ray " << p << " : " << dir << " with cube " << cube << '\n';
  vec3 nudge((rand() % 3 - 1) * 1e-6, (rand() % 3 - 1) * 1e-6, (rand() % 3 - 1) * 1e-6);
  return rayCubeIntersect(p, normalize(dir + nudge), norm, cube, size);
}

//desaturate a color
//k = 0: return shade of grey with same magnitude
//k = 1: return color
static vec3 desaturate(vec3 color, float k)
{
  float mag = glm::length(color);
  return vec3(color.x * k + mag * (1-k), color.y * k + mag * (1-k), color.z * k + mag * (1-k));
}

vec3 trace(vec3 origin, vec3 direction, bool& exact)
{
  exact = false;
  float eps = 1e-8;
  //sunlight direction
  vec3 sunlight = normalize(vec3(0.5, -1, 0.1));
  //iterate through blocks, finding the faces that player is looking through
  int bounces = 0;
  //color components take on the product of texture components
  vec3 color(1, 1, 1);
  while(bounces < MAX_BOUNCES)
  {
    if(origin.x <= 0 || origin.x >= chunksX * 16
        || origin.y <= 0 || origin.y >= chunksY * 16
        || origin.z <= 0 || origin.z >= chunksZ * 16)
    {
      //ray escaped world without hitting anything
      //return the sun color or the sky color
      if(bounces == 0)
      {
        exact = true;
        return skyBlue;
      }
      //otherwise, return combined value obtained from bouncing off materials
      //increase sharply if pointing directly at the sun
      float sunDot = glm::dot(direction, -sunlight);
      return color;
      if(sunDot <= 0)
        return vec3(0, 0, 0);
      return color * 1.7f;
    }
    //set blockIter to the block that ray is entering
    vec3 blockIter(ipart(origin.x + eps), ipart(origin.y + eps), ipart(origin.z + eps));
    if(fpart(origin.x) < eps && direction.x < 0)
      blockIter.x -= 1;
    if(fpart(origin.y) < eps && direction.y < 0)
      blockIter.y -= 1;
    if(fpart(origin.z) < eps && direction.z < 0)
      blockIter.z -= 1;
    Block prevMaterial = getBlock(blockIter.x, blockIter.y, blockIter.z);
    //get chunk that ray is entering,
    //and check if chunk is empty
    int cx = ipart(blockIter.x / 16);
    int cy = ipart(blockIter.y / 16);
    int cz = ipart(blockIter.z / 16);
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
    if(fpart(intersect.x) < eps && direction.x < 0)
      nextBlock.x -= 1;
    if(fpart(intersect.y) < eps && direction.y < 0)
      nextBlock.y -= 1;
    if(fpart(intersect.z) < eps && direction.z < 0)
      nextBlock.z -= 1;
    Block nextMaterial = getBlock(nextBlock.x, nextBlock.y, nextBlock.z);
    //hit a block: sample texture at point of intersection
    vec4 texel(1, 1, 1, 0);
    if(prevMaterial != nextMaterial || prevMaterial == LEAF)
    {
      if(normal.y > 0)
        texel = sample(nextMaterial, TOP, intersect.x, intersect.y, intersect.z);
      else if(normal.y < 0)
        texel = sample(nextMaterial, BOTTOM, intersect.x, intersect.y, intersect.z);
      else
        texel = sample(nextMaterial, SIDE, intersect.x, intersect.y, intersect.z);
    }
    if(MAX_BOUNCES == 1)
    {
      exact = true;
      if(nextMaterial == WATER)
        return vec3(0, 0.5, 0.6);
      if(texel.w > 0.5)
        return vec3(texel);
    }
    else
    {
      //depending on transparency of sampled texel and approx. Fresnel reflection coefficient,
      //choose whether to reflect or refract
      if(prevMaterial == AIR && nextMaterial == WATER)
      {
        //light, faint blue-green
        texel = vec4(0.7, 0.8, 0.9, 0);
        if(normal.y < 0)
          normal = -waterNormal(intersect);
        else if(normal.y > 0)
          normal = waterNormal(intersect);
      }
      else if(prevMaterial == AIR && nextMaterial == WATER)
      {
        if(normal.y < 0)
          normal = -waterNormal(intersect);
        else if(normal.y > 0)
          normal = waterNormal(intersect);
      }
      else if(texel.w < 0.5)
      {
        texel = vec4(1, 1, 1, 0);
      }
      bool refracted = false;
      if(texel.w == 0)
      {
        //passing from one transparent medium to another
        float nPrev = refractIndex[prevMaterial];
        float nNext = refractIndex[nextMaterial];
        //smaller angle of incidence means more likely to refract (Fresnel)
        float cosTheta = glm::dot(-normal, direction);
        float r0 = (nPrev - nNext) / (nPrev + nNext);
        r0 *= r0;
        float reflectProb = r0 + (1 - r0) * powf(1 - cosTheta, 5);
        if(nPrev == nNext)
        {
          //no bounce and no change in direction
          refracted = true;
        }
        else if(nPrev > nNext)
        {
          //check for total internal reflection here
          float cosCriticalAngle = cosf(asinf(nNext / nPrev));
          if(cosTheta <= cosCriticalAngle + eps)
          {
            direction = glm::reflect(direction, normal);
          }
          else
          {
            //always refract when going down in index of refraction
            direction = glm::refract(direction, normal, nPrev / nNext);
          }
          //apply color to ray
          color.x *= texel.x;
          color.y *= texel.y;
          color.z *= texel.z;
          direction = normalize(direction);
          bounces++;
          refracted = true;
        }
        else if(float(rand()) / RAND_MAX > reflectProb)
        {
          direction = normalize(glm::refract(direction, normal, nPrev / nNext));
          //apply color to ray
          color.x *= texel.x;
          color.y *= texel.y;
          color.z *= texel.z;
          bounces++;
          refracted = true;
        }
      }
      if(!refracted)
      {
        //reflect and blend sampled color and ray color,
        //but reduce effect and de-saturate after each bounce
        //this is not realistic but is better visually with the
        //very saturated texture pack
        float k = 1.0f * powf(bounces + 1, -1.5);
        color *= (1 - k);
        //color += k * desaturate(vec3(texel), 1 / (bounces + 1));
        color += k * vec3(texel);
        //set new ray position and direction based on reflection
        direction = scatter(direction, normal, nextMaterial);
        bounces++;
      }
    }
    origin = intersect;
  }
  //light bounced too many times without reaching light source,
  //so no light contributed from this ray
  return vec3(0, 0, 0);
}

vec3 scatter(vec3 direction, vec3 normal, Block material)
{ //compute ideal reflection vector
  vec3 s = normalize(glm::reflect(direction, normal));
  //compute the specular reflectino vector s
  //combine r and s based on material specularity
  float spec = specularity[material];
  vec3 r = normalize(vec3(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX));
  if(glm::dot(normal, r) < 0)
    r = -r;
  return normalize(s * spec + (1 - spec) * r);
}

vec3 waterNormal(vec3 position)
{
  //increase this for more ripples, but 1 is probably most realistic
  const int frequency = 1;
  const double timeScale = 1;
  //since the position of water fragments is perfectly flat,
  //amplitude needs to be very small
  const double k = 0.01;
  double scaledTime = fmod(currentTime, M_PI * 2) * timeScale;
  double x = fpart(position.x) * 2 * M_PI * frequency + scaledTime;
  double z = fpart(position.z) * 2 * M_PI * frequency + scaledTime;
  return normalize(vec3(-k * cos(x) * cos(z), 1, k * sin(x) * sin(z)));
}

void toggleFancy()
{
  fancy = !fancy;
  if(fancy)
  {
    RAY_W = 1920;
    RAY_H = 1080;
    MAX_BOUNCES = 10;
    RAYS_PER_PIXEL = 500;
    RAY_THREADS = 4;
  }
  else
  {
    RAY_W = 480;
    RAY_H = 320;
    MAX_BOUNCES = 1;
    RAYS_PER_PIXEL = 1;
    RAY_THREADS = 4;
  }
  delete[] frameBuf;
  frameBuf = new byte[4 * RAY_W * RAY_H];
}

