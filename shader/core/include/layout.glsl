#ifndef RENDER_LAYOUT_GLSL
#define RENDER_LAYOUT_GLSL

// ============================================================
// Descriptor Set
// ============================================================

// set 0：camera
// set 1：light
// set 2：material
// set 3：G-buffer
// set 4：OIT
// set 5：skybox（大气/地面 UBO）；太阳仍属于 set 1。

#define RENDER_CAMERA_SET 0
#define RENDER_LIGHT_SET 1
#define RENDER_MATERIAL_SET 2
#define RENDER_G_BUFFER_SET 3
#define RENDER_SKYBOX_SET 5
#define RENDER_SKYBOX_BINDING 0
#if defined(RENDER_OIT_RESOLVE) || defined(RENDER_OIT_SORT)
#define RENDER_OIT_SET 0
#else
#define RENDER_OIT_SET 4
#endif


// ============================================================
// Camera Binding
// ============================================================

#define RENDER_CAMERA_BINDING 0


// ============================================================
// Light Binding
// ============================================================

#define RENDER_MAIN_LIGHT_BINDING 0
#define RENDER_NORMAL_LIGHT_BINDING 1
#define RENDER_LIGHT_COUNT_BINDING 2


// ============================================================
// Material Binding
// ============================================================

#define RENDER_MATERIAL_BINDING 0
#define RENDER_BASE_COLOR_MAP_BINDING 1
#define RENDER_NORMAL_MAP_BINDING 2
#define RENDER_METALLIC_MAP_BINDING 3
#define RENDER_ROUGHNESS_MAP_BINDING 4
#define RENDER_AO_MAP_BINDING 5
#define RENDER_EMISSIVE_MAP_BINDING 6


// ============================================================
// G-Buffer Binding
// ============================================================

#define RENDER_G_BUFFER_COLOR_BINDING 0
#define RENDER_G_BUFFER_NORMAL_BINDING 1
#define RENDER_G_BUFFER_DEPTH_BINDING 2


// ============================================================
// OIT Binding
// ============================================================

// 透明片元数据SSBO
#define RENDER_OIT_FRAGMENT_BINDING 0

// 每像素固定容量大根堆
#define RENDER_OIT_HEAP_BINDING 2


// ============================================================
// Camera
// ============================================================

#ifdef RENDER_USE_CAMERA

layout(std140,set=RENDER_CAMERA_SET,binding=RENDER_CAMERA_BINDING) uniform camera_uniform{
    mat4 view;
    mat4 projection;
    mat4 vp;
} camera;

#endif

// ============================================================
// Light
// ============================================================

#ifdef RENDER_USE_LIGHT

layout(std140,set=RENDER_LIGHT_SET,binding=RENDER_MAIN_LIGHT_BINDING) uniform main_light_uniform{
    vec4 direction; // xyz 指向太阳；w 角半径 rad。
    vec4 solar_irradiance; // RGB 大气层外法向辐照度；w 圆盘亮度倍率。
} main_light;

struct normal_light_uniform{
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 parameter;//x emission，y k1，z k2
};

layout(std430,set=RENDER_LIGHT_SET,binding=RENDER_NORMAL_LIGHT_BINDING) readonly buffer normal_light_buffer{
    normal_light_uniform lights[];
} normal_lights;

layout(std140,set=RENDER_LIGHT_SET,binding=RENDER_LIGHT_COUNT_BINDING) uniform light_count_uniform{
    uint light_count;
} light_info;

#endif


#ifdef RENDER_USE_SKYBOX
// 与 skybox_data.cpp 的 atmosphere_uniform 逐 vec4 对应，std140 = 128 bytes。
layout(std140,set=RENDER_SKYBOX_SET,binding=RENDER_SKYBOX_BINDING) uniform skybox_uniform{
    vec4 planet; // x地球半径 y大气高度 z瑞利尺度高度 w米氏尺度高度，m。
    vec4 rayleigh; // RGB散射系数，w整体密度。
    vec4 mie; // RGB散射系数，w相函数g。
    vec4 extinction; // RGB米氏消光系数，w全场景曝光倍率。
    vec4 ozone; // RGB臭氧吸收系数，w层中心高度。
    vec4 ground_albedo; // RGB平均地表反照率，w地面贡献倍率。
    vec4 ground_color; // RGB可见地面漫反射底色，w臭氧半宽。
    ivec4 samples; // x视线采样数，y太阳光线采样数，zw保留。
} skybox;
#endif

// ============================================================
// Material
// ============================================================

#ifdef RENDER_USE_MATERIAL

layout(std140,set=RENDER_MATERIAL_SET,binding=RENDER_MATERIAL_BINDING) uniform material_uniform{
    vec4 base_color;//RGBA
    vec4 emissive_color;
    vec4 parameter;//x metallic，y roughness，z ao，w emissive_strength
    uvec4 texture_flags;//x base_color，y normal，z metallic，w roughness
    uvec4 texture_flags_2;//x ao，y emissive，z alpha模式，w cutoff浮点位
    uvec4 texture_channels;
    uvec4 alpha_options; // x：基础贴图 Alpha 参与不透明度；0 时仅采样 RGB。
} material;

