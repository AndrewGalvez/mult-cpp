#include "packet_types.h"
#include <enet/enet.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main() {
  if (enet_initialize() != 0) {
    printf("Could not initialize ENET.\n");
    return EXIT_FAILURE;
  }
  atexit(enet_deinitialize);

  ENetAddress address;
  ENetHost *server;
  address.host = ENET_HOST_ANY;
  address.port = 8080;
  server = enet_host_create(&address, 32, 2, 0, 0);
  if (server == NULL) {
    printf("Could not create host.\n");
    exit(EXIT_FAILURE);
  }

  bool game_running = true;

  while (game_running) {
    ENetEvent event;

    while (enet_host_service(server, &event, 10) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        printf("A new client connected from %x:%u.\n", event.peer->address.host,
               event.peer->address.port);

        /* Store any relevant client information here. */
        event.peer->data = NULL;

        break;

      case ENET_EVENT_TYPE_RECEIVE: {
        printf("A packet of length %zu was received from %s on "
               "channel %u.\n",
               event.packet->dataLength, (char *)event.peer->data,
               event.channelID);

        enum PACKET_TYPE ptype =
            (enum PACKET_TYPE)((unsigned char *)event.packet->data)[0];

        char data[event.packet->dataLength];
        memcpy(data, event.packet->data + 1, event.packet->dataLength - 1);
        data[event.packet->dataLength] = '\0';

        switch (ptype) {
        case PACKET_TYPE_USERNAME: {
          printf("packet type username.\n");
          if (event.packet->dataLength != 21) {
            fprintf(stderr, "INVALID PACKET SIZE.\n");
          } else {
            printf("Client username: %s\n", (char *)data);
            event.peer->data = strdup(data);
            if (event.peer->data == NULL) {
              fprintf(stderr, "failed to allocate memory.\n");
              return 0;
            }
          }
          break;
        }
        case PACKET_TYPE_CHAT_MSG: {
          printf("packet type chat msg.\n");
          printf("Chat message from client %s: %s\n", (char *)event.peer->data,
                 data);
          break;
        }
        }

        /* Clean up the packet now that we're done using it. */
        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT:
        printf("%s disconnected.\n", (char *)event.peer->data);

        if (event.peer->data != NULL) {
          free(event.peer->data);
          event.peer->data = NULL;
        }
      }
    }
  }

  enet_host_destroy(server);
  return 0;
};
