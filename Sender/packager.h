#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct _packet {
	unsigned char data[8];
	struct _packet *next;
}packet;

packet* make_packet_list(char *file_path);
unsigned char get_parity_char(packet *cur_packet);
