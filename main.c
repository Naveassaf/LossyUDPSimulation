#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")


#include "Receiver.h"


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

#define NUM_OF_WORKER_THREADS 2
#define MAX_INPUT_LENGTH_FROM_STDIN 100
#define PACKET_SIZE 8
#define SUMMARY_MESSAGE_LEN 100

#define NO_ERRORS_DETECTED 0
#define ERROR_CAN_BE_CORRECTED 1
#define ERROR_CANT_BE_CORRECTED 2
#define SIZE_OF_DATA_FRAME 49;
#define SIZE_OF_ENCODED_FRAME 64;

#define DATA_FRAME_FULL_BYTES 48  // The size in bits of the all full bytes in the frame
#define SUPER_FRAME_SIZE 8 // Amount of frames that has bits divisble to bytes
/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


HANDLE hThread[NUM_OF_WORKER_THREADS];

int countDetected;
int countCorrected;
int countFramesRecv;
int* DataBitsArray;
int currentLengthOfDataBitsArray;
SOCKET MainSocket;
SOCKADDR_IN service;
SOCKADDR_IN friendAddr;
int friendAddrSize;
FILE* fp;
int serverPort;

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/
int frameErrorDetector(char* bytes);
void frameErrorCorrection(char* bytes);
void addDataBitsToTheArray(char* bytes, int currentIndexInDataBitsArray);
//void printDataBitsArray(int numberOfFrames);
void writeFrameBytesToFile(FILE* fp, int startIndex);
void writeByteToFile(FILE* fp, int startIndex);

static DWORD UserThread(void)
{
	int numberOfRecvBits;
	int numberOfRecvDataBits;
	char SendStr[MAX_INPUT_LENGTH_FROM_STDIN];
	char summaryMessage[SUMMARY_MESSAGE_LEN];
	int iResult;
	
	while (1)
	{
		gets_s(SendStr, sizeof(SendStr)); //Reading a string from the keyboard
		if (STRINGS_ARE_EQUAL(SendStr, "End"))
		{
			TerminateThread(hThread[0], 0x555);
			numberOfRecvBits = countFramesRecv * SIZE_OF_ENCODED_FRAME;
			fprintf(stderr, "received: %d bytes\n", numberOfRecvBits / 8);
			numberOfRecvDataBits = countFramesRecv * SIZE_OF_DATA_FRAME;
			fprintf(stderr, "wrote: %d bytes\n", numberOfRecvDataBits / 8);
			fprintf(stderr, "detected: %d errors, corrected: %d errors\n", countDetected, countCorrected);

			sprintf(summaryMessage, "received: %d bytes\nwritten: %d bytes\ndetected: %d errors, corrected: %d errors\n", numberOfRecvBits / 8, numberOfRecvDataBits / 8, countDetected, countCorrected);
			iResult = sendto(MainSocket, summaryMessage, strlen(summaryMessage) + 1, 0, (SOCKADDR *)&friendAddr, sizeof(friendAddr));
			if (iResult == SOCKET_ERROR)
			{
				fprintf(stderr,"Server failed to send message\n");
				return 0;
			}
			return 0;
		}
	}
	return 0;
}
static DWORD RecvDataThread(void)
{
	int iResult;
	int errorsInCurrentFrame;
	int indexInDataBitsArrayWrittenToFile = 0; // Indicates the index of the next data bit to be written to the file
	char recvBuffer[PACKET_SIZE];
	int dataBitsArrayIndex = 0;
	countFramesRecv = 0;
	countDetected = 0;
	countCorrected = 0;
	currentLengthOfDataBitsArray = 0;
	friendAddrSize = sizeof(friendAddr);

	
	
	if (fp == NULL)
	{
		fprintf(stderr, "Error creating file\n");
		return 0;
	}
	
	fprintf(stderr,"Waiting for the file to be received\n");
	while (1)
	{

		iResult = recvfrom(MainSocket, recvBuffer, PACKET_SIZE, 0, &friendAddr, &friendAddrSize);
		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "Socket error while trying to write data to socket\n");
			return 0x555;
		}
		//Sleep(1);
		// Detectin errors and correcting if possible
		errorsInCurrentFrame = frameErrorDetector(recvBuffer);
		if (errorsInCurrentFrame > 0)
		{
			countDetected++;			
			if (errorsInCurrentFrame == ERROR_CAN_BE_CORRECTED)
			{
				frameErrorCorrection(recvBuffer);
				countCorrected++;
			
			}
		}


		// Adding the new frame's data bits to the int array DataBitsArray, which holds the stream of bits received (Each int represent a bit) 
		currentLengthOfDataBitsArray = dataBitsArrayIndex + SIZE_OF_DATA_FRAME;
		DataBitsArray = (int *)realloc(DataBitsArray, currentLengthOfDataBitsArray * sizeof(int));
		addDataBitsToTheArray(recvBuffer, dataBitsArrayIndex);
		dataBitsArrayIndex += SIZE_OF_DATA_FRAME;
		countFramesRecv++;
		//printDataBitsArray(countFramesRecv);
		
		
		// Adding the data from the array to the file
		writeFrameBytesToFile(fp, indexInDataBitsArrayWrittenToFile);
		indexInDataBitsArrayWrittenToFile += DATA_FRAME_FULL_BYTES;
		if (countFramesRecv % SUPER_FRAME_SIZE == 0) // Every super frame there's another byte to write (Extra bits each frame sum up to 1 byte)
		{
			writeByteToFile(fp, indexInDataBitsArrayWrittenToFile);
			indexInDataBitsArrayWrittenToFile += 8;
		}
		

	}

}

