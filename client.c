#include "packet_types.h"
#include <enet/enet.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  InitWindow(800, 600, "Multiplayer");
  SetTargetFPS(60);

  int chat_msg_max_len = 16;
  char chat_msg[chat_msg_max_len + 1];
  int chat_msg_index = 0;
  memset(chat_msg, 0, chat_msg_max_len + 1);

  while (!WindowShouldClose()) {
    if (enet_host_service(client, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_DISCONNECT: {
        printf("Disconnected from server.\n");
        break;
      }
      case ENET_EVENT_TYPE_RECEIVE: {
        printf("Received packet.\n");
        enet_packet_destroy(event.packet);
        break;
      }
      }
    }

    BeginDrawing();
    ClearBackground(WHITE);
    if (chat_msg_index < chat_msg_max_len) {
      int key = GetCharPressed();
      while (key > 0) {
        if ((key >= 32) && (key <= 125) &&
            (chat_msg_index < chat_msg_max_len)) {
          chat_msg[chat_msg_index] = (char)key;
          chat_msg[chat_msg_index + 1] = '\0';
          chat_msg_index++;
        }

        key = GetCharPressed();
      }

      if (IsKeyPressed(KEY_BACKSPACE)) {
        chat_msg_index--;
        if (chat_msg_index < 0)
          chat_msg_index = 0;
        chat_msg[chat_msg_index] = '\0';
      }
    }

    DrawText((const char *)chat_msg, 0, 0, 48, BLACK);

    if (IsKeyPressed(KEY_ENTER)) {
      ENetPacket *chat_msg_pkt;

      char chat_msg_out[chat_msg_max_len + 2];
      memset(chat_msg_out, 0, chat_msg_max_len + 2);
      chat_msg_out[0] = PACKET_TYPE_CHAT_MSG;

      strcpy(chat_msg_out + 1, chat_msg);

      chat_msg_pkt = enet_packet_create(chat_msg_out, chat_msg_max_len,
                                        ENET_PACKET_FLAG_RELIABLE);
      enet_peer_send(peer, 0, chat_msg_pkt);
      enet_host_flush(client);
    }

    EndDrawing();
  }

  enet_host_destroy(client);
  CloseWindow();
  return 0;
};
