#ifndef __RENDER_RANDOM_HPP
#define __RENDER_RANDOM_HPP

#include <random>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
namespace render {
class UUIDGenerator;
static constexpr uint64_t H_SEED = 2166136261u;
    //随机数生成器
    class PCG32 {
        friend class UUIDGenerator;
        uint64_t state;
        uint64_t inc;
        // 返回下一个 32-bit 无符号随机数
        uint32_t next_u32() {
            uint64_t oldstate = state;
            // linear congruential step: state = state * multiplier + inc
            state = oldstate * 6364136223846793005ULL + inc;
            // output function (xorshift right, rotate right)
            uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
            int32_t rot = static_cast<uint32_t>(oldstate >> 59u);
            return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
        }

        // 返回下一个 64-bit （用两次 next_u32 组合）
        uint64_t next_u64() {
            uint64_t hi = static_cast<uint64_t>(next_u32());
            uint64_t lo = static_cast<uint64_t>(next_u32());
            return (hi << 32) | lo;
        }

        // 返回一个 [0, 1) 的单精度浮点数
        float next_float() {
            // 将 32-bit 整数映射到 [0,1) —— 注意除以 2^32
            uint32_t v = next_u32();
            const float inv = 1.0f / 4294967296.0f; // 1/2^32
            return static_cast<float>(v) * inv;
        }

        inline uint64_t mix_hash(uint64_t h, uint64_t v) {
            h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }

        inline void hash64(uint64_t& x) {
            x ^= x >> 33;
            x *= 0xff51afd7ed558ccdULL;
            x ^= x >> 33;
            x *= 0xc4ceb9fe1a85ec53ULL;
            x ^= x >> 33;
        }

        inline void generate_seed_impl(uint64_t& h) {
            hash64(h);
            seed(h, h ^ 0x5DEECE66DULL);
        }

        template<typename T,typename... Ts>
        typename std::enable_if<std::is_integral<T>::value, void>::type
        generate_seed_impl(uint64_t& h,T v,Ts... vs) {
            h = mix_hash(h, static_cast<uint64_t>(v));
            generate_seed_impl(h, vs...);
        }
    public:
        using result_type = uint32_t;

        // 默认构造（可立即用 seed() 设定种子）
        PCG32(uint64_t initstate = 0x853c49e6748fea9bULL,
            uint64_t initseq = 0xda3e39cb94b95bdbULL)
        {
            seed(initstate, initseq);
        }

        // 设定种子（initseq 必须为奇数以保证周期覆盖）
        void seed(uint64_t initstate, uint64_t initseq) {
            state = 0u;
            inc = (initseq << 1u) | 1u;
            next_u32();
            state += initstate;
            next_u32();
        }

        template<typename... Ts>
        typename std::enable_if<std::conjunction<std::is_integral<Ts>...>::value, void>::type
        seed(Ts... vs) {
            uint64_t h = H_SEED;
            generate_seed_impl(h, vs...);
        }

        template<typename T>
        typename std::enable_if<std::is_integral<T>::value, T>::type
        uniform_distribution(T x,T y) {
            if (x >= y) {
                throw std::invalid_argument("uniform_distribution: invalid range [x,y)");
            }
            uint64_t range = static_cast<uint64_t>(y) - static_cast<uint64_t>(x);
            if (range <= UINT32_MAX) {
                uint32_t r = next_u32();
                uint64_t mult = static_cast<uint64_t>(r) * static_cast<uint32_t>(range);
                uint32_t off = static_cast<uint32_t>(mult >> 32);
                return static_cast<T>(x + off);
            }
            uint64_t r = next_u64();
            unsigned __int128 m = static_cast<unsigned __int128>(r) * range;
            uint64_t off = static_cast<uint64_t>(m >> 64);
            return static_cast<T>(x + off);
        }

        template<typename T>
        typename std::enable_if<std::is_floating_point<T>::value, T>::type
        uniform_distribution(T x, T y) {
            if (!(x < y)) {
                throw std::invalid_argument("uniform_distribution: invalid range [x,y)");
            }
            T t = static_cast<T>(next_float());  // [0,1)
            return x + (y - x) * t;
        }

        static constexpr uint32_t min() { return std::numeric_limits<uint32_t>::min(); }
        static constexpr uint32_t max() { return std::numeric_limits<uint32_t>::max(); }
    };
    //随机数引擎
    extern thread_local PCG32 random_engine;
    
    //设定随机数种子
    template<typename... Ts>
    typename std::enable_if<std::conjunction<std::is_integral<Ts>...>::value, void>::type
    seed(Ts... vs) noexcept{
        random_engine.seed(H_SEED, vs...);
    }

    //返回随机数引擎的引用
    inline static PCG32& rng() {
        return random_engine;
    }

    //生成均匀分布随机数
    template<typename T>
    inline T uniform_distribution(T x, T y) {
        static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                  "uniform only supports integral or floating point types");
        return random_engine.uniform_distribution<T>(x,y);
    }

    using UID = uint32_t;
    using UUID = uint64_t;
    using UUUID = unsigned __int128;
    class UUIDGenerator final{
    private:
        /// @brief 全局线程独立随机数引擎
        static thread_local PCG32 uuid_engine;

        /// @brief 当前对象独立随机数引擎
        PCG32 local_engine;

    public:
        /// @brief 创建独立UUID生成器
        UUIDGenerator():local_engine([](){
            std::random_device rd;
            uint64_t state = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
            uint64_t seq = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
            return PCG32(state, seq);
        }()){}

        /// @brief 32位随机数
        static UID st_generate_uid() noexcept{
            return uuid_engine.next_u32();
        }

        /// @brief 64位随机数
        static UUID st_generate_uuid() noexcept{
            return uuid_engine.next_u64();
        }

        /// @brief 128位随机数
        static UUUID st_generate_uuuid() noexcept{
            UUUID high = static_cast<UUUID>(uuid_engine.next_u64());
            UUUID low = static_cast<UUUID>(uuid_engine.next_u64());
            return (high << 64) | low;
        }

        /// @brief 当前对象生成32位随机数
        UID generate_uid() noexcept{
            return local_engine.next_u32();
        }

        /// @brief 当前对象生成64位随机数
        UUID generate_uuid() noexcept{
            return local_engine.next_u64();
        }

        /// @brief 当前对象生成128位随机数
        UUUID generate_uuuid() noexcept{
            UUUID high = static_cast<UUUID>(local_engine.next_u64());
            UUUID low = static_cast<UUUID>(local_engine.next_u64());
            return (high << 64) | low;
        }
    };
} // namespace core

#endif // __RENDER_RANDOM_HPP
