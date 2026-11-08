#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_OIT_RESOLVE
#define RENDER_COLOR_OUTPUT
#include "layout.glsl"
#include "oit_heap.glsl"

void main(){
    // 输入由 sort.comp 排好序；这里仅合成预乘 RGBA，不再排序。
    out_color = compose_oit(ivec2(gl_FragCoord.xy));
}
