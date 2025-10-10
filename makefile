# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS_COMMON = -lenet
LDFLAGS_CLIENT = $(LDFLAGS_COMMON) -lraylib
LDFLAGS_SERVER = $(LDFLAGS_COMMON)

# Targets
CLIENT = client
SERVER = server

# Default target
all: $(CLIENT) $(SERVER)

# Build client
$(CLIENT): client.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_CLIENT)

# Build server
$(SERVER): server.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_SERVER)

# Run targets
run-client: $(CLIENT)
	./$(CLIENT)

run-server: $(SERVER)
	./$(SERVER)

# Rebuild and run
rebuild-client: clean $(CLIENT)
	./$(CLIENT)

rebuild-server: clean $(SERVER)
	./$(SERVER)

# Clean build artifacts
clean:
	rm -f $(CLIENT) $(SERVER)

# Rebuild everything
rebuild: clean all

# Phony targets
.PHONY: all clean rebuild run-client run-server rebuild-client rebuild-server
