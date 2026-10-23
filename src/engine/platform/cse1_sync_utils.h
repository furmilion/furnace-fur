//
// Created by Administrator on 2026/9/20.
//

#ifndef FURNACE_CSE1_SYNC_UTILS_H
#define FURNACE_CSE1_SYNC_UTILS_H
#include "../dispatch.h"

#include "sound/cse1/cse1.hpp"

namespace CSE1_REG_INS_SYNC {
    void reg_to_ins(const CSE1_PACKED::CSE1_CHANNEL_REGISTERS* reg, DivInstrumentCSE1* ins);
    void ins_to_reg(const DivInstrumentCSE1* ins, CSE1_PACKED::CSE1_CHANNEL_REGISTERS* reg);
}

#endif //FURNACE_CSE1_SYNC_UTILS_H