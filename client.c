#include "packet_types.h"
#include <enet/enet.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAT_MSG_MAX_LEN 24

typedef struct {
  Rectangle rect;
  char (*msg)[CHAT_MSG_MAX_LEN + 1];
} ChatBox;

void update_chat(int *chat_msg_index, char (*chat_msg)[CHAT_MSG_MAX_LEN + 1],
                 ENetPeer *peer, ENetHost *client) {
  if (*chat_msg_index < CHAT_MSG_MAX_LEN) {
    int key = GetCharPressed();
    while (key > 0) {
      if ((key >= 32) && (key <= 125) && (*chat_msg_index < CHAT_MSG_MAX_LEN)) {
        (*chat_msg)[*chat_msg_index] = (char)key;
        (*chat_msg)[*chat_msg_index + 1] = '\0';
        (*chat_msg_index)++;
      }

      key = GetCharPressed();
    }
  }
  if (IsKeyPressed(KEY_BACKSPACE) && *chat_msg_index > 0) {
    (*chat_msg_index)--;
    if ((*chat_msg_index) < 0)
      (*chat_msg_index) = 0;
    (*chat_msg)[*chat_msg_index] = '\0';
  }

  if (IsKeyPressed(KEY_ENTER) && *chat_msg_index > 0) {
    ENetPacket *chat_msg_pkt;

    char chat_msg_out[CHAT_MSG_MAX_LEN + 2];
    memset(chat_msg_out, 0, CHAT_MSG_MAX_LEN + 2);
    chat_msg_out[0] = PACKET_TYPE_CHAT_MSG;

    strcpy(chat_msg_out + 1, *chat_msg);

    chat_msg_pkt = enet_packet_create(chat_msg_out, CHAT_MSG_MAX_LEN + 2,
                                      ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, chat_msg_pkt);
    enet_host_flush(client);

    memset(*chat_msg, 0, CHAT_MSG_MAX_LEN + 1);
    (*chat_msg)[0] = '\0';
    *chat_msg_index = 0;
  }
}

void send_username(ENetPeer *peer, ENetHost *client) {
  char username[20];
  printf("Enter username: ");
  fflush(stdout);
  scanf("%s", username);

  while (strlen(username) + 1 >= 20) {
    printf("Username too long.\nEnter username: ");
    fflush(stdout);
    scanf("%19s", username);
  }

  char username_out[21];
  memset(username_out, 0, 21);
  username_out[0] = PACKET_TYPE_USERNAME;
  strcpy(username_out + 1, username);

  ENetPacket *username_pkt =
      enet_packet_create(username_out, 21, ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, username_pkt);

  enet_host_flush(client);
}

int main() {
  if (enet_initialize() != 0) {
    printf("Could not initialize ENET.\n");
    return EXIT_FAILURE;
  }
  atexit(enet_deinitialize);

  ENetHost *client;

  client = enet_host_create(NULL /* create a client host */,
                            1 /* only allow 1 outgoing connection */,
                            2 /* allow up 2 channels to be used, 0 and 1 */,
                            0 /* assume any amount of incoming bandwidth */,
                            0 /* assume any amount of outgoing bandwidth */);

  if (client == NULL) {
    fprintf(stderr,
            "An error occurred while trying to create an ENet client host.\n");
    exit(EXIT_FAILURE);
  }

  ENetAddress address;
  ENetPeer *peer;
  ENetEvent event;

  enet_address_set_host(&address, "127.0.0.1");
  address.port = 8080;

  peer = enet_host_connect(client, &address, 2, 0);

  if (peer == NULL) {
    printf("Could not initialize peer\n");
    exit(EXIT_FAILURE);
  }

  if (enet_host_service(client, &event, 10000) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT) {
    printf("Connected to server successfully.\n");
  } else {
    enet_peer_reset(peer);
    printf("Could not connect to server.\n");
    exit(EXIT_FAILURE);
  }

  bool game_running = true;

  send_username(peer, client);

  InitWindow(800, 600, "Multiplayer");
  SetTargetFPS(60);

  char chat_msg[CHAT_MSG_MAX_LEN + 1];
  int chat_msg_index = 0;

  memset(chat_msg, 0, CHAT_MSG_MAX_LEN + 1);

  ChatBox chatbox;
  chatbox.rect.x = 10;
  chatbox.rect.y = 545;
  chatbox.rect.width = 780;
  chatbox.rect.height = 50;
  chatbox.msg = &chat_msg;

  char *msgs[15] = {0};
  int msg_index = -1; // Fixed: start at -1

  while (!WindowShouldClose()) {
    if (enet_host_service(client, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_DISCONNECT: {
        printf("Disconnected from server.\n");
        break;
      }
      case ENET_EVENT_TYPE_RECEIVE: {
        printf("Received packet.\n");

        enum PACKET_TYPE ptype =
            (enum PACKET_TYPE)((unsigned char *)event.packet->data)[0];

        char data[event.packet->dataLength];
        memcpy(data, event.packet->data + 1, event.packet->dataLength - 1);
        data[event.packet->dataLength - 1] =
            '\0'; // Fixed: correct null terminator position

        switch (ptype) {
        case PACKET_TYPE_CHAT_MSG: {
          printf("packet type chat msg.\n");
          // Free the oldest message before it gets overwritten
          if (msgs[0] != NULL) {
            free(msgs[0]);
          }
          // Shift messages up (oldest at top)
          for (int i = 0; i < 14; i++) {
            msgs[i] = msgs[i + 1];
          }
          msg_index++;
          if (msg_index > 14)
            msg_index = 14;
          msgs[14] = strdup(data);
          break;
        }
        default:
          fprintf(stderr, "unsupported packet type: %d\n", ptype);
          break;
        }

        enet_packet_destroy(event.packet);
        break;
      }
      }
    }

    update_chat(&chat_msg_index, &chat_msg, peer, client);

    BeginDrawing();
    ClearBackground(WHITE);

    for (int i = 0; i < 15; i++) {
      if (msgs[i] != NULL) {
        DrawText((const char *)msgs[i], 5, 50 + 30 * i, 25, BLACK);
      }
    }

    DrawRectangleRec(chatbox.rect, BLACK);
    DrawText(*chatbox.msg, chatbox.rect.x + 5,
             chatbox.rect.y + chatbox.rect.height / 2 - 48.0f / 2, 48, WHITE);

    EndDrawing();
  }

  for (int i = 0; i < 15; i++) {
    free(msgs[i]);
  }

  enet_host_destroy(client);
  CloseWindow();
  return 0;
}
