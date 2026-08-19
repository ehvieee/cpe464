#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "checksum.h"

int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen) {
    int pduLen = 0;
    uint32_t sequenceNumber_no = 0;
    unsigned short checksum = 0;

    pduLen = 4 + 2 + 1 + payloadLen;

    // copy 4-byte sequence number in network order
    sequenceNumber_no = htonl(sequenceNumber);
    memcpy(&pduBuffer[0], &sequenceNumber_no, 4);

    // place 0's in checksum bytes
    pduBuffer[4] = 0;
    pduBuffer[5] = 0;

    // copy 1-byte flag
    memcpy(&pduBuffer[6], &flag, 1);

    // copy payload 
    memcpy (&pduBuffer[7], payload, payloadLen);

    // calculate and copy 2-byte checksum
    checksum = in_cksum((unsigned short*) &pduBuffer[0], pduLen);
    memcpy(&pduBuffer[4], &checksum, 2);

    return pduLen;
}

void printPDU(uint8_t *aPDU, int pduLength) {
    uint32_t sequenceNumber_ho = 0;
    unsigned short checksum = 0;
    uint32_t payloadLen = 0;
    uint8_t flag = 0;

    payloadLen = pduLength - 7;
    uint8_t payload[payloadLen];

    // calculate checksum
    checksum = in_cksum((unsigned short*) &aPDU[0], pduLength);
    if (checksum != 0) {
        printf("PDU corrupted\n");
        return;
    }
    printf("Checksum correct\n");
    
    // print sequence number in host order
    sequenceNumber_ho = ntohl(*(uint32_t *) &aPDU[0]);
    printf("Sequence number: %u\n", sequenceNumber_ho);

    // print flag
    flag = aPDU[6];
    printf("Flag: %u\n", flag);

    // print payload
    memcpy(payload, &aPDU[7], pduLength - 7);
    printf("Payload: %s\n", payload);

    // print payload length
    printf("Payload length: %u\n", payloadLen);
}