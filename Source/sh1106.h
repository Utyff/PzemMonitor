#ifndef PZEMMONITOR_SH1106_H
#define PZEMMONITOR_SH1106_H

void SH1106_Init(void);

void SH1106_Print(uint8 x, uint8 y, const char *str);

void SH1106_Erase(uint8 x, uint8 y, uint8 count);

void HW_DelayUs(uint16 microSecs);

#endif // PZEMMONITOR_SH1106_H