void MainServer()
{
	MainSocket = INVALID_SOCKET;
	unsigned long Address;
	int bindRes;
	// Initialize Winsock.
	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (StartupRes != NO_ERROR)
	{
		fprintf(stderr, "error %ld at WSAStartup( ), ending program.\n", WSAGetLastError());
		// Tell the user that we could not find a usable WinSock DLL.                                  
		return;
	}

	/* The WinSock DLL is acceptable. Proceed. */

	// Create a socket.    
	MainSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	if (MainSocket == INVALID_SOCKET)
	{
		fprintf(stderr, "Error at socket( ): %ld\n", WSAGetLastError());
		goto server_cleanup_1;
	}

	
	// Create a sockaddr_in object and set its values.
	// Declare variables

	Address = inet_addr(SERVER_ADDRESS_STR);
	if (Address == INADDR_NONE)
	{
		fprintf(stderr,"The string \"%s\" cannot be converted into an ip address. ending program.\n",
			SERVER_ADDRESS_STR);
		goto server_cleanup_1;
	}

	service.sin_family = AF_INET;
	service.sin_addr.s_addr = Address;
	service.sin_port = htons(serverPort); 
										   
	bindRes = bind(MainSocket, (SOCKADDR*)&service, sizeof(service));
	if (bindRes == SOCKET_ERROR)
	{
		fprintf(stderr,"bind( ) failed with error %ld. Ending program\n", WSAGetLastError());
		goto server_cleanup_1;
	}
	
	hThread[0] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);
	
	hThread[1] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)UserThread,
		NULL,
		0,
		NULL
	);
	

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	TerminateThread(hThread[0], 0x555);
	TerminateThread(hThread[1], 0x555);

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

server_cleanup_1:
	closesocket(MainSocket);


	WSACleanup();

	fclose(fp);
	free(DataBitsArray);
	return;
	
}




int main(int argc, char *argv[])
{
	fp = fopen(argv[2], "wb");
	serverPort = atoi(argv[1]);
	MainServer();
	return 0;
}

