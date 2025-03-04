#ifndef _WEB_
#define _WEB_

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "stringbuilder.h"
#include "strfuncs.h"

#define BACKLOG 5
#define MAXHEADERS 30
#define LLBUFLEN 16384
#define LBUFLEN 4096
#define BUFLEN 1024
#define SBUFLEN 256
#define SSBUFLEN 64

typedef struct response {
    int status_code;
    int content_length;
    char content_type[32]; // 32 should be more than enough
} response;

typedef struct ka {
    int max;
    int timeout;
} ka;

#endif