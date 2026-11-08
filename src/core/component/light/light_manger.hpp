#pragma once
#include <vector>
namespace render {
class normal_light;
class main_light;
class light_manger final {
    friend class main_light;
    friend class normal_light;
    std::vector<normal_light*> lights;
    std::vector<main_light*> main_lights;
    main_light* main=nullptr;
public:
    light_manger()=default;
    light_manger(const light_manger&)=delete;
    light_manger& operator=(const light_manger&)=delete;
};
}
