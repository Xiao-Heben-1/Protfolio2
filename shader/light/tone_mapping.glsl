#ifndef RENDER_TONE_MAPPING_GLSL
#define RENDER_TONE_MAPPING_GLSL
// 旧材质中的 ACES 拟合移到最终合成阶段，避免对透明层分别映射。
vec3 aces_film(vec3 linear_color){
    vec3 x=max(linear_color,vec3(0));
    return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);
}
vec3 linear_to_srgb(vec3 x){
    return mix(1.055*pow(x,vec3(1.0/2.4))-.055,12.92*x,lessThanEqual(x,vec3(.0031308)));
}
#endif
