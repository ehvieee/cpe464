/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "srPDU.h"
#include "pollLib.h"
#include "functions.h"

#define DEBUG_FLAG 1

// max STDIN / message len
#define MAXLEN 1400

void sendToServer(int socketNum);
int readFromStdin(uint8_t *buffer);
void checkArgs(int argc, char *argv[]);
void clientControl(int socketNum, char *handle);
void processStdin(int socketNum, char *handle);
void processMsgFromServer(int socketNum);
void initialize(int socketNum, char *handle);
void processCommand(int socketNum, char *command, uint8_t *buffer, char *handle);
void processFlag(int socketNum, uint8_t flag, uint8_t *buffer);
void processM(int socketNum, uint8_t *buffer, char *handle);
void processC(int socketNum, uint8_t *buffer, char *handle);
void processB(int socketNum, uint8_t *buffer, char *handle);
void processL(int socketNum, uint8_t *buffer);
void sendFlag4(int socketNum, char *message, char *handle);
void sendFlag5(int socketNum, char *message, char *srcHandle, char *destHandle);
void sendFlag6(int socketNum, char* message, char *srcHandle, char *destHandles[], uint8_t handles);
void flag4(uint8_t *buffer);
void flag5(uint8_t *buffer);
void flag6(uint8_t *buffer);
void flag7(int socketNum, uint8_t *buffer);
void flag11(int socketNum, uint8_t *buffer);
void flag12(uint8_t *buffer);
void flag13(void);

int main(int argc, char *argv[]) {
	int socketNum = 0;
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	// create poll set
	setupPollSet();

	// add socketNum and STDIN to poll set
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	// send initial packet to server and block for response
	char *handle = argv[1];
	initialize(socketNum, argv[1]);

	// print prompt
	printf("$: ");
	fflush(stdout);

	while (1) {
		clientControl(socketNum, handle);
	}
	
	close(socketNum);
	return 0;
}

void clientControl(int socketNum, char *handle) {
	int pollReturn;

	// waits for return
	pollReturn = pollCall(-1);

	if (pollReturn == STDIN_FILENO) {
		processStdin(socketNum, handle);
	}
	else {
		processMsgFromServer(socketNum);
	}
}

void processStdin(int socketNum, char *handle) {
	uint8_t buffer[MAXLEN];
	char *command;
	int inputLen;

	inputLen = readFromStdin(buffer);

	if (inputLen < 0) {
		printf("Error: Input too long\n");

		// print prompt
		printf("$: ");
		fflush(stdout);

		return;
	}

	// get commanad from input buffer
	command = strtok((char *) buffer, " ");

	// process command
	if (command != NULL) {
		processCommand(socketNum, command, buffer, handle);
	}
	else {
		printf("Please enter a command\n");
		printf("$: ");
		fflush(stdout);
	}

}

void processCommand(int socketNum, char *command, uint8_t *buffer, char *handle) {
	if ((strcmp(command, "%M") == 0) || (strcmp(command, "%m") == 0)) {
		processM(socketNum, buffer, handle);
	}
	else if ((strcmp(command, "%C") == 0) || (strcmp(command, "%c") == 0)) {
		processC(socketNum, buffer, handle);
	}
	else if ((strcmp(command, "%B") == 0) || (strcmp(command, "%b") == 0)) {
		processB(socketNum, buffer, handle);
	}
	else if ((strcmp(command, "%L") == 0) || (strcmp(command, "%l") == 0)) {
		processL(socketNum, buffer);
	}
	else {
		printf("Invalid command\n");
		printf("$: ");
		fflush(stdout);
	}
}

