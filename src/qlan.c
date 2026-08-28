#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdint.h>

#define BROADCAST_PORT 4444
#define TRANSFER_PORT 3333

#define TRANSFER_BUF_SIZE 1024

#define VERSION "v0.0.4"

#define MAGIC_PREFIX (uint32_t)(0xF5F20000)
#define PROTOCOL_VERSION (uint32_t)4
#define MAGIC (uint32_t)( MAGIC_PREFIX | PROTOCOL_VERSION )

struct __attribute__((packed)) broadcast_msg 
{
    uint32_t magic;
    uint16_t transfer_code;
};

int broadcastFd, transferFd, connFd; // file descriptors

struct sockaddr_in addr, conn_addr;
socklen_t len;

char buf[TRANSFER_BUF_SIZE];

void die(const char* msg)
{
    perror(msg);
    _exit(1);
}

uint8_t opt_mask = 0;
enum OPT_FLAGS
{
    FLAG_SEND = (1 << 0),
    FLAG_RECV = (1 << 1),
    FLAG_VERBOSE = (1 << 2),
};

void log_verbose(const char* format, ...)
{
    if (opt_mask & FLAG_VERBOSE)
    {
        va_list args;
        va_start(args, format);

        vprintf(format, args);
        va_end(args);
    }
}

