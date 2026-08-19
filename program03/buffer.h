#ifndef __BUFFER_H__
#define __BUFFER_H__

void setupBuffer(int windowSize);
void insertBuffer(uint32_t sequenceNum, uint8_t *PDU, int pduLen);
int checkValid(uint32_t sequenceNum);
int getBuffer(uint32_t sequenceNum, uint8_t *PDU);
int getExpected(void);
int updateExpected(int num);
int updateHighest(int num);
int bufferEmpty(void);
int getHighest();
void setInvalid(uint32_t sequenceNum);

#endif