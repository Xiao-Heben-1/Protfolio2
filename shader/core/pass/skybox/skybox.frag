#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_CAMERA
#define RENDER_USE_LIGHT
#define RENDER_USE_SKYBOX
#define RENDER_COLOR_OUTPUT
#define RENDER_NORMAL_OUTPUT
#include "layout.glsl"
#include "light/atmosphere.glsl"
layout(location=0) in vec2 screen_position;
void main(){
    vec4 view=inverse(camera.projection)*vec4(screen_position,1,1);
    vec3 ray=normalize(mat3(inverse(camera.view))*(view.xyz/view.w));
    out_color=vec4(integrate_sky(inverse(camera.view)[3].xyz,ray),1);
    out_normal=vec4(0);
}
