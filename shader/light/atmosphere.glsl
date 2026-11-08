#ifndef RENDER_ATMOSPHERE_GLSL
#define RENDER_ATMOSPHERE_GLSL
const float ATM_PI=3.141592653589793;
// 世界 y=0 是海平面，单位 m；球心为 (0,-earth_radius,0)。
vec3 atmosphere_position(vec3 world){return vec3(world.x,skybox.planet.x+max(world.y,1.0),world.z);}
vec2 sphere_interval(vec3 p,vec3 d,float radius){
    float b=dot(p,d),r=length(p);
    float discriminant=b*b-(r-radius)*(r+radius);
    if(discriminant<0.0)return vec2(1.0,-1.0);
    float root=sqrt(discriminant);return vec2(-b-root,-b+root);
}
vec3 atmosphere_density(vec3 p){
    float height=max(length(p)-skybox.planet.x,0.0);
    return vec3(exp(-height/skybox.planet.z),exp(-height/skybox.planet.w),
        max(1.0-abs(height-skybox.ozone.w)/skybox.ground_color.w,0.0))*skybox.rayleigh.w;
}
vec3 optical_extinction(vec3 depth){
    return skybox.rayleigh.rgb*depth.x+skybox.extinction.rgb*depth.y+skybox.ozone.rgb*depth.z;
}
vec3 sun_transmittance(vec3 p,vec3 sun){
    vec2 earth=sphere_interval(p,sun,skybox.planet.x);
    if(earth.x>0.0 && earth.y>0.0)return vec3(0); // 地球遮挡太阳，夜侧无直射光。
    vec2 atmosphere=sphere_interval(p,sun,skybox.planet.x+skybox.planet.y);
    float start=max(atmosphere.x,0.0),end=max(atmosphere.y,0.0);
    if(end<=start)return vec3(1);
    float step_length=(end-start)/float(skybox.samples.y);vec3 depth=vec3(0);
    for(int i=0;i<skybox.samples.y;++i)depth+=atmosphere_density(p+sun*(start+(float(i)+.5)*step_length))*step_length;
    return exp(-optical_extinction(depth));
}
vec3 atmosphere_ambient(vec3 normal){
    float daylight=smoothstep(-.08,.2,main_light.direction.y);
    vec3 scatter=1.0-exp(-(skybox.rayleigh.rgb*skybox.planet.z+skybox.mie.rgb*skybox.planet.w)*skybox.rayleigh.w);
    vec3 hemisphere=mix(skybox.ground_albedo.rgb*skybox.ground_albedo.w,vec3(1),normal.y*.5+.5);
    return main_light.solar_irradiance.rgb*scatter*hemisphere*(.12*daylight);
}
// 单次散射：视线光学深度 × 太阳光学深度。包含 Rayleigh、Mie 与臭氧吸收。
vec3 integrate_sky(vec3 world,vec3 ray){
    vec3 origin=atmosphere_position(world),sun=main_light.direction.xyz;
    float mu=dot(ray,sun),disk_width=max(fwidth(mu),1e-7);
    vec2 atmosphere=sphere_interval(origin,ray,skybox.planet.x+skybox.planet.y);
    float start=max(atmosphere.x,0.0),end=atmosphere.y;
    vec2 earth=sphere_interval(origin,ray,skybox.planet.x);
    bool ground=earth.x>0.0 && earth.x<end;
    if(ground)end=earth.x;
    vec3 depth=vec3(0),sum_r=vec3(0),sum_m=vec3(0);
    if(end>start){
        float step_length=(end-start)/float(skybox.samples.x);
        for(int i=0;i<skybox.samples.x;++i){
            vec3 p=origin+ray*(start+(float(i)+.5)*step_length);
            vec3 local=atmosphere_density(p)*step_length;
            vec3 transmission=exp(-optical_extinction(depth+.5*local))*sun_transmittance(p,sun);
            sum_r+=transmission*local.x;sum_m+=transmission*local.y;depth+=local;
        }
    }
    float g=skybox.mie.w;
    float phase_r=3.0*(1.0+mu*mu)/(16.0*ATM_PI);
    float phase_m=3.0*(1.0-g*g)*(1.0+mu*mu)/(8.0*ATM_PI*(2.0+g*g)*pow(1.0+g*g-2.0*g*mu,1.5));
    vec3 color=(sum_r*skybox.rayleigh.rgb*phase_r+sum_m*skybox.mie.rgb*phase_m)*main_light.solar_irradiance.rgb;
    vec3 view_transmission=exp(-optical_extinction(depth));
    if(ground){
        vec3 hit=origin+ray*end,n=normalize(hit);
        vec3 direct=main_light.solar_irradiance.rgb*sun_transmittance(hit+n*2.0,sun)*max(dot(n,sun),0.0)/ATM_PI;
        color+=view_transmission*skybox.ground_color.rgb*(direct+atmosphere_ambient(n))*skybox.ground_albedo.w;
    }else{
        float radius=main_light.direction.w,edge=cos(radius);
        float disk=smoothstep(edge-disk_width,edge+disk_width,mu);
        // E=π L sin²(radius)，由法向辐照度反推均匀圆盘辐亮度。
        color+=view_transmission*main_light.solar_irradiance.rgb*main_light.solar_irradiance.w*disk/(ATM_PI*sin(radius)*sin(radius));
    }
    return color;
}
#endif
