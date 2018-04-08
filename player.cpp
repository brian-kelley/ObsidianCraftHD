#include "player.hpp"
#include "world.hpp"
#include "ray.hpp"
#include <iostream>

using std::cout;
using std::ostream;

ostream& operator<<(ostream& os, vec3 v);
ostream& operator<<(ostream& os, vec4 v);

//position
vec3 player;
//look direction
vec3 look;
//velocity
vec3 vel;
float yaw;
float pitch;

int GRAVITY = 20;

mat4 view;
mat4 proj;
mat4 viewInv;
mat4 projInv;

bool onGround;

int breakFrames; //how many frames player has been breaking (breakX, breakY, breakZ)
int breakX;
int breakY;
int breakZ;

struct Hitbox
{
  Hitbox(float xx, float yy, float zz, float ll, float hh, float ww)
  {
    x = xx;
    y = yy;
    z = zz;
    l = ll;
    h = hh;
    w = ww;
  }
  float x;
  float y;
  float z;
  float l;  //x size
  float h;  //y size
  float w;  //z size
};

//6 cardinal directions: plus/minus x/y/z
enum
{
  HB_MX,
  HB_PX,
  HB_MY,
  HB_PY,
  HB_MZ,
  HB_PZ
};

void updateView()
{
  look = vec3(cosf(pitch) * cosf(yaw), sinf(pitch), cosf(pitch) * sinf(yaw));
  vec3 right(-sinf(yaw), 0, cosf(yaw));
  vec3 up = glm::cross(right, look);
  view = glm::lookAt(player, player + look, up);
  viewInv = glm::inverse(view);
}

void initPlayer()
{
  //place player just above ceiling, center of world
  player = vec3(chunksX * 16 / 2, chunksY * 16 + 2, chunksZ * 16 / 2);
  onGround = false;
  vel = vec3(0, 0, 0);
  yaw = 0;
  pitch = -M_PI / 2;
  updateView();
  proj = glm::perspective<float>(M_PI / 4, (float) RAY_W / RAY_H, NEAR_PLANE, FAR_PLANE);
  projInv = glm::inverse(proj);
}

bool moveHitbox(Hitbox* hb, int dir, float d)
{
  //make sure d is positive
  if(d < 0)
  {
    dir--;
    d = -d;
  }
  while(d > 0.5)
  {
    bool hit = moveHitbox(hb, dir, 0.5);
    d -= 0.5;
    if(hit)
      return true;
  }
  switch(dir)
  {
    case HB_MX:
      hb->x -= d;
      break;
    case HB_PX:
      hb->x += d;
      break;
    case HB_MY:
      hb->y -= d;
      break;
    case HB_PY:
      hb->y += d;
      break;
    case HB_MZ:
      hb->z -= d;
      break;
    case HB_PZ:
      hb->z += d;
      break;
    default:;
  }
  //scan for collision between hitbox and blocks
  bool collide = false;
  for(int bx = hb->x; bx <= hb->x + hb->l; bx++)
  {
    for(int by = hb->y; by <= hb->y + hb->h; by++)
    {
      for(int bz = hb->z; bz <= hb->z + hb->w; bz++)
      {
        if(getBlock(bx, by, bz) != AIR)
        {
          collide = true;
          break;
        }
      }
    }
  }
#ifdef NOCLIP
  collide = false;
#endif
  if(collide)
  {
    float eps = 1e-5;
    //depending on movement direction, nudge in opposite direction so not colliding
    switch(dir)
    {
      case HB_MX:
        hb->x = ((int) hb->x) + 1 + eps;
        break;
      case HB_PX:
        hb->x = ((int) (hb->x + hb->l)) - hb->l - eps;
        break;
      case HB_MY:
        hb->y = ((int) hb->y) + 1 + eps;
        break;
      case HB_PY:
        hb->y = ((int) (hb->y + hb->h)) - hb->h - eps;
        break;
      case HB_MZ:
        hb->z = ((int) hb->z) + 1 + eps;
        break;
      case HB_PZ:
        hb->z = ((int) (hb->z + hb->w)) - hb->w - eps;
        break;
    }
  }
  return collide;
}

