#ifndef RENDER_BLINN_PHONG_GLSL
#define RENDER_BLINN_PHONG_GLSL
//========================================
// Blinn-Phong
//========================================

const float ambient_strength = 0.05;
const float specular_strength = 0.5;


vec3 calculate_main_light(vec3 normal,vec3 view_dir,vec3 base_color,vec3 specular_tint,float shininess){
    vec3 light_dir = normalize(main_light.direction.xyz);

    float emission = 1.0;

    float diffuse_factor = max(dot(normal,light_dir),0.0);

    vec3 half_dir = normalize(light_dir + view_dir);

    float specular_factor = 0.0;

    if(diffuse_factor > 0.0){
        specular_factor = pow(max(dot(normal,half_dir),0.0),shininess);
    }

    vec3 light_color = main_light.solar_irradiance.rgb * emission;

    vec3 diffuse = base_color * diffuse_factor * light_color;
    vec3 specular = specular_strength * specular_factor * light_color * specular_tint;

    return diffuse + specular;
}


vec3 calculate_normal_light(normal_light_uniform light,vec3 position,vec3 normal,vec3 view_dir,vec3 base_color,vec3 specular_tint,float shininess){
    bool directional=light.position.w==0.0;
    vec3 to_light = directional ? -light.direction.xyz : light.position.xyz - position;

    float distance = length(to_light);
    vec3 light_dir = normalize(to_light);

    float emission = light.parameter.x;
    float k1 = light.parameter.y;
    float k2 = light.parameter.z;

    float attenuation = directional ? 1.0 : 1.0 / (1.0 + k1 * distance + k2 * distance * distance);

    float diffuse_factor = max(dot(normal,light_dir),0.0);

    vec3 half_dir = normalize(light_dir + view_dir);

    float specular_factor = 0.0;

    if(diffuse_factor > 0.0){
        specular_factor = pow(max(dot(normal,half_dir),0.0),shininess);
    }

    vec3 light_color = light.color.rgb * emission * attenuation;

    vec3 diffuse = base_color * diffuse_factor * light_color;
    vec3 specular = specular_strength * specular_factor * light_color * specular_tint;

    return diffuse + specular;
}


// 调用方先包含 layout.glsl 并启用 camera/light/material 定义。
// 返回非预乘 RGBA；是否 discard、写附件或存入 OIT 由入口 shader 决定。
vec4 shade_blinn_phong(vec3 position,vec3 normal,vec2 uv,out bool clipped){
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

    vec3 camera_position = inverse(camera.view)[3].xyz;
    vec3 view_dir = normalize(camera_position - position);


    //PBR roughness 暂时映射成 Blinn-Phong shininess
    float shininess = mix(256.0,4.0,clamp(roughness,0.0,1.0));

    vec3 color = base_color * ambient_strength * ao;
    vec3 specular_tint=mix(vec3(1.0),base_color,clamp(metallic,0.0,1.0));
    base_color*=1.0-clamp(metallic,0.0,1.0);
    color += calculate_main_light(surface_normal,view_dir,base_color,specular_tint,shininess);
    for(uint i = 0;i < light_info.light_count;i++){
        color += calculate_normal_light(normal_lights.lights[i],position,surface_normal,view_dir,base_color,specular_tint,shininess);
    }

    color += emissive;

    return vec4(color,clamp(alpha,0.0,1.0));
}

#endif
