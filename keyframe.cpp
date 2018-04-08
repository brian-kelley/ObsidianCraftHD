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

struct Spline
{
  Spline(vec3* points)
  {
    for(int i = 0; i < 4; i++)
    {
      cout << points[i] << '\n';
    }
    cout << '\n';
    //columns of G are point coordinates
    glm::mat4x3 G;
    for(int i = 0; i < 4; i++)
    {
      G[i][0] = points[i].x;
      G[i][1] = points[i].y;
      G[i][2] = points[i].z;
    }
    float bvals[16] = {
      0, -0.5, 1, -0.5,
      1, 0, -2.5, 1.5,
      0, 0.5, 2, -1.5,
      0, 0, -0.5, 0.5};
    glm::mat4 B = glm::transpose(glm::make_mat4(bvals));
    matrix = G*B;
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
      ulength[i] /= ulength[splineSteps];
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

void animate(float sec, string folder)
{
  //construct Catmull-Rom splines to pass through each keyframe
  //need at least 4 keyframes to do this
  int n = keyframes.size();
  if(n < 3)
  {
    cout << "Need at least 3 keyframes to form a loop\n";
    exit(1);
  }
  //construct a sequence of splines to pass between each pair of keyframes
  vector<Spline> splines;
  for(int i = 0; i < n; i++)
  {
    vec3 points[4];
    for(int j = 0; j < 4; j++)
    {
      points[j] = keyframes[(i + j - 1 + n) % n].pos;
    }
    splines.emplace_back(points);
  }
  vector<float> arclenPrefix(keyframes.size() + 1);
  arclenPrefix[0] = 0;
  for(int i = 0; i < n; i++)
  {
    arclenPrefix[i + 1] = arclenPrefix[i] + splines[i].arclen;
  }
  for(int i = 0; i <= n; i++)
  {
    arclenPrefix[i] /= arclenPrefix[n];
    cout << "arclenPrefix[i] = " << arclenPrefix[i] << '\n';
  }
  for(int i = 0; i < splines.size(); i++)
  {
    cout << "spline " << i << " has length " << splines[i].arclen << '\n';
  }
  //float timesteps = sec * 60;
  float timesteps = 40;
  vector<glm::quat> keyframeQuats;
  for(auto& kf : keyframes)
  {
    keyframeQuats.push_back(eulerToQuat(kf.y, kf.p));
  }
  for(int f = 0; f < timesteps; f++)
  {
    cout << "Rendering frame " << f+1 << " of " << timesteps << '\n';
    float t = float(f) / timesteps;
    cout << "t = " << t << '\n';
    //figure out which spline t is in
    int current;
    for(current = 0; current < n; current++)
    {
      if(t >= arclenPrefix[current])
        break;
    }
    Spline& spline = splines[current];
    cout << "in spline " << current << " with arclen " << spline.arclen << '\n';
    float st = (t - arclenPrefix[current]) * spline.arclen;
    cout << "t within spline is " << st << '\n';
    float su = 0;
    //figure out proper t within spline to keep constant speed
    for(int i = 0; i < splineSteps; i++)
    {
      if(st >= spline.ulength[i])
      {
        float k = (st - spline.ulength[i]) / (spline.ulength[i + 1] - spline.ulength[i]);
        su = k * spline.ulength[i + 1] + (1 - k) * spline.ulength[i];
        cout << "u within spline is " << su << '\n';
        break;
      }
    }
    //compute proper point on current spline
    vec4 splineArg(1, su, su*su, su*su*su);
    //set camera position
    player = spline.matrix * splineArg;
    //slerp between the current two keyframe orientations
    setViewQuat(glm::slerp(keyframeQuats[current], keyframeQuats[(current + 1) % n], su));
    cout << "camera position is " << player << '\n';
    cout << "player look dir is " << look << '\n';
    ostringstream oss;
    oss << folder << "/f_" << f << ".png";
    render(true, oss.str());
  }
}

