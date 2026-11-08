#version 450
#extension GL_GOOGLE_include_directive : require
#define RENDER_GEOMETRY_FRAGMENT_INPUT
#define RENDER_COLOR_OUTPUT
#define RENDER_NORMAL_OUTPUT
#include "layout.glsl"

void main(){
    //暂时用 UV 显示颜色，方便测试模型和 UV
    out_color = vec4(frag_uv, 0.0, 1.0);

    //normal texture 是浮点格式，可以直接保存 [-1,1] 法线
    out_normal = vec4(normalize(frag_normal), 1.0);
}