void processM(int socketNum, uint8_t *buffer, char *handle) {
	char *destHandle;
	char *message;
	int msgLen;

	// get destHandle
	destHandle = strtok(NULL, " ");

	// check if handle name is entered
	if (destHandle == NULL) {
		printf("usage: %%M destination-handle [text]\n");
		printf("$: ");
		fflush(stdout);
		return;
	}

	// check if handle name is > 100 characters
	if (strlen(destHandle) > 100) {
		printf("Invalid handle, handle longer than 100 characters: %s\n", handle);
		printf("$: ");
	fflush(stdout);
		return;
	}

	// check for empty message
	message = strtok(NULL, "\n");
	if (message == NULL) {
		message = "";
	}

	// get msgLen
	msgLen = strlen(message);

	// check if message len is <= 199 characters (not including null)
	if (msgLen <= 199) {
		sendFlag5(socketNum, message, handle, destHandle);
	}
	// break up messages and send as needed
	else {
		while (msgLen > 199) {
			char temp[200];

			// copy 199 bytes of user data into temp
			strncpy(temp, message, 199);

			// null terminate
			temp[199] = '\0';

			// send 200 byte total chunk
			sendFlag5(socketNum, temp, handle, destHandle);

			// update message pointer
			message = &message[199];

			// update remaining msgLen
			msgLen = strlen(message);
		}

		// check for remaining message
		if (msgLen > 0) {
			char temp[msgLen + 1];

			// copy remaining message
			strncpy(temp, message, msgLen);

			// null terminate
			temp[msgLen] = '\0';

			// send rest of message
			sendFlag5(socketNum, temp, handle, destHandle);
		}
	}

	printf("$: ");
	fflush(stdout);
}

void processC(int socketNum, uint8_t *buffer, char *srcHandle) {
	uint8_t handles;
	char *handles_string;
	char *message;
	int msgLen;
	char *destHandles[9];

	// get number of destinations while checking for valid inputs
	handles_string = strtok(NULL, " ");
	if (handles_string == NULL) {
		printf("usage: %%C num-handles destination-handle [destination-handle] [text]\n");
		printf("$: ");
		fflush(stdout);
		return;
	}
	handles = (uint8_t) atoi(handles_string);
	if ((handles < 2) || (handles > 9)) {
		printf("Please enter a number between 2-9\n");
		printf("$: ");
		fflush(stdout);
		return;
	}

	// get all destHandles
	for (int i = 0; i < handles; i++) {
		destHandles[i] = strtok(NULL, " ");

		if (destHandles[i] == NULL) {
			printf("Missing handle names\n");
			printf("$: ");
			fflush(stdout);
			return;
		}
	}

	// get msgLen
	message = strtok(NULL, "\n");
	if (message == NULL) {
		message = "";
	}
	msgLen = strlen(message);

	// check if message len is <= 199 characters (not including null)
	if (msgLen <= 199) {
		sendFlag6(socketNum, message, srcHandle, destHandles, handles);
	}
	// break up messages and send as needed
	else {
		while (msgLen > 199) {
			char temp[200];

			// copy 199 bytes of user data into temp
			strncpy(temp, message, 199);

			// null terminate
			temp[199] = '\0';

			// send 200 byte total chunk
			sendFlag6(socketNum, temp, srcHandle, destHandles, handles);

			// update message pointer
			message = &message[199];

			// update remaining msgLen
			msgLen = strlen(message);
		}

		// check for remaining message
		if (msgLen > 0) {
			char temp[msgLen + 1];

			// copy remaining message
			strncpy(temp, message, msgLen);

			// null terminate
			temp[msgLen] = '\0';

			// send rest of message
			sendFlag6(socketNum, temp, srcHandle, destHandles, handles);
		}
	}

	printf("$: ");
	fflush(stdout);
}

void processB(int socketNum, uint8_t *buffer, char *handle) {
	char *message;
	int msgLen;

	// check for empty message
	message = strtok(NULL, "\n");
	if (message == NULL) {
		message = "";
	}

	// get msgLen
	msgLen = strlen(message);

	// check if message len is <= 199 characters (not including null)
	if (msgLen <= 199) {
		sendFlag4(socketNum, message, handle);
	}
	// break up messages and send as needed
	else {
		while (msgLen > 199) {
			char temp[200];

			// copy 199 bytes of user data into temp
			strncpy(temp, message, 199);

			// null terminate
			temp[199] = '\0';

			// send 200 byte total chunk
			sendFlag4(socketNum, temp, handle);

			// update message pointer
			message = &message[199];

			// update remaining msgLen
			msgLen = strlen(message);
		}

		// check for remaining message
		if (msgLen > 0) {
			char temp[msgLen + 1];

			// copy remaining message
			strncpy(temp, message, msgLen);

			// null terminate
			temp[msgLen] = '\0';

			// send rest of message
			sendFlag4(socketNum, temp, handle);
		}
	}

	printf("$: ");
	fflush(stdout);
}

void processL(int socketNum, uint8_t *buffer) {
	// 1 byte flag
	uint8_t PDU[1];

	// send packet with flag = 10
	insertFlag(PDU, 10, 0);
	sendPDU(socketNum, PDU, 1);
}

