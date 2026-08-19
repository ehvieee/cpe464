#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int windowSize = 0;
static int expected = 0;
static int highest = 0;

struct Buffer {
    uint8_t buff[1407];
    int valid;
    int pduLen;
};

struct Buffer *buffer = NULL;

// initialize buffer
void setupBuffer(int size) {
    windowSize = size;
    buffer = malloc(windowSize * sizeof(struct Buffer));

    for (int i = 0; i < windowSize; i++) {
        buffer[i].valid = 0;
        buffer[i].pduLen = 0;
    }
}

// insert packet into buffer and set valid bit
void insertBuffer(uint32_t sequenceNum, uint8_t *PDU, int pduLen) {
    int index = sequenceNum % windowSize;

    memcpy(&(buffer[index].buff), PDU, pduLen);
    buffer[index].valid = 1;
    buffer[index].pduLen = pduLen;
}

// check valid bit of packet
int checkValid(uint32_t sequenceNum) {
    int index = sequenceNum % windowSize;

    return buffer[index].valid;
}

// put buffer into PDU and return buffer len (return -1 if invalid)
int getBuffer(uint32_t sequenceNum, uint8_t *PDU) {
    int index = sequenceNum % windowSize;

    // check if data is valid
    if (!(buffer[index].valid)) {
        return -1;
    }

    memcpy(&PDU[0], &(buffer[index].buff), buffer[index].pduLen);

    return buffer[index].pduLen;
}

void setInvalid(uint32_t sequenceNum) {
    int index = sequenceNum % windowSize;

    buffer[index].valid = 0;
}

int getExpected() {
    return expected;
}

int updateExpected(int num) {
    if (num > expected) {
        expected = num;
    }
    return expected;
}

int updateHighest(int num) {
    if (num > highest) {
        highest = num;
    }
    return highest;
}

int getHighest() {
    return highest;
}

// check if buffer is empty (check valid bit of all elements)
int bufferEmpty() {
    for (int i = 0; i < windowSize; i++) {
        if (buffer[i].valid == 1) {
            return 0;
        }
    }

    return 1;
}