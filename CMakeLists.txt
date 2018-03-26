cmake_minimum_required (VERSION 3.1)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk)
project (ObsidianCraftHD)

#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c11 -Wall -march=native -mavx2 -ffast-math -fassociative-math")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -Wall -Wno-extern-c-compat -march=native -mavx2 -ffast-math -fassociative-math")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -std=c11 -Wall -Ofast -march=native -mtune=native -mavx2 -ffast-math -fomit-frame-pointer -fassociative-math -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -Ofast -march=native -mtune=native -mavx2 -ffast-math -fomit-frame-pointer -fassociative-math -Wno-extern-c-compat")

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
  message(FATAL_ERROR "OpenGL library not found.")
endif()

INCLUDE_DIRECTORIES($ENV{GLM_INCLUDE_DIR})

# Create Mandelbrot executable
add_executable(OCHD
  main.cpp
  ray.cpp
  world.cpp
  tiles.cpp
  player.cpp
)

target_link_libraries(OCHD ${PTHREAD_LIB} ${SDL2_LIB} ${OPENGL_LIB})

