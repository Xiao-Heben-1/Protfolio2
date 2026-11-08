#version 450
layout(location=0) out vec2 screen_position;
void main(){
    screen_position=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2)*2.0-1.0;
    gl_Position=vec4(screen_position,1.0,1.0); // 远平面，不写深度。
}
