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

//ambient factor should be small, as it is not realistic at
//all but just makes shadows easier on the eyes
const float ambient = 0.08;
//scale all specular light contributions by this
const float specularScale = 1;
const float specExpo = 80;
//All materials use the same Blinn-Phong specular exponent,
//but they have a range of specular intensities
//If this is ever changed to be per-material, water should have
//this exact value
const vec3 skyBlue(0.34, 0.78, 1.0);
const vec3 sunYellow(1, 1, 0.8);
//color of water in non-fancy mode
const vec3 waterBlue(0.15, 0.3, 0.5);
//color applied to water when reflect/refract from air
const vec3 waterHue = vec3(0.6, 0.8, 0.9);
const float waterClarity = 0.96;
//sunlight direction
vec3 sunlight = normalize(vec3(1.0, -1, 0.5));
const float cosSunRadius = 0.998;
//multiply all ray contributions by this to keep image
//brightness in a reasonable range
const float brightnessAdjust = 5;

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
  vec3 direction = normalize(vec3(frontWorld) - vec3(backWorld));
  vec3 color(0, 0, 0);
  if(fancy)
  {
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
  }
  else
  {
    color = traceFast(vec3(backWorld), direction);
  }
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
  //iterate through blocks, finding the faces that player is looking through
  int bounces = 0;
  //color components take on the product of texture components
  vec3 color(0, 0, 0);
  vec3 colorInfluence(1, 1, 1);
  while(bounces < MAX_BOUNCES)
  {
    ivec3 blockIter;
    bool escape = false;
    vec3 normal;
    Block prevMaterial, nextMaterial;
    vec3 intersect = collideRay(origin, direction, blockIter, normal, prevMaterial, nextMaterial, escape);
    if(glm::length(colorInfluence) < 0.05f)
    {
      return color * brightnessAdjust;
    }
    if(escape)
    {
      return processEscapedRay(intersect, direction, color, colorInfluence, bounces, exact);
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
    if(isTransparent(prevMaterial) && nextMaterial == WATER)
    {
      //entering water from transparent material
      //apply water hue to all light from now on
      if(normal.y < 0)
        normal = -waterNormal(intersect);
      else if(normal.y > 0)
        normal = waterNormal(intersect);
    }
    else if(isTransparent(nextMaterial) && prevMaterial == WATER)
    {
      //apply a blue color to ray
      if(normal.y < 0)
        normal = -waterNormal(intersect);
      else if(normal.y > 0)
        normal = waterNormal(intersect);
    }
    if(nextMaterial == WATER)
    {
      texel = vec4(waterHue, 0);
    }
    else if(texel.w < 0.5)
    {
      texel = vec4(1, 1, 1, 0);
    }
    bool refract = false;
    //note: if reflecting off opaque material, these won't be used
    float nPrev = refractIndex[prevMaterial];
    float nNext = refractIndex[nextMaterial];
    float r0 = (nPrev - nNext) / (nPrev + nNext);
    r0 *= r0;
    //compute the Fresnel term using Schlick's approximatoin
    //this is used to decide refraction vs. reflection,
    //and also to determine reflectivity during reflection
    float cosTheta = fabsf(glm::dot(normal, direction));
    float fresnel = r0 + (1 - r0) * powf(1 - cosTheta, 5);
    if(texel.w < 0.5)
    {
      //from one transparent medium to another
      if(nPrev <= nNext)
      {
        //the smaller the fresnel coefficient,
        //the more likely ray is to refract
        if(float(rand()) / RAND_MAX > fresnel)
          refract = true;
      }
      else
      {
        float cosCriticalAngle = cosf(asinf(nNext / nPrev));
        if(cosTheta > cosCriticalAngle)
        {
          refract = true;
        }
        else if(prevMaterial == WATER)
        {
          colorInfluence *= waterHue;
        }
      }
    }
    if(refract)
    {
      direction = normalize(glm::refract(direction, normal, nPrev / nNext));
      if(prevMaterial == WATER)
      {
        float waterDarken = powf(waterClarity, glm::length(origin - intersect));
        colorInfluence *= waterDarken;
      }
    }
    else
    {
      //reflect ray
      if(prevMaterial == WATER)
      {
        //passed through some water, so reduce ray's brightness
        float waterDarken = powf(waterClarity, glm::length(origin - intersect));
        colorInfluence *= waterDarken;
      }
      if(nextMaterial == WATER)
      {
        //use waterBlue as the reflection color for water
        texel = vec4(waterBlue, 1);
      }
      float spec = ks[nextMaterial];
      float diff = kd[nextMaterial];
      bool shadowed = !visibleFromSun(intersect, normal, nPrev == 1);
      float diffContrib = 0;
      float specContrib = 0;
      if(!shadowed)
      {
        diffContrib = diff * fmax(0, glm::dot(normal, -sunlight));
        vec3 reflected = glm::reflect(direction, normal);
        if(glm::dot(reflected, -sunlight) >= cosSunRadius)
          specContrib = spec * specularScale;
      }
      color += (ambient + diffContrib + specContrib) * vec3(texel) * colorInfluence;
      //decide whether to add specular or diffuse lighting from sun,
      //based on ks and kd for nextMaterial
      float reflectivity = fmin(1, 0.5 * (spec + 0.3 * diff) * fresnel);
      vec3 bounceColor;
      if(nextMaterial != WATER && float(rand()) / RAND_MAX > (spec / (spec + diff)))
      {
        direction = normalize(vec3(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX));
        if(glm::dot(direction, normal) < 0)
          direction = -direction;
        //update color influence: very little light from subsequent bounces
        //will reflect off diffuse material and have significant color bleed
        bounceColor = reflectivity * desaturate(vec3(texel), 1 - fresnel);
      }
      else
      {
        direction = normalize(glm::reflect(direction, normal));
        //Specular reflection doesn't affect ray color, only intensity
        bounceColor = reflectivity * desaturate(vec3(texel), 0);
      }
      colorInfluence *= bounceColor;
      bounces++;
    }
    //continue from intersection
    origin = intersect;
  }
  //light bounced too many times without reaching light source,
  //so no light contributed from this ray
  return vec3(0, 0, 0);
}

