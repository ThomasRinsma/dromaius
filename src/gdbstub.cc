#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#include "dromaius.h"

#define BUFSIZE 4096

/*
radare2 -a gb -d gdb://localhost:9999
*/

static int debugThread(void *ptr)
{
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	char packet_data[BUFSIZE];
	char response_data[BUFSIZE];
	bool done = false;
	size_t len;

	printf("Starting server...\n");
	TCPsocket server, client;
	IPaddress ip;

	if (SDLNet_ResolveHost(&ip, NULL, 9999) == -1) {
		printf("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		exit(1);
	}

	server = SDLNet_TCP_Open(&ip);
	if (!server) {
		printf("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
		exit(2);
	}

	while (!done) {
		client = SDLNet_TCP_Accept(server);
		if (!client) {
			SDL_Delay(100);
			continue;
		}

		IPaddress *clientip;
		clientip = SDLNet_TCP_GetPeerAddress(client);
		if (!clientip) {
			printf("SDLNet_TCP_GetPeerAddress: %s\n", SDLNet_GetError());
			continue;
		}

		uint32_t ipaddr;
		ipaddr = SDL_SwapBE32(clientip->host);
		printf("Accepted a connection from %d.%d.%d.%d port %hu\n", ipaddr >> 24,
			 (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff,
			 clientip->port);

		while (1) {
			bzero(inbuf, BUFSIZE);
			bzero(outbuf, BUFSIZE);
			bzero(packet_data, BUFSIZE);
			bzero(response_data, BUFSIZE);

			len = SDLNet_TCP_Recv(client, inbuf, BUFSIZE);
			if (!len) {
				printf("SDLNet_TCP_Recv: %s\n", SDLNet_GetError());
				break;
			}

			if (len == 1) {
				printf("Received: '%c'\n", inbuf[0]);
				continue;
			}

			char *read_ptr = inbuf;
			if (inbuf[0] == '+') {
				read_ptr++;
			}

			// Parse packet
			sscanf(read_ptr, "$%[^#]#", packet_data);
			
			printf("packet_data: %s\n", packet_data);
			
			switch (packet_data[0]) {
				case 'q':
					printf("Received general query: '%s'\n", packet_data);
					// qSupported:multiprocess+;qRelocInsn+;xmlRegisters=i386
					// qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;xmlRegisters=i386
					
					// Initial packet sent by gdb
					if (memcmp(packet_data, "qSupported", strlen("qSupported")) == 0) {
						// Reply with supported features
						printf("qSupported packet found\n");
						strcpy(response_data, "hwbreak+;qRelocInsn+");
					}
					break;

				case '!':
					printf("Extended mode supported");
					strcpy(response_data, "OK");
					break;

				default:
					printf("Received unsupported packet: '%s'\n", packet_data);
					// empty response_data results in: '$#00'
					break;
			}

			// Calculate checksum
			uint8_t checksum_out = 0;
			for (int i = 0; i < strlen(response_data); ++i) {
				checksum_out += response_data[i];
			}

			// Create output buffer
			sprintf(outbuf, "$%s#%02x", response_data, checksum_out);
			printf("Sending outbuf: '%s'\n", outbuf);
			len = SDLNet_TCP_Send(client, outbuf, BUFSIZE);
			if (!len) {
				printf("SDLNet_TCP_Send: %s\n", SDLNet_GetError());
				break;
			}

		}

		SDLNet_TCP_Close(client);
	}

	return 0;
}