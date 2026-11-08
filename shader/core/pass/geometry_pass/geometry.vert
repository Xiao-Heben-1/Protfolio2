#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_CAMERA
#define RENDER_MESH_VERTEX_INPUT
#define RENDER_GEOMETRY_VERTEX_OUTPUT
#include "layout.glsl"

void main(){
    gl_Position = camera.vp * vec4(in_position, 1.0);

    frag_uv = in_uv;
    frag_normal = normalize(in_normal);
}
