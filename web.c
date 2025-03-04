#include "web.h"

int str_start(const char *haystack, const char *needle) {
    return ! strncmp(haystack, needle, strlen(needle));
}

int str_end(const char *haystack, const char *needle) {
    return ! strcmp(needle, haystack + strlen(haystack) - strlen(needle));
}

char *build_header(response *rsp) { // the return value is a boolean indicating success
    sb dest;
    if (! sb_init(BUFLEN, &dest)) {
        return NULL;
    }
    char buf[SBUFLEN];
    time_t current = time(NULL);

    if (rsp->status_code == 200) {
        if (! sb_append("HTTP/1.1 200 OK\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 400) {
        if (! sb_append("HTTP/1.1 400 Bad Request\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 403) {
        if (! sb_append("HTTP/1.1 403 Forbidden\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 404) {
        if (! sb_append("HTTP/1.1 404 Not Found\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 418) {
        if (! sb_append("HTTP/1.1 418 I'm a teapot\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 431) {
        if (! sb_append("HTTP/1.1 431 Request Header Fields Too Large\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 500) {
        if (! sb_append("HTTP/1.1 500 Internal Server Error\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else {
        sb_destroy(&dest);
        return NULL;
    }

    if (rsp->content_length > 0) {
        sprintf(buf, "Content-Length: %d\r\n", rsp->content_length);
        if (! sb_append(buf, &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    }

    if (NULL != rsp->content_type) {
        sprintf(buf, "Content-Type: %s\r\n", rsp->content_type);
        if (! sb_append(buf, &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    }

    if (0 == strftime(buf, sizeof(buf), "Date: %a, %d %b %Y %T %Z\r\n", localtime(&current)) || ! sb_append(buf, &dest)) { // Short-circuit the or
        sb_destroy(&dest);
        return NULL;
    }

    if (! sb_append("\r\n", &dest)) {
        sb_destroy(&dest);
        return NULL;
    }

    char *out = sb_build(&dest);
    sb_destroy(&dest);
    return out;
}

char *prepare_file(char *path, int *len, char *type) {
    char filepath[SBUFLEN];
    sprintf(filepath, "WebFiles%s", path);
    FILE *file = fopen(filepath, "r");
    if (NULL == file) {
        perror("fopen");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    *len = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buf = malloc(*len);
    if (buf == NULL) {
        perror("malloc");
        return NULL;
    }
    if (0 == fread(buf, 1, *len, file)) {
        perror("fread");
        free(buf);
        return NULL;
    }

    if (str_end(path, ".html")) {
        strcpy(type, "text/html");
    } else if (str_end(path, ".ico")) {
        strcpy(type, "image/x-icon");
    } else {
        type[0] = 0;
    }

    return buf;
}

void send_500(FILE *client) { // Sends a status code 500 to the client, should only be called if send_error or send_response fails, otherwise call send_error(client, 500)
    fprintf(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n500 Internal Server Error");
    fflush(client);
}

int send_response(FILE *client, response *rsp, char *content) {
    char *head;
    if (NULL ==  (head = build_header(rsp))) {
        send_500(client);
        return 500;
    }
    fprintf(client, "%s", head);
    fwrite(content, 1, rsp->content_length, client);
    fflush(client);
    free(head);
    return rsp->status_code;
}

int send_error(FILE *client, int code) {
    char *file;
    response rsp;
    rsp.status_code = code;
    char path[16];
    if (code == 400) {
        strcpy(path, "400.html");
    } else if (code == 403) {
        strcpy(path, "403.html");
    } else if (code == 404) {
        strcpy(path, "404.html");
    } else if (code == 431) {
        strcpy(path, "431.html");
    } else if (code == 500) {
        strcpy(path, "500.html");
    } else {
        strcpy(path, "500.html");
    }
    if (NULL == (file = prepare_file(path, &rsp.content_length, rsp.content_type))) {
        send_500(client);
        return 500;
    }
    code = send_response(client, &rsp, file);
    free(file);
    return code;
}

int send_file(FILE *client, char *path) {
    char *file;
    response rsp;
    char filepath[SBUFLEN];
    int code;
    sprintf(filepath, "WebFiles%s", path);
    if (0 != access(filepath, F_OK)) { // Check the existence of the file
        perror("access");
        send_error(client, 404);
        return 404;
    }
    if (NULL == (file = prepare_file(path, &rsp.content_length, rsp.content_type))) {
        send_error(client, 500);
        return 500;
    } else {
        rsp.status_code = 200;
        code = send_response(client, &rsp, file);
    }
    free(file);
    return code;
}

int process_request(FILE *client, ka *keep_alive, char **headers, int num_headers, int len_header){
    int i = 0;
    while (i < num_headers) {
        while (NULL == fgets(headers[i], len_header, client)) {
            if (! (errno == EAGAIN || errno == EWOULDBLOCK) || feof(client)) {
                perror("fgets");
                send_error(client, 500);
                return 500;
            }
        }
        printf("%s", headers[i]);
        if (! strcmp(headers[i], "\n") || ! strcmp(headers[i], "\r\n")) {
            break;
        }
        i++;
    }

    int code;
    if (i >= MAXHEADERS) {
        send_error(client, 431);
        return 431;
    } else {
        char method[SSBUFLEN] = {0}, path[SBUFLEN] = {0}, protocol[SSBUFLEN] = {0};
        char format[32];
        sprintf(format, "%%%lis %%%lis %%%lis", sizeof(method), sizeof(path), sizeof(protocol));
        sscanf(headers[0], format, method, path, protocol);
        if (! strcmp(method, "GET")) {
            if (protocol[0] != 0 && (strncmp(protocol, "HTTP/1.", 7) || (protocol[7] != '0' && protocol[7] != '1'))) { // Not an HTTP request, consider no protocol to still be HTTP
                send_error(client, 400);
                return 400;
            } else {
                if (NULL != strstr(path, "/../")) { // Prevent people from accessing stuff outside the folder
                    send_error(client, 403);
                    return 403;
                } else {
                    if (0 == path[0] || ! strcmp(path, "/")) {
                        strcpy(path, "/index.html");
                    }
                    code = send_file(client, path);
                    printf("%d\n", code);
                    if (keep_alive != NULL && code >= 200 && code < 300) {
                        for (i = 0; i < num_headers; i++) {
                            if (str_start(headers[i], "Connection: ")) {
                                if (! strcmp(headers[i], "Connection: keep-alive\r\n")) {
                                    keep_alive->max = 99;
                                    keep_alive->timeout = 4;
                                    for (i = 0; i < num_headers; i++) {
                                        if (str_start(headers[i], "Keep-Alive: ")) {
                                            char *tmp = strstr(headers[i], "timeout");
                                            if (tmp == NULL) {
                                                break;
                                            }
                                            keep_alive->timeout = strtol(tmp + strlen("timeout="), NULL, 10);
                                            tmp = strstr(headers[i], "max");
                                            if (tmp == NULL) {
                                                keep_alive->timeout = 0;
                                                break;
                                            }
                                            keep_alive->max = strtol(tmp + strlen("max="), NULL, 10);
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        } else {
            send_error(client, 400);
            return 400;
        }
    }
    return code;
}

void process_client(FILE *client, int client_fd) {
    ka keep_alive = {0, 0};
    char *headers[MAXHEADERS];
    for (int i = 0; i < MAXHEADERS; i++) {
        if (NULL == (headers[i] = malloc(BUFLEN))) {
            perror("malloc");
            for (int j = 0; j < i; j++) {
                free(headers[j]);
            }
            send_error(client, 500);
            fclose(client);
            return;
        }
    }

    int code = process_request(client, &keep_alive, headers, MAXHEADERS, BUFLEN);
    int max = 0;

    if (code >= 200 && code < 300 && keep_alive.max > 0 && keep_alive.timeout > 0) {
        max = keep_alive.max - 1;
    }
    
    struct pollfd fds;
    fds.events = POLLIN;
    fds.fd = client_fd;
    int ret;
    while (max > 0) {
        ret = poll(&fds, 1, 1000 * keep_alive.timeout);
        if (ret == 0) {
            break;
        } else if (ret < 0) {
            perror("poll");
            break;
        }
        if (feof(client)) {
            break;
        }
        code = process_request(client, NULL, headers, MAXHEADERS, BUFLEN);
        if (code < 200 || code >= 300) {
            break;
        }
        max--;
    }

    fclose(client);
    for (int i = 0; i < MAXHEADERS; i++) {
        free(headers[i]);
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in address;
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > server_fd) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(8080);

    if (-1 == bind(server_fd, (struct sockaddr *) &address, sizeof(address))) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (-1 == listen(server_fd, BACKLOG)) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    int client_fd;
    FILE *client;

    while (1) {
        if (-1 == (client_fd = accept(server_fd, NULL, NULL))) {
            perror("accept");
            close(server_fd);
            return 1;
        }
        if (-1 == fcntl(client_fd, F_SETFL, O_NONBLOCK)) {
            perror("fcntl");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        client = fdopen(client_fd, "r+");
        process_client(client, client_fd);
    }

    close(server_fd);
    if (client != NULL) {
        fclose(client);
    }
    return 0;
}