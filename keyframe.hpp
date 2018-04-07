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
  Keyframe() : pos(player), dir(look)
  {}
  vec3 pos;
  vec3 dir;
};

extern vector<Keyframe> keyframes;
void captureKeyframe();
void saveKeyframes(string fname);
void loadKeyframes(string fname);

#endif

