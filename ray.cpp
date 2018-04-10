#include "ray.hpp"
#include "world.hpp"
#include "player.hpp"
#include <cstdlib>
#include <string>
#include <sstream>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include "stdatomic.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using std::ostream;
using std::cout;
using std::string;
using std::ostringstream;

byte* frameBuf;

extern double currentTime;
int RAY_W = 200;
int RAY_H = 150;
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

const vec3 skyBlue(120 / 255.0, 120 / 255.0, 210 / 255.0);
const vec3 sunYellow(1, 1, 0.8);
//color of water in non-fancy mode
const vec3 waterBlue(0.1, 0.2, 0.5);
//color applied to water when reflect/refract from air
const vec3 waterHue = vec3(0.2, 0.4, 0.6);
const float waterClarity = 0.95;
//sunlight direction
//vec3 sunlight = normalize(vec3(0.5, -1, 0.1));
vec3 sunlight = normalize(vec3(3.0, -1, 2.0));
const float cosSunRadius = 0.996;

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

static atomic_int workCounter;

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
void* renderWorker(void*)
{
  while(true)
  {
    const int batchSize = 8;
    int workIndex = atomic_fetch_add(&workCounter, batchSize);
    for(int i = 0; i < batchSize; i++)
    {
      if(workIndex >= RAY_W * RAY_H)
        return NULL;
      renderPixel(workIndex % RAY_W, workIndex / RAY_W);
      workIndex++;
    }
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

void render(bool write, string fname)
{
  pthread_t threads[RAY_THREADS];
  //reset counter - as image is rendered,
  //this is atomically incremented up to RAY_W * RAY_H
  atomic_store(&workCounter, 0);
  //launch workers
  for(int i = 0; i < RAY_THREADS; i++)
  {
    pthread_create(threads + i, NULL, renderWorker, NULL);
  }
  //then wait for all to terminate
  while(true)
  {
    int pixelsDone = atomic_load(&workCounter);
    if(fancy)
    {
      printf("Image is %.1f%% done\n", 100.0 * pixelsDone / (RAY_W * RAY_H));
      sleep(1);
    }
    if(pixelsDone >= RAY_W * RAY_H)
    {
      break;
    }
  }
  for(int i = 0; i < RAY_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(write)
  {
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
  cout << "Warning; failed to intersect ray " << p << " : " << dir << " with cube " << cube << '\n';
  vec3 nudge((rand() % 3 - 1) * 1e-6, (rand() % 3 - 1) * 1e-6, (rand() % 3 - 1) * 1e-6);
  return rayCubeIntersect(p, normalize(dir + nudge), norm, cube, size);
}

/*
//desaturate a color
//k = 0: return shade of grey with same magnitude
//k = 1: return color
static vec3 desaturate(vec3 color, float k)
{
  float mag = glm::length(color);
  return vec3(color.x * k + mag * (1-k), color.y * k + mag * (1-k), color.z * k + mag * (1-k));
}
*/

vec3 trace(vec3 origin, vec3 direction, bool& exact)
{
  exact = false;
  float eps = 1e-8;
  //iterate through blocks, finding the faces that player is looking through
  int bounces = 0;
  //color components take on the product of texture components
  vec3 color(1, 1, 1);
  while(bounces < MAX_BOUNCES)
  {
    ivec3 blockIter;
    bool escape = false;
    vec3 normal;
    Block prevMaterial, nextMaterial;
    vec3 intersect = collideRay(origin, direction, blockIter, normal, prevMaterial, nextMaterial, escape);
    if(escape)
    {
      return processEscapedRay(intersect, direction, color, bounces, exact);
    }
    //hit a block: sample texture at point of intersection
    vec4 texel;
    if(normal.y > 0)
      texel = sample(nextMaterial, TOP, intersect.x, intersect.y, intersect.z);
    else if(normal.y < 0)
      texel = sample(nextMaterial, BOTTOM, intersect.x, intersect.y, intersect.z);
    else
      texel = sample(nextMaterial, SIDE, intersect.x, intersect.y, intersect.z);
    if(MAX_BOUNCES == 1)
    {
      exact = true;
      if(nextMaterial == WATER)
        return waterBlue;
      if(texel.w > 0.5)
        return vec3(texel);
    }
    //depending on transparency of sampled texel and approx. Fresnel reflection coefficient,
    //choose whether to reflect or refract
    if(prevMaterial == AIR && nextMaterial == WATER)
    {
      //light, faint blue-green
      texel = vec4(waterHue, 0);
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
    bool transparent = texel.w < 0.5;
    //decide what behavior this ray should take
    {
      //passing from one transparent medium to another
      float nPrev = refractIndex[prevMaterial];
      float nNext = refractIndex[nextMaterial];
      //smaller angle of incidence means more likely to refract (Fresnel)
      float cosTheta = fabsf(glm::dot(-normal, direction));
      float r0 = (nPrev - nNext) / (nPrev + nNext);
      r0 *= r0;
      float reflectProb = r0 + (1 - r0) * powf(1 - cosTheta, 5);
      if(nPrev == nNext)
      {
        if(prevMaterial == WATER)
        {
          //first argument is how much light passes through 1m of water
          color *= powf(waterClarity, glm::length(origin - intersect));
        }
        //refracted means don't process reflection and scattering below
        refracted = true;
      }
      else if(nPrev > nNext)
      {
        //check for total internal reflection here
        float cosCriticalAngle = cosf(asinf(nNext / nPrev));
        if(cosTheta >= cosCriticalAngle + eps)
        {
          //always refract when going down in index of refraction
          direction = normalize(glm::refract(direction, normal, nPrev / nNext));
          color.x *= texel.x;
          color.y *= texel.y;
          color.z *= texel.z;
          refracted = true;
          bounces++;
        }
        //else: reflect (below)
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
      //but de-saturate after each bounce
      float brightness = glm::length(vec3(texel));
      if(bounces == 0)
      {
        color = vec3(texel);
      }
      //how much color a ray picks up from reflecting off a surface
      float colorBlend = 0.1;
      color *= (0.5f + 0.5f * brightness) * (1 - colorBlend);
      color.x += colorBlend * texel.x;
      color.y += colorBlend * texel.y;
      color.z += colorBlend * texel.z;
      direction = scatter(direction, normal, nextMaterial);
      bounces++;
    }
    origin = intersect;
  }
  //light bounced too many times without reaching light source,
  //so no light contributed from this ray
  return vec3(0, 0, 0);
}

vec3 collideRay(vec3 origin, vec3 direction, ivec3& block, vec3& normal, Block& prevMat, Block& nextMat, bool& escape)
{
  const float eps = 1e-8;
  ivec3 blockIter(ipart(origin.x + eps), ipart(origin.y + eps), ipart(origin.z + eps));
  if(fpart(origin.x) < eps && direction.x < 0)
    blockIter.x -= 1;
  if(fpart(origin.y) < eps && direction.y < 0)
    blockIter.y -= 1;
  if(fpart(origin.z) < eps && direction.z < 0)
    blockIter.z -= 1;
  prevMat = getBlock(blockIter.x, blockIter.y, blockIter.z);
  while(true)
  {
    if(blockIter.x < 0 || blockIter.y < 0 || blockIter.z < 0 ||
        blockIter.x >= chunksX * 16 || blockIter.y >= chunksY * 16 || blockIter.z >= chunksZ * 16)
    {
      escape = true;
      block = blockIter;
      return origin;
    }
    Block prevMaterial = getBlock(blockIter.x, blockIter.y, blockIter.z);
    //trace ray through space until a different material is encountered
    int cx = ipart(blockIter.x / 16.0f);
    int cy = ipart(blockIter.y / 16.0f);
    int cz = ipart(blockIter.z / 16.0f);
    bool chunkInBounds = cx >= 0 && cy >= 0 && cz >= 0 &&
      cx < chunksX && cy < chunksY && cz < chunksZ;
    bool emptyChunk = !chunkInBounds || (chunkInBounds && chunks[cx][cy][cz].numFilled == 0);
    //the origin (corner) of chunk that ray is in (or about to enter, if on boundary)
    vec3 chunkOrigin(16 * cx, 16 * cy, 16 * cz);
    //point of intersection with next cube face (block or chunk)
    vec3 intersect;
    if(emptyChunk)
      intersect = rayCubeIntersect(origin, direction, normal, chunkOrigin, 16);
    else
      intersect = rayCubeIntersect(origin, direction, normal, blockIter, 1);
    ivec3 nextBlock(ipart(intersect.x + eps), ipart(intersect.y + eps), ipart(intersect.z + eps));
    if(fpart(intersect.x) < eps && direction.x < 0)
      nextBlock.x -= 1;
    if(fpart(intersect.y) < eps && direction.y < 0)
      nextBlock.y -= 1;
    if(fpart(intersect.z) < eps && direction.z < 0)
      nextBlock.z -= 1;
    Block nextMaterial = getBlock(nextBlock.x, nextBlock.y, nextBlock.z);
    if(prevMaterial != nextMaterial)
    {
      block = nextBlock;
      nextMat = nextMaterial;
      return intersect;
    }
    origin = intersect;
    blockIter = nextBlock;
  }
  return vec3(0, 0, 0);
}

vec3 waterNormal(vec3 position)
{
  //higher freq = more ripples per distance
  const float frequency = 0.4;
  const double timeScale = 1;
  //since the position of water fragments is perfectly flat,
  //amplitude needs to be very small
  const double k = 0.01;
  double scaledTime = fmod(currentTime, M_PI * 2) * timeScale;
  double x = fpart(position.x) * 2 * M_PI * (frequency * 1.5) + scaledTime;
  double z = fpart(position.z) * 2 * M_PI * frequency + scaledTime;
  return normalize(vec3(-k * cos(x) * cos(z), 1, k * sin(x) * sin(z)));
}

vec3 processEscapedRay(vec3 pos, vec3 direction, vec3 color, int bounces, bool& exact)
{
  float sunDot = glm::dot(direction, -sunlight);
  //if nothing has been hit yet, return sky or sun color
  if(bounces == 0 && direction.y > 0)
  {
    exact = true;
    if(sunDot >= cosSunRadius)
      return sunYellow;
    else
      return skyBlue;
  }
  if(MAX_BOUNCES == 1 && direction.y < 0)
  {
    return waterBlue;
  }
  if(pos.y <= seaLevel && direction.y <= 0)
  {
    //ray goes through infinitely deep ocean: no light escapes
    return vec3(0, 0, 0);
  }
  float nwater = refractIndex[WATER];
  if(pos.y < seaLevel && direction.y > 0)
  {
    vec3 throughWater = direction * (pos.y / direction.y);
    vec3 intersect = pos + throughWater;
    vec3 normal = waterNormal(intersect);
    float cosTheta = glm::dot(direction, normal);
    //check for total internal reflection
    float cosCriticalAngle = cosf(asinf(1 / nwater));
    if(cosTheta >= cosCriticalAngle)
    {
      direction = normalize(glm::refract(direction, normal, nwater));
      color *= powf(waterClarity, glm::length(throughWater));
    }
    else
    {
      //internal reflection
      return vec3(0, 0, 0);
    }
  }
  else if(pos.y > seaLevel && direction.y < 0)
  {
    vec3 intersect = pos + direction * (pos.y / direction.y);
    vec3 normal = waterNormal(intersect);
    //smaller angle of incidence means more likely to refract (Fresnel)
    float cosTheta = glm::dot(-normal, direction);
    float r0 = (1 - nwater) / (1 + nwater);
    r0 *= r0;
    float reflectProb = r0 + (1 - r0) * powf(1 - cosTheta, 5);
    if(float(rand()) / RAND_MAX < reflectProb)
    {
      //reflect off surface and apply water color
      color.x *= waterHue.x;
      color.y *= waterHue.y;
      color.z *= waterHue.z;
      direction = glm::reflect(direction, normal);
    }
    else
    {
      //enter water
      return vec3(0, 0, 0);
    }
  }
  sunDot = glm::dot(direction, -sunlight);
  const float ambient = 1.0;
  const float diffuse = 0.0;
  const float specular = 10.0;
  if(sunDot < 0)
    sunDot = 0;
  return (ambient + diffuse * sunDot + specular * powf(sunDot, 10)) * color;
}

bool visibleFromSun(vec3 pos, bool air)
{
  vec3 dir = -sunlight;
  //if ray is not in air,
  //  use monte carlo (if any one of several rays escapes
  //  near the sun, return true
  //  This allows the ray to pass through all transparent materials, with ideal refraction
  //otherwise just trace directly towards sun, and
  //  check if center of sun is directly visible
  if(air)
  {
    ivec3 block;
    vec3 normal;
    Block prevMat, nextMat;
    bool escape;
    //does a ray pointed at the sun escape?
    collideRay(pos, -sunlight, block, normal, prevMat, nextMat, escape);
    return escape;
  }
  else
  {
    const int samples = 16;
    const int maxBounces = 4;
    float threshold = cosSunRadius;
    vec3 dir = -sunlight;
    vec3 normal;
    ivec3 block;  //don't care about this
    Block prevMat, nextMat;
    bool escape = false;
    for(int i = 0; i < samples; i++)
    {
      int bounces = 0;
      while(bounces < maxBounces)
      {
        vec3 intersect = collideRay(pos, dir, block, normal, prevMat, nextMat, escape);
        if(escape && glm::dot(dir, -sunlight) >= cosSunRadius)
        {
          //Found a ray that escaped to sun
          return true;
        }
        //update normal if it's water
        if(prevMat == WATER || nextMat == WATER)
        {
          if(normal.y < 0)
            normal = -waterNormal(intersect);
          else if(normal.y > 0)
            normal = waterNormal(intersect);
        }
        //sample texel
        vec4 texel;
        if(normal.y > 0)
          texel = sample(nextMat, TOP, intersect.x, intersect.y, intersect.z);
        else if(normal.y < 0)
          texel = sample(nextMat, BOTTOM, intersect.x, intersect.y, intersect.z);
        else
          texel = sample(nextMat, SIDE, intersect.x, intersect.y, intersect.z);
        if(texel.w > 0.5)
        {
          //opaque texel, this ray doesn't reach sun
          break;
        }
        //otherwise, use refraction or total internal reflection to update dir
        float nPrev = refractIndex[prevMat];
        float nNext = refractIndex[nextMat];
        if(nPrev > nNext)
        {
          //Going down in index: need to check for total internal reflection
          float cosTheta = fabsf(glm::dot(normal, dir));
          float cosCriticalAngle = cosf(asinf(nNext / nPrev));
          if(cosTheta >= cosCriticalAngle + eps)
          {
            dir = normalize(glm::refract(dir, normal, nPrev / nNext));
          }
          else
          {
            dir = normalize(glm::reflect(dir, normal));
          }
        }
        else if(nNext > nPrev)
        {
          //always refract (no critical angle)
          dir = normalize(glm::refract(dir, normal, nPrev / nNext));
        }
        //else: direction can't change
        pos = intersect;
        bounces++;
      }
    }
    return false;
  }
}

void toggleFancy()
{
  fancy = !fancy;
  if(fancy)
  {
    RAY_W = 320;
    RAY_H = 200;
    MAX_BOUNCES = 4;
    RAYS_PER_PIXEL = 50;
    RAY_THREADS = 4;
  }
  else
  {
    RAY_W = 200;
    RAY_H = 150;
    MAX_BOUNCES = 1;
    RAYS_PER_PIXEL = 1;
    RAY_THREADS = 4;
  }
  delete[] frameBuf;
  frameBuf = new byte[4 * RAY_W * RAY_H];
}

