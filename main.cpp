#include "openglHeaders.hpp"
#include "SDL2/SDL.h"
#include <ctime>
#include "ray.hpp"
#include "world.hpp"
#include "player.hpp"
#include <sys/types.h>
#include <unistd.h>

#define GLERR {int e = glGetError(); if(e) \
  {printf("GL error %i, line %d\n", e, __LINE__); exit(1);}}

SDL_Window* window;
SDL_GLContext glContext;
GLuint textureID;
bool running;

const int viewportW = RAY_W;
const int viewportH = RAY_H;

double currentTime;

Uint32 ticksLastFrame;

void initWindow()
{
  //open window
  if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
  {
    puts("Failed to initialize SDL");
    exit(1);
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  window = SDL_CreateWindow("ObsidianCraft",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      viewportW, viewportH,
      SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
  SDL_CaptureMouse(SDL_TRUE);
  SDL_ShowCursor(SDL_DISABLE);
  SDL_WarpMouseInWindow(window, viewportW / 2, viewportH / 2);
  glContext = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1);
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glViewport(0, 0, viewportW, viewportH);
  glMatrixMode(GL_PROJECTION);
  glOrtho(0, viewportW, 0, viewportH, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClearColor(0, 0, 0, 1);
}

void initTexture()
{
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RAY_W, RAY_H, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void renderFrame()
{
  //run the ray tracer
  render(false);
  glClear(GL_COLOR_BUFFER_BIT);
  //update texture
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RAY_W, RAY_H, GL_RGBA, GL_UNSIGNED_BYTE, frameBuf);
  //draw the texture over whole viewport
  glColor3f(1, 1, 1);
  glColor4f(1, 1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1);
  glVertex2i(0, viewportH);
  glTexCoord2f(1, 1);
  glVertex2i(viewportW, viewportH);
  glTexCoord2f(1, 0);
  glVertex2i(viewportW, 0);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glEnd();
  SDL_GL_SwapWindow(window);
}

void processInput()
{
  SDL_PumpEvents();
  int dx = 0;
  int dz = 0;
  float dyaw = 0;
  float dpitch = 0;
  bool jump = false;
  SDL_Event event;
  while(SDL_PollEvent(&event))
  {
    switch(event.type)
    {
      case SDL_KEYDOWN:
        if(event.key.state == SDL_PRESSED)
        {
          if(event.key.keysym.scancode == SDL_SCANCODE_SPACE)
          {
            jump = true;
          }
          else if(event.key.keysym.scancode == SDL_SCANCODE_F)
          {
            //Render a high quality screenshot of the current view
            //Fork a new process to do this so that the original interactive
            //process can continue
            if(fork() == 0)
            {
              cout << "Rendering a high-quality screenshot in the background\n";
              cout << "You can close this application and the rendering will still run.\n";
              toggleFancy();
              render(true);
              toggleFancy();
              cout << "Done rendering screenshot.\n";
              exit(0);
            }
          }
          else if(event.key.keysym.scancode == SDL_SCANCODE_Q)
          {
            //Q = quit
            running = false;
          }
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        break;
      case SDL_MOUSEBUTTONUP:
        break;
      case SDL_MOUSEMOTION:
        dyaw = event.motion.xrel;
        dpitch = event.motion.yrel;
        SDL_WarpMouseInWindow(window, viewportW / 2, viewportH / 2);
        break;
      case SDL_MOUSEWHEEL:
        break;
      case SDL_QUIT:
        running = false;
        break;
      default:;
    }
  }
  //get state of every key
  const Uint8* keystate = SDL_GetKeyboardState(NULL);
  if(keystate[SDL_SCANCODE_W])
    dx++;
  if(keystate[SDL_SCANCODE_A])
    dz--;
  if(keystate[SDL_SCANCODE_S])
    dx--;
  if(keystate[SDL_SCANCODE_D])
    dz++;
  Uint32 nowTicks = SDL_GetTicks();
  float dt = (nowTicks - ticksLastFrame) / 1000.0f;
  ticksLastFrame = nowTicks;
  updatePlayer(dt, dx, dz, dyaw, dpitch, jump);
}

extern float fpart(float);
extern float ipart(float);

int main()
{
  frameBuf = new byte[4 * RAY_W * RAY_H];
  cout << "Generating terrain...\n";
  //terrainGen();
  flatGen();
  cout << "Done with terrain\n";
  //printWorldComposition();
  initWindow();
  initAtlas();
  initTexture();
  initPlayer();
  SDL_RaiseWindow(window);
  time_t timeSec = time(NULL);
  int fps = 0;
  running = true;
  while(running)
  {
    time_t currentTimeSec = time(NULL);
    if(timeSec != currentTimeSec && timeSec % 5 == 0)
    {
      printf("%i frames per second\n", fps);
      fps = 0;
      timeSec = currentTimeSec;
    }
    currentTime = SDL_GetTicks() / 1000.f;
    //process input also updates player physics
    processInput();
    renderFrame();
    fps++;
  }
  return 0;
}

