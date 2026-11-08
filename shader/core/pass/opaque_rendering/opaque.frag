#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_CAMERA
#define RENDER_USE_LIGHT
#define RENDER_USE_SKYBOX
#define RENDER_USE_MATERIAL
#define RENDER_LIT_FRAGMENT_INPUT
#define RENDER_COLOR_OUTPUT
#define RENDER_NORMAL_OUTPUT
#include "layout.glsl"
#include "light/pbr.glsl"

void main(){
    bool clipped;
    vec4 color = shade_pbr(frag_position,frag_normal,frag_uv,clipped);
    if(clipped) discard;
    out_color = vec4(color.rgb,1.0);
    out_normal = vec4(normalize(frag_normal),1.0);
}
