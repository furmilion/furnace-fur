//
// Created by Administrator on 2026/9/20.
//

#include "cse1_sync_utils.h"

namespace CSE1_REG_INS_SYNC {
    void ins_to_reg(const DivInstrumentCSE1 *ins, CSE1_PACKED::CSE1_CHANNEL_REGISTERS *reg) {
        for (size_t a = 0; a < CSE1_OPER_NUMBER; a++) {
            for (size_t b = 0; b < CSE1_OPER_NUMBER; b++) {
                reg->OPS.OP[a].MIS.MI[b] = ins->op[a].mi[b];
            }
            auto& op = reg->OPS.OP[a];
            op.PHASE = ins->op[a].phase;
            op.FLAGS_A.SET_WAVE(ins->op[a].wave);
            op.DUTY = ins->op[a].duty;

            op.ADSR.AR = ins->op[a].adsr.ar;
            op.ADSR.DR = ins->op[a].adsr.dr;
            op.ADSR.SL = ins->op[a].adsr.sl;
            op.ADSR.SR = ins->op[a].adsr.sr;
            op.ADSR.RR = ins->op[a].adsr.rr;
            op.ENV_STATE.SET_ENV_DIVIDER(ins->op[a].env_divider);
            op.FLAGS_A.SET_REV(ins->op[a].rev);

            op.FLAGS_A.SET_ML(ins->op[a].ml);
            op.FLAGS_A.SET_OPN2_DETUNE(ins->op[a].dn);
            op.PITCH = (ins->op[a].hpitch<<16)+ins->op[a].lpitch;
            op.FLAGS_A.SET_FIXED(ins->op[a].pitchMode != 0);

            op.FLAGS_A.SET_AM1(ins->op[a].am1);
            op.FLAGS_A.SET_AM2(ins->op[a].am2);
            op.FLAGS_A.SET_FM1(ins->op[a].fm1);
            op.FLAGS_A.SET_FM2(ins->op[a].fm2);

            op.START_PHASE = ins->op[a].startP;
            op.END_PHASE = ins->op[a].endP;
        }
        for (size_t o = 0; o < CSE1_OPER_NUMBER; o++) {
            reg->OUT.IN_L.MI[o] = ins->out.inLeft[o];
            reg->OUT.IN_R.MI[o] = ins->out.inRight[o];
        }

        reg->OUT.OUT_L = ins->out.outLeft;
        reg->OUT.OUT_R = ins->out.outRight;

        reg->SPEC.LFO_CONFIG.SET_FREQ(ins->special.lfo1.freq);
        reg->SPEC.LFO_CONFIG.SET_DEPTH(ins->special.lfo1.depth);
        reg->SPEC.LFO_CONFIG.SET_SHAPE(ins->special.lfo1.wave);

        reg->SPEC.LFO_CONFIG.SET_FREQ2(ins->special.lfo2.freq);
        reg->SPEC.LFO_CONFIG.SET_DEPTH2(ins->special.lfo2.depth);
        reg->SPEC.LFO_CONFIG.SET_SHAPE2(ins->special.lfo2.wave);

        uint8_t buf_neg_l = 0;
        uint8_t buf_neg_r = 0;
        for (size_t i = 0; i < CSE1_OPER_NUMBER+1; i++) {
            buf_neg_l |= ins->out.negLeft[i]<<i;
            buf_neg_r |= ins->out.negRight[i]<<i;
        }
        reg->OUT.FLAGS_A.SET_NEG_LEFT(buf_neg_l);
        reg->OUT.FLAGS_A.SET_NEG_RIGHT(buf_neg_r);

        for (size_t i = 0; i < reg->SPEC.FILTERS.size(); i++) {
            const auto& insf = ins->special.filter[i];
            reg->SPEC.FILTERS[i].OUT_L = insf.outLeft;
            reg->SPEC.FILTERS[i].OUT_R = insf.outRight;
            reg->SPEC.FILTERS[i].CUTOFF = insf.cutoff;
            reg->SPEC.FILTERS[i].SET_RES(insf.resonance);
            reg->SPEC.FILTERS[i].SET_TYPES(insf.types);
            reg->SPEC.FILTERS[i].SET_INPUTS(insf.in_out | (insf.in_f1 << 1) | (insf.in_f2 << 2) | (insf.in_f3 << 3));
        }
    }

