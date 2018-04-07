#include "keyframe.hpp"
#include <cstdio>
#include <iostream>

using std::cout;

std::ostream& operator<<(std::ostream& os, vec3 v);
std::ostream& operator<<(std::ostream& os, vec4 v);

vector<Keyframe> keyframes;

void captureKeyframe()
{
  cout << "Capturing keyframe: pos = " << player << ", dir = " << look << '\n';
  keyframes.emplace_back();
}

void saveKeyframes(string fname)
{
  FILE* f = fopen(fname.c_str(), "w");
  fprintf(f, "n %d\n", (int) keyframes.size());
  for(auto& kf : keyframes)
  {
    fprintf(f, "p %f %f %f d %f %f %f\n",
        kf.pos.x, kf.pos.y, kf.pos.z, kf.dir.x, kf.dir.y, kf.dir.z);
  }
  fclose(f);
}

void loadKeyframes(string fname)
{
  keyframes.clear();
  FILE* f = fopen(fname.c_str(), "r");
  int n = 0;
  fscanf(f, "n %d", &n);
  for(int i = 0; i < n; i++)
  {
    Keyframe kf;
    fscanf(f, "p %f %f %f d %f %f %f\n",
        &kf.pos.x, &kf.pos.y, &kf.pos.z, &kf.dir.x, &kf.dir.y, &kf.dir.z);
    keyframes.push_back(kf);
  }
  fclose(f);
}