void processMsgFromServer(int socketNum) {
	uint8_t buffer[MAXLEN];
	int recvBytes = 0;
	uint8_t flag;

	recvBytes = recvPDU(socketNum, buffer, MAXLEN);

	flag = buffer[0];
	processFlag(socketNum, flag, buffer);

	if (recvBytes <= 0) {
		printf("\nServer has terminated.\n");
		exit(0);
	}
}

int readFromStdin(uint8_t *buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	
	while (inputLen < (MAXLEN - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}

	// return -1 if no '\n' detected at end of buffer (message too long)
	if (aChar != '\n') {
		// discard rest of message
		while ((aChar = getchar()) != '\n' && aChar != EOF);

        return -1;
    }
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

// upon initial connection to server, send flag = 1 packet and block for response
void initialize(int socketNum, char *handle) {
    uint8_t handleLen;
    int pollReturn;
    int flag = 0;
    int dataLen;
    uint8_t receivedData[MAXLEN];

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
            recvPDU(socketNum, receivedData, MAXLEN);

            // Get flag byte from PDU
            flag = getFlag(receivedData, 0);
        }
    }

    // If the handle is already in use, print an error and exit
    if (flag == 3) {
        printf("Handle already in use: %s\n", handle);
		exit(1);
    }
}

void processFlag(int socketNum, uint8_t flag, uint8_t *buffer) {
	switch (flag) {
		case (4):
			flag4(&buffer[1]);
			break;

		case (5):
			flag5(&buffer[1]);
			break;

		case (6):
			flag6(&buffer[1]);
			break;

		case (7):
			flag7(socketNum, &buffer[1]);
			break;

		case (11):
			flag11(socketNum, &buffer[1]);
			break;

		case (12):
			flag12(&buffer[1]);
			break;

		case (13):
			flag13();
			break;

		default:
			return;
	}
}

// broadcasts flag = 4 packet
void sendFlag4(int socketNum, char *message, char *handle) {
	int handleLen;
	int msgLen;
	int offset;
	int dataLen;

	// get handleLen
	handleLen = strlen(handle);

	// get msgLen
	msgLen = strlen(message);

	// 1 byte flag + 1 byte handleLen + handle + msg + null char
	dataLen = 1 + 1 + handleLen + msgLen + 1;
	uint8_t PDU[dataLen];

	// insert flag = 4
	offset = insertFlag(PDU, 4, 0);

	// insert handleLen + handle
	offset = insertHandle(handle, PDU, offset);

	// insert message
	memcpy(&PDU[offset], message, msgLen);

	// null terminate
	PDU[dataLen - 1] = '\0';

	// send complete flag = 4 packet to server
	sendPDU(socketNum, PDU, dataLen);
}

// process flag = 4 packet
void flag4(uint8_t *buffer) {
	char srcHandle[100];
	int offset;

	// get srcHandle
	offset = getHandle(srcHandle, buffer, 0);
	
	// get msgLen by finding null byte (start at offset)
	int msgLen = 0;
	while (buffer[msgLen + offset] != '\0') {
		msgLen++;
	}

	// get message (null terminated)
	char message[msgLen + 1];
	memcpy(message, (char *) &buffer[offset], msgLen);
	message[msgLen] = '\0';

	printf("\n%s: %s\n", srcHandle, message);
	printf("$: ");
	fflush(stdout);
}

// sends flag = 5 packet to server / other clients
void sendFlag5(int socketNum, char *message, char *srcHandle, char *destHandle) {
	int srcLen;
	int destLen;
	int msgLen;
	int srcOffset;
	int destOffset;
	int dataLen;

	// get lengths of data
	srcLen = strlen(srcHandle);
	destLen = strlen(destHandle);
	msgLen = strlen(message);

	// 1 byte flag + 1 byte srcLen + srcLen + 1 byte destination num + 1 byte destLen + destLen + msg + '\0' char
	dataLen = 1 + 1 + srcLen + 1 + 1 + destLen + msgLen + 1;
	uint8_t PDU[dataLen];

	// insert flag = 5 into PDU
	insertFlag(PDU, 5, 0);

	// insert srcLen into PDU
	PDU[1] = srcLen;

	// insert srcHandle into PDU
	srcOffset = insertHandle(srcHandle, PDU, 1);

	// insert byte indicating # of destinations (1)
	PDU[srcOffset] = 1;

	// insert destHandle into PDU (skip destinations byte for offset)
	destOffset = insertHandle(destHandle, PDU, srcOffset + 1);

	// insert message into PDU
	memcpy(&PDU[destOffset], message, msgLen);

	// insert '\0' char
	PDU[dataLen - 1] = '\0';

	// send complete flag = 5 packet to server
	sendPDU(socketNum, PDU, dataLen);
}