vec3 traceFast(vec3 origin, vec3 direction)
{
  //color components take on the product of texture components
  vec3 color(0, 0, 0);
  vec3 colorInfluence(1, 1, 1);
  while(true)
  {
    ivec3 blockIter;
    bool escape = false;
    vec3 normal;
    Block prevMaterial, nextMaterial;
    vec3 intersect = collideRay(origin, direction, blockIter, normal, prevMaterial, nextMaterial, escape);
    if(escape)
    {
      if(glm::dot(direction, -sunlight) >= cosSunRadius)
        return sunYellow;
      else if(prevMaterial == AIR)
        return skyBlue;
      else
        return vec3(0, 0, 0);
    }
    //hit a block: sample texture at point of intersection
    vec4 texel;
    if(normal.y > 0)
      texel = sample(nextMaterial, TOP, intersect.x, intersect.y, intersect.z);
    else if(normal.y < 0)
      texel = sample(nextMaterial, BOTTOM, intersect.x, intersect.y, intersect.z);
    else
      texel = sample(nextMaterial, SIDE, intersect.x, intersect.y, intersect.z);
    //depending on transparency of sampled texel and approx. Fresnel reflection coefficient,
    //choose whether to reflect or refract
    if(isTransparent(prevMaterial) && nextMaterial == WATER)
    {
      //entering water from transparent material
      //apply water hue to all light from now on
      if(normal.y < 0)
        normal = -waterNormal(intersect);
      else if(normal.y > 0)
        normal = waterNormal(intersect);
    }
    else if(isTransparent(nextMaterial) && prevMaterial == WATER)
    {
      //apply a blue color to ray
      if(normal.y < 0)
        normal = -waterNormal(intersect);
      else if(normal.y > 0)
        normal = waterNormal(intersect);
    }
    if(prevMaterial == WATER)
    {
      colorInfluence *= waterHue * powf(waterClarity, glm::length(origin - intersect));
    }
    const float nwater = refractIndex[WATER];
    if(nextMaterial == WATER)
    {
      float r0 = (1 - nwater) / (1 + nwater);
      r0 *= r0;
      //compute the Fresnel term using Schlick's approximatoin
      //this is used to decide refraction vs. reflection,
      //and also to determine reflectivity during reflection
      float cosTheta = fabsf(glm::dot(normal, direction));
      float fresnel = r0 + (1 - r0) * powf(1 - cosTheta, 5);
      //draw refracted ray of whatever's underwater, and reflected ray of what's above water
      vec3 reflectRay = normalize(glm::reflect(direction, normal));
      vec3 refractRay = normalize(glm::refract(direction, normal, 1 / refractIndex[WATER]));
      vec3 base = (1-fresnel) * waterHue * traceFast(intersect, refractRay) + fresnel * traceFast(intersect, reflectRay);
      if(visibleFromSun(intersect, normal, true))
      {
        //compute additional specular component
        vec3 halfway = -normalize(direction + sunlight);
        float specContrib = specularScale * powf(fmax(0, glm::dot(halfway, normal)), specExpo);
        return base + specContrib * vec3(1, 1, 1);
      }
    }
    if(texel.w < 0.5)
    {
      //from one transparent medium to another
      if(prevMaterial == WATER && nextMaterial == AIR)
      {
        float cosCriticalAngle = cosf(asinf(1 / nwater));
        float cosTheta = fabsf(glm::dot(normal, direction));
        if(cosTheta > cosCriticalAngle)
        {
          return waterHue * traceFast(intersect, normalize(glm::refract(direction, normal, nwater)));
        }
        else
        {
          //total internal reflection, return dark water color to represent
          return 0.2f * waterBlue;
        }
      }
    }
    if(texel.w > 0.5)
    {
      //reflect ray
      if(prevMaterial == WATER)
      {
        //passed through some water, so reduce ray's brightness
        float waterDarken = powf(waterClarity, glm::length(origin - intersect));
        colorInfluence *= waterDarken;
      }
      if(nextMaterial == WATER)
      {
        //use waterBlue as the reflection color for water
        texel = vec4(waterBlue, 1);
      }
      float spec = ks[nextMaterial];
      float diff = kd[nextMaterial];
      //pretend point of interest is in air because it's a much faster test
      //this means shadows won't be refracted in fast mode
      bool shadowed = !visibleFromSun(intersect, normal, true);
      float diffContrib = 0;
      float specContrib = 0;
      if(!shadowed)
      {
        diffContrib = diff * fmax(0, glm::dot(normal, -sunlight));
        vec3 halfway = -normalize(sunlight + direction);
        specContrib = specularScale * spec * powf(fmax(0, glm::dot(halfway, normal)), specExpo);
      }
      return brightnessAdjust * colorInfluence * ((ambient + diffContrib) * vec3(texel) + vec3(1, 1, 1) * specContrib);
    }
    else
    {
      //continue tracing in same direction
      origin = intersect;
    }
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
  while(true)
  {
    prevMat = getBlock(blockIter.x, blockIter.y, blockIter.z);
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
    nextMat = getBlock(nextBlock.x, nextBlock.y, nextBlock.z);
    if(nextBlock.x < 0 || nextBlock.y < 0 || nextBlock.z < 0 ||
        nextBlock.x >= chunksX * 16 || nextBlock.y >= chunksY * 16 || nextBlock.z >= chunksZ * 16)
    {
      escape = true;
      block = nextBlock;
      return intersect;
    }
    if(prevMat != nextMat)
    {
      escape = false;
      block = nextBlock;
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
  //frequency is 1 near origin and approaches 2 at horizon
  const float frequency = 0.3 - 0.1 * powf(M_PI / 2, -2) * atanf(sqrtf(sqrtf(position.x * position.x + position.z * position.z)));
  const float timeScale = M_PI;
  //this is just a normal map over perfectly smooth water
  //to be plausible in shallow water, amplitude needs to be fairly small
  const float k = 0.08;
  float scaledTime = fmod(currentTime, M_PI * 2) * timeScale;
  float x = position.x * 2 * M_PI * frequency + scaledTime;
  float z = position.z * 2 * M_PI * frequency + scaledTime;
  //add a circular displacement to x/z parameters to reduce Moire patterns at long distance
  x *= (1 + 0.05 * sin(position.x / 11) * cos(position.z / 17));
  z *= (1 + 0.05 * cos(position.x / 17) * sin(position.z / 11));
  return normalize(vec3(-k * cos(x) * cos(z), 1, k * sin(x) * sin(z)));
}

vec3 processEscapedRay(vec3 pos, vec3 direction, vec3 color, vec3 colorInfluence, int bounces, bool& exact)
{
  float sunDot = glm::dot(direction, -sunlight);
  //if nothing has been hit yet, return sky or sun color
  if(bounces == 0 && direction.y > 0)
  {
    exact = true;
    if(sunDot >= cosSunRadius)
      return sunYellow;
    else
      return skyBlue * (0.7f + 0.3f * acosf(direction.y));
  }
  float nwater = refractIndex[WATER];
  if(pos.y >= seaLevel && direction.y < 0)
  {
    vec3 intersect = pos - direction * ((pos.y - seaLevel) / direction.y);
    vec3 normal = waterNormal(intersect);
    //smaller angle of incidence means more likely to refract (Fresnel)
    float cosTheta = fabsf(glm::dot(normal, -direction));
    float r0 = (1 - nwater) / (1 + nwater);
    r0 *= r0;
    float fresnel = r0 + (1 - r0) * powf(1 - cosTheta, 5);
    //reflect off surface; apply water color times ambient, diffuse, specular
    if(float(rand()) / RAND_MAX <= fresnel)
    {
      float diffContrib = kd[WATER] * fmax(0, glm::dot(-sunlight, normal));
      direction = normalize(glm::reflect(direction, normal));
      float specContrib = glm::dot(direction, -sunlight) > cosSunRadius ? ks[WATER] : 0;
      specContrib *= specularScale;
      color += (colorInfluence * waterBlue) * (ambient + diffContrib + specContrib);
      float reflectivity = fmin(1, 0.5 * (ks[WATER] + 0.3 * kd[WATER]) * fresnel);
      colorInfluence *= (reflectivity * desaturate(waterBlue, 0));
    }
    else
    {
      //refract to below water
      direction = normalize(glm::refract(direction, normal, 1 / nwater));
    }
  }
  if(direction.y > 0)
  {
    if(pos.y < seaLevel)
    {
      //trace ray up to water surface
      vec3 intersect = pos - direction * ((pos.y - seaLevel) / direction.y);
      vec3 normal = waterNormal(intersect);
      if(glm::dot(normal, direction) > cosf(acosf(1 / nwater)))
      {
        //refract to air and escape
        colorInfluence *= powf(glm::length(intersect - pos), waterClarity);
        direction = normalize(glm::refract(direction, normal, nwater));
      }
      else
      {
        //internal reflection: turn back underwater
        direction = normalize(glm::reflect(direction, normal));
      }
    }
    //if direction is still upwards, add sky/sun contribution
    if(fancy && direction.y > 0)
    {
      if(glm::dot(direction, -sunlight) >= cosSunRadius)
        color += colorInfluence * 0.5f * sunYellow;
      else
        color += colorInfluence * 0.5f * skyBlue;
    }
  }
  return color * brightnessAdjust;
}

vec3 processEscapedRayFast(vec3 pos, vec3 direction, vec3 color, vec3 colorInfluence)
{
  float sunDot = glm::dot(direction, -sunlight);
  //if nothing has been hit yet, return sky or sun color
  if(pos.y > 0 && direction.y > 0)
  {
    if(sunDot >= cosSunRadius)
      return sunYellow;
    else
      return skyBlue * (0.7f + 0.3f * acosf(direction.y));
  }
  float nwater = refractIndex[WATER];
  if(pos.y >= seaLevel && direction.y < 0)
  {
    vec3 intersect = pos - direction * ((pos.y - seaLevel) / direction.y);
    vec3 normal = waterNormal(intersect);
    //smaller angle of incidence means more likely to refract (Fresnel)
    float cosTheta = fabsf(glm::dot(normal, -direction));
    float r0 = (1 - nwater) / (1 + nwater);
    r0 *= r0;
    float fresnel = r0 + (1 - r0) * powf(1 - cosTheta, 5);
    //reflect off surface; apply water color times ambient, diffuse, specular
    float diffContrib = kd[WATER] * fmax(0, glm::dot(-sunlight, normal));
    vec3 halfway = -normalize(direction + sunlight);
    float specContrib = specularScale * ks[WATER] * powf(fmax(0, glm::dot(halfway, normal)), specExpo);
    return brightnessAdjust * fresnel * (waterBlue * (ambient + diffContrib) + vec3(1, 1, 1) * specContrib);
  }
  return color * brightnessAdjust;
}

bool visibleFromSun(vec3 pos, vec3 norm, bool air)
{
  if(glm::dot(norm, sunlight) > 0)
    return false;
  const float eps = 1e-8;
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
    bool escape = false;
    //does a ray pointed at the sun escape?
    while(true)
    {
      pos = collideRay(pos, -sunlight, block, normal, prevMat, nextMat, escape);
      if(escape)
        return true;
      if(!isTransparent(nextMat))
      {
        //hit a fully opaque block
        return false;
      }
      else
      {
        if(nextMat == GLASS || nextMat == LEAF)
        {
          //need to sample texture to figure out if specific
          //point of intersection is transparent or not
          //note that top and bottom of glass/leaves are identical
          vec4 texel;
          if(normal.y != 0)
            texel = sample(nextMat, TOP, pos.x, pos.y, pos.z);
          else
            texel = sample(nextMat, SIDE, pos.x, pos.y, pos.z);
          if(texel.w > 0.5)
          {
            return false;
          }
          //otherwise, continue ray
        }
      }
    }
    return false;
  }
  else
  {
    const int samples = 32;
    const int maxBounces = 4;
    float threshold = 0.99;
    vec3 normal;
    ivec3 block;  //don't care about this
    Block prevMat, nextMat;
    bool escape = false;
    for(int i = 0; i < samples; i++)
    {
      //Choose a random direction to try which is in hemisphere of sun
      vec3 dir;
      if(i == 0)
      {
        dir = -sunlight;
      }
      else
      {
        dir = vec3(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX);
        if(glm::dot(dir, -sunlight) < 0)
        {
          dir = -dir;
        }
      }
      int bounces = 0;
      while(bounces < maxBounces)
      {
        vec3 intersect = collideRay(pos, dir, block, normal, prevMat, nextMat, escape);
        if(escape && glm::dot(dir, -sunlight) >= threshold)
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
    RAY_W = 640;
    RAY_H = 480;
    MAX_BOUNCES = 6;
    RAYS_PER_PIXEL = 40;
    RAY_THREADS = 4;
  }
  else
  {
    RAY_W = 640;
    RAY_H = 480;
    MAX_BOUNCES = 1;
    RAYS_PER_PIXEL = 1;
    RAY_THREADS = 4;
  }
  delete[] frameBuf;
  frameBuf = new byte[4 * RAY_W * RAY_H];
}

