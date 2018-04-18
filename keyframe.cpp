#include "keyframe.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>

using std::cout;
using std::ostringstream;

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
    fprintf(f, "p %f %f %f y %f p %f\n",
        kf.pos.x, kf.pos.y, kf.pos.z, kf.y, kf.p);
  }
  fclose(f);
}

void loadKeyframes(string fname)
{
  keyframes.clear();
  FILE* f = fopen(fname.c_str(), "r");
  if(!f)
  {
    cout << "Failed to open " << fname << " for reading\n";
    exit(1);
  }
  int n = 0;
  fscanf(f, "n %d\n", &n);
  for(int i = 0; i < n; i++)
  {
    Keyframe kf;
    fscanf(f, "p %f %f %f y %f p %f\n",
        &kf.pos.x, &kf.pos.y, &kf.pos.z, &kf.y, &kf.p);
    keyframes.push_back(kf);
  }
  fclose(f);
}

const int splineSteps = 256;

const float catmullValues[16] = {
  0, -0.5, 1, -0.5,
  1, 0, -2.5, 1.5,
  0, 0.5, 2, -1.5,
  0, 0, -0.5, 0.5};

const mat4 catmullBasis = glm::transpose(glm::make_mat4(catmullValues));

struct Spline
{
  Spline(vec3* points)
  {
    //columns of G are point coordinates
    glm::mat4x3 G;
    for(int i = 0; i < 4; i++)
    {
      G[i][0] = points[i].x;
      G[i][1] = points[i].y;
      G[i][2] = points[i].z;
    }
    matrix = G * catmullBasis;
    ulength.resize(splineSteps + 1);
    ulength[0] = 0;
    for(int i = 1; i <= splineSteps; i++)
    {
      float u1 = float(i - 1) / splineSteps;
      float u2 = float(i) / splineSteps;
      vec4 a1(1, u1, u1*u1, u1*u1*u1);
      vec4 a2(1, u2, u2*u2, u2*u2*u2);
      vec3 p1 = matrix * a1;
      vec3 p2 = matrix * a2;
      ulength[i] = ulength[i - 1] + glm::length(p1 - p2);
    }
    arclen = ulength[splineSteps];
    //normalize ulength
    for(int i = 0; i <= splineSteps; i++)
    {
      ulength[i] /= arclen;
    }
  }
  glm::mat4x3 matrix;
  vector<float> ulength;
  float arclen;
};

void render(bool, string);

//Convert yaw (tx), pitch (ty) to a quaternion (roll always 0)
glm::quat eulerToQuat(float tx, float ty)
{
  return glm::quat(vec3(tx, ty, 0));
}

//Set pitch, yaw, look and view from quaternion
void setViewQuat(glm::quat q)
{
  vec3 angles = glm::eulerAngles(q);
  yaw = angles.x;
  pitch = angles.y;
  updateView();
}

void toggleFancy();
extern double currentTime;

void animate(float sec, string folder)
{
  const int fps = 30;
  //timesteps = how many frames to render
  int timesteps = sec * fps;
  //construct Catmull-Rom splines to pass through each keyframe
  //need at least 4 keyframes to do this
  int n = keyframes.size();
  if(n == 0)
  {
    cout << "Need at least one keyframe!\n";
    exit(1);
  }
  if(n == 1)
  {
    //stationary video but with animated water
    toggleFancy();
    player = keyframes[0].pos;
    setViewQuat(eulerToQuat(keyframes[0].y, keyframes[0].p));
    for(int f = 0; f < timesteps; f++)
    {
      cout << "Rendering frame " << f+1 << " of " << timesteps << '\n';
      currentTime = float(f) / fps;
      char fname[32];
      sprintf(fname, "%s/f_%05d.png", folder.c_str(), f);
      render(true, string(fname));
    }
  }
  keyframes.insert(keyframes.begin(), keyframes.front());
  keyframes.insert(keyframes.end(), keyframes.back());
  n += 2;
  //construct a sequence of splines to pass between each pair of keyframes,
  //except the first two and last two control points
  vector<Spline> splines;
  for(int i = 1; i < n - 2; i++)
  {
    vec3 points[4];
    for(int j = 0; j < 4; j++)
    {
      points[j] = keyframes[i + j - 1].pos;
    }
    splines.emplace_back(points);
  }
  vector<float> arclenPrefix(splines.size() + 1);
  arclenPrefix[0] = 0;
  for(size_t i = 0; i < splines.size(); i++)
  {
    arclenPrefix[i + 1] = arclenPrefix[i] + splines[i].arclen;
  }
  for(int i = 0; i <= splines.size(); i++)
  {
    arclenPrefix[i] /= arclenPrefix[splines.size()];
  }
  vector<glm::quat> keyframeQuats;
  for(auto& kf : keyframes)
  {
    keyframeQuats.push_back(eulerToQuat(kf.y, kf.p));
  }
  //make sure all LERPs produce shortest path
  for(size_t i = 1; i < keyframeQuats.size(); i++)
  {
    if(glm::dot(keyframeQuats[i - 1], keyframeQuats[i]) < 0)
    {
      keyframeQuats[i] *= -1.0f;
    }
  }
  toggleFancy();
  for(int f = 0; f < timesteps; f++)
  {
    cout << "Rendering frame " << f+1 << " of " << timesteps << '\n';
    currentTime = float(f) / fps;
    float t = float(f) / timesteps;
    //use arclenPrefix to figure out which spline t corresponds to
    //call the spline index s
    int s = 0;
    for(size_t i = 0; i < splines.size(); i++)
    {
      if(t >= arclenPrefix[i] && t < arclenPrefix[i + 1])
      {
        s = i;
        break;
      }
    }
    Spline& spline = splines[s];
    //get t parameter within spline (still arclength parameterized and in unit interval)
    float st = (t - arclenPrefix[s]) / (arclenPrefix[s + 1] - arclenPrefix[s]);
    //get final spline parameter u (NOT arclength parameterized)
    float su = 0;
    for(int i = 0; i < splineSteps; i++)
    {
      if(st >= spline.ulength[i] && st < spline.ulength[i + 1])
      {
        float k = (st - spline.ulength[i]) / (spline.ulength[i + 1] - spline.ulength[i]);
        su = (float(i) + k) / splineSteps;
        break;
      }
    }
    //compute the point on spline
    vec4 splineArg(1, su, su*su, su*su*su);
    player = spline.matrix * splineArg;
    //compute orientation by interpolating keyframe quaternions
    glm::mat4 controlQuatMat;
    quat controlQuats[4];
    for(int i = 0; i < 4; i++)
    {
      controlQuats[i] = keyframeQuats[s + i - 1];
    }
    for(int i = 1; i < 4; i++)
    {
      if(glm::dot(controlQuats[i - 1], controlQuats[i]) < 0)
      {
        controlQuats[i] = -controlQuats[i];
      }
    }
    for(int i = 0; i < 4; i++)
    {
      for(int j = 0; j < 4; j++)
      {
        controlQuatMat[i][j] = controlQuats[i][j];
      }
    }
    vec4 orientVector = normalize(controlQuatMat * catmullBasis * splineArg);
    setViewQuat(quat(orientVector.w, orientVector.x, orientVector.y, orientVector.z));
    char fname[32];
    sprintf(fname, "%s/f_%05d.png", folder.c_str(), f);
    render(true, string(fname));
  }
}

