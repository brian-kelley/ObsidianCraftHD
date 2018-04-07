cmake_minimum_required (VERSION 3.1)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_C_FLAGS "-std=gnu11 -Wall -O3 -march=native -mtune=native -mavx2 -ffast-math -fomit-frame-pointer -fassociative-math -ffast-math")
set(CMAKE_CXX_FLAGS "-std=c++11 -Wc++11-extensions -O3 -Wall -march=native -mtune=native -mavx2 -ffast-math -fomit-frame-pointer -fassociative-math -Wno-extern-c-compat")

#project (ObsidianCraftHD)

find_library(PTHREAD_LIB pthread)
if(NOT PTHREAD_LIB)
  message(FATAL_ERROR "pthread library not found.")
endif()

# Interactive requires: SDL2, OpenGL 2+
find_library(SDL2_LIB SDL2)
if(NOT SDL2_LIB)
  message(FATAL_ERROR "SDL2 library not found.")
endif()

find_library(OPENGL_LIB OpenGL)
if(NOT OPENGL_LIB)
  find_library(OPENGL_LIB GL)
  if(NOT OPENGL_LIB)
    message(FATAL_ERROR "OpenGL library not found.")
  endif()
endif()

include_directories(/usr/local/include)

# Create Mandelbrot executable
add_executable(OCHD
  main.cpp
  ray.cpp
  world.cpp
  tiles.cpp
  player.cpp
  keyframe.cpp
)

target_link_libraries(OCHD ${PTHREAD_LIB} ${SDL2_LIB} ${OPENGL_LIB})