void send_broadcast_msg(int sock_fd, uint16_t code)
{
    struct broadcast_msg bcMsg;
    bcMsg.magic = htonl(MAGIC);
    bcMsg.transfer_code = htons(code);

    if (sendto(sock_fd, &bcMsg, sizeof(bcMsg), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(bcMsg))
        die("sendto");

}

void receive_broadcast_msg(int sock_fd, struct broadcast_msg *bcMsg)
{
    if (recvfrom(sock_fd, bcMsg, sizeof(*bcMsg), 0, (struct sockaddr*)&conn_addr, &len) != sizeof(*bcMsg))
        die("recvfrom");

    bcMsg->magic = ntohl(bcMsg->magic);
    bcMsg->transfer_code = ntohs(bcMsg->transfer_code);
}

uint16_t genCode()
{
    uint16_t code;

    int genFd = open("/dev/urandom", O_RDONLY);
    if (genFd < 0)
        die("open");

    ssize_t n;
    if ((n = read(genFd, &code, sizeof(code))) < sizeof(code))
    {
        if (n < 0)
            die("read");
        
        fprintf(stderr, "failed to generate transfer code: partial read\n");
        exit(EXIT_FAILURE);
    }    

    close(genFd);
    return code;
}

int enable = 1;

void senderMode()
{
    log_verbose("Generating code...\n");

    uint16_t codeLocal = genCode();
    printf("transfer code: %04X\n", codeLocal);

    log_verbose("Creating socket to listen for broadcast...\n");
    // setup broadcast receiver
    broadcastFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastFd < 0)
        die("socket");

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);

    log_verbose("Enabling SO_REUSEADDR\n");
    if (setsockopt(broadcastFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
        die("setsockopt");

    log_verbose("Binding socket\n");
    if (bind(broadcastFd, (struct sockaddr*)&addr, sizeof(addr) ) != 0)
        die("bind");

    len = sizeof(conn_addr);

    while (1)
    {

    struct broadcast_msg bcMsg;

    log_verbose("Waiting for broadcast message...\n");
    receive_broadcast_msg(broadcastFd, &bcMsg);

    log_verbose("Comparing magic numbers...\n");
    // interpet broadcast message
    if ( (bcMsg.magic >> 16) == (MAGIC >> 16) )
    {
        if ( (bcMsg.magic & (uint32_t)0x0000ffff) != (PROTOCOL_VERSION) )
        {
            fprintf(stderr, "Receiver is using an incompatible protocol version\nPlease ensure both instances use the same protocol version\n");
            exit(EXIT_FAILURE);
        }

        log_verbose("Magic number is correct\n");

        if (bcMsg.transfer_code == codeLocal)
        {
            close(broadcastFd);

            log_verbose("Creating TCP socket...\n");
            conn_addr.sin_port = htons(TRANSFER_PORT);
            transferFd = socket(AF_INET, SOCK_STREAM, 0);
            if (transferFd < 0)
                die("socket");

            log_verbose("Enabling SO_REUSEADDR...\n");
            if (setsockopt(transferFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
                die("setsockopt");

            if (bind(transferFd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
                die("bind");

            log_verbose("Connecting for receiver...\n");
            if (connect(transferFd, (struct sockaddr*)&conn_addr, sizeof(conn_addr) ) != 0)
                die("connect");

            log_verbose("Connection established\n");

            int n;
            while ( (n = read(STDIN_FILENO, buf, TRANSFER_BUF_SIZE)) > 0)
                write(transferFd, buf, n);
            
            close(transferFd);

            exit(EXIT_SUCCESS);
        }
    }

}

}

void receiverMode(char* arg_code)
{
    char *endptr = NULL;
    uint16_t code = strtoul(arg_code, &endptr, 16);
    if (*endptr != '\0')
    {
        fprintf(stderr, "invalid transfer code\n");
        exit(EXIT_FAILURE);
    }

    // setup sockets
    log_verbose("Creating datagram socket\n");
    broadcastFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastFd < 0)
        die("socket");

    addr.sin_addr.s_addr = INADDR_BROADCAST;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);

    log_verbose("Enabling SO_BROADCAST\n");
    if (setsockopt(broadcastFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) != 0)
        die("setsockopt");

    log_verbose("Sending broadcast message\nAwaiting response...\n");
    send_broadcast_msg(broadcastFd, code);
    close(broadcastFd);

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TRANSFER_PORT);

    // listen for a response
    log_verbose("Creating TCP socket\n");
    transferFd = socket(AF_INET, SOCK_STREAM, 0);
    if (transferFd < 0)
        die("socket");

    log_verbose("Binding TCP socket\n");
    if (bind(transferFd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        die("bind");

    listen(transferFd, 1);


    int len = sizeof(conn_addr);
    log_verbose("Awaiting connection...\n");
    int conn_fd = accept(transferFd, (struct sockaddr*)&conn_addr, &len);
    if (conn_fd < 0)
        die("accept");

    log_verbose("Connection established.\n");

    int n;
    while ( (n = read(conn_fd, buf, TRANSFER_BUF_SIZE)) > 0)
        write(STDOUT_FILENO, buf, n);
    
    exit(EXIT_SUCCESS);
}

void help(int status, char const* program_name)
{
    printf("Usage: %s [OPTION], ...\n", program_name);
    printf("Conveniently transfer data over LAN\n");
    printf("\t-s\t\tSend mode\n");
    printf("\t-r [code]\tReceive mode\n");
    printf("\t-v \t\tVerbose\n");
    printf("\t-V \t\tVersion\n");

    exit(status);
}


char code[5] = {0};

void show_version()
{
    printf("QLAN version: %s\n", VERSION);
    printf("Protocol version: v%u\n", PROTOCOL_VERSION);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int opt;

    if (argc == 1)
        help(1, argv[0]);

    while ( (opt = getopt(argc, argv, "sr:hvV")) != -1)
    {
        switch (opt)
        {
        case 's':
            opt_mask |= FLAG_SEND;
            break;
        
        case 'r':
            opt_mask |= FLAG_RECV;
            strncpy(code, optarg, 4);
            break;

        case 'h':
            help(EXIT_SUCCESS, argv[0]);
            break;

        case 'v':
            opt_mask |= FLAG_VERBOSE;
            break;

        case 'V':
            show_version();
            break;

        default:
            help(EXIT_FAILURE, argv[0]);
            break;
        }
    }

    if ( (opt_mask & (FLAG_RECV | FLAG_SEND)) == (FLAG_RECV | FLAG_SEND) )
        {
            fprintf(stderr, "You cannot enable both sender and receiver mode at once\n");
            exit(1);
        }
    else
    if (opt_mask & FLAG_RECV)
    {
        receiverMode(code);
    }
    else
    if (opt_mask & FLAG_SEND)
    {
        senderMode();
    }
}
