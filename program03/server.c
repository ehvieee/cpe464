/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "cpe464.h"
#include "pollLib.h"
#include "window.h"

#define MAXBUF 1407

// states typedef 
typedef enum State STATE;
enum State {
	FILENAME, WAIT_OK_ACK, FILE_TRANSFER, WAIT_EOF_ACK, DONE
};

void processClient(int socketNum);
int checkArgs(int argc, char *argv[]);
void childProcess(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen);
STATE open_file(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen, int *fd, uint16_t *bufferSize);
STATE wait_ok_ack(int socketNum, struct sockaddr_in6 client, int *fd, uint16_t *bufferSize);
STATE file_transfer(int socketNum, struct sockaddr_in6 client, int fd, uint16_t bufferSize);
STATE wait_eof_ack(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize);
void resendLowest(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize);
uint8_t processResponse(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize);

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;
	float errorRate = 0.0;

	portNumber = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(portNumber);

	errorRate = atof(argv[1]);
	sendErr_init(errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	int dataLen = 0; 
	char buffer[MAXBUF];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);	
	pid_t pid = 0;
	
	// infinitely process clients
	while (1) {
		dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
		// fork child
		if ((pid = fork()) < 0) {
			perror("fork");
			exit(-1);
		}

		// child process
		if (pid == 0) {
			printf("Child fork() - child pid: %d\n", getpid());
			printIPInfo(&client);

			// close main socket
			close(socketNum);

			// create new socket with random port
			socketNum = udpServerSetup(0);
			setupPollSet();
			addToPollSet(socketNum);
			childProcess(socketNum, client, (uint8_t *) buffer, dataLen);
			exit(0);
		}

	}
}

void childProcess(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen) {
	STATE current_state = FILENAME;
	int fd = 0;
	uint16_t bufferSize = 0;

	while (current_state != DONE) {
		switch (current_state) {
			case (FILENAME):
				current_state = open_file(socketNum, client, buffer, dataLen, &fd, &bufferSize);
				break;

			case (WAIT_OK_ACK):
				current_state = wait_ok_ack(socketNum, client, &fd, &bufferSize);
				break;

			case (FILE_TRANSFER):
				current_state = file_transfer(socketNum, client, fd, bufferSize);
				break;

			case (WAIT_EOF_ACK):
				current_state = wait_eof_ack(socketNum, client, bufferSize);
				break;

			case (DONE):
				break;

			default:
				break;
		}
	}

}

STATE open_file(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen, int *fd, uint16_t *bufferSize) {
	char filename[100];
	uint16_t bufferSize_no = 0;
	uint32_t windowSize_no = 0;
	int pduLen = 0;
	uint8_t sendBuffer[MAXBUF];
	
	// get filename, window size, buffer size
	memcpy(&bufferSize_no, &buffer[7], 2);
	memcpy(&windowSize_no, &buffer[9], 4);
	*bufferSize = ntohs(bufferSize_no);
	uint32_t windowSize = ntohl(windowSize_no);
	memcpy(filename, &buffer[13], dataLen - 13);

	// set up window
	setupWindow(windowSize);

	// create response payload
	uint8_t payload[1];

	// failed to open file
	if ((*fd = open(filename, O_RDONLY)) < 0) {
		// send response = 0 payload
		payload[0] = 0;
		pduLen = createPDU(sendBuffer, 0, 9, payload, 1);
		safeSendto(socketNum, sendBuffer, pduLen, 0, (struct sockaddr *) &client, sizeof(client));

		return DONE;
	}
	// otherwise, wait on OK ACK
	else {
		// send response = 1 payload
		payload[0] = 1;
		pduLen = createPDU(sendBuffer, 0, 9, payload, 1);
		safeSendto(socketNum, sendBuffer, pduLen, 0, (struct sockaddr *) &client, sizeof(client));

		return WAIT_OK_ACK;
	}
}

STATE wait_ok_ack(int socketNum, struct sockaddr_in6 client, int *fd, uint16_t *bufferSize) {
	int socket = 0;
	uint8_t recvBuffer[MAXBUF];
	int clientAddrLen = sizeof(client);

	// poll for 10 seconds
	socket = pollCall(10000);
	
	// exit on timeout
	if (socket < 0) {
		printf("Error: timeout.\n");
		return DONE;
	}

	// receive ok ack PDU
	safeRecvfrom(socketNum, recvBuffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);

	// begin file transfer
	return FILE_TRANSFER;
}

