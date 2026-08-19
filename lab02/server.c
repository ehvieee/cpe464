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

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainServerSocket);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	// wait for client to connect
	// clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

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
	struct sockaddr_in6 clientAddress;   
	int clientAddressSize = sizeof(clientAddress);
	int clientSocket = 0;

	clientSocket = accept(mainServerSocket, (struct sockaddr*) &clientAddress, (socklen_t *) &clientAddressSize);
	if (clientSocket < 0) {
		perror("accept error");
		exit(-1);
	}
	
	addToPollSet(clientSocket);
}

void processClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;

	// receive message from client
	messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF);

	// send message back to client
	if (messageLen > 0)
	{
		printf("Socket %d: Message received, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);
		
		messageLen = sendPDU(clientSocket, dataBuffer, messageLen);
		printf("Socket %d: msg sent: %d bytes, text: %s\n", clientSocket, messageLen, dataBuffer);
	}
	else
	{
		printf("Socket %d: Connection closed by other side\n", clientSocket);

		// close and remove socket from poll set
		close(clientSocket);
		removeFromPollSet(clientSocket);
	}
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

