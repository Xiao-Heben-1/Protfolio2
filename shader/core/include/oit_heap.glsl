#ifndef RENDER_OIT_HEAP_GLSL
#define RENDER_OIT_HEAP_GLSL

uint64_t heap_key(ivec2 pixel,uint slot){
    uint64_t key = imageLoad(oit_heap,ivec3(pixel,int(slot))).x;
    // 未使用的槽以无限远深度参与最大堆；低位对应独立的颜色槽。
    return key == uint64_t(0) ? (uint64_t(0xffffffffu) << 32) | uint64_t(slot) : key;
}
void heap_store(ivec2 pixel,uint slot,uint64_t key){
    imageStore(oit_heap,ivec3(pixel,int(slot)),u64vec4(key));
}

// 在 pixel interlock 内执行。每个像素独占 16 个颜色槽，不使用全局分配计数。
// 保留最近的 16 个片元，堆外片元或被替换的片元立即交给固定功能混合。
vec4 retain_nearest(ivec2 pixel,vec4 color,float depth){
    uint base = (uint(pixel.y) * uint(imageSize(oit_heap).x) + uint(pixel.x)) * OIT_HEAP_SIZE;
    uint depth_bits = floatBitsToUint(depth) + 1u;//避免深度 0、槽 0 与空槽标记重合
    uint64_t root = heap_key(pixel,0u);
    if(depth_bits >= uint(root >> 32)) return color;

    uint index = uint(root);
    vec4 displaced = vec4(0.0);
    if(uint(root >> 32) != 0xffffffffu) displaced = oit_fragments.fragments[base + index].color;
    oit_fragments.fragments[base + index].color = color;
    uint64_t key = (uint64_t(depth_bits) << 32) | uint64_t(index);
    uint slot = 0u;
    while(slot * 2u + 1u < OIT_HEAP_SIZE){
        uint child = slot * 2u + 1u;
        if(child + 1u < OIT_HEAP_SIZE && heap_key(pixel,child + 1u) > heap_key(pixel,child)) ++child;
        uint64_t child_key = heap_key(pixel,child);
        if(child_key <= key) break;
        heap_store(pixel,slot,child_key);
        slot = child;
    }
    heap_store(pixel,slot,key);
    return displaced;
}

// 采集结束后调用，原地把有效 key 按深度从近到远排列，空槽置零。
// 排序后该像素不再是最大堆，下一次采集前必须清空图像。
void sort_oit(ivec2 pixel){
    uint64_t keys[OIT_HEAP_SIZE];
    uint size = 0u;
    for(uint i = 0u; i < OIT_HEAP_SIZE; ++i){
        uint64_t key = imageLoad(oit_heap,ivec3(pixel,int(i))).x;
        if(key == uint64_t(0) || uint(key >> 32) == 0xffffffffu) continue;
        uint j = size++;
        while(j > 0u && keys[j - 1u] > key){keys[j] = keys[j - 1u]; --j;}
        keys[j] = key;
    }
    for(uint i = 0u; i < OIT_HEAP_SIZE; ++i)
        heap_store(pixel,i,i < size ? keys[i] : uint64_t(0));
}

// 只读取已排序的 key 并合成颜色，不修改堆或颜色 SSBO。
vec4 compose_oit(ivec2 pixel){
    uint base = (uint(pixel.y) * uint(imageSize(oit_heap).x) + uint(pixel.x)) * OIT_HEAP_SIZE;
    vec4 result = vec4(0.0);
    for(uint i = 0u; i < OIT_HEAP_SIZE; ++i){
        uint64_t key = imageLoad(oit_heap,ivec3(pixel,int(i))).x;
        if(key == uint64_t(0)) break;
        vec4 color = oit_fragments.fragments[base + uint(key)].color;
        float contribution = (1.0 - result.a) * color.a;
        result.rgb += color.rgb * contribution;
        result.a += contribution;
        if(result.a == 1.0) break;
    }
    return result;
}
#endif
