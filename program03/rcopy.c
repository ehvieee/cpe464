// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

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
#include <fcntl.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "cpe464.h"
#include "pollLib.h"
#include "buffer.h"

#define MAXBUF 1407

// states typedef 
typedef enum State STATE;
enum State {
	WAIT_OK, FILE_TRANSFER, IN_ORDER, BUFFERING, FLUSHING, SEND_EOF_ACK, DONE
};

// function declarations
void talkToServer(int socketNum, struct sockaddr_in6 *server, char *from_filename, char *to_filename, char *hostname, int portNumber, char *bufferSize, char *windowSize);
int checkArgs(int argc, char * argv[]);
int restartSocket(int socketNum, struct sockaddr_in6 *server, char *hostname, int portNumber);
STATE wait_ok(int socketNum, struct sockaddr_in6 *server, char *hostname, int portNumber, char *from_filename, char *bufferSize, char *windowSize);
STATE file_transfer(int socketNum, struct sockaddr_in6 *server, int bufferSize, char *to_filename);
STATE in_order(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd);
STATE buffering(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd);
STATE flushing(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd);
STATE send_eof_ack(int socketNum, struct sockaddr_in6 *server);
void writePacket(int fd, uint8_t *PDU, int pduLen);
void respond(int socketNum, struct sockaddr_in6 *server, int flag, uint32_t sequenceNum);

int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	float errorRate = 0.0;
	
	// get port number
	portNumber = checkArgs(argc, argv);
	
	// set up socket and poll
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	setupPollSet();
	addToPollSet(socketNum);
	
	// set up error rate
	errorRate = atof(argv[5]);
	sendErr_init(errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	// start program
	talkToServer(socketNum, &server, argv[1], argv[2], argv[6], portNumber, argv[4], argv[3]);
	
	// close socket
	close(socketNum);

	return 0;
}

int restartSocket(int socketNum, struct sockaddr_in6 *server, char *hostname, int portNumber) {
	// remove socket from poll
	removeFromPollSet(socketNum);

	// close socket
	close(socketNum);

	// reopen new socket
	socketNum = setupUdpClientToServer(server, hostname, portNumber);
	addToPollSet(socketNum);

	return socketNum;
}

void talkToServer(int socketNum, struct sockaddr_in6 *server, char *from_filename, char *to_filename, char *hostname, int portNumber, char *bufferSize, char *windowSize)
{
	int winSize = atoi(windowSize);
	int bufSize = atoi(bufferSize);
	setupBuffer(winSize);
	STATE current_state = WAIT_OK;

	// loop until reaching DONE state
	while (current_state != DONE) {
		switch (current_state) {
			case (WAIT_OK):
				current_state = wait_ok(socketNum, server, hostname, portNumber, from_filename, bufferSize, windowSize);
				break;

			case (FILE_TRANSFER):
				current_state = file_transfer(socketNum, server, bufSize, to_filename);
				break;

			case (DONE):
				break;

			default:
				break;
		}
	}

}

