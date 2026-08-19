#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "srPDU.h"
#include "functions.h"

int insertHandle(char *handleName, uint8_t *PDU, int startingOffset) {
    int offset;
    uint8_t handleLen;

    // get handleLen (ignores null character)
    handleLen = strlen(handleName);

    // insert handleLen at startingOffset
    PDU[startingOffset] = handleLen;

    // insert handleName after handleLen (ignoring null character)
    memcpy(&PDU[startingOffset + 1], handleName, handleLen);

    // new offset = startingOffset + handle + handleLen byte
    offset = startingOffset + handleLen + 1;

    return offset;
}

int getHandle(char *handleName, uint8_t *PDU, int startingOffset) {
    int offset;
    uint8_t handleLen;
    
    // get handleLen from PDU[startingOffset]
    handleLen = PDU[startingOffset];

    // copy data starting at PDU[startingOffset + 1]  (ignore handleLen byte)
    memcpy(handleName, &PDU[startingOffset + 1], handleLen);

    // terminate with null character
    handleName[handleLen] = '\0';

    // new offset = startingOffset + handle + handleLen byte
    offset = startingOffset + handleLen + 1;

    return offset;
}

int insertFlag(uint8_t *PDU, uint8_t flag, int startingOffset) {
    int offset;

    // put flag into startingOffset
	PDU[startingOffset] = flag;

    // new offset = startingOffset + flag byte
    offset = startingOffset + 1;

    return offset;
}

int getFlag(uint8_t *PDU, int startingOffset) {
    // get flag from first byte
    return PDU[startingOffset];
}

void sendFlag(int socket, int flag) {
	uint8_t PDU[1];

	// set flag byte to flag
	PDU[0] = flag;

	// send flag
	sendPDU(socket, PDU, 1);
}