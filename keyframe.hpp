#ifndef KEYFRAME_H
#define KEYFRAME_H

#include <vector>
#include <string>
#include "glmHeaders.hpp"
#include "player.hpp"

using std::vector;
using std::string;

struct Keyframe
{
  Keyframe() : pos(player), y(yaw), p(pitch)
  {}
  vec3 pos;
  float y;  //yaw
  float p;  //pitch
};

extern vector<Keyframe> keyframes;
void captureKeyframe();
void saveKeyframes(string fname);
void loadKeyframes(string fname);
void animate(float sec, string folder);

#endif