STATE file_transfer(int socketNum, struct sockaddr_in6 client, int fd, uint16_t bufferSize) {
	ssize_t bytesRead = 1;
	uint8_t dataBuffer[bufferSize];
	uint32_t sequenceNum = 0;
	uint8_t PDU[bufferSize + 7];
	int pduLen = 0;
	int count = 0;
	int socket = 0;
	int flag = 16;
	int open = 0;
	
	// loop until reaching EOF
	while (bytesRead > 0) {
		// send packets when window is open
		while ((open = windowOpen()) && (bytesRead > 0)) {
			flag = 16;

			// read from file (set flag = 10 EOF packet if last packet)
			bytesRead = read(fd, dataBuffer, bufferSize);

			// set flag = 10 if EOF packet
			if (bytesRead < bufferSize) {
				flag = 10;
			}

			if (bytesRead == 0) {
				break;
			}

			// construct and send flag = 16 or flag = 10 data packet
			pduLen = createPDU(PDU, sequenceNum, flag, dataBuffer, bytesRead);
			safeSendto(socketNum, PDU, pduLen, 0, (struct sockaddr *) &client, sizeof(client));

			// buffer packet in window
			insertWindow(sequenceNum, PDU, pduLen);
			incrementCurrent();

			// increment sequenceNum
			sequenceNum++;

			// non-blocking poll for RR or SREJ packets
			while ((socket = pollCall(0)) > 0) {
				flag = processResponse(socketNum, client, bufferSize);

				if (flag == 33) {
					return DONE;
				}
			}

			if (bytesRead < bufferSize) {
				break;
			}

		}

		// reset count when window closes
		count = 0;
		while (!(open = windowOpen()) && (bytesRead > 0)) {
			// poll for 1 second
			socket = pollCall(1000);

			// on timeout resend lowest packet and increment count
			if (socket < 0) {
				resendLowest(socketNum, client, bufferSize);
				count++;
			}
			// on response, process RR or SREJ packet and reset count
			else {
				flag = processResponse(socketNum, client, bufferSize);

				if (flag == 33) {
					return DONE;
				}

				count = 0;
			}
	
			if (count > 9) {
				printf("Error: timeout.\n");
				return DONE;
			}
		}
	}

	// close file and wait for EOF ACK
	close(fd);
	return WAIT_EOF_ACK;
}

STATE wait_eof_ack(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize) {
	int count = 0;
	int flag = 0;
	int socket = 0;

	// process all outstanding RR / SREJ packets (10 second timeout)
	while (count < 10) {
		socket = pollCall(1000);

		if (socket < 0) {
			resendLowest(socketNum, client, bufferSize);
			count++;
		}
		else {
			flag = processResponse(socketNum, client, bufferSize);

			// check if it has received EOF ACK packet
			if (flag == 33) {
				return DONE;
			}

			count = 0;
		}
	}

	printf("Error: timeout.\n");
	return DONE;
}

// resend lowest packet in window with flag = 18
void resendLowest(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize) {
	int pduLen = 0;
	uint8_t PDU[bufferSize + 7];

	pduLen = getLowest(PDU);

	safeSendto(socketNum, PDU, pduLen, 0, (struct sockaddr *) &client, sizeof(client));
}

// process RR / SREJ / EOF ACK packets from client
uint8_t processResponse(int socketNum, struct sockaddr_in6 client, uint16_t bufferSize) {
	int clientAddrLen = sizeof(client);
	uint8_t recvBuffer[11];
	uint32_t recvSeqNum_no = 0;
	uint32_t recvSeqNum = 0;
	uint8_t dataBuffer[bufferSize];
	uint8_t PDU[bufferSize + 7];
	int pduLen = 0;
	int checksum = 0;
	uint8_t flag = 0;

	// receive response
	pduLen = safeRecvfrom(socketNum, recvBuffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);

	// check checksum
	checksum = in_cksum((short unsigned int *) recvBuffer, pduLen);

	if (checksum != 0) {
		return -1;
	}
	
	flag = recvBuffer[6];

	// check if RR / SREJ packet
	if ((flag == 5) || (flag == 6)) {
		memcpy(&recvSeqNum_no, &recvBuffer[7], 4);
		recvSeqNum = ntohl(recvSeqNum_no);
	}

	// RR flag = 5
	if (flag == 5) {
		processRR(recvSeqNum);
	}
	// SREJ flag = 6
	else if (flag == 6) {
		// get packet and change flag to 17 (need to recalculate checksum)
		pduLen = getWindow(recvSeqNum, PDU);
		memcpy(&dataBuffer[0], &PDU[7], pduLen - 7);
		flag = 17;

		// if SREJ EOF, keep flag = 10
		uint8_t temp = 0;
		memcpy(&temp, &PDU[6], 1);

		if (temp == 10) {
			flag = temp;
		}

		pduLen = createPDU(PDU, recvSeqNum, flag, dataBuffer, pduLen - 7);

		safeSendto(socketNum, PDU, pduLen, 0, (struct sockaddr *) &client, sizeof(client));
	}

	return flag;
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if ((argc > 3) || (argc < 2))
	{
		fprintf(stderr, "Usage %s error-rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}


