#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <stdint.h>

void setupWindow(int windowSize);
int windowOpen(void);
void processRR(uint32_t sequenceNum);
int getWindow(uint32_t sequenceNum, uint8_t *buffer);
void insertWindow(uint32_t sequenceNum, uint8_t *buffer, int pduLen);
void incrementCurrent();
int getLowest(uint8_t *buffer);

#endif