#include <core/object/object_loader.hpp>
#include <core/component/material/material.hpp>
#include <engine.hpp>
#include <file/tools/file_path.hpp>
#include <file/tools/library_cache.hpp>
#include <algorithm>
#include <assimp/GltfMaterial.h>
#include <cstring>

namespace render {
void object_loader::load_textures(const aiScene* scene,const aiMaterial* source,material* mat,const std::filesystem::path& model){
    auto& fs=egn.get_file_system();auto& scan=fs.get_scan();auto& sub=*mat->sub_materials.back();
    const auto locate=[&](std::initializer_list<aiTextureType> types){
        aiString name;bool found=false;
        for(auto type:types)if(source->GetTexture(type,0,&name)==AI_SUCCESS){found=true;break;}
        if(!found)return std::string();
        if(const auto* texture=scene->GetEmbeddedTexture(name.C_Str())){
            std::vector<uint8_t> bytes;
            std::string extension=texture->achFormatHint;
            if(texture->mHeight==0)bytes.assign(reinterpret_cast<const uint8_t*>(texture->pcData),reinterpret_cast<const uint8_t*>(texture->pcData)+texture->mWidth);
            else{
                extension="tga";bytes.resize(18+size_t(texture->mWidth)*texture->mHeight*4);bytes[2]=2;
                bytes[12]=texture->mWidth&255;bytes[13]=texture->mWidth>>8;bytes[14]=texture->mHeight&255;bytes[15]=texture->mHeight>>8;bytes[16]=32;bytes[17]=0x28;
                std::memcpy(bytes.data()+18,texture->pcData,bytes.size()-18);
            }
            if(extension.empty())extension="png";
            const auto key=library_cache::hash(path_to_utf8(model)+name.C_Str());
            return scan.create_file(fs.get_config().library_root/"generated/embedded_textures"/(key+"."+extension),bytes);
        }
        auto value=std::string(name.C_Str());std::replace(value.begin(),value.end(),'\\','/');
        auto path=(model.parent_path()/path_from_utf8(value)).lexically_normal();
        const auto source_relative=path.lexically_relative(fs.get_config().source_root);
        if(source_relative.empty() || *source_relative.begin()==L".." || !std::filesystem::exists(path)){
            const auto filename=path_from_utf8(value).filename();path=model.parent_path()/filename;
            if(!std::filesystem::exists(path))for(const auto& entry:std::filesystem::recursive_directory_iterator(model.parent_path()))
                if(entry.is_regular_file() && entry.path().filename()==filename){path=entry.path();break;}
        }
        if(!std::filesystem::exists(path))return std::string();
        const auto relative=path.lexically_relative(fs.get_config().source_root);scan.add_file(relative);return scan.load_guid(relative);
    };
    sub.base_color_map.image_uid=locate({aiTextureType_BASE_COLOR,aiTextureType_DIFFUSE});
    sub.normal_map.image_uid=locate({aiTextureType_NORMALS,aiTextureType_NORMAL_CAMERA});
    sub.metallic_map.image_uid=locate({aiTextureType_METALNESS});
    sub.roughness_map.image_uid=locate({aiTextureType_DIFFUSE_ROUGHNESS});
    sub.ao_map.image_uid=locate({aiTextureType_AMBIENT_OCCLUSION,aiTextureType_LIGHTMAP});
    sub.emissive_map.image_uid=locate({aiTextureType_EMISSION_COLOR,aiTextureType_EMISSIVE});
    sub.opacity_map.image_uid=locate({aiTextureType_OPACITY});
    auto& u=sub.uniform_data;
    u.texture_flags={!sub.base_color_map.image_uid.empty(),!sub.normal_map.image_uid.empty(),!sub.metallic_map.image_uid.empty(),!sub.roughness_map.image_uid.empty()};
    u.texture_flags_2.x=!sub.ao_map.image_uid.empty();u.texture_flags_2.y=!sub.emissive_map.image_uid.empty();u.texture_channels.z=!sub.opacity_map.image_uid.empty();
    if((model.extension()==".gltf" || model.extension()==".glb") && !sub.metallic_map.image_uid.empty())u.texture_channels.x=2;
    if((model.extension()==".gltf" || model.extension()==".glb") && !sub.roughness_map.image_uid.empty())u.texture_channels.y=1;
    if(u.texture_channels.z && u.texture_flags_2.z==0)u.texture_flags_2.z=3;
    // 图片含 Alpha 不代表透明：游戏模型经常在这里打包材质遮罩。
    // glTF 按声明的 alphaMode；其他格式只接受明确的 UseAlpha 标记。
    aiString alpha_mode;u.alpha_options.x=source->Get(AI_MATKEY_GLTF_ALPHAMODE,alpha_mode)==AI_SUCCESS;
    for(auto type:{aiTextureType_BASE_COLOR,aiTextureType_DIFFUSE})if(source->GetTextureCount(type)){
        unsigned flags=0;source->Get(AI_MATKEY_TEXFLAGS(type,0),flags);
        if(flags & aiTextureFlags_UseAlpha)u.alpha_options.x=1;
        if(flags & aiTextureFlags_IgnoreAlpha)u.alpha_options.x=0;
        break;
    }
    if(u.alpha_options.x && u.texture_flags_2.z==0)u.texture_flags_2.z=3;
}
}
