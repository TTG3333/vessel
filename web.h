#ifndef _WEB_
#define _WEB_

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "stringbuilder.h"
#include "strfuncs.h"

#define BACKLOG 5
#define MAXHEADERS 64
#define LLBUFLEN 16384
#define LBUFLEN 4096
#define BUFLEN 1024
#define SBUFLEN 256
#define SSBUFLEN 64

typedef struct header {
    char name[SSBUFLEN];
    char value[BUFLEN];
} header;

typedef struct headergroup {
    header *headers;
    int num_headers;
    int headers_used;
} headergroup;

typedef struct response {
    int status_code;
    int num_headers;
    headergroup headers;
} response;

typedef struct file_response {
    char file_path[SBUFLEN];
    long file_size;
    char type[SSBUFLEN];
} file_response;

typedef struct ka {
    int max;
    int timeout;
} ka;

#endif