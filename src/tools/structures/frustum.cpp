#include <tools/structures/frustum.hpp>
namespace render {
frustum::frustum(const glm::mat4& vp){
    const auto rows=glm::transpose(vp);
    planes={rows[3]+rows[0],rows[3]-rows[0],rows[3]+rows[1],rows[3]-rows[1],rows[2],rows[3]-rows[2]};
    const auto inverse=glm::inverse(vp);
    for(int z=0;z<2;++z) for(int y=-1;y<=1;y+=2) for(int x=-1;x<=1;x+=2){
        const auto p=inverse*glm::vec4(x,y,z,1);
        bounds=merge(bounds,AABB(glm::vec3(p)/p.w));
    }
}
bool frustum::intersects(const AABB& box) const noexcept {
    for(const auto& p:planes){
        const auto v=glm::mix(box.get_min(),box.get_max(),glm::greaterThanEqual(glm::vec3(p),glm::vec3(0)));
        if(glm::dot(glm::vec3(p),v)+p.w<0) return false;
    }
    return true;
}
}
