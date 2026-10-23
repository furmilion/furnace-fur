//
// Created by Administrator on 2026/9/13.
//

#ifndef FURNACE_UTILS_H
#define FURNACE_UTILS_H
template <typename T, unsigned Shift, unsigned Width>
struct CSE1_BIT_FIELD {
    static_assert(Width > 0, "CSE1_BIT_FIELD: Width must be > 0");
    static_assert(Shift + Width <= sizeof(T) * 8, "CSE1_BIT_FIELD: Out of Range!");

    using ValueType = T;
    static constexpr T Mask = (1 << Width) - 1;

    static constexpr T Get(T reg) noexcept {
        return (reg >> Shift) & Mask;
    }

    static constexpr T Set(T reg, T value) noexcept {
        return (reg & ~(Mask << Shift)) | ((value & Mask) << Shift);
    }
};
#endif //FURNACE_UTILS_H
