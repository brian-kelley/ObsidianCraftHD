#ifndef PLAYER_H
#define PLAYER_H

#include "glmHeaders.hpp"

#define PLAYER_HEIGHT 1.8
#define PLAYER_EYE 1.5
#define PLAYER_WIDTH 0.8
#define PLAYER_REACH 5      //how far player can place and destroy
#define BREAK_TIME 15       //how many frames it takes to break a block
#define PLAYER_SPEED 5      //horizontal movement speed
#define JUMP_SPEED 9        //vertical takeoff speed of jump
#define GRAVITY 20          //g, in m/s^2
#define TERMINAL_VELOCITY 20

#define X_SENSITIVITY 0.008
#define Y_SENSITIVITY 0.008

#define NEAR_PLANE 0.01f
#define FAR_PLANE 500.f

//position
extern vec3 player;
//velocity
extern vec3 vel;
extern float yaw;     //yaw, 0 means looking +X
extern float pitch;   //pitch, 0 means in XZ plane
extern mat4 projInv;
extern mat4 viewInv;
extern mat4 view;
extern mat4 proj;

void initPlayer();
void updatePlayer(float dt, int dx, int dy, float dyaw, float dpitch, bool jump);

#endif

