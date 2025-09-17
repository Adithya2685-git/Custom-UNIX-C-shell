# Compiler and flags
CC = gcc
# Use this for MacOS with Homebrew OpenSSL
# CFLAGS = -Wall -I$(shell brew --prefix openssl)/include
# LDFLAGS = -L$(shell brew --prefix openssl)/lib -lcrypto

# Use this for Linux
CFLAGS = -Wall
LDFLAGS = -lcrypto

# Source files
SERVER_SRC = server.c logger.c
CLIENT_SRC = client.c logger.c
SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

# Executables
SERVER_EXEC = server
CLIENT_EXEC = client

# Default target
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Rule to build the server
$(SERVER_EXEC): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to build the client
$(CLIENT_EXEC): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f *.o $(SERVER_EXEC) $(CLIENT_EXEC) server_log.txt client_log.txt received_file.txt

.PHONY: all clean