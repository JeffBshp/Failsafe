# Failsafe

A game in plain C with SDL and OpenGL.

At this point it's just a sandbox for me to learn 3D graphics. I am working on voxel rendering and collisions. Name is tentative.

### Usage

This assumes you're using Linux, otherwise you'll have to figure out how to link the required libraries yourself.
The project depends on SDL2, GLEW, CGLM, and stb_image.h.

1. Install prerequisites
	- OpenGL driver (e.g. Mesa or NVidia)
	- gcc, gdb, make
	- On Arch Linux: sdl2, glew, AUR/cglm, stb
	- On Debian 12: libsdl2-dev, libglew-dev, libcglm-dev, libstb-dev
2. Build
	- In a terminal, just enter: `make`
3. Run
	- `./game.bin`