// process flag = 5 packet from server
void flag5(uint8_t *buffer) {
	char destHandle[100];
	char srcHandle[100];
	int srcOffset;
	int destOffset;

	// check if destHandle exists
	srcOffset = getHandle(srcHandle, buffer, 0);

	// account for destination byte
	destOffset = getHandle(destHandle, buffer, srcOffset + 1);
	
	// get msgLen by finding null byte (start at destOffset)
	int msgLen = 0;
	while (buffer[msgLen + destOffset] != '\0') {
		msgLen++;
	}

	// get message (null terminated)
	char message[msgLen + 1];
	memcpy(message, (char *) &buffer[destOffset], msgLen);
	message[msgLen] = '\0';

	printf("\n%s: %s\n", srcHandle, message);
	printf("$: ");
	fflush(stdout);
}

// sends flag = 6 packet to server / other clients
void sendFlag6(int socketNum, char *message, char *srcHandle, char *destHandles[], uint8_t handles) {
	int dataLen;
	int offset;
	int msgLen = strlen(message);

	// get dataLen to create PDU (1 byte flag + 1 byte srcLen + scrHandle + 1 byte handles + message + '\0')
	dataLen = 1 + 1 + strlen(srcHandle) + 1 + msgLen + 1;
	for (int i = 0; i < handles; i++) {
		// add 1 byte destHandle len + destHandle
		dataLen = dataLen + 1 + strlen(destHandles[i]);
	}
	uint8_t PDU[dataLen];

	// insert flag = 6
	offset = insertFlag(PDU, 6, 0);

	// insert srcHandle
	offset = insertHandle(srcHandle, PDU, offset);

	// insert handles byte
	PDU[offset] = handles;
	offset++;

	// insert each handle
	for (int i = 0; i < handles; i++) {
		offset = insertHandle(destHandles[i], PDU, offset);
	}

	// insert message
	memcpy(&PDU[offset], message, msgLen);

	// null terminate
	PDU[offset + msgLen] = '\0';

	// send PDU to server
	sendPDU(socketNum, PDU, dataLen);
}

// process flag = 6 packet from server
void flag6(uint8_t *buffer) {
	char handle[100];
	int offset;
	uint8_t handles;
	char temp[100];
	
	// get handle name
	offset = getHandle(handle, buffer, 0);

	// get number of handles
	handles = buffer[offset];
	offset++;

	// go past all handle names
	for (int i = 0; i < handles; i++) {
		offset = getHandle(temp, buffer, offset);
	}

	// get msgLen by finding null byte (start at offset)
	int msgLen = 0;
	while (buffer[msgLen + offset] != '\0') {
		msgLen++;
	}

	// get message (null terminated)
	char message[msgLen + 1];
	memcpy(message, (char *) &buffer[offset], msgLen);
	message[msgLen] = '\0';

	printf("\n%s: %s\n", handle, message);
	printf("$: ");
	fflush(stdout);
}

// process flag = 7 packet from server
void flag7(int socketNum, uint8_t *buffer) {
	char handle[100];
	
	// get handle name
	getHandle(handle, buffer, 0);

	printf("\nClient with handle %s does not exist\n", handle);
	printf("$: ");
	fflush(stdout);
}

// process flag = 11 packet from server
void flag11(int socketNum, uint8_t *buffer) {
	int handles;
	int handles_ho;

	// get num handles from buffer (currently in network order)
	memcpy(&handles, buffer, 4);

	// get handles in host order and print
	handles_ho = ntohl(handles);
	printf("Number of clients: %d\n", handles_ho);
}

// process flag = 12 packets from server
void flag12(uint8_t *buffer) {
    char handle[100];

	// get handle from packet and print
    getHandle(handle, buffer, 0);
    printf("%s\n", handle);
}

// process flag = 13 packet from server
void flag13() {
	// print prompt
	printf("$: ");
	fflush(stdout);
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}

	// check handle length
	if (strlen(argv[1]) > 100) {
		printf("Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(1);
	}
}