//This function gets a frame (8 bytes) and returns 0 if no errors detected, 1 if can be corrected(1 wrong column and 1 wrong row) or 2 if can't be corrected 
int frameErrorDetector(char* bytes)
{
	int i;
	int j;
	int countRow = 0;
	int countCol = 0;
	char mask = 1;
	char parity = 0;
	for (j = 0; j < 8; j++)
	{
		mask = 1;
		parity = 0;
		for (i = 0; i < 8; i++)
		{
			if (bytes[j] & mask)
			{
				parity = parity ^ 1;
			}
			mask = mask << 1;
		}
		if (parity != 0)
		{
			countRow++;
		}

	}
	mask = 1;
	for (i = 0; i < 8; i++)
	{
		parity = 0;
		for (j = 0; j < 8; j++)
		{
			if (bytes[j] & mask)
			{
				parity = parity ^ 1;
			}
		}
		if (parity != 0)
		{
			countCol++;
		}
		mask = mask << 1;
	}

	if (countRow == 1 && countCol == 1)
	{
		return ERROR_CAN_BE_CORRECTED;
	}
	if (countRow == 0 && countCol == 0)
	{
		return NO_ERRORS_DETECTED;
	}
	return ERROR_CANT_BE_CORRECTED;
}

//This function gets a frame (8 bytes) with one wrong bit and corrects it 
void frameErrorCorrection(char* bytes)
{
	int i;
	int j;
	int row = 0;
	int col = 0;
	char mask = 1;
	char parity = 0;
	for (j = 0; j < 8; j++)
	{
		mask = 1;
		parity = 0;
		for (i = 0; i < 8; i++)
		{
			if (bytes[j] & mask)
			{
				parity = parity ^ 1;
			}
			mask = mask << 1;
		}
		if (parity != 0)
		{
			row = j;
			break;
		}

	}
	mask = 1;
	for (i = 0; i < 8; i++)
	{
		parity = 0;
		for (j = 0; j < 8; j++)
		{
			if (bytes[j] & mask)
			{
				parity = parity ^ 1;
			}
		}
		if (parity != 0)
		{
			col = i;
			break;
		}
		mask = mask << 1;
	}

	mask = 1;
	mask = mask << col;
	bytes[row] = bytes[row] ^ mask; 
}

// This function takes only the data bits from the frame and put them in an integers array, where each int represents 1 bit, zero or one
void addDataBitsToTheArray(char* bytes, int currentIndexInDataBitsArray)
{
	int i;
	int j;
	unsigned char mask;
	
	for (j = 0; j < 7; j++)
	{
		mask = 0b10000000;
		for (i = 0; i < 7; i++)
		{
			if (bytes[j] & mask)
			{
				DataBitsArray[currentIndexInDataBitsArray] = 1;
			}
			else
			{
				DataBitsArray[currentIndexInDataBitsArray] = 0;
			}
			currentIndexInDataBitsArray++;
			mask = mask >> 1;
		}
	}
}

/*
void printDataBitsArray(int numberOfFrames)
{
	int numberOfBits = numberOfFrames * SIZE_OF_DATA_FRAME;
	for (int i = 0; i < numberOfBits; i++)
	{
		printf("%d",DataBitsArray[i]);
		if (i % 7 == 6)
		{
			printf("\n");
		}
	}
}
*/
// This function writes the bytes of the frame from DataBitsArray to the file
void writeFrameBytesToFile(FILE* fp, int startIndex)
{
	char writeByte[DATA_FRAME_FULL_BYTES/8];
	unsigned char mask;
	for (int i = 0; i < DATA_FRAME_FULL_BYTES/8; i++)
	{
		writeByte[i] = 0;
		mask = 0b10000000;
		for (int j = 0; j < 8; j++)
		{
			if (DataBitsArray[startIndex+(i*8)+j] == 1)
			{
				writeByte[i] = writeByte[i] ^ mask;
			}

			mask = mask >> 1;
		}
	}
	fwrite(writeByte,sizeof(char), DATA_FRAME_FULL_BYTES / 8, fp);
}
// This function writes 1 byte from DataBitsArray to the file
void writeByteToFile(FILE* fp, int startIndex)
{
	char writeByte[1];
	unsigned char mask;
	
	writeByte[0] = 0;
	mask = 0b10000000;
	for (int j = 0; j < 8; j++)
	{
		if (DataBitsArray[startIndex+j] == 1)
		{
			writeByte[0] = writeByte[0] ^ mask;
		}

		mask = mask >> 1;
	}
	fwrite(writeByte, sizeof(char), 1, fp);
}