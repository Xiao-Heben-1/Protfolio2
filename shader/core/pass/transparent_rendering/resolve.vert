#version 450
#extension GL_GOOGLE_include_directive : require
// 全屏三角形仅使用内建变量，无需启用额外布局。
#include "layout.glsl"
void main(){
    vec2 position = vec2((gl_VertexIndex << 1) & 2,gl_VertexIndex & 2);
    gl_Position = vec4(position * 2.0 - 1.0,0.0,1.0);
}
