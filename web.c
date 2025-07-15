#include "web.h"

int hg_init(headergroup *hg) {
    hg->headers = malloc(MAXHEADERS * sizeof(header));
    if (hg->headers == NULL) {
        perror("malloc");
        return 1;
    }
    hg->num_headers = MAXHEADERS;
    hg->headers_used = 0;
    return 0;
}

int hg_add(headergroup *hg, const char *name, const char *value) {
    if (hg->headers_used >= hg->num_headers) {
        header *new_headers = realloc(hg->headers, (hg->num_headers + MAXHEADERS) * sizeof(header));
        if (new_headers == NULL) {
            perror("realloc");
            return 1;
        }
        hg->headers = new_headers;
        hg->num_headers += MAXHEADERS;
    }
    strncpy(hg->headers[hg->headers_used].name, name, SSBUFLEN - 1);
    strncpy(hg->headers[hg->headers_used].value, value, BUFLEN - 1);
    hg->headers[hg->headers_used].name[SSBUFLEN - 1] = '\0';
    hg->headers[hg->headers_used].value[BUFLEN - 1] = '\0';
    hg->headers_used++;
    return 0;
}

void hg_destroy(headergroup *hg) {
    free(hg->headers);
    hg->headers = NULL;
    hg->num_headers = 0;
    hg->headers_used = 0;
}

void sig_catch(int signum) {
    if (signum == SIGINT) {
        printf("\nServer shutting down...\n");
        exit(0);
    }
}

int sanitize_http_path(char *path, char *out, int len) {
    // TODO: Handle HTTP escape sequences like %20, and re-escape them when calling send_redirect

    char current_slash = 0;
    int i = 0, j = 0;
    while (j < len && path[i] != 0) {
        if (current_slash && (! strncmp(path + i, "../", 3) || ! strncmp(path + i, "..\\", 3) || (strlen(path + i) == 2 && ! strncmp(path + i, "..", 2)))) { // Ignore "/../" sequences
            i += 3;
            continue;
        }
        if (current_slash && (! strncmp(path + i, "./", 2) || ! strncmp(path + i, ".\\", 2) || (strlen(path + i) == 1 && ! path[i] == '.'))) { // Ignore "/./" sequences
            i += 2;
            continue;
        }
        if (path[i] == '/' || path[i] == '\\') {
            i++;
            if (current_slash != 0) {
                continue; // Skip consecutive slashes
            }
            out[j++] = '/';
            current_slash = 1;
        } else {
            current_slash = 0;
            out[j++] = path[i++];
        }
    }
    if (j == len) {
        return 1;
    }
    if (j > 0 && out[j - 1] == '/') {
        j--; // Remove trailing slash
    }
    out[j] = 0; // Null-terminate the string
    if (j == 0) {
        out[0] = '/'; // Ensure at least a root slash
        out[1] = 0;
    }
    return 0;
}

