ObsidianCraftHD: a voxel ray tracer

Requirements: UNIX-like operating system (Linux/OSX), SDL2, OpenGL, GLM, Pthreads, CMake

Program generates finite world and renders it in real time.
Navigate with WASD (move), space (jump), and mouse/IJKL (look).

Press F to produce a high-quality ray traced rendering of the current perspective. This happens
offline in a separate process (the interactive application can still be used). Rendering will take a while!

Thanks to the [Painterly Pack](http://painterlypack.net/) for textures (using a version from 2011).
Thanks to the [STB libraries](https://github.com/nothings/stb) for PNG encoding and decoding.

![A sample render](https://raw.githubusercontent.com/brian-kelley/ObsidianCraftHD/master/sample.png)

