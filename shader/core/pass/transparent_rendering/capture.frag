#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_USE_CAMERA
#define RENDER_USE_LIGHT
#define RENDER_USE_SKYBOX
#define RENDER_USE_MATERIAL
#define RENDER_LIT_FRAGMENT_INPUT
#define RENDER_OIT_CAPTURE
#define RENDER_COLOR_OUTPUT
#include "layout.glsl"
#include "light/pbr.glsl"
#include "oit_heap.glsl"

void main(){
    // 颜色计算与堆维护各自调用独立接口。
    bool clipped;
    vec4 color = shade_pbr(frag_position,frag_normal,frag_uv,clipped);
    out_color = vec4(0.0);
    // 所有 invocation 都经过同一对互锁调用，不在临界区内 discard/return。
    beginInvocationInterlockARB();
    if(!clipped && color.a > 0.0)
        out_color = retain_nearest(ivec2(gl_FragCoord.xy),color,gl_FragCoord.z);
    memoryBarrierImage();
    memoryBarrierBuffer();
    endInvocationInterlockARB();
    // 返回的堆外颜色由固定功能混合；此阶段不做最终深度排序或 16 层合成。
}
