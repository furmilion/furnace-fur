#include "CMyFilter.h"
#include "../pantable_opna.h"

float* freqTable;
float* gainTable;
float* QTable;

float convInt = 21474.83647f;

int pmtable[2][8][FM_LFOENTS];
uint32_t amtable[2][4][FM_LFOENTS];
bool tablemade = false;

float panTable_opna[4] = { 1.0f, 0.7512f, 0.4512f, 0.0500f };
uint32_t sinetable_opna[12][4][1024];

int amtable_opna[FM_LFOENTS];
int pmtable_opna[FM_LFOENTS];
int tltable_opna[FM_TLENTS + FM_TLPOS];

void CMyFilter::makeTable()
{
    freqTable = new float[256];
    gainTable = new float[256];
    QTable = new float[256];

    for (int i = 0; i < 256; i++)
    {
        //freqTableの作成(1～38500まで)
        if (i < 256 / 8 * 3)
        {
            freqTable[i] = i + 1;
        }
        else if (i < 256 / 8 * 5)
        {
            freqTable[i] = (i - 256 / 8 * 3) * 10 + 100;
        }
        else if (i < 256 / 8 * 7)
        {
            freqTable[i] = (i - 256 / 8 * 5) * 100 + 800;
        }
        else
        {
            freqTable[i] = (i - 256 / 8 * 7) * 1000 + 7500;
        }


        //gainTableの作成(-20～+19.84375まで)
        if (i < 128)
        {
            gainTable[i] = (float)(-20.0 / 128.0 * (128 - i));
        }
        else
        {
            gainTable[i] = (float)(20.0 / 128.0 * (i - 128));
        }


        //QTableの作成(0.1～20.0まで)
        if (i < 256 / 8 * 3)
        {
            QTable[i] = (float)(1.0 / (256 / 8 * 3) * (i + 1)); // 0-95 : 0.01041667 ～ 1.0
        }
        else if (i < 256 / 8 * 6)
        {
            QTable[i] = (float)(10.0 / (256 / 8 * 3) * (i + 1 - 256 / 8 * 3) + 1.0); // 96-191 : 1.104167 ～ 11.0
        }
        else
        {
            QTable[i] = (float)(10.0 / (256 / 8 * 2) * (i + 1 - 256 / 8 * 6) + 11.0); // 192-255 : 11.15625 ～ 21.0
        }
    }
}