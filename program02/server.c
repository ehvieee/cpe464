/******************************************************************************
* myServer.c
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
#include "handles.h"
#include "functions.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainServerSocket);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void processFlag(int clientSocket, int flag, uint8_t *dataBuffer, int msgLen);
void flag1(int clientSocket, uint8_t *dataBuffer);
void flag4(int clientSocket, uint8_t *dataBuffer, int msgLen);
void flag5(int clientSocket, uint8_t *dataBuffer, int msgLen);
void flag6(int clientSocket, uint8_t *dataBuffer, int msgLen);
void flag7(int clientSocket, char *destHandle);
void flag11(int clientSocket);
void flag12(int clientSocket);
void flag13(int clientSocket);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	// setup handle table
	setup_table();

	serverControl(mainServerSocket);
	
	/* close the sockets */
	close(mainServerSocket);
	
	return 0;
}

void serverControl(int mainServerSocket) {
	int pollReturn;

	// create poll set
	setupPollSet();

	// add main server to set
	addToPollSet(mainServerSocket);

	// infinite loop
	while (1) {
		// waits for return
		pollReturn = pollCall(-1);
		
		// addNewSocket if main server socket is returned
		if (pollReturn == mainServerSocket) {
			addNewSocket(pollReturn);
		}
		// processClient if client socket is returned
		else {
			processClient(pollReturn);
		}
	}
}

void addNewSocket(int mainServerSocket) {
	int clientSocket = 0;

	// accept client
	clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

	// check for errors
	if (clientSocket < 0) {
		perror("accept error");
		exit(-1);
	}

	// add client to poll set	
	addToPollSet(clientSocket);
}

void processClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	int msgLen = 0;
	int flag;

	// receive message from client
	msgLen = recvPDU(clientSocket, dataBuffer, MAXBUF);

	// send message back to client
	if (msgLen > 0)
	{
		flag = dataBuffer[0];
		processFlag(clientSocket, flag, dataBuffer, msgLen);
	}
	else
	{
		// close and remove socket from poll set and handle table
		removeFromPollSet(clientSocket);
		remove_handle(clientSocket);
		close(clientSocket);
	}
}

// calls necessary function depending on flag value
void processFlag(int clientSocket, int flag, uint8_t *dataBuffer, int msgLen) {
	switch (flag) {
		case (1):
			flag1(clientSocket, &dataBuffer[1]);
			break;

		case (4):
			flag4(clientSocket, dataBuffer, msgLen);
			break;

		case (5):
			flag5(clientSocket, dataBuffer, msgLen);
			break;

		case (6):
			flag6(clientSocket, dataBuffer, msgLen);
			break;

		case (10):
			flag11(clientSocket);
			flag12(clientSocket);
			break;
	}
}

// process flag = 1 (client connection) packet
void flag1(int clientSocket, uint8_t *dataBuffer) {
	int handleLen;
	handleLen = dataBuffer[0];

	// need to account for null char
	char handle[handleLen + 1];

	// copy handle into buffer
	memcpy(handle, &dataBuffer[1], handleLen);

	// null terminate to ensure table stores strings properly
	handle[handleLen] = '\0';

	// check if handle is already in table
	if (lookup_handle(handle) < 0) {
		// add handle in table
		add_handle(handle, clientSocket);

		// send flag = 2 packet
		sendFlag(clientSocket, 2);
	}
	else {
		// send flag = 3 packet
		sendFlag(clientSocket, 3);
	}
}

// broadcasts flag = 4 packet
void flag4(int clientSocket, uint8_t *dataBuffer, int msgLen) {
	char *handle;
	int socket;

	// iterate through each handle in table
	while ((handle = get_handle()) != NULL) {
		// get socket associated with handle
		socket = lookup_handle(handle);

		// send PDU to socket (except for original client)
		if (socket != clientSocket) {
			sendPDU(socket, dataBuffer, msgLen);
		}
	}
}

// sends flag = 5 packet to destination client
void flag5(int clientSocket, uint8_t *dataBuffer, int msgLen) {
	char destHandle[100];
	char srcHandle[100];
	int srcOffset;
	int destSocket;

	// check if destHandle exists
	srcOffset = getHandle(srcHandle, dataBuffer, 1);
	// skip destinations byte and srcHandle
	getHandle(destHandle, dataBuffer, srcOffset + 1);
	destSocket = lookup_handle(destHandle);
	
	// client doesn't exist; send flag = 7 PDU back
	if (destSocket < 0) {
		flag7(clientSocket, destHandle);
	}
	// else send flag = 5 PDU to destSocket
	else {
		sendPDU(destSocket, dataBuffer, msgLen);
	}

}

// sends flag = 6 packets to specified clients
void flag6(int clientSocket, uint8_t *dataBuffer, int msgLen) {
	char srcHandle[100];
	char destHandle[100];
	int offset;
	int destSocket;
	uint8_t handles;

	// get scrHandle
	offset = getHandle(srcHandle, dataBuffer, 1);

	// get handles
	handles = dataBuffer[offset];
	offset++;

	// send packets for each handle
	for (int i = 0; i < handles; i++) {
		// get destSocket from destHandle
		offset = getHandle(destHandle, dataBuffer, offset);
		destSocket = lookup_handle(destHandle);
		
		// client doesn't exist; send flag = 7 PDU back
		if (destSocket < 0) {
			flag7(clientSocket, destHandle);
		}
		// else send flag = 6 PDU to destSocket
		else {
			sendPDU(destSocket, dataBuffer, msgLen);
		}
	}
}

// sends flag = 7 packet to client
void flag7(int clientSocket, char *destHandle) {
	int handleLen;
	int dataLen;

	// get handleLen
	handleLen = strlen(destHandle);

	// 1 byte flag + 1 byte handleLen + handle
	dataLen = 1 + 1 + handleLen;
	uint8_t PDU[dataLen];

	// insert flag = 7
	insertFlag(PDU, 7, 0);

	// insert handle
	insertHandle(destHandle, PDU, 1);
	
	// send flag = 7 PDU to client
	sendPDU(clientSocket, PDU, dataLen);
}

// creates and sends flag = 11 packet
void flag11(int clientSocket) {
	int handles;
	int handles_no;

	// 1 byte flag + 4 byte int (network order) for number of handles
	uint8_t PDU[5];

	// insert flag = 11
	insertFlag(PDU, 11, 0);

	// insert number of handles in network order
	handles = get_table_size();
	handles_no = htonl(handles);
	memcpy(&PDU[1], &handles_no, 4);

	// send PDU to client
	sendPDU(clientSocket, PDU, 5);
}

// creates and sends flag = 12 packets
void flag12(int clientSocket) {
	char *handle;
	int handleLen;

	// send packet with flag = 12, handleLen, and handleName for each handle
	while ((handle = get_handle()) != NULL) {
		handleLen = strlen(handle);

		// 1 byte handleLen + 1 byte flag + handle
		uint8_t PDU[handleLen + 2];

		// insert flag = 12
		insertFlag(PDU, 12, 0);

		// insert handle name
		insertHandle(handle, PDU, 1);

		// send complete PDU to client
		sendPDU(clientSocket, PDU, handleLen + 2);
	}

	// send flag 13 to client
	flag13(clientSocket);
}

// sends flag = 13 packet
void flag13(int clientSocket) {
	sendFlag(clientSocket, 13);
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

