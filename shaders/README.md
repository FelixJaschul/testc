# Shader Editing Guide

## Shader responsibilities

- `voxel_raymarch.vert/.frag`: raymarch + shading from uniforms
- `basic_triangle.vert/.frag`: draw/shade triangle from uniforms

## Uniform flow

- `main.c` fills its local voxel uniform struct and sends it to voxel fragment shader
- `tri.c` fills vertex + fragment uniform structs and sends them each frame
- Wrapper only submits uniforms and draw calls
