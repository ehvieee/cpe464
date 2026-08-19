#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "safeUtil.h"

int sendPDU(int socketNumber, uint8_t *dataBuffer, int lengthOfData) {
    int bytes;
    int PDU_len;
    int no_len;
    uint8_t buf[lengthOfData + 2];

    // add 2 len bytes
    PDU_len = lengthOfData + 2;

    // put PDU_len (in network order) into buf
    no_len = htons(PDU_len);
    memcpy(&buf[0], &no_len, 2);

    // copy dataBuffer into rest of buf
    memcpy(&buf[2], dataBuffer, lengthOfData);

    // check for send error
    if ((bytes = safeSend(socketNumber, buf, PDU_len, 0)) < 0) {
        perror("send call");
		exit(-1);
    }

    return bytes - 2;
}

int recvPDU(int clientSocket, uint8_t *dataBuffer, int bufferSize) {
    int bytes;
    uint8_t lenBuf[2];
    uint16_t PDU_len;
    
    // receive first 2 bytes (len)
    if ((bytes = safeRecv(clientSocket, lenBuf, 2, MSG_WAITALL)) < 0) {
        perror("recv call");
		exit(-1);
    }
    
    // check initial recv
    if (bytes == 0) {
        return bytes;
    }

    // get PDU_len (host order) - 2 len bytes
    PDU_len = ntohs(*(uint16_t *) &lenBuf[0]) - 2;

    // bug check for if PDU_len > bufferSize
    if (PDU_len > bufferSize) {
        perror("buffer size error");
		exit(-1);
    }

    // receive rest of data
    if ((bytes = safeRecv(clientSocket, dataBuffer, PDU_len, MSG_WAITALL)) < 0) {
        perror("recv call");
        exit(-1);
    }

    return bytes;
}