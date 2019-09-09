/*
Source File: Sender: main.c
Functionality: This file contains nearly all functions having to do with the sender module. The main() function
				here is in charge of parsing the provided command line arguments and calling the function MainClient() - to be the main thread of the module.
				The main thread calls the Recv and Send threads which do as their name suggests and packs the data from the input file into a linked list of 
				2D parity encoded packets.
*/


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define PACKET_SIZE 8
#define SUMMARY_FROM_SERVER 100
#define IP_LEN 16
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include "Sender.h"
#include "packager.h"


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


SOCKET s_socket;
SOCKADDR_IN clientSendService;
SOCKADDR_IN clientRecvService;
SOCKADDR_IN channelService;
FILE* fp;
packet* head;
HANDLE hThread[2];
char channelIP[IP_LEN];
int channelPort;
/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/


/*
Name: RecvDataThread()
Functionality: This threads creates a Buffer on which to receive a message from the channel. It then waits during the whole run to receive a message from the channel.
Once received, the message is printed and the thread terminated.
*/
static DWORD RecvDataThread(void)
{
	int iResult;
	char RecvBuf[SUMMARY_FROM_SERVER];
	int channelServiceSize = sizeof(channelService); //We don't need channelService because we know the port and IP of the channel
	
	while (1)
	{
		
		// we should add a checking to see if the message is from the channel
		iResult = recvfrom(s_socket, RecvBuf, SUMMARY_FROM_SERVER, 0, &channelService, &channelServiceSize);

		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "Socket error while trying to receive data to socket\n");
			return 0x555;
		}
		else
		{
			fprintf(stderr, "%s\n", RecvBuf);
			return 0;
		}
		
	}

	return 0;
}

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

/*
Name: SendDataThread()
Functionality: This thread is in charge of sending data to the channel, packet by packet. It iterates over the global linked list starting at packet* head
				and sends 8-character long strings in each packet (64 bits encoded by 2D parity). The linked list's packets' strings are send one by one until the last packet
				of the linked list is reached (its next field is NULL).
*/
static DWORD SendDataThread(void)
{
	char SendStr[PACKET_SIZE+1];
	int iResult;
	packet* cur = head;
	int counter = 0;
	while (1)
	{
		//Send packet's string
		iResult = sendto(s_socket, cur->data, PACKET_SIZE, 0, &clientSendService, sizeof(clientSendService));
		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "Socket error while trying to write data to socket\n");
			return 0x555;
		}
		cur = cur->next;
		//if reached end of linked list, terminate sending thread
		if (cur == NULL)
		{
			return 0;
		}
		
	}
}

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

/*
Name: MainClient()
Functionality: This is the main thread, in charge of properly dispatching the recv and send threads. It is called from the main() function
				and initializes the whole WSA framework. The thread then creates the socket on which the Sender will send and receive data 
				from the channel, binding it to the Sender port. It also initializes the port and IP addresses of the channel and sender.
*/
void MainClient()
{
	

	// Initialize Winsock.
	WSADATA wsaData; //Create a WSADATA object called wsaData.
					 //The WSADATA structure contains information about the Windows Sockets implementation.

					 //Call WSAStartup and check for errors.
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
		fprintf(stderr, "Error at WSAStartup()\n");

	// Create a socket.
	s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	// Check for errors to ensure that the socket is a valid socket.
	if (s_socket == INVALID_SOCKET) {
		fprintf(stderr, "Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	//Create a sockaddr_in object clientSendService and set  values.
	clientSendService.sin_family = AF_INET;
	clientSendService.sin_addr.s_addr = inet_addr(channelIP); //Setting the IP address to connect to
	clientSendService.sin_port = htons(channelPort); //Setting the port to connect to.

	//Create a sockaddr_in object clientRecvService and set  values.
	
	clientRecvService.sin_family = AF_INET;
	clientRecvService.sin_addr.s_addr = inet_addr(SERVER_ADDRESS_STR); //Setting the IP address to connect to
	clientRecvService.sin_port = htons(CLIENT_PORT); //Setting the port to connect to.
	

	iResult = bind(s_socket, (SOCKADDR *)&clientRecvService, sizeof(clientRecvService));
	if (iResult != 0)
	{
		fprintf(stderr, "Bind failed\n");
		return;
	}


	hThread[0] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)SendDataThread,
		NULL,
		0,
		NULL
	);
	hThread[1] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	TerminateThread(hThread[0], 0x555);
	TerminateThread(hThread[1], 0x555);

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	closesocket(s_socket);
	

	WSACleanup();

	return;
}

/*
Name: main
Functionality: Short main() function. The main simply sets global variables equal to the provided comman line arguments and then calls the effective main thread function, MainClient().
				This is the only function which uses the packager module. It calls make_packet_list() which reads the provided binary file and packs into 2D parity encoded linked list of packet 
				structs.
*/
int main(int argc, char *argv[])
{
	channelPort = atoi(argv[2]);
	strncpy(channelIP, argv[1], IP_LEN);
	head = make_packet_list(argv[3]);
	MainClient();
	return 0;
}