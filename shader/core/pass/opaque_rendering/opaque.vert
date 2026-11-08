#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_CAMERA
#define RENDER_USE_TRANSFORM
#define RENDER_MESH_VERTEX_INPUT
#define RENDER_LIT_VERTEX_OUTPUT
#include "layout.glsl"

void main(){
    vec4 world_position = transform.model * vec4(in_position,1.0);

    frag_position = world_position.xyz;
    frag_uv = in_uv;

    mat3 normal_matrix = transpose(inverse(mat3(transform.model)));
    frag_normal = normalize(normal_matrix * in_normal);

    gl_Position = camera.vp * world_position;
}