char *build_header(response *rsp) {
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
    } else if (rsp->status_code == 301) {
        if (! sb_append("HTTP/1.1 301 Moved Permanently\r\n", &dest)) {
            sb_destroy(&dest);
            return NULL;
        }
    } else if (rsp->status_code == 302) {
        if (! sb_append("HTTP/1.1 302 Found\r\n", &dest)) {
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
    } else if (rsp->status_code == 405) {
        if (! sb_append("HTTP/1.1 405 Method Not Allowed\r\n", &dest)) {
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

    for (int i = 0; i < rsp->headers.headers_used; i++) {
        if (! sb_append(rsp->headers.headers[i].name, &dest) ||
            ! sb_append(": ", &dest) ||
            ! sb_append(rsp->headers.headers[i].value, &dest) ||
            ! sb_append("\r\n", &dest)) {
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

int prepare_file(char *path, file_response *fr) {
    char filepath[SBUFLEN];
    sprintf(filepath, "WebFiles%s", path);
    if (0 != access(filepath, F_OK)) { // Check the existence of the file
        perror("access");
        return 1;
    }
    strcpy(fr->file_path, filepath);
    if (fr->file_path == NULL) {
        perror("strdup");
        return 1;
    }
    struct stat st;
    if (0 != stat(filepath, &st)) {
        perror("stat");
        return 1;
    }
    fr->file_size = (long) st.st_size;
    if (str_end(path, ".html")) {
        strcpy(fr->type, "text/html");
    } else if (str_end(path, ".ico")) {
        strcpy(fr->type, "image/x-icon");
    } else {
        fr->type[0] = 0;
    }
    return 0;
}

void send_500(FILE *client) { // Sends a status code 500 to the client, should only be called if send_error or send_response fails, otherwise call send_error(client, 500)
    fprintf(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n500 Internal Server Error");
    fflush(client);
}

int send_response(FILE *client, response *rsp, file_response *fr) {
    char *head;
    if (NULL == (head = build_header(rsp))) {
        send_500(client);
        return 500;
    }
    fprintf(client, "%s", head);
    if (NULL != fr) {
        char buf[LLBUFLEN];
        FILE *file = fopen(fr->file_path, "rb");
        if (file == NULL) {
            perror("fopen");
            free(head);
            send_500(client);
            return 500;
        }
        int bytes_read = 0;
        while (LLBUFLEN == (bytes_read = fread(buf, 1, LLBUFLEN, file))) {
            fwrite(buf, 1, bytes_read, client);
        }
        fwrite(buf, 1, bytes_read, client);
        if (! feof(file)) {
            perror("fread");
            fclose(file);
            free(head);
            send_500(client);
            return 500;
        }
        fclose(file);
    }
    fflush(client);
    free(head);
    return rsp->status_code;
}

int send_error(FILE *client, int code) {
    response rsp;
    if (0 != hg_init(&rsp.headers)) {
        send_500(client);
        return 500;
    }
    rsp.status_code = code;
    char path[16];
    if (code == 400) {
        strcpy(path, "/400.html");
    } else if (code == 403) {
        strcpy(path, "/403.html");
    } else if (code == 404) {
        strcpy(path, "/404.html");
    } else if (code == 405) {
        strcpy(path, "/405.html");
    } else if (code == 431) {
        strcpy(path, "/431.html");
    } else if (code == 500) {
        strcpy(path, "/500.html");
    } else {
        strcpy(path, "/500.html");
    }
    file_response fr;
    if (0 != prepare_file(path, &fr)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    char len_str[16];
    sprintf(len_str, "%ld", fr.file_size);
    if (0 != hg_add(&rsp.headers, "Content-Length", len_str)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    if (0 != hg_add(&rsp.headers, "Content-Type", fr.type)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    code = send_response(client, &rsp, &fr);
    hg_destroy(&rsp.headers);
    return code;
}

int send_redirect(FILE *client, char *location, int code) {
    response rsp;
    if (0 != hg_init(&rsp.headers)) {
        send_error(client, 500);
        return 500;
    }
    rsp.status_code = code;
    if (0 != hg_add(&rsp.headers, "Location", location)) {
        send_error(client, 500);
        hg_destroy(&rsp.headers);
        return 500;
    }
    char path[16];
    if (code == 301) {
        strcpy(path, "/301.html");
    } else if (code == 302) {
        strcpy(path, "/302.html");
    } else {
        send_error(client, 500);
        hg_destroy(&rsp.headers);
        return 500;
    }
    file_response fr;
    if (0 != prepare_file(path, &fr)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    char len_str[16];
    sprintf(len_str, "%ld", fr.file_size);
    if (0 != hg_add(&rsp.headers, "Content-Length", len_str)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    if (0 != hg_add(&rsp.headers, "Content-Type", fr.type)) {
        send_500(client);
        hg_destroy(&rsp.headers);
        return 500;
    }
    int c = send_response(client, &rsp, &fr);
    hg_destroy(&rsp.headers);
    return c;
}

int send_file(FILE *client, char *path) {
    response rsp;
    if (0 != hg_init(&rsp.headers)) {
        send_error(client, 500);
        return 500;
    }
    rsp.status_code = 200;
    char filepath[SBUFLEN];
    int code;
    sprintf(filepath, "WebFiles%s", path);
    if (0 != access(filepath, F_OK)) { // Check the existence of the file
        perror("access");
        send_error(client, 404);
        hg_destroy(&rsp.headers);
        return 404;
    }
    file_response fr;
    if (0 != prepare_file(path, &fr)) {
        send_error(client, 500);
        hg_destroy(&rsp.headers);
        return 500;
    } else {
        rsp.status_code = 200;
        char len_str[16];
        sprintf(len_str, "%ld", fr.file_size);
        if (0 != hg_add(&rsp.headers, "Content-Length", len_str)) {
            send_500(client);
            hg_destroy(&rsp.headers);
            return 500;
        }
        if (0 != hg_add(&rsp.headers, "Content-Type", fr.type)) {
            send_500(client);
            hg_destroy(&rsp.headers);
            return 500;
        }
        code = send_response(client, &rsp, &fr);
    }
    hg_destroy(&rsp.headers);
    return code;
}

int process_request(FILE *client, ka *keep_alive, headergroup *headers) {
    char *initial_line = malloc(LBUFLEN);
    if (NULL == initial_line) {
        perror("malloc");
        send_error(client, 500);
        return 500;
    }

    while (NULL == fgets(initial_line, LBUFLEN, client)) {
        if (! (errno == EAGAIN || errno == EWOULDBLOCK) || feof(client)) {
            perror("fgets");
            free(initial_line);
            send_error(client, 500);
            return 500;
        }
    }

    printf("%s", initial_line);

    if (! str_end(initial_line, "\n") && ! str_end(initial_line, "\r\n")) {
        free(initial_line);
        send_error(client, 400);
        return 400;
    }

    char method[SSBUFLEN] = {0}, path[LBUFLEN] = {0}, protocol[SSBUFLEN] = {0};
    char format[32];
    sprintf(format, "%%%lis %%%lis %%%lis", sizeof(method), sizeof(path), sizeof(protocol));
    sscanf(initial_line, format, method, path, protocol);
    free(initial_line);

    char sanitized_path[LBUFLEN];
    if (0 != sanitize_http_path(path, sanitized_path, LBUFLEN)) {
        send_error(client, 431);
        return 431;
    }

    if (protocol[0] != 0 && (strncmp(protocol, "HTTP/1.", 7) || (protocol[7] != '0' && protocol[7] != '1'))) { // Not an HTTP request, consider no protocol to still be HTTP
        send_error(client, 400);
        return 400;
    }

    int i = 0;
    while (i < MAXHEADERS) {
        char header[BUFLEN];
        while (NULL == fgets(header, BUFLEN, client)) {
            if (! (errno == EAGAIN || errno == EWOULDBLOCK) || feof(client)) {
                perror("fgets");
                send_error(client, 500);
                return 500;
            }
        }
        if (! str_end(header, "\n") && ! str_end(header, "\r\n")) {
            send_error(client, 431);
            return 431;
        }
        if (! strcmp(header, "\n") || ! strcmp(header, "\r\n")) { // Blank line indicates end of headers
            break;
        }
        printf("%s", header);
        char header_name[SSBUFLEN], header_value[BUFLEN];
        if (2 != sscanf(header, "%63[^:]: %1023[^\r\n]", header_name, header_value)) {
            send_error(client, 400);
            return 400;
        }
        if (0 != hg_add(headers, header_name, header_value)) {
            send_error(client, 500);
            return 500;
        }
        i++;
    }

    if (0 != strcmp(path, sanitized_path)) {
        char new_path[LLBUFLEN];
        sprintf(new_path, "http://localhost:8080%s", sanitized_path);
        send_redirect(client, new_path, 301);
        return 301;
    }

    if (i >= MAXHEADERS) {
        send_error(client, 431);
        return 431;
    }

    int code;
    if (! strcmp(method, "GET")) {
        if (! strcmp(sanitized_path, "/")) {
            strcpy(sanitized_path, "/index.html");
        }
        code = send_file(client, sanitized_path);
        printf("%d\n\n", code);
        if (keep_alive != NULL && code >= 200 && code < 300) {
            for (i = 0; i < headers->headers_used; i++) {
                if (! strcmp(headers->headers[i].name, "Connection")) {
                    if (! strcmp(headers->headers[i].value, "keep-alive")) {
                        keep_alive->max = 99;
                        keep_alive->timeout = 4;
                        for (i = 0; i < headers->headers_used; i++) {
                            if (! strcmp(headers->headers[i].name, "Keep-Alive")) {
                                char *tmp = strstr(headers->headers[i].value, "timeout");
                                if (tmp == NULL) {
                                    break;
                                }
                                keep_alive->timeout = strtol(tmp + strlen("timeout="), NULL, 10);
                                tmp = strstr(headers->headers[i].value, "max");
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
    } else {
        send_error(client, 405);
        return 405;
    }
    return code;
}

void process_client(FILE *client, int client_fd) {
    ka keep_alive = {0, 0};
    headergroup headers;
    if (0 != hg_init(&headers)) {
        send_error(client, 500);
        fclose(client);
        return;
    }

    int code = process_request(client, &keep_alive, &headers);
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
        code = process_request(client, NULL, &headers);
        if (code < 200 || code >= 300) {
            break;
        }
        max--;
    }

    fclose(client);
    hg_destroy(&headers);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in address;
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > server_fd) {
        perror("socket");
        return 1;
    }

    if (0 != setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    signal(SIGINT, sig_catch);

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