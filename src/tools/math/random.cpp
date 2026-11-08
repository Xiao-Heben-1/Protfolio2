#include <tools/math/random.hpp>
namespace render{
    thread_local PCG32 random_engine{0x12345678u,0x87654321u};
    thread_local PCG32 UUIDGenerator::uuid_engine = []{
        std::random_device source;
        const uint64_t state = (uint64_t(source()) << 32) | source();
        const uint64_t sequence = (uint64_t(source()) << 32) | source();
        return PCG32(state,sequence);
    }();
}
