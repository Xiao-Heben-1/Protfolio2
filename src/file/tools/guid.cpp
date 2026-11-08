#include <file/tools/guid.hpp>
#include <tools/math/random.hpp>
namespace render {
std::string generate_guid(){
    static thread_local UUIDGenerator generator;
    auto value=generator.generate_uuuid();
    const char* hex="0123456789abcdef";
    std::string guid(32,'0');
    for(int i=31;i>=0;--i){guid[i]=hex[unsigned(value&15)];value>>=4;}
    return guid;
}
}
