# Custom Unix Shell & Network Utilities

A custom Unix shell written in C with job control, I/O redirection, and multi-level piping. Also includes a reliable UDP transport protocol with sequenced delivery.

## Components

### 1. Custom Shell (`shell/`)
A fully functional Unix shell supporting:
- **I/O Redirection**: `<`, `>`, `>>` operators
- **Multi-level Piping**: Arbitrary-length pipe chains (`cmd1 | cmd2 | cmd3`)
- **Background Processes**: `&` operator with job tracking
- **Signal Handling**: SIGINT (Ctrl+C), SIGCHLD (child termination), SIGTSTP (Ctrl+Z)
- **Built-in Commands**: `cd`, `pwd`, `echo`, `exit`, `history` (up to 15 entries)
- **Command History**: Persisted to `~/.my_shell_history`, up to 4096-char lines, 128 args per command

### 2. Reliable UDP Transport (`networking/`)
A custom reliability layer over UDP (S.H.A.M protocol) providing:
- **Sliding Window**: 10-packet window with go-back-N retransmission
- **Retransmission**: 500ms adaptive RTO with exponential backoff
- **Flow Control**: Receiver-advertised window based on available buffer (2x window size)
- **Packet Framing**: 1400-byte MTU-aware packets with 16-byte headers (seq, ack, flags, window)
- **Connection Lifecycle**: SYN → data transfer → FIN handshake

## Building

```bash
# Build shell
cd shell && make

# Build networking tools
cd networking && make
```

## Usage

### Shell
```bash
./shell/shell
myshell> ls -la | grep .c | wc -l
myshell> cat file.txt > output.txt &
myshell> cd /tmp && pwd
```

### UDP Server
```bash
./networking/server
# Listens on port 8080, logs to server_log.txt
```

### UDP Client
```bash
./networking/client <server_ip> <file_to_send>
# Transfers file with reliable delivery
```

Enable debug logging:
```bash
export RUDP_LOG=1
```

## Design Decisions

- **Per-process execution**: Each command in a pipeline runs in its own forked child process, connected via `pipe()`.
- **Job table**: Background processes tracked in a linked list with job IDs, reaped via SIGCHLD handler.
- **UDP reliability**: Implemented at application layer — sequence numbers, cumulative ACKs, and timer-based retransmission without kernel-level TCP overhead.
