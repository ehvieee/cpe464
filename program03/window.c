#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int upper = 0;
static int lower = 0;
static int current = 0;
static int windowSize = 0;

struct Buffer {
    uint8_t buffer[1407];
    int len;
};

struct Buffer *window = NULL;

// initialize window
void setupWindow(int size) {
    windowSize = size;
    current = 0;
    lower = 0;
    upper = lower + windowSize;
    window = malloc(windowSize * sizeof(struct Buffer));
    
    for (int i = 0; i < windowSize; i++) {
        window[i].len = 0;
    }
}

// slide window on RR packet reception
void processRR(uint32_t sequenceNum) {
    if (sequenceNum > lower) {
        lower = sequenceNum;
        upper = lower + windowSize;
    }
}

// get packet for resend (returns len)
int getWindow(uint32_t sequenceNum, uint8_t *buffer) {
    int index = sequenceNum % windowSize;

    memcpy(buffer, &window[index].buffer, window[index].len);

    return window[index].len;
}

// insert packet into window
void insertWindow(uint32_t sequenceNum, uint8_t *buffer, int pduLen) {
    int index = sequenceNum % windowSize;

    memcpy(&(window[index].buffer), buffer, pduLen);
    window[index].len = pduLen;
}

void incrementCurrent() {
    current++;
}

int getLowest(uint8_t *buffer) {
    int index = lower % windowSize;

    memcpy(buffer, &window[index].buffer, window[index].len);

    return window[index].len;
}

// checks if window is full
int windowOpen() {
    if (current < upper) {
        return 1;
    }
    else {
        return 0;
    }
}