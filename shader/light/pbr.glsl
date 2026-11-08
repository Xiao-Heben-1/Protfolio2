#ifndef RENDER_PBR_GLSL
#define RENDER_PBR_GLSL
#include "light/atmosphere.glsl"
// 由旧 shader.frag 的 Cook-Torrance / GGX / Schlick / Oren-Nayar 整理。
// roughness 为感知粗糙度，微表面 alpha=roughness²；返回线性 HDR，透明度单独保留。
vec3 fresnel_schlick(float vh,vec3 f0){return f0+(1.0-f0)*pow(1.0-vh,5.0);}
float distribution_ggx(float nh,float roughness){
    float a=roughness*roughness,a2=a*a;
    float d=nh*nh*(a2-1.0)+1.0;
    return a2/(ATM_PI*d*d);
}
float geometry_schlick(float nd,float roughness){
    float k=(roughness+1.0)*(roughness+1.0)/8.0;
    return nd/(nd*(1.0-k)+k);
}
float oren_nayar(vec3 n,vec3 l,vec3 v,float nl,float nv,float roughness){
    float sigma2=roughness*roughness;
    float a=1.0-sigma2/(2.0*(sigma2+.33)),b=.45*sigma2/(sigma2+.09);
    // 不归一化零长切向量；该写法在正视/正入射时自然归零。
    float azimuth=max(dot(l-n*nl,v-n*nv),0.0);
    return (a+b*azimuth/max(max(nl,nv),1e-5))/ATM_PI;
}
vec3 evaluate_pbr(vec3 n,vec3 v,vec3 l,vec3 albedo,float metallic,float roughness,vec3 irradiance){
    float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.0);
    if(nl<=0.0 || nv<=0.0)return vec3(0);
    vec3 h=normalize(l+v);
    vec3 f0=mix(vec3(.04),albedo,metallic); // 当前材质没有 IOR；电介质使用 IOR=1.5。
    vec3 f=fresnel_schlick(max(dot(v,h),0.0),f0);
    vec3 specular=distribution_ggx(max(dot(n,h),0.0),roughness)*geometry_schlick(nl,roughness)*geometry_schlick(nv,roughness)*f/(4.0*nl*nv);
    vec3 diffuse=(1.0-f)*(1.0-metallic)*albedo*oren_nayar(n,l,v,nl,nv,roughness);
    return (diffuse+specular)*irradiance*nl; // N·L 仅乘一次。
}
vec4 shade_pbr(vec3 position,vec3 normal,vec2 uv,out bool clipped){
    clipped = false;
    vec3 base_color = material.base_color.rgb;
    float alpha = material.base_color.a;

    if(material.texture_flags.x != 0){
        vec4 sample_color = texture(base_color_map,uv);

        base_color *= sample_color.rgb;
        if(material.alpha_options.x != 0u)alpha *= sample_color.a;
    }


    if(material.texture_channels.z != 0u)alpha *= texture(opacity_map,uv)[material.texture_channels.w];
    if(material.texture_flags_2.z == 1u){
        alpha = 1.0;
    }else if(material.texture_flags_2.z == 2u){
        if(alpha < uintBitsToFloat(material.texture_flags_2.w)){
            clipped = true;
            alpha = 0.0;
        }else alpha = 1.0;
    }

    float roughness = material.parameter.y;

    if(material.texture_flags.w != 0){
        roughness *= texture(roughness_map,uv)[material.texture_channels.y];
    }


    float ao = material.parameter.z;

    if(material.texture_flags_2.x != 0){
        ao *= texture(ao_map,uv).r;
    }


    vec3 emissive = material.emissive_color.rgb * material.parameter.w;

    if(material.texture_flags_2.y != 0){
        emissive *= texture(emissive_map,uv).rgb;
    }


    vec3 surface_normal = normalize(normal);
    if(material.texture_flags.y != 0u){
        vec3 dp1=dFdx(position),dp2=dFdy(position);vec2 duv1=dFdx(uv),duv2=dFdy(uv);
        vec3 tangent=dp1*duv2.y-dp2*duv1.y,bitangent=-dp1*duv2.x+dp2*duv1.x;
        float scale=inversesqrt(max(max(dot(tangent,tangent),dot(bitangent,bitangent)),1e-12));
        surface_normal=normalize(mat3(tangent*scale,bitangent*scale,surface_normal)*(texture(normal_map,uv).xyz*2.0-1.0));
    }
    float metallic=material.parameter.x;
    if(material.texture_flags.z != 0u)metallic*=texture(metallic_map,uv)[material.texture_channels.x];


    roughness=clamp(roughness,.045,1.0);metallic=clamp(metallic,0.0,1.0);
    vec3 view_dir=normalize(inverse(camera.view)[3].xyz-position);
    vec3 sun=main_light.direction.xyz;
    vec3 radiance=main_light.solar_irradiance.rgb*sun_transmittance(atmosphere_position(position),sun);
    vec3 color=evaluate_pbr(surface_normal,view_dir,sun,base_color,metallic,roughness,radiance);
    for(uint i=0u;i<light_info.light_count;++i){
        normal_light_uniform light=normal_lights.lights[i];
        bool directional=light.position.w==0.0;
        vec3 delta=directional ? -light.direction.xyz : light.position.xyz-position;
        float distance=length(delta);
        float attenuation=directional ? 1.0 : 1.0/(1.0+light.parameter.y*distance+light.parameter.z*distance*distance);
        color+=evaluate_pbr(surface_normal,view_dir,delta/max(distance,1e-5),base_color,metallic,roughness,light.color.rgb*light.parameter.x*attenuation);
    }
    // 大气半球漫反射近似；尚非卷积环境贴图/镜面 IBL，AO 只影响此间接项。
    color+=base_color*(1.0-metallic)*atmosphere_ambient(surface_normal)*ao;
    return vec4(color+emissive,clamp(alpha,0.0,1.0));
}
#endif