//dt = elapsed time
//(dx, dz) = player motion (WASD)
//(dyaw, dpitch) = mouse motion
void updatePlayer(float dt, int dx, int dz, float dyaw, float dpitch, bool jump, int dy)
{
  bool viewStale = false;
  if(dy)
  {
    GRAVITY = 0;
    vel.y = dy * JUMP_SPEED;
  }
  else if(jump && onGround)
  {
    GRAVITY = 20;
    vel.y = JUMP_SPEED;
    onGround = false;
  }
  //handle orientation changes via ijkl
  const float pitchLimit = 90 / (180.0f / M_PI);
  yaw += X_SENSITIVITY * dyaw;
  //clamp yaw to 0:2pi (but preserve rotation beyond the bounds)
  if(yaw < 0)
    yaw += 2 * M_PI;
  else if(yaw >= 2 * M_PI)
    yaw -= 2 * M_PI;
  pitch -= Y_SENSITIVITY * dpitch;
  //clamp pitch to -pitchLimit:pitchLimit
  if(pitch < -pitchLimit)
    pitch = -pitchLimit;
  else if(pitch > pitchLimit)
    pitch = pitchLimit;
  vel.x =
    dx * PLAYER_SPEED * cosf(yaw) +
    dz * PLAYER_SPEED * sinf(-yaw);
  vel.z =
    dx * PLAYER_SPEED * sinf(yaw) +
    dz * PLAYER_SPEED * cosf(-yaw);
  if(dyaw != 0 || dpitch != 0 || dx || dz)
  {
    //player look direction changed, so view matrix must be updated
    viewStale = true;
  }
  vec3 old = player;
  Hitbox hb(player.x - PLAYER_WIDTH / 2, player.y - PLAYER_EYE, player.z - PLAYER_WIDTH / 2, PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH);
  //gravitational acceleration
  vel.y -= dt * GRAVITY;
  if(vel.y < -TERMINAL_VELOCITY)
    vel.y = -TERMINAL_VELOCITY;
  if(vel.y > 0)
  {
    bool hit = moveHitbox(&hb, HB_PY, dt * vel.y);
    onGround = false;
    if(hit)
      vel.y = 0;
  }
  else if(vel.y < 0)
  {
    bool hit = moveHitbox(&hb, HB_MY, dt * -vel.y);
    if(hit)
    {
      vel.y = 0;
      onGround = true;
    }
  }
  //clear vertical velocity each frame while flying
  if(GRAVITY == 0)
    vel.y = 0;
  //move horizontally
  moveHitbox(&hb, vel.x > 0 ? HB_PX : HB_MX, fabsf(dt * vel.x));
  moveHitbox(&hb, vel.z > 0 ? HB_PZ : HB_MZ, fabsf(dt * vel.z));
  //clamp player to world boundaries (can't fall out of world)
  if(hb.x < 0)
    hb.x = 0;
  if(hb.x > chunksX * 16 - PLAYER_WIDTH)
    hb.x = chunksX * 16 - PLAYER_WIDTH;
  //bedrock is at y = 1, so clamp there
  if(hb.y < 1)
    hb.y = 1;
  //don't need to clamp on sky limit, because gravity will always bring player back
  if(hb.z < 0)
    hb.z = 0;
  if(hb.z > chunksZ * 16 - PLAYER_WIDTH)
    hb.z = chunksZ * 16 - PLAYER_WIDTH;
  //copy position back to player vector
  player.x = hb.x + PLAYER_WIDTH / 2;
  player.y = hb.y + PLAYER_EYE;
  player.z = hb.z + PLAYER_WIDTH / 2;
  //test whether player position actually changed this update
  if(old != player)
    viewStale = true;
  if(viewStale)
  {
    //recompute view matrix and its inverse
    updateView();
  }
}

