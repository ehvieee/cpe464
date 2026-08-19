#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include "networks.h"
#include "safeUtil.h"
#include "srPDU.h"
#include "pollLib.h"
#include "functions.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_CONNECTIONS 300

void initialize(int socketNum, char *handle);

int main(int argc, char *argv[]) {
    int socketNum[MAX_CONNECTIONS];
    char handleName[MAXBUF];

    // Create poll set
    setupPollSet();

    // Add STDIN to poll set
    addToPollSet(STDIN_FILENO);

    // Create 300 connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        /* Set up the TCP Client socket  */
        socketNum[i] = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);

        // Create a unique handle name
        sprintf(handleName, "test%d", i);

        printf("handleName: %s\n", handleName);

        // Add the socket to the poll set (even though we're not doing anything after)
        addToPollSet(socketNum[i]);

        // Send the handle to the server
        initialize(socketNum[i], handleName);
    }

    // Now the program will not do anything, but the connections will be open
    // You can modify this if you'd like to add more functionality later.
    while (1) {
        // Poll to keep connections open indefinitely
        pollCall(-1);  // You may need to adjust this part if you want to handle events on the sockets
    }

    return 0;
}

// Upon initial connection to server, send flag = 1 packet and block for response
void initialize(int socketNum, char *handle) {
    uint8_t handleLen;
    int pollReturn;
    int flag = 0;
    int dataLen;
    uint8_t receivedData[MAXBUF];

    handleLen = strlen(handle);

    // Handle + 1 byte flag + 1 byte handle len
    dataLen = handleLen + 2;
    uint8_t PDU[dataLen];

    // Send packet with flag = 1, handleLen, and handleName
    insertFlag(PDU, 1, 0);
    insertHandle(handle, PDU, 1);
    sendPDU(socketNum, PDU, dataLen);

    // Block until receiving packet with flag = 2 or flag = 3
    while ((flag != 2) && (flag != 3)) {
        // Block and wait for socket activity (like data being ready to read)
        pollReturn = pollCall(-1);  // Modify this as needed to match your specific poll function setup

        if (pollReturn > 0) {
            // Receive PDU from server
            recvPDU(socketNum, receivedData, MAXBUF);

            // Get flag byte from PDU
            flag = getFlag(receivedData, 0);
        }
    }

    // If the handle is already in use, print an error but don't exit
    if (flag == 3) {
        printf("Handle already in use: %s\n", handle);
    }
}
