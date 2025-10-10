#include "packet_types.h"
#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
} PlayerData;
typedef struct {
  PlayerData *player;
  int id;
  char *username;
} Client;
typedef struct {
  Client clients[16];
} Game;

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
  Game game;
  memset(&game, 0, sizeof(Game));
  for (int i = 0; i < 16; i++) {
    game.clients[i].id = -1;
  }

  while (game_running) {
    ENetEvent event;
    while (enet_host_service(server, &event, 10) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        printf("A new client connected from %x:%u.\n", event.peer->address.host,
               event.peer->address.port);
        enet_peer_ping_interval(event.peer, 1000);
        enet_peer_timeout(event.peer, 3, 3000, 5000);

        int id = -1;
        for (int i = 0; i < 16; i++) {
          if (game.clients[i].id == -1) {
            id = i;
            break;
          }
        }
        if (id == -1) {
          printf("Server full, rejecting connection.\n");
          enet_peer_disconnect(event.peer, 0);
          break;
        }
        printf("Assigned new client to slot %d\n", id);
        event.peer->data = (void *)(intptr_t)id;
        game.clients[id].id = id + 1;
        for (int i = 0; i < 16; i++) {
          if (id == -1)
            continue;
          if (game.clients[i].username != NULL) {
            printf(
                "Sending username '%s' from slot %d to new client in slot %d\n",
                game.clients[i].username, i, id);
            size_t uname_len = strlen(game.clients[i].username);
            char *upktstr = malloc(uname_len + 2);
            if (upktstr == NULL) {
              fprintf(stderr, "FAILED TO ALLOCATE MEMORY (upktstr).\n");
              continue;
            }
            upktstr[0] = PACKET_TYPE_NEW_USER;
            memcpy(upktstr + 1, game.clients[i].username, uname_len + 1);
            ENetPacket *upkt = enet_packet_create(upktstr, uname_len + 2,
                                                  ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, 0, upkt);
            free(upktstr);
          }
        }
        break;
      case ENET_EVENT_TYPE_RECEIVE: {
        int client_id = (int)(intptr_t)event.peer->data;

        printf("A packet of length %zu was received from client %d on "
               "channel %u.\n",
               event.packet->dataLength, client_id, event.channelID);

        enum PACKET_TYPE ptype =
            (enum PACKET_TYPE)((unsigned char *)event.packet->data)[0];

        char data[event.packet->dataLength];
        memcpy(data, event.packet->data + 1, event.packet->dataLength - 1);
        data[event.packet->dataLength - 1] = '\0';

        switch (ptype) {
        case PACKET_TYPE_USERNAME: {
          printf("packet type username.\n");
          if (event.packet->dataLength != 21) {
            fprintf(stderr, "INVALID PACKET SIZE.\n");
          } else {
            printf("Client username: %s\n", (char *)data);
            if (game.clients[client_id].username != NULL) {
              free(game.clients[client_id].username);
            }
            game.clients[client_id].username = strdup(data);
            if (game.clients[client_id].username == NULL) {
              fprintf(stderr, "failed to allocate memory.\n");
              return 0;
            }
            size_t username_len = strlen(game.clients[client_id].username);
            char *nucmps = malloc(1 + 28 + username_len + 1);
            if (nucmps == NULL) {
              fprintf(stderr, "Failed to allocate memory\n");
              break;
            }
            nucmps[0] = PACKET_TYPE_CHAT_MSG;
            sprintf(nucmps + 1, " A new user has connected: %s",
                    game.clients[client_id].username);
            ENetPacket *new_user_chat_msg_pkt = enet_packet_create(
                nucmps, strlen(nucmps + 1) + 2, ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, new_user_chat_msg_pkt);
            free(nucmps);

            char *nupkt = malloc(username_len + 2);
            if (nupkt == NULL) {
              break;
            }
            nupkt[0] = PACKET_TYPE_NEW_USER;
            sprintf(nupkt + 1, "%s", game.clients[client_id].username);
            ENetPacket *new_u_pkt = enet_packet_create(
                nupkt, strlen(nupkt + 1) + 2, ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, new_u_pkt);
            free(nupkt);
          }
          break;
        }
        case PACKET_TYPE_CHAT_MSG: {
          printf("packet type chat msg.\n");

          const char *username = game.clients[client_id].username;
          if (username == NULL) {
            username = "Unknown";
          }

          size_t username_len = strlen(username);
          size_t data_len = strlen(data);
          size_t total_len = 1 + username_len + 1 + data_len + 1;

          char *echopktstr = (char *)malloc(total_len);
          if (echopktstr == NULL) {
            fprintf(stderr, "Failed to allocate memory for echo packet\n");
            break;
          }

          echopktstr[0] = PACKET_TYPE_CHAT_MSG;
          strcpy(echopktstr + 1, username);
          strcat(echopktstr + 1, ": ");
          strcat(echopktstr + 1, data);

          ENetPacket *echopkt;
          echopkt = enet_packet_create(echopktstr, strlen(echopktstr) + 1,
                                       ENET_PACKET_FLAG_RELIABLE);

          enet_host_broadcast(server, 0, echopkt);
          enet_host_flush(server);

          free(echopktstr);
          break;
        }
        }

        enet_packet_destroy(event.packet);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT: {
        int client_id = (int)(intptr_t)event.peer->data;

        printf("Client %d disconnected.\n", client_id);

        const char *template = "%s has disconnected.";
        char *dc_msg_str = malloc(strlen(game.clients[client_id].username) +
                                  strlen(template) + 1);
        dc_msg_str[0] = PACKET_TYPE_CHAT_MSG;
        sprintf(dc_msg_str + 1, template, game.clients[client_id].username);

        ENetPacket *dc_msg_pkt = enet_packet_create(
            dc_msg_str, strlen(dc_msg_str) + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, 0, dc_msg_pkt);
        free(dc_msg_str);

        char *dc_pkt_str = malloc(strlen(game.clients[client_id].username) + 2);
        dc_pkt_str[0] = PACKET_TYPE_DEL_USER;
        strcpy(dc_pkt_str + 1, game.clients[client_id].username);

        ENetPacket *dc_pkt = enet_packet_create(
            dc_pkt_str, strlen(dc_pkt_str) + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, 0, dc_pkt);

        free(dc_pkt_str);

        if (game.clients[client_id].username != NULL) {
          free(game.clients[client_id].username);
        }

        memset(&game.clients[client_id], 0, sizeof(Client));
        game.clients[client_id].id = -1;
        event.peer->data = NULL;
        break;
      }
      }
    }
  }

  for (int i = 0; i < 16; i++) {
    if (game.clients[i].username != NULL) {
      free(game.clients[i].username);
    }
  }

  enet_host_destroy(server);
  return 0;
}