    void reg_to_ins(const CSE1_PACKED::CSE1_CHANNEL_REGISTERS *reg, DivInstrumentCSE1 *ins) {
        for (size_t a = 0; a < CSE1_OPER_NUMBER; a++) {
            for (size_t b = 0; b < CSE1_OPER_NUMBER; b++) {
                ins->op[a].mi[b] = reg->OPS.OP[a].MIS.MI[b];
            }
            auto& op = reg->OPS.OP[a];
            ins->op[a].phase = op.PHASE;
            ins->op[a].wave = op.FLAGS_A.GET_WAVE();
            ins->op[a].duty = op.DUTY;

            ins->op[a].adsr.ar = op.ADSR.AR;
            ins->op[a].adsr.dr = op.ADSR.DR;
            ins->op[a].adsr.sl = op.ADSR.SL;
            ins->op[a].adsr.sr = op.ADSR.SR;
            ins->op[a].adsr.rr = op.ADSR.RR;
            ins->op[a].env_divider = op.ENV_STATE.GET_ENV_DIVIDER();
            ins->op[a].rev = op.FLAGS_A.GET_REV();

            ins->op[a].ml = op.FLAGS_A.GET_ML();
            ins->op[a].dn = op.FLAGS_A.GET_OPN2_DETUNE();
            ins->op[a].hpitch = op.PITCH>>16&0xffff;
            ins->op[a].lpitch = op.PITCH&0xffff;
            ins->op[a].pitchMode = !op.FLAGS_A.GET_FIXED();

            ins->op[a].am1 = op.FLAGS_A.GET_AM1();
            ins->op[a].am2 = op.FLAGS_A.GET_AM2();
            ins->op[a].fm1 = op.FLAGS_A.GET_FM1();
            ins->op[a].fm2 = op.FLAGS_A.GET_FM2();

            ins->op[a].startP = op.START_PHASE;
            ins->op[a].endP = op.END_PHASE;
        }
        for (size_t o = 0; o < CSE1_OPER_NUMBER; o++) {
            ins->out.inLeft[o] = reg->OUT.IN_L.MI[o];
            ins->out.inRight[o] = reg->OUT.IN_R.MI[o];
        }
        for (size_t i = 0; i < CSE1_OPER_NUMBER+1; i++) {
            ins->out.negLeft[i] = (reg->OUT.FLAGS_A.GET_NEG_LEFT()>>i)&1;
            ins->out.negRight[i] = (reg->OUT.FLAGS_A.GET_NEG_RIGHT()>>i)&1;
        }

        ins->out.outLeft = reg->OUT.OUT_L;
        ins->out.outRight = reg->OUT.OUT_R;

        ins->special.lfo1.freq = reg->SPEC.LFO_CONFIG.GET_FREQ();
        ins->special.lfo1.depth = reg->SPEC.LFO_CONFIG.GET_DEPTH();
        ins->special.lfo1.wave = reg->SPEC.LFO_CONFIG.GET_SHAPE();

        ins->special.lfo2.freq = reg->SPEC.LFO_CONFIG.GET_FREQ2();
        ins->special.lfo2.depth = reg->SPEC.LFO_CONFIG.GET_DEPTH2();
        ins->special.lfo2.wave = reg->SPEC.LFO_CONFIG.GET_SHAPE2();

        for (size_t i = 0; i < reg->SPEC.FILTERS.size(); i++) {
            auto& insf = ins->special.filter[i];
            insf.resonance = reg->SPEC.FILTERS[i].GET_RES();
            insf.types = reg->SPEC.FILTERS[i].GET_TYPES();
            insf.outLeft = reg->SPEC.FILTERS[i].OUT_L;
            insf.outRight = reg->SPEC.FILTERS[i].OUT_R;
            insf.cutoff = reg->SPEC.FILTERS[i].CUTOFF;
            const auto inputs = reg->SPEC.FILTERS[i].GET_INPUTS();
            insf.in_out = (inputs>>0)&1;
            insf.in_f1 = (inputs>>1)&1;
            insf.in_f2 = (inputs>>2)&1;
            insf.in_f3 = (inputs>>3)&1;
        }
    }
}