STATE wait_ok(int socketNum, struct sockaddr_in6 *server, char *hostname, int portNumber, char *from_filename, char *bufferSize, char *windowSize) {
	int count = 0;
	int socket = 0;
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int pduLen = 0;
	
	uint8_t recvBuffer[MAXBUF];
	uint16_t checksum = 0;
	uint8_t flag = 0;

	// construct filename payload (2 byte buffer size + 4 byte window size + filename + \0)
	int payloadLen = 2 + 4 + strlen(from_filename) + 1;
	uint8_t payload[payloadLen];
	uint16_t bufferBytes = htons((uint16_t) atoi(bufferSize));
	uint32_t windowBytes = htonl((uint32_t) atoi(windowSize));
	memcpy(&payload[0], &bufferBytes, 2);
	memcpy(&payload[2], &windowBytes, 4);
	memcpy(&payload[6], (uint8_t *) from_filename, strlen(from_filename) + 1);

	// put on 7 byte header with flag = 8
	uint8_t buffer[7 + payloadLen];
	pduLen = createPDU(buffer, 0, 8, payload, payloadLen);

	// initial send to 
	safeSendto(socketNum, buffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);

	// begin count loop
	while (count < 9) {
		// poll for 1 second
		socket = pollCall(1000);

		// poll timeout
		if (socket < 0) {
			// increment count
			count++;

			// restart socket
			socketNum = restartSocket(socketNum, server, hostname, portNumber);

			// resend filename packet
			safeSendto(socketNum, buffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		}
		// received packet from server
		else {
			pduLen = safeRecvfrom(socketNum, recvBuffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);

			// grab flag and checksum
			flag = recvBuffer[6];
			checksum = in_cksum((short unsigned int *) recvBuffer, pduLen);

			// check if packet is expected flag and checksum valid
			if ((flag == 9) && (checksum == 0)) {
				// good filename response = 1
				if (recvBuffer[7] == 1) {
					// send file ok ack
					uint8_t payload[1] = {1};
					pduLen = createPDU(buffer, 0, 32, payload, 1);
					safeSendto(socketNum, buffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);

					return FILE_TRANSFER;
				}
				// bad filename response = 0
				else {
					printf("Error: file %s not found.\n", from_filename);
					return DONE;
				}
			}
		}
	}

	// timeout and counter > 9
	printf("Error: timeout.\n");
	return DONE;
}

STATE file_transfer(int socketNum, struct sockaddr_in6 *server, int bufferSize, char *to_filename) {
	STATE current_state = IN_ORDER;

	// open file for write
	int fd = 0;
	if ((fd = open(to_filename, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644)) < 0) {
		printf("Error: failed to open file.\n");
		return DONE;
	}

	// process files until receiving EOF packet or error (DONE)
	while (1) {
		switch(current_state) {
			case (IN_ORDER):
				current_state = in_order(socketNum, server, bufferSize, fd);
				break;

			case (BUFFERING):
				current_state = buffering(socketNum, server, bufferSize, fd);
				break;

			case (FLUSHING):
				current_state = flushing(socketNum, server, bufferSize, fd);
				break;

			case (SEND_EOF_ACK):
				current_state = send_eof_ack(socketNum, server);
				break;

			case (DONE):
				return DONE;
				break;

			default:
				break;
		}
	}

	return DONE;
}

// process in order packets
STATE in_order(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd) {
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int expected = 0;
	int socket = 0;
	int recvBytes = 0;
	uint8_t recvBuffer[bufferSize + 7];
	uint32_t recvNum = 0;
	uint32_t recvNum_no = 0;
	uint16_t checksum = 0;

	// timeout after 10 seconds
	socket = pollCall(10000);
	if (socket < 0) {
		printf("Error: timeout.\n");
		return DONE;
	}

	// receive PDU and check sequenceNum
	recvBytes = safeRecvfrom(socketNum, recvBuffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
	memcpy(&recvNum_no, &recvBuffer[0], 4);
	recvNum = ntohl(recvNum_no);
	checksum = in_cksum((short unsigned int *) recvBuffer, recvBytes);

	// checksum
	if (checksum != 0) {
		return IN_ORDER;
	}

	// write packet if received == expected and send RR packet
	expected = getExpected();
	if (recvNum == expected) {
		writePacket(fd, recvBuffer, recvBytes);
		setInvalid(recvNum);
		updateHighest(expected);
		expected = updateExpected(recvNum + 1);
		respond(socketNum, server, 5, expected);

		// check if received packet contains EOF flag
		if (recvBuffer[6] == 10) {
			return SEND_EOF_ACK;
		}
		
		return IN_ORDER;
	}
	// if received > expected, SREJ expected and go to buffering state
	else if (recvNum > expected) {
		if (!checkValid(recvNum)) {
			respond(socketNum, server, 6, expected);
			insertBuffer(recvNum, recvBuffer, recvBytes);
			updateHighest(recvNum);
			return BUFFERING;
		}

		return IN_ORDER;
	}
	// else, send RR
	else {
		respond(socketNum, server, 5, expected);
		return BUFFERING;
	}
}

// buffer out of order packets
STATE buffering(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd) {
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int expected = 0;
	int socket = 0;
	int recvBytes = 0;
	uint8_t recvBuffer[bufferSize + 7];
	uint32_t recvNum = 0;
	uint32_t recvNum_no = 0;
	uint16_t checksum = 0;

	// timeout after 10 seconds
	socket = pollCall(10000);
	if (socket < 0) {
		printf("Error: timeout.\n");
		return DONE;
	}
	
	// receive PDU and check sequenceNum
	recvBytes = safeRecvfrom(socketNum, recvBuffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
	memcpy(&recvNum_no, &recvBuffer[0], 4);
	recvNum = ntohl(recvNum_no);
	checksum = in_cksum((short unsigned int *) recvBuffer, recvBytes);

	if (checksum != 0) {
		return BUFFERING;
	}

	// continue buffering if received > expected
	expected = getExpected();
	if (recvNum > expected) {
		if (!checkValid(recvNum)) {
			insertBuffer(recvNum, recvBuffer, recvBytes);
			updateHighest(recvNum);
			return BUFFERING;
		}
		
		return IN_ORDER;
	}
	// flush buffer if received == expected
	else if (recvNum == expected) {
		writePacket(fd, recvBuffer, recvBytes);
		setInvalid(recvNum);
		expected++;
		updateExpected(expected);
		respond(socketNum, server, 5, expected);
	
		// check if EOF packet
		if (recvBuffer[6] == 10) {
			return SEND_EOF_ACK;
		}

		return FLUSHING;
	}
	// resend SREJ if received < expected
	else {
		respond(socketNum, server, 6, expected);
		respond(socketNum, server, 5, expected);
		return BUFFERING;
	}
}

// flush buffer
STATE flushing(int socketNum, struct sockaddr_in6 *server, int bufferSize, int fd) {
	int expected = 0;
	uint8_t PDU[bufferSize + 7];
	int pduLen = 0;
	int sequenceNum = 0;
	int sequenceNum_no = 0;
	int valid = 1;

	// flush until missing packet or buffer empty
	while (!bufferEmpty() && valid) {
		expected = getExpected();

		// check if packet is valid
		valid = checkValid(expected);
		if (!valid) {
			return BUFFERING;
		}

		// grab packet from buffer
		pduLen = getBuffer(expected, PDU);
		memcpy(&sequenceNum_no, &PDU[0], 4);
		sequenceNum = ntohl(sequenceNum_no);

		// wrong sequenceNum (missing data)
		if (sequenceNum != expected) {
			// send SREJ and RR packets
			respond(socketNum, server, 6, expected);
			respond(socketNum, server, 5, expected);

			// go back to buffering
			return BUFFERING;
		}

		// write packet to disk and increment expected
		writePacket(fd, PDU, pduLen);
		setInvalid(sequenceNum);
		expected = updateExpected(sequenceNum + 1);

		// check if last packet is flushed
		if (PDU[6] == 10) {
			return SEND_EOF_ACK;
		}

		// buffer empty; send RR and go to in order state
		if (bufferEmpty()) {
			respond(socketNum, server, 5, expected);
			return IN_ORDER;
		}
	}

	return IN_ORDER;
}

// send EOF ACK packet flag = 33
STATE send_eof_ack(int socketNum, struct sockaddr_in6 *server) {
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t payload[1] = {1};
	uint8_t sendBuffer[8];
	int pduLen;

	// send flag = 33 packet
	payload[0] = 1;
	pduLen = createPDU(sendBuffer, 0, 33, payload, 1);
	safeSendto(socketNum, sendBuffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);

	return DONE;
}

// write packet data to disk
void writePacket(int fd, uint8_t *PDU, int pduLen) {
	int dataLen = pduLen - 7;
	uint8_t data[dataLen];

	// extract payload from PDU
	memcpy(&data[0], &PDU[7], dataLen);

	// write payload to file
	write(fd, data, dataLen);
}

// send RR / SREJ response to server
void respond(int socketNum, struct sockaddr_in6 *server, int flag, uint32_t sequenceNum) {
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t sendBuffer[11];
	int pduLen = 0;
	uint8_t payload[4];
	uint32_t sequenceNum_no = 0;

	// build response packet with sequenceNum + 1 as data
	sequenceNum_no = htonl(sequenceNum);
	memcpy(&payload[0], &sequenceNum_no, 4);
	pduLen = createPDU(sendBuffer, 0, flag, payload, 4);

	// send packet
	safeSendto(socketNum, sendBuffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
}

int checkArgs(int argc, char * argv[])
{

	int portNumber = 0;

	/* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port \n", argv[0]);
		exit(1);
	}

	if ((strlen(argv[1]) > 100) || (strlen(argv[2]) > 100)) {
		printf("Error: filenames must be less than 100 characters.\n");
		exit(1);
	}
	
	portNumber = atoi(argv[7]);
		
	return portNumber;
}





