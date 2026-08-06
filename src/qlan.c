#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <string.h>
#include <arpa/inet.h>

// this program has two modes (sender and receiver mode)

/* Sender mode:
    - read from stdin
    - generate 4 digit code

    - enter lsiten mode for broadcast from receiver
    - upon receiving broadcast message, connect to receiver and send the input to receiver
*/

/* Receiver mode:
    - receive 4 digit code from user
    - craft broadcast message with 4 digit code

    - send broadcast
    - enter listen mode for input
    - receive input
    - write input to stdout
*/

#define BROADCAST_PORT 4444
#define TRANSFER_PORT 3333
#define MAGIC "LJ_001"

#define CODE_LENGTH 4

#define BC_MSG_SIZE sizeof(MAGIC) + sizeof(int)
#define TRANSFER_BUF_SIZE 128

char bcMsg[BC_MSG_SIZE];             // broadcast msg buffer
int broadcastFd, transferFd, connFd; // file descriptors

struct sockaddr_in addr, conn_addr;
socklen_t len;

char buf[TRANSFER_BUF_SIZE];

void die(const char* msg)
{
    perror(msg);
    _exit(1);
}

int genCode()
{
    int code=0;
    int genFd = open("/dev/urandom", O_RDONLY);
    if (genFd < 0)
        die("open");

    unsigned char n;
    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (read(genFd, &n, sizeof(n)) <= 0)
            die("read");

        code += (n % 10)*pow(10, i); 
    }

    close(genFd);
    return code;
}

void senderMode()
{
    int codeLocal = genCode();
    printf("transfer code: %d\n", codeLocal);

    // setup broadcast receiver
    broadcastFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastFd < 0)
        die("socket");

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);

    if (bind(broadcastFd, (struct sockaddr*)&addr, sizeof(addr) ) != 0)
        die("bind");

    len = sizeof(conn_addr);

    while (1)
    {

    recvfrom(broadcastFd, bcMsg, BC_MSG_SIZE, 0, (struct sockaddr*)&conn_addr, &len);

    // interpet broadcast message
    if (strncmp(bcMsg, MAGIC, sizeof(MAGIC)-1) == 0)
    {
        int codeReceived = *(int*)(bcMsg+sizeof(MAGIC)-1);

        if (codeReceived == codeLocal)
        {
            close(broadcastFd);

            conn_addr.sin_port = htons(TRANSFER_PORT);
            transferFd = socket(AF_INET, SOCK_STREAM, 0);
            if (transferFd < 0)
                die("socket");

            int enable = 1;
            if (setsockopt(transferFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) != 0)
                die("setsockopt");

            if (bind(transferFd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
                die("bind");

            if (connect(transferFd, (struct sockaddr*)&conn_addr, sizeof(conn_addr) ) != 0)
                die("connect");

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
    int code = atoi(arg_code);

    // setup sockets
    broadcastFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastFd < 0)
        die("socket");

    addr.sin_addr.s_addr = INADDR_BROADCAST;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);

    int enable = 1;
    if (setsockopt(broadcastFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) != 0)
        die("setsockopt");

    memcpy(bcMsg, MAGIC, sizeof(MAGIC));
    *(int*)(bcMsg + sizeof(MAGIC)-1) = code;

    printf("%d\n", *(int*)(bcMsg + sizeof(MAGIC)-1));

    if (sendto(broadcastFd, bcMsg, BC_MSG_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        die("sendto");

    close(broadcastFd);

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TRANSFER_PORT);

    // listen for a response
    transferFd = socket(AF_INET, SOCK_STREAM, 0);
    if (transferFd < 0)
        die("socket");

    enable = 1;
    setsockopt(transferFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));

    if (bind(transferFd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        perror("bind");

    listen(transferFd, 1);


    int len = sizeof(conn_addr);
    int conn_fd = accept(transferFd, (struct sockaddr*)&conn_addr, &len);
    if (conn_fd < 0)
        die("accept");

    int n;
    while ( (n = read(conn_fd, buf, TRANSFER_BUF_SIZE)) > 0)
        write(STDOUT_FILENO, buf, n);
    
    exit(EXIT_SUCCESS);
}

void help(int status, char const* program_name)
{
    printf("Usage: %s [OPTION]\n", program_name);
    printf("Conveniently transfer data over LAN\n");
    printf("\t-s\t\tSend mode\n");
    printf("\t-r [code]\tReceive mode\n");

    exit(status);
}

int main(int argc, char *argv[])
{
    int opt;

    if (argc == 1)
        help(1, argv[0]);

    while ( (opt = getopt(argc, argv, "sr:h")) != -1)
    {
        switch (opt)
        {
        case 's':
            senderMode();
            break;
        
        case 'r':
            receiverMode(optarg);
            break;

        case 'h':
            help(EXIT_SUCCESS, argv[0]);
            break;

        default:
            help(EXIT_FAILURE, argv[0]);
            break;
        }
    }
}
