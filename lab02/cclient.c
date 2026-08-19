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

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);
void processStdin(int socketNum);
void processMsgFromServer(int socketNum);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);

	while (1) {
		clientControl(socketNum);
	}
	
	close(socketNum);
	
	return 0;
}

void clientControl(int socketNum) {
	int pollReturn;

	// create poll set
	setupPollSet();

	// add socketNum and STDIN to poll set
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	// waits for return
	pollReturn = pollCall(-1);

	if (pollReturn == STDIN_FILENO) {
		processStdin(socketNum);
	}
	else {
		processMsgFromServer(socketNum);
	}
}

void processStdin(int socketNum) {
	uint8_t buffer[MAXBUF];
	int sendLen = 0;
	int sent = 0;
	
	sendLen = readFromStdin(buffer);
	printf("read: %s string len: %d (including null)\n", buffer, sendLen);
	
	sent = sendPDU(socketNum, buffer, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Socket:%d: Sent, Length: %d msg: %s\n", socketNum, sent, buffer);
}

void processMsgFromServer(int socketNum) {
	uint8_t buffer[MAXBUF];
	int recvBytes = 0;

	recvBytes = recvPDU(socketNum, buffer, MAXBUF);

	if (recvBytes > 0) {
		printf("Socket %d: Byte recv: %d message: %s\n", socketNum, recvBytes, buffer);
	}
	else {
		printf("Server has terminated.\n");
		exit(0);
	}
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
