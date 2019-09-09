/*
Source File: Sender: packager.c
Functionality: This source file contains the functions necessary inorder to convert a given input file of data into a linked list 
				of "packets" which use 2D parity encoding to send 49 bits of data as 64 bits of encoded data. Essentially the only
				funtion here called in a different souce file is the make_packet_list() function
*/


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packager.h"

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

/*
Name: get_parity_char()
Functionality: A help function which, given an "unsealed" packet struct (one where the last 8 bits of data have still not been encoded) returns the character
			   that needs to be placed in the last letter of the 8 letter string of the packet. It choses this letter through parity encoding column by column.
*/
unsigned char get_parity_char(packet *cur_packet)
{
	int bit_index;
	int char_index;
	int cur_bit = 0;
	int mask = 1;
	unsigned char parity_char = 0, temp_bit = 0;

	for (bit_index = 0; bit_index <= 7; bit_index++)
	{
		for (char_index = 0; char_index <= 6; char_index++)
		{
			temp_bit ^= (cur_packet->data[char_index] & mask);
		}
		mask = mask << 1;
		parity_char |= temp_bit;
		temp_bit = 0;
	}
	return parity_char;
}

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

/*
Name: make_packet_list()
Functionality: In charge of encoding the data into a linked list of packets. Iterates bit by bit of the file, every 7 biuts sealing an 8 bit char with the
				proper parity bit and every 7 letters calling the get_parity_char() function to encoded the last letter of the 8 char string. This function returns the 
				head packet of the packet linked list.
*/
packet* make_packet_list(char *file_path)
{
	FILE* fp;
	char* bin_data = NULL;
	packet *head = NULL;
	packet *cur_packet = NULL;
	packet *prev_packet = NULL;
	int success = 0;
	int size;
	int file_bit_index;
	int packet_letter_index = 0;
	int read_bit = 0;
	int packet_bit_index = 7;
	unsigned char parity = 0;
	
	// Open file
	fp = fopen(file_path, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "failed to open file: %s", file_path);
		return -1;
	}

	//check file length (in bytes) and allocate memory accordingly
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, 0);
	
	
	bin_data = (char*)malloc(size);
	if (bin_data == NULL)
	{
		fprintf(stderr, "Memory allocation failed");
		return 0;
	}

	// Read data into buffer	
	if (size != (success = fread(bin_data, 1, size, fp)))
	{
		fprintf(stderr, "File read failure\n");
		return 0;
	}


	// Prep linked list head
	if (NULL == (head = (packet*)malloc(sizeof(packet))))
	{
		fprintf(stderr, "Memory allocation failed");
		return 0;
	}
	strncpy(head->data, "\0\0\0\0\0\0\0\0", 8);
	cur_packet = head;

	//iterate over data, forming packet for each 7 bits
	for (file_bit_index = 0; file_bit_index < size * 8 ; file_bit_index++)
	{
		// Reads in current bit and places in 
		read_bit = (((1 << (7 - file_bit_index % 8))&bin_data[file_bit_index / 8]) > 0);
		parity = parity ^ read_bit;
		cur_packet->data[packet_letter_index] |= (read_bit << packet_bit_index);
		packet_bit_index--;

		// ~~~~ Check if new packet or letter (or both) need to be created ~~~ //
		// Check for new letter
		if (0 == packet_bit_index)
		{
			cur_packet->data[packet_letter_index] |= parity;
			packet_letter_index++;
			packet_bit_index = 7;
			parity = 0;
		}
		if (7 <= packet_letter_index)
		{
			cur_packet->data[7] = get_parity_char(cur_packet);
		}
		//If new block needs to be created and this is not the last iteration
		if (7 <= packet_letter_index && file_bit_index != size * 8 - 1)
		{
			prev_packet = cur_packet;
			if (NULL == (cur_packet = (packet*)malloc(sizeof(packet))))
			{
				fprintf(stderr, "Memory allocation failed");
				return 0;
			}
			strncpy(cur_packet->data, "\0\0\0\0\0\0\0\0", 8);
			prev_packet->next = cur_packet;
			packet_letter_index = 0;
			cur_packet->next = NULL;
		}




	}
	//free all resources but created linked list
	free(bin_data);
	fclose(fp);														// closing the file.
	return head;

}
