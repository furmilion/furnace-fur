//
// Created by Administrator on 2026/9/19.
//

#include "CSE_INV.h"
#include "../../../engine/platform/sound/cse1/cse1.hpp"
#include "../gui/insEdit/insEditCommon.h"

// LE-Only

constexpr uint64_t gen_min = 0;
constexpr uint16_t gen_max      = 0xffff;
constexpr uint16_t gen_min_s      = 0xff00;
constexpr uint16_t gen_max_s      = 0x0100;
constexpr uint16_t b14Bit_max      = 0x3fff;
constexpr uint8_t  twoBit_max   = 0x03;
constexpr uint8_t  threeBit_max = 0x07;
constexpr uint8_t  fourBit_max  = 0x0f;
constexpr uint8_t  wave_max     = 0x04;
constexpr uint8_t  wave_sample_mode_min = 0x05;
constexpr uint8_t  wave_sample_mode_max = 0x07;
constexpr int32_t  big_max      = 0x7fffffff;

const std::array<std::string, 4> pitchTableMenu = {
    "Normal",
    "Fixed",
    "Pitch",
    "Sample"
};

void FurnaceGUI::drawInsCSE1(DivInstrument *ins) {
    std::vector<FurnaceGUIMacroDesc> macroList;
    auto& cse1 = ins -> cse1;
    if (ImGui::BeginTabItem("CSE-1")) {
        if (ImGui::BeginTable("Table", 5, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("OP");
            ImGui::TableSetupColumn("Control");
            ImGui::TableSetupColumn("Pitch Control");
            ImGui::TableSetupColumn("ADSR");
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (int i = 0; i < CSE1_OPER_NUMBER; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("OP%d", i+1);
                ImGui::PushID(i);
                for (int j = 0; j < CSE1_OPER_NUMBER; j++) {
                    ImGui::PushID(j);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    P(ImGui::SliderScalar(
                    "##MI",
                    ImGuiDataType_U16,
                    &cse1.op[i].mi[j],
                    &gen_min, &gen_max,
                    fmt::format("MI{0}: %d", j + 1).c_str()
                    ));
                    ImGui::PopID();
                }

                const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;
                ImGui::SetNextItemWidth(halfWidth);
                P(ImGui::SliderScalar("##IN_LEFT",
                ImGuiDataType_U16,
                &cse1.out.inLeft[i],
                &gen_min, &gen_max,
                fmt::format("OUTL{0}: %d", i + 1).c_str()));

                ImGui::SameLine();

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar("##IN_RIGHT",
                ImGuiDataType_U16,
                &cse1.out.inRight[i],
                &gen_min, &gen_max,
                "OUTR: %d"));

                P(ImGui::Checkbox(fmt::format("LN{0}", i + 1).c_str(), &cse1.out.negLeft[i]));
                ImGui::SameLine();
                P(ImGui::Checkbox(fmt::format("RN{0}", i + 1).c_str(), &cse1.out.negRight[i]));

                ImGui::TableNextColumn();
                ImGui::NewLine();

                P(ImGui::Checkbox("REV", &cse1.op[i].rev));

                P(ImGui::Checkbox("useSample", &cse1.op[i].useSample));

                if (!cse1.op[i].useSample) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    P(ImGui::SliderScalar(
                    "##PHASE",
                    ImGuiDataType_S32,
                    &cse1.op[i].phase,
                    &gen_min, &big_max,
                    "PHASE: %d"
                    ));

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    P(ImGui::SliderScalar(
                    "##WAVE",
                    ImGuiDataType_U8,
                    &cse1.op[i].wave,
                    &gen_min, &wave_max,
                    "WAVE: %d"
                    ));

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    P(ImGui::SliderScalar(
                    "##DUTY",
                    ImGuiDataType_U16,
                    &cse1.op[i].duty,
                    &gen_min, &gen_max,
                    "DUTY: %d"
                    ));
                } else {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    P(ImGui::SliderScalar(
                    "##SMMODE",
                    ImGuiDataType_U8,
                    &cse1.op[i].wave,
                    &wave_sample_mode_min, &wave_sample_mode_max,
                    "SMMODE: %d"
                    ));

                    String sName;
                    if (cse1.op[i].sample_tables.sampleIndex >= e->song.sampleLen) {
                        sName = _("none selected");
                    } else {
                        sName = e->song.sample[cse1.op[i].sample_tables.sampleIndex]->name;
                    }

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text(_("Sample"));
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##CSESample", sName.c_str())) {
                        String id;
                        for (int s = 0; s < e->song.sampleLen; s++) {
                            id = fmt::sprintf("%d: %s", s, e->song.sample[s]->name);
                            if (ImGui::Selectable(id.c_str(), cse1.op[i].sample_tables.sampleIndex == s)) {
                                PARAMETER;
                                cse1.op[i].sample_tables.sampleIndex = s;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::TableNextColumn();
                ImGui::NewLine();

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##HPITCH",
                ImGuiDataType_U16,
                &cse1.op[i].hpitch,
                &gen_min, &gen_max,
                "HPITCH: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##LPITCH",
                ImGuiDataType_U16,
                &cse1.op[i].lpitch,
                &gen_min, &gen_max,
                "LPITCH: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##ML",
                ImGuiDataType_U8,
                &cse1.op[i].ml,
                &gen_min, &fourBit_max,
                "ML: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##OPN2DT",
                ImGuiDataType_U8,
                &cse1.op[i].dn,
                &gen_min, &threeBit_max,
                "OPN2DT: %d"
                ));

                ImGui::Text(_("PitchMode"));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::BeginCombo("##PitchMode", pitchTableMenu[cse1.op[i].pitchMode].c_str())) {
                    String id;
                    for (size_t s = 0; s < pitchTableMenu.size(); s++) {
                        id = pitchTableMenu[s];
                        if (ImGui::Selectable(id.c_str(), cse1.op[i].pitchMode == s)) {
                            PARAMETER;
                            cse1.op[i].pitchMode = s;
                        }
                    }
                    ImGui::EndCombo();
                }

                P(ImGui::Checkbox("AM1", &cse1.op[i].am1));
                ImGui::SameLine();
                P(ImGui::Checkbox("AM2", &cse1.op[i].am2));

                P(ImGui::Checkbox("FM1", &cse1.op[i].fm1));
                ImGui::SameLine();
                P(ImGui::Checkbox("FM2", &cse1.op[i].fm2));

                ImGui::TableNextColumn();
                ImGui::NewLine();

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##AR",
                ImGuiDataType_U16,
                &cse1.op[i].adsr.ar,
                cse1.op[i].adsr.ar <= gen_min_s ? reinterpret_cast<const uint16_t*>(&gen_min) : &gen_min_s, &gen_max,
                "A: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##DR",
                ImGuiDataType_U16,
                &cse1.op[i].adsr.dr,
                cse1.op[i].adsr.dr <= gen_min_s ? reinterpret_cast<const uint16_t*>(&gen_min) : &gen_min_s, &gen_max,
                "D: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##SR",
                ImGuiDataType_U16,
                &cse1.op[i].adsr.sr,
                cse1.op[i].adsr.sr <= gen_min_s ? reinterpret_cast<const uint16_t*>(&gen_min) : &gen_min_s, &gen_max,
                "D2: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##SL",
                ImGuiDataType_U16,
                &cse1.op[i].adsr.sl,
                &gen_min, &gen_max,
                "S: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##RR",
                ImGuiDataType_U16,
                &cse1.op[i].adsr.rr,
                cse1.op[i].adsr.rr <= gen_min_s ? reinterpret_cast<const uint16_t*>(&gen_min) : &gen_min_s, &gen_max,
                "R: %d"
                ));

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                "##EDIV",
                ImGuiDataType_U16,
                &cse1.op[i].env_divider,
                &gen_min,  cse1.op[i].env_divider >= gen_max_s ? &b14Bit_max : &gen_max_s,
                "EDIV: %d"
                ));

                ImGui::TableNextColumn();

                ImVec2 sliderSize=ImVec2(dpiScale,dpiScale);

                drawCSE1Env(gen_min,
                    cse1.op[i].adsr.ar,
                    cse1.op[i].adsr.dr,
                    cse1.op[i].adsr.sr,
                    cse1.op[i].adsr.rr,
                    65535-cse1.op[i].adsr.sl,
                    gen_max,
                    gen_min,
                    gen_min,
                    gen_max,
                    gen_max,
                    gen_max,
                    ImVec2(ImGui::GetContentRegionAvail().x,200*sliderSize.y),
                    ins->type);

                ImGui::PopID();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("OUT");

            ImGui::PushID("OUT");
            const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;
            ImGui::SetNextItemWidth(halfWidth);
            P(ImGui::SliderScalar("##OUT_LEFT",
            ImGuiDataType_U16,
            &cse1.out.outLeft,
            &gen_min, &gen_max, "OUTL: %d"));

            ImGui::SameLine();

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar("##OUT_RIGHT",
                    ImGuiDataType_U16,
                    &cse1.out.outRight,
                    &gen_min, &gen_max, "OUTR: %d"));

            ImGui::PopID();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("SPEC");
            ImGui::PushID("SPEC");

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L1FREQ",
            ImGuiDataType_U8,
            &cse1.special.lfo1.freq,
            &gen_min, &twoBit_max,
            "L1FREQ: %d"
            ));

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L1DEPTH",
            ImGuiDataType_U8,
            &cse1.special.lfo1.depth,
            &gen_min, &fourBit_max,
            "L1DEPTH: %d"
            ));

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L1SHAPE",
            ImGuiDataType_U8,
            &cse1.special.lfo1.wave,
            &gen_min, &twoBit_max,
            "L1SHAPE: %d"
            ));

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L2FREQ",
            ImGuiDataType_U8,
            &cse1.special.lfo2.freq,
            &gen_min, &twoBit_max,
            "L2FREQ: %d"
            ));

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L2DEPTH",
            ImGuiDataType_U8,
            &cse1.special.lfo2.depth,
            &gen_min, &fourBit_max,
            "L2DEPTH: %d"
            ));

            ImGui::SetNextItemWidth(-FLT_MIN);
            P(ImGui::SliderScalar(
            "##L2SHAPE",
            ImGuiDataType_U8,
            &cse1.special.lfo2.wave,
            &gen_min, &twoBit_max,
            "L2SHAPE: %d"
            ));

            for (int i = 0; i < 3; i++) {
                ImGui::TableNextColumn();
                ImGui::Text("FILTER %d", i+1);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                    "##CUTOFF",
                    ImGuiDataType_U16,
                    &cse1.special.filter[i].cutoff,
                    &gen_min, &gen_max,
                    "CUTOFF: %d"
                ));
                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar(
                    "##RES",
                    ImGuiDataType_U8,
                    &cse1.special.filter[i].resonance,
                    &gen_min, &gen_max,
                    "RES: %d"
                ));
                const float halfWidth_f = ImGui::GetContentRegionAvail().x * 0.5f;
                ImGui::SetNextItemWidth(halfWidth_f);
                P(ImGui::SliderScalar("##FOUTL",
                ImGuiDataType_U16,
                &cse1.special.filter[i].outLeft,
                &gen_min, &gen_max, "OUTL: %d"));

                ImGui::SameLine();

                ImGui::SetNextItemWidth(-FLT_MIN);
                P(ImGui::SliderScalar("##FOUTR",
                        ImGuiDataType_U16,
                        &cse1.special.filter[i].outRight,
                        &gen_min, &gen_max, "OUTR: %d"));

                P(ImGui::Checkbox("IN OUT", &cse1.special.filter[i].in_out));
                ImGui::SameLine();
                P(ImGui::Checkbox("IN F1 ", &cse1.special.filter[i].in_f1));

                P(ImGui::Checkbox("IN F2 ", &cse1.special.filter[i].in_f2));
                ImGui::SameLine();
                P(ImGui::Checkbox("IN F3 ", &cse1.special.filter[i].in_f3));

                P(ImGui::SliderScalar("##FTYPE",
                ImGuiDataType_U8,
                &cse1.special.filter[i].types,
                &gen_min, &threeBit_max, "TYPE: %d"));

                ImGui::PopID();
            }

            ImGui::PopID();
            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(_("Macros"))) {
        macroList.push_back(FurnaceGUIMacroDesc(_("Volume"),&ins->std.volMacro,0,65535,160,uiColors[GUI_COLOR_MACRO_VOLUME]));
        macroList.push_back(FurnaceGUIMacroDesc(_("Arpeggio"),&ins->std.arpMacro,-120,120,160,uiColors[GUI_COLOR_MACRO_PITCH],true,NULL,macroHoverNote,false,NULL,true,ins->std.arpMacro.val));
        macroList.push_back(FurnaceGUIMacroDesc(_("Panning (Left)"),&ins->std.panLMacro,0,65535,160,uiColors[GUI_COLOR_MACRO_OTHER]));
        macroList.push_back(FurnaceGUIMacroDesc(_("Panning (Right)"),&ins->std.panRMacro,0,65535,160,uiColors[GUI_COLOR_MACRO_OTHER],false,NULL,NULL,false));
        macroList.push_back(FurnaceGUIMacroDesc(_("Pitch"),&ins->std.pitchMacro,-2048,2047,160,uiColors[GUI_COLOR_MACRO_PITCH],true,macroRelativeMode));
        macroList.push_back(FurnaceGUIMacroDesc(_("Phase Reset"),&ins->std.phaseResetMacro,0,1,32,uiColors[GUI_COLOR_MACRO_OTHER],false,NULL,NULL,true));

        drawMacros(macroList,macroEditStateMacros,ins);
        ImGui::EndTabItem();
    }

    for (int i = 0; i < CSE1_OPER_NUMBER; i++) {
    }

    if (ImGui::BeginTabItem("About")) {
        ImGui::Text("Soft Core Version: Beta 0.1.1");
        ImGui::Text("Name: Cecnull1 Sound Engine 1");
        ImGui::Text("Or  : Audio Cecnull1 Engine 1");
        ImGui::Text("Author: Cecnull1");
        ImGui::EndTabItem();
    }
}

