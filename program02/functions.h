#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <stdint.h>

int insertHandle(char *handleName, uint8_t *PDU, int startingOffset);
int getHandle(char *handleName, uint8_t *PDU, int startingOffset);
int insertFlag(uint8_t *PDU, uint8_t flag, int startingOffset);
int getFlag(uint8_t *PDU, int startingOffset);
void sendFlag(int socket, int flag);

#endif