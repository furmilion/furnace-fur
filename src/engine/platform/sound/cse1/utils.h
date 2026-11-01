//
// Created by Administrator on 2026/9/13.
//

#ifndef FURNACE_UTILS_H
#define FURNACE_UTILS_H
#include <cstring>

template <typename T, unsigned Shift, unsigned Width>
struct CSE1_BIT_FIELD {
    static_assert(static_cast<uint64_t>(Width) > 0, "CSE1_BIT_FIELD: Width must be > 0");
    static_assert(static_cast<uint64_t>(Shift) + static_cast<uint64_t>(Width) <= sizeof(T) * static_cast<uint64_t>(8), "CSE1_BIT_FIELD: Out of Range!");

    using ValueType = T;
    static constexpr T Mask = (1 << Width) - 1;

    static constexpr T Get(T reg) noexcept {
        return (reg >> Shift) & Mask;
    }

    static constexpr T Set(T reg, T value) noexcept {
        return (reg & ~(Mask << Shift)) | ((value & Mask) << Shift);
    }
};


template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast: size mismatch");
    static_assert(std::is_trivially_copyable<To>::value &&
                  std::is_trivially_copyable<From>::value, "bit_cast: must be trivially copyable");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}
#endif //FURNACE_UTILS_H