#include "../insEditCommon.h"

void FurnaceGUI::drawCSE1Env(uint16_t tl, uint16_t ar, uint16_t dr, uint16_t d2r, uint16_t rr, uint16_t sl, uint16_t sus, uint16_t egt, uint16_t algOrGlobalSus, float maxTl, float maxArDr, float maxRr, const ImVec2& size, unsigned short instType) {
  ImDrawList* dl=ImGui::GetWindowDrawList();
  ImGuiWindow* window=ImGui::GetCurrentWindow();

  ImVec2 minArea=window->DC.CursorPos;
  ImVec2 maxArea=ImVec2(
    minArea.x+size.x,
    minArea.y+size.y
  );
  ImRect rect=ImRect(minArea,maxArea);
  ImGuiStyle& style=ImGui::GetStyle();
  ImU32 color=ImGui::GetColorU32(uiColors[GUI_COLOR_FM_ENVELOPE]);
  ImU32 colorR=ImGui::GetColorU32(uiColors[GUI_COLOR_FM_ENVELOPE_RELEASE]); // Relsease triangle
  ImU32 colorS=ImGui::GetColorU32(uiColors[GUI_COLOR_FM_ENVELOPE_SUS_GUIDE]); // Sustain horiz/vert line color
  ImGui::ItemSize(size,style.FramePadding.y);
  if (ImGui::ItemAdd(rect,ImGui::GetID("fmEnv"))) {
    ImGui::RenderFrame(rect.Min,rect.Max,ImGui::GetColorU32(ImGuiCol_FrameBg),true,style.FrameRounding);

    // Adjust for OPLL global sustain setting
    if (instType==DIV_INS_OPLL && algOrGlobalSus==1.0) {
      rr=5.0;
    }
    // calculate x positions
    float arPos=float(maxArDr-(float)ar)/maxArDr; // peak of AR, start of DR
    float drPos=arPos+(((float)sl/65535.0)*(float(maxArDr-(float)dr)/maxArDr)); // end of DR, start of D2R
    float d2rPos=drPos+(((65535.0-(float)sl)/65535.0)*(float(65535.0-(float)d2r)/65535.0)); // End of D2R
    float rrPos=(float(maxRr-(float)rr)/float(maxRr)); // end of RR

    // shrink all the x positions horizontally
    arPos/=2.0;
    drPos/=2.0;
    d2rPos/=2.0;
    rrPos/=1.0;

    ImVec2 pos1=ImLerp(rect.Min,rect.Max,ImVec2(0.0,1.0)); // the bottom corner
    ImVec2 pos2=ImLerp(rect.Min,rect.Max,ImVec2(arPos,((float)tl/maxTl))); // peak of AR, start of DR
    ImVec2 pos3=ImLerp(rect.Min,rect.Max,ImVec2(drPos,(float)(((float)tl/maxTl)+((float)sl/65535.0)-(((float)tl/maxTl)*((float)sl/65535.0))))); // end of DR, start of D2R
    ImVec2 pos4=ImLerp(rect.Min,rect.Max,ImVec2(d2rPos,1.0)); // end of D2R
    ImVec2 posRStart=ImLerp(rect.Min,rect.Max,ImVec2(0.0,((float)tl/maxTl))); // release start
    ImVec2 posREnd=ImLerp(rect.Min,rect.Max,ImVec2(rrPos,1.0));// release end
    ImVec2 posSLineHEnd=ImLerp(rect.Min,rect.Max,ImVec2(1.0,(float)(((float)tl/maxTl)+((float)sl/65535.0)-(((float)tl/maxTl)*((float)sl/65535.0))))); // sustain horizontal line end
    ImVec2 posSLineVEnd=ImLerp(rect.Min,rect.Max,ImVec2(drPos,1.0)); // sustain vertical line end
    ImVec2 posDecayRate0Pt=ImLerp(rect.Min,rect.Max,ImVec2(1.0,((float)tl/maxTl))); // Height of the peak of AR, forever
    ImVec2 posDecay2Rate0Pt=ImLerp(rect.Min,rect.Max,ImVec2(1.0,(float)(((float)tl/maxTl)+((float)sl/65535.0)-(((float)tl/maxTl)*((float)sl/65535.0))))); // Height of the peak of SR, forever

    // dl->Flags=ImDrawListFlags_AntiAliasedLines|ImDrawListFlags_AntiAliasedLinesUseTex;
    if ((float)ar==0.0) { // if AR = 0, the envelope never starts
      dl->AddTriangleFilled(posRStart,posREnd,pos1,colorS); // draw release as shaded triangle behind everything
      addAALine(dl,pos1,pos4,color); // draw line on ground
    } else if ((float)dr==0.0 && (float)sl!=0.0) { // if DR = 0 and SL is not 0, then the envelope stays at max volume forever
      dl->AddTriangleFilled(posRStart,posREnd,pos1,colorS); // draw release as shaded triangle behind everything
      // addAALine(dl,pos3,posSLineHEnd,colorS); // draw horiz line through sustain level
      // addAALine(dl,pos3,posSLineVEnd,colorS); // draw vert. line through sustain level
      addAALine(dl,pos1,pos2,color); // A
      addAALine(dl,pos2,posDecayRate0Pt,color); // Line from A to end of graph
    } else if ((float)d2r==0.0 || ((instType==DIV_INS_OPL || instType==DIV_INS_SNES || instType==DIV_INS_ESFM || instType==DIV_INS_CSE1) && sus==1.0) || (instType==DIV_INS_OPLL && egt!=0.0)) { // envelope stays at the sustain level forever
      dl->AddTriangleFilled(posRStart,posREnd,pos1,colorS); // draw release as shaded triangle behind everything
      addAALine(dl,pos3,posSLineHEnd,colorR); // draw horiz line through sustain level
      addAALine(dl,pos3,posSLineVEnd,colorR); // draw vert. line through sustain level
      addAALine(dl,pos1,pos2,color); // A
      addAALine(dl,pos2,pos3,color); // D
      addAALine(dl,pos3,posDecay2Rate0Pt,color); // Line from D to end of graph
    } else { // draw graph normally
      dl->AddTriangleFilled(posRStart,posREnd,pos1,colorS); // draw release as shaded triangle behind everything
      addAALine(dl,pos3,posSLineHEnd,colorR); // draw horiz line through sustain level
      addAALine(dl,pos3,posSLineVEnd,colorR); // draw vert. line through sustain level
      addAALine(dl,pos1,pos2,color); // A
      addAALine(dl,pos2,pos3,color); // D
      addAALine(dl,pos3,pos4,color); // D2
    }
    //dl->Flags^=ImDrawListFlags_AntiAliasedLines|ImDrawListFlags_AntiAliasedLinesUseTex;
  }
}