layout(set=RENDER_MATERIAL_SET,binding=RENDER_BASE_COLOR_MAP_BINDING) uniform sampler2D base_color_map;
layout(set=RENDER_MATERIAL_SET,binding=7) uniform sampler2D opacity_map;
layout(set=RENDER_MATERIAL_SET,binding=RENDER_NORMAL_MAP_BINDING) uniform sampler2D normal_map;
layout(set=RENDER_MATERIAL_SET,binding=RENDER_METALLIC_MAP_BINDING) uniform sampler2D metallic_map;
layout(set=RENDER_MATERIAL_SET,binding=RENDER_ROUGHNESS_MAP_BINDING) uniform sampler2D roughness_map;
layout(set=RENDER_MATERIAL_SET,binding=RENDER_AO_MAP_BINDING) uniform sampler2D ao_map;
layout(set=RENDER_MATERIAL_SET,binding=RENDER_EMISSIVE_MAP_BINDING) uniform sampler2D emissive_map;

#endif


// ============================================================
// G-Buffer
// ============================================================

#ifdef RENDER_USE_G_BUFFER

layout(set=RENDER_G_BUFFER_SET,binding=RENDER_G_BUFFER_COLOR_BINDING) uniform sampler2D g_buffer_color;
layout(set=RENDER_G_BUFFER_SET,binding=RENDER_G_BUFFER_NORMAL_BINDING) uniform sampler2D g_buffer_normal;
layout(set=RENDER_G_BUFFER_SET,binding=RENDER_G_BUFFER_DEPTH_BINDING) uniform sampler2D g_buffer_depth;

#endif


// ============================================================
// Transform
// ============================================================

#ifdef RENDER_USE_TRANSFORM

layout(push_constant) uniform transform_uniform{
    mat4 model;
} transform;

#endif


// ============================================================
// Mesh Vertex Input
// ============================================================

#ifdef RENDER_MESH_VERTEX_INPUT

layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;

#endif


// ============================================================
// Lit Stage Varying
// ============================================================

#if defined(RENDER_LIT_VERTEX_OUTPUT) || defined(RENDER_LIT_FRAGMENT_INPUT)

#ifdef RENDER_LIT_VERTEX_OUTPUT
#define RENDER_VARYING out
#else
#define RENDER_VARYING in
#endif

layout(location=0) RENDER_VARYING vec3 frag_position;
layout(location=1) RENDER_VARYING vec2 frag_uv;
layout(location=2) RENDER_VARYING vec3 frag_normal;

#undef RENDER_VARYING

#endif


// ============================================================
// Geometry Stage Varying
// ============================================================

#if defined(RENDER_GEOMETRY_VERTEX_OUTPUT) || defined(RENDER_GEOMETRY_FRAGMENT_INPUT)

#ifdef RENDER_GEOMETRY_VERTEX_OUTPUT
#define RENDER_VARYING out
#else
#define RENDER_VARYING in
#endif

layout(location=0) RENDER_VARYING vec2 frag_uv;
layout(location=1) RENDER_VARYING vec3 frag_normal;

#undef RENDER_VARYING

#endif


// ============================================================
// Fragment Output
// ============================================================

#ifdef RENDER_COLOR_OUTPUT

layout(location=0) out vec4 out_color;

#endif

#ifdef RENDER_NORMAL_OUTPUT

layout(location=1) out vec4 out_normal;

#endif


// ============================================================
// OIT
// ============================================================

#if defined(RENDER_OIT_CAPTURE) || defined(RENDER_OIT_SORT) || defined(RENDER_OIT_RESOLVE)

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_image_int64 : require

#ifdef RENDER_OIT_CAPTURE

#extension GL_ARB_fragment_shader_interlock : require
layout(pixel_interlock_ordered) in;

// 透明物体只进行深度测试，不写入深度。
// 只有通过不透明深度测试的透明fragment才进入OIT。
layout(early_fragment_tests) in;

#endif


// ------------------------------------------------------------
// Transparent Fragment
// ------------------------------------------------------------

// 真正需要保存的透明fragment数据。
// depth和像素内颜色槽编号打包进64-bit heap，不需要在这里重复保存。
struct oit_fragment{
    vec4 color;//非预乘颜色，线性空间
};


// ------------------------------------------------------------
// Fragment Buffer
// ------------------------------------------------------------

// ((pixel_y * width + pixel_x) * OIT_HEAP_SIZE + slot) -> 透明fragment最终 RGBA
layout(std430,set=RENDER_OIT_SET,binding=RENDER_OIT_FRAGMENT_BINDING) coherent buffer oit_fragment_buffer{
    oit_fragment fragments[];
} oit_fragments;


// ------------------------------------------------------------
// Fixed OIT Heap
// ------------------------------------------------------------

const uint OIT_HEAP_SIZE = 16u;
// 固定 16 层；0 表示空槽，没有额外计数层，分辨率通过 imageSize 查询。
layout(r64ui,set=RENDER_OIT_SET,binding=RENDER_OIT_HEAP_BINDING) uniform coherent u64image2DArray oit_heap;

#endif


#endif // RENDER_LAYOUT_GLSL
