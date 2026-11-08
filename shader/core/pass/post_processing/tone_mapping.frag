#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_G_BUFFER
#define RENDER_USE_SKYBOX
#define RENDER_COLOR_OUTPUT
#include "layout.glsl"
#include "light/tone_mapping.glsl"
layout(location=0) in vec2 uv;
layout(push_constant) uniform display_options { layout(offset=64) uint enabled; } options;
void main(){
    vec3 color=texture(g_buffer_color,uv).rgb;
    color=options.enabled!=0u ? aces_film(color*skybox.extinction.w) : clamp(color,0.0,1.0);
    out_color=vec4(linear_to_srgb(color),1); // 显示图像 UNORM，显式编码一次。
}
