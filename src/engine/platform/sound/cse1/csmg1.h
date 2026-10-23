//
// Created by Administrator on 2026/10/6.
//

#ifndef FURNACE_CSMG1_H
#define FURNACE_CSMG1_H
#include <array>

#include "utils.h"

#define CSE1_BIT_FIELD_MEMBER(name, type, shift, width, reg) \
    using name = CSE1_BIT_FIELD<type, shift, width>; \
    constexpr void SET_##name(const type value) noexcept { \
        reg = name::Set(reg, value); \
    } \
    constexpr type GET_##name() const noexcept { \
        return name::Get(reg); \
    }

#define CSMG1_MAX_CHANNELS 32
#define CSMG1_MIN_CHANNELS 1

namespace CSMG1_PACKED {
    struct CSMG1_CONFIG_AND_TABLE;
    typedef uint16_t CSMG1_REG;
    typedef uint32_t CSMG1_DOUBLE_REG;
    typedef int16_t CSMG1_SIG_REG;
    typedef int32_t CSMG1_DOUBLE_SIG_REG;

    enum WAVE_TABLE_TYPE: uint8_t {
        SINE = 0,
        TRIANGLE = 1,
        SQUARE = 2,
        SAW = 3,
        NOISE = 4,
        WAVETABLE_SAMPLE = 5,
        ONESHOT_SAMPLE = 6,
        LOOP_SAMPLE = 7,
    };

    struct CSMG1_CHANNEL_REGISTER {
        CSMG1_DOUBLE_REG PHASE;
        CSMG1_REG DUTY;
        CSMG1_REG FLAGS;
        CSMG1_DOUBLE_REG PITCH;
        CSMG1_REG OUT_L;
        CSMG1_REG OUT_R;
        CSMG1_DOUBLE_REG START_PHASE;
        CSMG1_DOUBLE_REG END_PHASE;
        CSMG1_DOUBLE_REG TL_TABLE;
        CSMG1_DOUBLE_REG OUT_TABLE;
        void clock(const CSMG1_CONFIG_AND_TABLE &wave_table, CSMG1_DOUBLE_SIG_REG &LeftBuf, CSMG1_DOUBLE_SIG_REG &RightBuf) noexcept;

        CSMG1_REG option_wave(const CSMG1_CONFIG_AND_TABLE &wave_table, CSMG1_REG index) noexcept;

        CSE1_BIT_FIELD_MEMBER(REV, CSMG1_REG, 0, 1, FLAGS);
        CSE1_BIT_FIELD_MEMBER(WAVE, CSMG1_REG, 1, 3, FLAGS);
        CSE1_BIT_FIELD_MEMBER(NEG_L, CSMG1_REG, 4, 1, FLAGS);
        CSE1_BIT_FIELD_MEMBER(NEG_R, CSMG1_REG, 5, 1, FLAGS);
        CSE1_BIT_FIELD_MEMBER(N_DTL, CSMG1_REG, 6, 1, FLAGS); // Not Default TL Table
        CSE1_BIT_FIELD_MEMBER(N_DOT, CSMG1_REG, 7, 1, FLAGS); // Not Default Out Table
        CSE1_BIT_FIELD_MEMBER(WP, CSMG1_REG, 8, 2, FLAGS);
        CSE1_BIT_FIELD_MEMBER(CUBIC, CSMG1_REG, 10, 1, FLAGS);
        CSE1_BIT_FIELD_MEMBER(EXT_WAVE, CSMG1_REG, 11, 1, FLAGS);
    };

    struct CSMG1_CHANNELS {
        std::array<CSMG1_CHANNEL_REGISTER, CSMG1_MAX_CHANNELS> REGISTER;
        std::array<CSMG1_DOUBLE_SIG_REG, CSMG1_MAX_CHANNELS> OUT_L;
        std::array<CSMG1_DOUBLE_SIG_REG, CSMG1_MAX_CHANNELS> OUT_R;
        void clock(const CSMG1_CONFIG_AND_TABLE &config_and_table) noexcept;
    };

    struct CSMG1 {
        CSMG1_CHANNELS CHANNELS;
        void clock(const CSMG1_CONFIG_AND_TABLE &config_and_table) noexcept;

        CSMG1_DOUBLE_SIG_REG CSE1_GET_SAMPLE(uint8_t ch) const noexcept;

        void hard_reset();
    };

    struct CSMG1_CONFIG_AND_TABLE {
        std::array<CSMG1_REG, 65536> sine{};
        std::array<CSMG1_REG, 65536> triangle{};
        std::array<CSMG1_REG, 65536> volume_line{};
        CSMG1_REG default_volume_line;
        CSMG1_REG* memPCM;
        CSMG1_REG fastSpeed;
        CSMG1_REG chipType;
        uint8_t CHANNEL_SIZE; // 0~CSMG1_MAX_CHANNELS-1

        CSMG1_CONFIG_AND_TABLE();

        void reset() noexcept;
    };

    CSMG1_DOUBLE_REG MULT_CALC(CSMG1_DOUBLE_REG left, uint8_t right) noexcept;
    CSMG1_REG BIT_EX(bool v) noexcept;
    CSMG1_DOUBLE_REG BIT_EX_32(bool v) noexcept;
}

#undef CSE1_BIT_FIELD_MEMBER

#endif //FURNACE_CSMG1_H
