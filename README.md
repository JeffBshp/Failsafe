# Failsafe

A game in plain C with SDL and OpenGL.

At this point it's just a sandbox for me to learn 3D graphics. I am working on voxel rendering and collisions. Name is tentative.

### Building on Debian Linux

1. Install packages
   - build-essential
   - libsdl2-dev
   - libglew-dev
   - libcglm-dev
   - libstb-dev
2. Build
   - In a terminal, just enter: `make`
3. Run
   - `./game.bin`

A project file for KDevelop is included as well.

It should be easy enough to build on other platforms. The project just needs access to SDL2, GLEW, CGLM, and stb_image.h.
